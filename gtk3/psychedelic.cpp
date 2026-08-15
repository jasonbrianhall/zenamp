#include "visualization.h"
#include <stdlib.h>

// ---- Psychedelic Vortex ----
// Neon flow-lines drawn with a curl-noise field, blended with an inward pull
// toward the center so they spiral into a dark "tunnel mouth" - blue/cyan
// dominant with a minority of magenta/pink lines, glowing on a near-black
// backdrop, all audio reactive.

#define PSY_LINE_COUNT 46
#define PSY_STEPS 70

typedef struct {
    double angle;       // starting angle around the outer ring
    double radius_jit;  // per-line radius jitter so the ring isn't uniform
    double phase;       // per-line time phase offset (keeps lines distinct)
    bool magenta;       // minority of lines pop pink/magenta instead of blue/cyan
} PsyLine;

static PsyLine s_psy_lines[PSY_LINE_COUNT];
static bool s_psy_lines_init = false;

static cairo_surface_t *s_psy_trail = NULL;
static int s_psy_trail_w = 0, s_psy_trail_h = 0;

static void hsv_to_rgb_psy(double h, double s, double v, double *r, double *g, double *b) {
    h = fmod(h, 1.0);
    if (h < 0) h += 1.0;
    double i = floor(h * 6.0);
    double f = h * 6.0 - i;
    double p = v * (1.0 - s);
    double q = v * (1.0 - f * s);
    double t = v * (1.0 - (1.0 - f) * s);
    switch ((int)i % 6) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

static void psy_ensure_trail(int width, int height) {
    if (s_psy_trail && s_psy_trail_w == width && s_psy_trail_h == height) return;
    if (s_psy_trail) cairo_surface_destroy(s_psy_trail);
    s_psy_trail = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    s_psy_trail_w = width;
    s_psy_trail_h = height;
    cairo_t *c = cairo_create(s_psy_trail);
    cairo_set_operator(c, CAIRO_OPERATOR_CLEAR);
    cairo_paint(c);
    cairo_destroy(c);
}

static void psy_init_lines() {
    if (s_psy_lines_init) return;
    for (int i = 0; i < PSY_LINE_COUNT; i++) {
        s_psy_lines[i].angle = (double)i / PSY_LINE_COUNT * 2.0 * M_PI;
        s_psy_lines[i].radius_jit = 0.85 + 0.3 * ((double)rand() / RAND_MAX);
        s_psy_lines[i].phase = (double)rand() / RAND_MAX * 1000.0;
        s_psy_lines[i].magenta = (i % 7 == 0); // roughly 1 in 7 lines pop magenta
    }
    s_psy_lines_init = true;
}

// Curl of a scalar potential built from a few time-shifted sine waves. Gives
// each flow-line its organic, swirling shape (classic "curl noise" trick).
static void psy_field(double x, double y, double t, double *vx, double *vy) {
    const double e = 0.01;
    double p1  = sin(x * 1.6 + t) + sin(y * 1.3 - t * 0.8) + sin((x + y) * 0.9 + t * 0.5);
    double p2x = sin((x + e) * 1.6 + t) + sin(y * 1.3 - t * 0.8) + sin(((x + e) + y) * 0.9 + t * 0.5);
    double p2y = sin(x * 1.6 + t) + sin((y + e) * 1.3 - t * 0.8) + sin((x + (y + e)) * 0.9 + t * 0.5);
    double dpsi_dx = (p2x - p1) / e;
    double dpsi_dy = (p2y - p1) / e;
    *vx = dpsi_dy;   // curl(psi) = (d/dy, -d/dx)
    *vy = -dpsi_dx;
}

static double s_psy_energy = 0.0;       // smoothed VU-style envelope: fast attack, slow decay
static double s_psy_extra_angle = 0.0;  // audio-driven spin, on top of vis->rotation

void init_psychedelic_system(Visualizer *vis) {
    (void)vis;
    psy_init_lines();
    s_psy_energy = 0.0;
    s_psy_extra_angle = 0.0;
}

void update_psychedelic(Visualizer *vis, double dt) {
    // Boost the raw RMS volume so typical listening levels actually swing
    // the envelope, then chase it with a fast attack / slow decay so hits
    // punch in immediately but settle out smoothly (classic VU behavior).
    double target = vis->volume_level * 2.6;
    if (target > 1.0) target = 1.0;
    double rate = (target > s_psy_energy) ? 9.0 : 2.2;
    s_psy_energy += (target - s_psy_energy) * fmin(1.0, dt * rate);
    if (s_psy_energy < 0.0) s_psy_energy = 0.0;

    // The whole field spins faster the louder it gets - this is the clearest
    // "it's reacting to the music" cue, and it persists across frames instead
    // of being recomputed from scratch each draw.
    s_psy_extra_angle += (0.15 + s_psy_energy * 2.6) * dt;
    if (s_psy_extra_angle > 2.0 * M_PI) s_psy_extra_angle -= 2.0 * M_PI;
}

void draw_psychedelic(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;

    psy_init_lines();
    psy_ensure_trail(vis->width, vis->height);

    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    double max_r = fmin(center_x, center_y);
    double t = vis->time_offset;
    double energy = s_psy_energy;

    // Deep indigo/near-black backdrop - this look is specific to the vortex,
    // so it's painted directly rather than using the user's configured bg.
    cairo_set_source_rgb(cr, 0.02, 0.015, 0.06);
    cairo_paint(cr);

    // Fade the persistent glow trail slightly each frame instead of
    // clearing it, so the neon lines leave a soft, bloomy afterglow.
    cairo_t *tcr = cairo_create(s_psy_trail);
    cairo_set_operator(tcr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(tcr, 0, 0, 0, 0.18);
    cairo_paint(tcr);
    cairo_set_operator(tcr, CAIRO_OPERATOR_ADD);
    cairo_set_line_join(tcr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(tcr, CAIRO_LINE_CAP_ROUND);

    // Bass/volume pulls lines into the vortex much harder on loud passages
    // and barely at all when quiet; the envelope also speeds up the pull.
    double sink_strength = 0.25 + energy * 1.9;
    double step_len = max_r * (0.016 + energy * 0.05);
    double avg_mag = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) avg_mag += vis->frequency_bands[i];
    avg_mag /= VIS_FREQUENCY_BARS;

    for (int li = 0; li < PSY_LINE_COUNT; li++) {
        PsyLine *line = &s_psy_lines[li];
        int band = li % VIS_FREQUENCY_BARS;
        double magnitude = vis->frequency_bands[band];

        double start_r = max_r * 0.98 * line->radius_jit;
        double angle = line->angle + vis->rotation * 0.3 + s_psy_extra_angle;
        double px = cos(angle) * start_r;
        double py = sin(angle) * start_r;

        double points_x[PSY_STEPS], points_y[PSY_STEPS];
        int npts = 0;

        // Each band drives how forcefully its own lines swirl - quiet bands
        // barely drift, loud bands whip around, so the spectrum is legible.
        double drive = 0.25 + magnitude * 1.75;

        for (int s = 0; s < PSY_STEPS; s++) {
            points_x[npts] = px;
            points_y[npts] = py;
            npts++;

            double dist = sqrt(px * px + py * py);
            if (dist < max_r * 0.08) break; // reached the tunnel mouth

            double fx, fy;
            psy_field(px / max_r * 3.0, py / max_r * 3.0, t * 0.6 + line->phase, &fx, &fy);
            double flen = sqrt(fx * fx + fy * fy) + 1e-6;
            fx /= flen; fy /= flen;

            // Inward pull toward the vortex, blended with the swirl so
            // lines spiral into the center like the tunnel look.
            double nx = -px / (dist + 1e-6);
            double ny = -py / (dist + 1e-6);

            double vx = fx * drive + nx * sink_strength;
            double vy = fy * drive + ny * sink_strength;

            px += vx * step_len;
            py += vy * step_len;
        }

        if (npts < 2) continue;

        double hue = line->magenta
                   ? fmod(0.85 + avg_mag * 0.1, 1.0)     // pink/magenta band
                   : fmod(0.55 + magnitude * 0.15, 1.0);  // blue/cyan band

        // Brightness tracks this line's own band plus the overall envelope,
        // so quiet bands sink toward the background and loud ones flare up.
        double val = 0.35 + magnitude * 0.45 + energy * 0.3;
        if (val > 1.0) val = 1.0;

        double r_, g_, b_;
        hsv_to_rgb_psy(hue, 0.85, val, &r_, &g_, &b_);

        cairo_move_to(tcr, center_x + points_x[0], center_y + points_y[0]);
        for (int p = 1; p < npts; p++) {
            cairo_line_to(tcr, center_x + points_x[p], center_y + points_y[p]);
        }

        double glow_alpha = 0.06 + energy * 0.10 + magnitude * 0.06;
        double core_alpha = 0.45 + energy * 0.35 + magnitude * 0.2;
        if (core_alpha > 1.0) core_alpha = 1.0;

        // Layered glow: a soft, wide, low-alpha pass under a bright thin core.
        cairo_set_source_rgba(tcr, r_, g_, b_, glow_alpha);
        cairo_set_line_width(tcr, (6.0 + magnitude * 8.0) * (0.6 + energy * 0.8));
        cairo_stroke_preserve(tcr);

        cairo_set_source_rgba(tcr, r_ * 0.6 + 0.4, g_ * 0.6 + 0.4, b_ * 0.6 + 0.4, core_alpha);
        cairo_set_line_width(tcr, 1.2 + magnitude * 1.0);
        cairo_stroke(tcr);
    }

    cairo_destroy(tcr);

    cairo_set_operator(cr, CAIRO_OPERATOR_ADD);
    cairo_set_source_surface(cr, s_psy_trail, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Dark tunnel mouth at the center, breathing hard with the beat.
    double mouth_r = max_r * (0.045 + energy * 0.22);
    cairo_pattern_t *grad = cairo_pattern_create_radial(center_x, center_y, 0,
                                                          center_x, center_y, mouth_r * 2.2);
    cairo_pattern_add_color_stop_rgba(grad, 0.0, 0.0, 0.0, 0.0, 1.0);
    cairo_pattern_add_color_stop_rgba(grad, 0.5, 0.02, 0.01, 0.05, 0.9);
    cairo_pattern_add_color_stop_rgba(grad, 1.0, 0.02, 0.01, 0.05, 0.0);
    cairo_set_source(cr, grad);
    cairo_arc(cr, center_x, center_y, mouth_r * 2.2, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(grad);

    // Bright rim flash right at the mouth on hits, additive so it glows.
    if (energy > 0.05) {
        cairo_set_operator(cr, CAIRO_OPERATOR_ADD);
        cairo_set_source_rgba(cr, 0.6, 0.8, 1.0, energy * 0.5);
        cairo_set_line_width(cr, 2.0 + energy * 3.0);
        cairo_arc(cr, center_x, center_y, mouth_r * 1.15, 0, 2 * M_PI);
        cairo_stroke(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    }
}
