#include "floppyfish_common.h"

// --- Volcanic vents theme --------------------------------------------------
// A deep-sea hydrothermal vent field - the twelfth theme zone alongside
// reef/ship/cave/atlantis/rainbow/dino/antarctic/aquarium/galaxy/swamp/party.
// Obstacles are black-smoker chimney stacks with glowing magma cracks
// running through the rock and a billowing dark smoke plume at the
// gap-facing tip, the floor is black basalt scattered with glowing mineral
// nodules, the seaweed slot becomes a cluster of tube worms, and the whole
// scene sits under red vent-glow lighting rather than open-water blue.

void ff_volcanic_sky_colors(double *top_r, double *top_g, double *top_b,
                             double *bot_r, double *bot_g, double *bot_b) {
    // Near-black abyssal water up top, fading down into a dull vent-glow red
    // near the floor - the theme's signature lighting.
    *top_r = 0.05; *top_g = 0.02; *top_b = 0.03;
    *bot_r = 0.30; *bot_g = 0.08; *bot_b = 0.06;
}

void ff_volcanic_particle_color(double *r, double *g, double *b, double *a) {
    // Rising embers/mineral bubbles, warm orange rather than the usual pale
    // air bubble.
    *r = 1.0; *g = 0.55; *b = 0.20; *a = 0.35;
}

// Ember/magma glow tones shared by the chimney cracks and the distant vent
// field glow, so the palette reads consistently everywhere it shows up.
typedef struct {
    double glow_r, glow_g, glow_b;
} FFEmberColor;

static const FFEmberColor FF_EMBER_COLORS[] = {
    {1.00, 0.55, 0.10}, // orange
    {1.00, 0.30, 0.08}, // red-orange
    {1.00, 0.78, 0.20}, // yellow-orange
    {0.95, 0.15, 0.10}, // deep red
};
#define FF_EMBER_COLOR_COUNT (int)(sizeof(FF_EMBER_COLORS) / sizeof(FF_EMBER_COLORS[0]))

// A single tube worm: a pale stalk with a feathery red plume at the tip,
// used both by the seaweed slot and scattered around the backdrop chimneys.
static void ff_draw_volcanic_tubeworm(cairo_t *cr, double x, double base_y, double h,
                                       double t, double alpha_mult) {
    double sway = sin(t * 0.7) * h * 0.10;
    double tip_x = x + sway, tip_y = base_y - h;

    cairo_set_source_rgba(cr, 0.88, 0.85, 0.78, 0.85 * alpha_mult);
    cairo_set_line_width(cr, h * 0.14);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, x, base_y);
    cairo_curve_to(cr, x + sway * 0.4, base_y - h * 0.4,
                        x + sway * 0.8, base_y - h * 0.75,
                        tip_x, tip_y);
    cairo_stroke(cr);

    // Feathery red plume at the tip.
    cairo_set_source_rgba(cr, 0.85, 0.12, 0.10, 0.85 * alpha_mult);
    for (int i = -1; i <= 1; i++) {
        double fx = tip_x + i * h * 0.12;
        double fy = tip_y - h * 0.10 + fabs(i) * h * 0.05;
        cairo_move_to(cr, tip_x, tip_y);
        cairo_line_to(cr, fx, fy);
        cairo_set_line_width(cr, h * 0.05);
        cairo_stroke(cr);
    }
}

// Distant black-smoker chimneys jutting up from the seafloor, each leaking
// a faint glow at its base and a thin dark smoke plume, plus a scatter of
// small glowing vents between them - this theme's equivalent of the ship's
// hull or Atlantis's temple: the one backdrop that most says "volcanic
// vent field" at a glance.
void ff_draw_volcanic_backdrop(cairo_t *cr, double w, double h, double base_y) {
    for (int i = 0; i < 5; i++) {
        double cx = w * (0.10 + 0.20 * i);
        double ch = h * (0.16 + 0.10 * fabs(sin(i * 1.7)));
        double cw = ch * 0.30;

        cairo_set_source_rgba(cr, 0.10, 0.07, 0.07, 0.70);
        cairo_move_to(cr, cx - cw * 0.5, base_y);
        cairo_line_to(cr, cx - cw * 0.28, base_y - ch);
        cairo_line_to(cr, cx + cw * 0.28, base_y - ch);
        cairo_line_to(cr, cx + cw * 0.5, base_y);
        cairo_close_path(cr);
        cairo_fill(cr);

        // Glowing crack up the side of the chimney.
        const FFEmberColor *col = &FF_EMBER_COLORS[i % FF_EMBER_COLOR_COUNT];
        cairo_set_source_rgba(cr, col->glow_r, col->glow_g, col->glow_b, 0.55);
        cairo_set_line_width(cr, fmax(1.0, cw * 0.06));
        cairo_move_to(cr, cx - cw * 0.05, base_y);
        cairo_line_to(cr, cx + cw * 0.05, base_y - ch * 0.5);
        cairo_line_to(cr, cx - cw * 0.02, base_y - ch * 0.85);
        cairo_stroke(cr);

        // Base glow pooling around the foot of the chimney.
        cairo_set_source_rgba(cr, col->glow_r, col->glow_g, col->glow_b, 0.20);
        cairo_arc(cr, cx, base_y, cw * 0.7, 0, 2 * M_PI);
        cairo_fill(cr);

        // Thin dark smoke plume drifting up from the tip.
        cairo_set_source_rgba(cr, 0.05, 0.05, 0.06, 0.35);
        double px = cx, py = base_y - ch;
        cairo_move_to(cr, px - cw * 0.14, py);
        for (int k = 1; k <= 4; k++) {
            double t = (double)k / 4.0;
            px += sin(i * 3.1 + k) * cw * 0.10;
            cairo_line_to(cr, px + cw * 0.14 * (1.0 + t), py - h * 0.10 * t);
        }
        for (int k = 4; k >= 0; k--) {
            double t = (double)k / 4.0;
            cairo_line_to(cr, px - cw * 0.14 * (1.0 + t), py - h * 0.10 * t);
        }
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    // A couple of tube worm clusters rooted directly in the backdrop.
    ff_draw_volcanic_tubeworm(cr, w * 0.24, base_y, h * 0.05, 0.6, 0.6);
    ff_draw_volcanic_tubeworm(cr, w * 0.62, base_y, h * 0.04, 1.4, 0.6);
}

// Static: the black basalt base fill. No bubble_phase dependency, so this
// is the cacheable part.
void ff_draw_volcanic_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.09, 0.07, 0.07);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
}

// Scroll: drifting cracks in the basalt, plus a scatter of glowing mineral
// nodules that pulse with bubble_phase - this theme's equivalent of the
// ship's coins or the Atlantis floor's gems.
void ff_draw_volcanic_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    cairo_set_source_rgba(cr, 0.55, 0.20, 0.08, 0.5);
    for (double x = -fmod(bubble_phase * (h * 0.28), 46.0); x < w; x += 46.0) {
        cairo_move_to(cr, x, h - floor_h);
        cairo_line_to(cr, x + 9, h);
        cairo_set_line_width(cr, 2.5);
        cairo_stroke(cr);
    }

    double pulse = 0.65 + 0.35 * sin(bubble_phase * 3.0);
    for (int i = 0; i < 10; i++) {
        double nx = fmod(i * 163.0 + 40.0, w);
        double ny = h - floor_h * (0.20 + 0.45 * ((i * 47) % 5) / 5.0);
        double s = floor_h * 0.05;
        const FFEmberColor *col = &FF_EMBER_COLORS[i % FF_EMBER_COLOR_COUNT];

        // Soft glow halo, pulsing.
        cairo_set_source_rgba(cr, col->glow_r, col->glow_g, col->glow_b, 0.25 * pulse);
        cairo_arc(cr, nx, ny, s * 2.0, 0, 2 * M_PI);
        cairo_fill(cr);

        // Bright nodule core.
        cairo_set_source_rgba(cr, col->glow_r, col->glow_g, col->glow_b, 0.9);
        cairo_arc(cr, nx, ny, s * (0.7 + 0.3 * pulse), 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Fissures cracking open the foreground floor, each hissing a
    // continuous stream of small gas bubbles up past the obstacles into
    // the water column - drawn after everything else in the floor's
    // live/scroll layer (and so on top of the pipes, which paint earlier
    // in draw_floppy_fish), the theme's most immediate "the ground itself
    // is venting" detail. Fixed screen-space x positions, same as the
    // mineral nodules above, rather than scrolling with the world.
    static const double FF_FISSURE_X_FRAC[3] = {0.18, 0.52, 0.84};
    for (int i = 0; i < 3; i++) {
        double fx = w * FF_FISSURE_X_FRAC[i];
        double fy = h - floor_h * 0.32;
        const FFEmberColor *col = &FF_EMBER_COLORS[i % FF_EMBER_COLOR_COUNT];

        // The crack itself: a jagged bright split splitting the basalt,
        // with a soft glow halo behind it.
        cairo_set_source_rgba(cr, col->glow_r, col->glow_g, col->glow_b, 0.30);
        cairo_set_line_width(cr, floor_h * 0.22);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, fx - floor_h * 0.10, h);
        cairo_line_to(cr, fx - floor_h * 0.02, fy);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, col->glow_r, col->glow_g, col->glow_b, 0.9);
        cairo_set_line_width(cr, floor_h * 0.05);
        cairo_move_to(cr, fx - floor_h * 0.10, h);
        cairo_line_to(cr, fx + floor_h * 0.04, fy + floor_h * 0.15);
        cairo_line_to(cr, fx - floor_h * 0.02, fy);
        cairo_stroke(cr);

        // Small bubbles rising from the crack mouth, looping continuously
        // rather than spawning/dying, and fading out the higher they climb
        // so they dissolve into the water instead of popping off-screen.
        for (int k = 0; k < 6; k++) {
            double speed = h * (0.35 + 0.08 * (k % 3));
            double travel = h * 0.85;
            double rise = fmod(bubble_phase * speed + k * 97.0 + i * 251.0, travel);
            double by = fy - rise;
            double wobble = sin(bubble_phase * 4.0 + k * 1.9 + i * 3.1) * floor_h * 0.12;
            double bx = fx + wobble;
            double br = floor_h * (0.025 + 0.02 * ((k * 31 + i * 13) % 5) / 5.0);
            double fade = 1.0 - rise / travel;
            cairo_set_source_rgba(cr, 1.0, 0.6, 0.25, 0.55 * fade);
            cairo_arc(cr, bx, by, br, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
}

// Tube worms standing in for seaweed - a small cluster of the theme's
// signature critter in the same three-strand slot as every other theme's
// decoration.
void ff_draw_volcanic_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    static const double off_x[3] = {-0.10, 0.0, 0.11};
    static const double h_mult[3] = {0.65, 1.0, 0.78};
    for (int i = 0; i < 3; i++) {
        double wx = x + off_x[i] * height;
        double wh = height * h_mult[i];
        ff_draw_volcanic_tubeworm(cr, wx, base_y, wh, t * 1.1 + i * 1.7, alpha_mult);
    }
}

// One black-rock segment of a smoker chimney: a rectangle in charred basalt
// with a jagged glowing magma crack running through it - the "the rock
// itself is cracked open and glowing" look - plus a thin dark mineral
// streak for texture.
static void ff_draw_volcanic_block(cairo_t *cr, double cx, double cy, double w, double h,
                                    double seed, const FFEmberColor *col) {
    double x0 = cx - w * 0.5, y0 = cy - h * 0.5;
    cairo_set_source_rgb(cr, 0.11, 0.09, 0.09);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_fill(cr);

    // Darker mineral streak.
    cairo_set_source_rgba(cr, 0.06, 0.05, 0.05, 0.6);
    cairo_move_to(cr, x0, y0 + h * 0.15);
    cairo_line_to(cr, x0 + w, y0 + h * 0.35);
    cairo_line_to(cr, x0 + w, y0 + h * 0.55);
    cairo_line_to(cr, x0, y0 + h * 0.75);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Jagged glowing magma crack.
    cairo_set_source_rgba(cr, col->glow_r, col->glow_g, col->glow_b, 0.9);
    cairo_set_line_width(cr, fmax(1.0, w * 0.06));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    double cx0 = x0 + w * (0.30 + 0.15 * ff_hash(seed));
    cairo_move_to(cr, cx0, y0);
    double px = cx0, py = y0;
    for (int k = 1; k <= 3; k++) {
        double t = (double)k / 3.0;
        px = x0 + w * (0.20 + 0.60 * ff_hash(seed * 3.1 + k * 1.7));
        py = y0 + h * t;
        cairo_line_to(cr, px, py);
    }
    cairo_stroke(cr);

    // A soft glow halo around the crack so it reads as light, not paint.
    cairo_set_source_rgba(cr, col->glow_r, col->glow_g, col->glow_b, 0.25);
    cairo_set_line_width(cr, w * 0.22);
    cairo_move_to(cr, cx0, y0);
    px = cx0; py = y0;
    for (int k = 1; k <= 3; k++) {
        double t = (double)k / 3.0;
        px = x0 + w * (0.20 + 0.60 * ff_hash(seed * 3.1 + k * 1.7));
        py = y0 + h * t;
        cairo_line_to(cr, px, py);
    }
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_set_line_width(cr, fmax(1.0, w * 0.02));
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_stroke(cr);
}

// Draws one obstacle column as a stack of charred, glowing-cracked basalt
// blocks capped, at the gap-facing tip, by a smoker chimney rim billowing
// a dark plume - a black-smoker chimney rather than a straight pipe.
// Rectilinear like the ship/Atlantis/antarctic columns, so it needs no
// safety scale-down to stay inside the collision box.
void ff_draw_volcanic_column(cairo_t *cr, double x, double y0, double y1,
                              double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;

    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double seg_h = width * 0.62;
    int segs = (int)fmax(2.0, round(total_h / seg_h));
    seg_h = total_h / segs;

    for (int i = 0; i < segs; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        double jitter = (ff_hash(seed * 4.4 + i * 1.3) - 0.5) * width * 0.05;
        const FFEmberColor *col = &FF_EMBER_COLORS[(int)(ff_hash(seed * 9.1 + i * 2.6) * 97.0) % FF_EMBER_COLOR_COUNT];
        ff_draw_volcanic_block(cr, cx + jitter, seg_y, width * 0.92, seg_h * 0.94,
                                seed + i * 5.7, col);
    }

    // Smoker chimney rim at the gap-facing tip: a slightly flared dark lip.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double rim_h = fmin(width * 0.20, total_h * 0.18);

    cairo_set_source_rgba(cr, 0.08, 0.06, 0.06, 0.95);
    cairo_move_to(cr, cx - width * 0.56, tip_y);
    cairo_line_to(cr, cx - width * 0.46, tip_y + inward * rim_h);
    cairo_line_to(cr, cx + width * 0.46, tip_y + inward * rim_h);
    cairo_line_to(cr, cx + width * 0.56, tip_y);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Ember glow ring right at the rim.
    const FFEmberColor *rim_col = &FF_EMBER_COLORS[(int)(ff_hash(seed * 61.0) * 97.0) % FF_EMBER_COLOR_COUNT];
    cairo_set_source_rgba(cr, rim_col->glow_r, rim_col->glow_g, rim_col->glow_b, 0.7);
    cairo_set_line_width(cr, fmax(1.0, width * 0.04));
    cairo_move_to(cr, cx - width * 0.46, tip_y + inward * rim_h);
    cairo_line_to(cr, cx + width * 0.46, tip_y + inward * rim_h);
    cairo_stroke(cr);

    // Billowing dark smoke plume streaming away from the rim - this
    // theme's equivalent of the coral head / ship's flag / Atlantis
    // capital / penguin lookout / castle turret. Fixed shape per pipe
    // (seeded), since this column art is rendered once per pipe and
    // cached, not redrawn live.
    double plume_y = tip_y + inward * rim_h;
    cairo_set_source_rgba(cr, 0.10, 0.09, 0.10, 0.5);
    for (int k = 0; k < 4; k++) {
        double t = (double)(k + 1) / 4.0;
        double drift = sin(seed * 7.0 + k * 1.9) * width * 0.22 * t;
        double puff_y = plume_y + inward * width * 0.55 * t;
        double puff_r = width * (0.14 + 0.10 * t);
        cairo_arc(cr, cx + drift, puff_y, puff_r, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}
