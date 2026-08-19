#include "floppyfish_common.h"

// --- Galaxy theme ----------------------------------------------------------
// Deep outer space - the ninth theme zone alongside
// reef/ship/cave/atlantis/rainbow/dino/antarctic/aquarium. Obstacles are
// candy-riveted rockets with a nose cone facing the gap and fins/exhaust
// facing the screen edge, the "sea floor" is grey moon dust scattered with
// asteroid pebbles, and the backdrop carries distant ringed planets, a
// cratered moon, soft nebula glow, and a far-off asteroid-field skyline.
// Astronauts are the theme's real signature guest, drifting past in pairs
// (see ff_draw_astronaut_group in floppyfish.cpp).

void ff_galaxy_sky_colors(double *top_r, double *top_g, double *top_b,
                           double *bot_r, double *bot_g, double *bot_b) {
    *top_r = 0.02; *top_g = 0.02; *top_b = 0.08;
    *bot_r = 0.12; *bot_g = 0.08; *bot_b = 0.22;
}

void ff_galaxy_particle_color(double *r, double *g, double *b, double *a) {
    // Distant twinkling stars/stardust, brighter and cooler than any other
    // theme's ambient particles.
    *r = 0.95; *g = 0.95; *b = 1.0; *a = 0.35;
}

// Rocket colors per obstacle, plus the porthole-glass tint - shared by
// the body segments, nose cone, and fins so one rocket reads as a single
// coherent vehicle.
typedef struct {
    double body_r, body_g, body_b;
    double stripe_r, stripe_g, stripe_b;
    double window_r, window_g, window_b;
} FFRocketPalette;

static const FFRocketPalette FF_ROCKET_PALETTES[] = {
    {0.90, 0.90, 0.94,  0.85, 0.20, 0.22,  0.35, 0.85, 1.00}, // white body / red stripe
    {0.85, 0.88, 0.92,  0.20, 0.55, 0.90,  0.55, 0.95, 1.00}, // pale body / blue stripe
    {0.92, 0.90, 0.84,  0.95, 0.60, 0.15,  0.45, 0.90, 0.95}, // cream body / orange stripe
    {0.88, 0.88, 0.90,  0.85, 0.80, 0.20,  0.40, 0.90, 1.00}, // silver body / yellow stripe
};
#define FF_ROCKET_PALETTE_COUNT (int)(sizeof(FF_ROCKET_PALETTES) / sizeof(FF_ROCKET_PALETTES[0]))

// Soft nebula clouds, a couple of distant planets, a cratered moon, and a
// tiny tethered figure drifting near them - the backdrop that most says
// "galaxy" at a glance, this theme's equivalent of the ship's hull or
// Atlantis's temple.
void ff_draw_galaxy_backdrop(cairo_t *cr, double w, double h, double base_y) {
    static const double neb_x[3]   = {0.15, 0.62, 0.85};
    static const double neb_y[3]   = {0.18, 0.10, 0.30};
    static const double neb_r[3]   = {0.26, 0.14, 0.12};
    static const double neb_col[3][3] = {
        {0.55, 0.25, 0.75}, {0.20, 0.45, 0.85}, {0.85, 0.35, 0.55}
    };
    for (int i = 0; i < 3; i++) {
        double nx = w * neb_x[i], ny = h * neb_y[i], nr = h * neb_r[i];
        cairo_pattern_t *grad = cairo_pattern_create_radial(nx, ny, 0, nx, ny, nr);
        cairo_pattern_add_color_stop_rgba(grad, 0.0, neb_col[i][0], neb_col[i][1], neb_col[i][2], 0.18);
        cairo_pattern_add_color_stop_rgba(grad, 1.0, neb_col[i][0], neb_col[i][1], neb_col[i][2], 0.0);
        cairo_set_source(cr, grad);
        cairo_arc(cr, nx, ny, nr, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(grad);
    }

    // A distant ringed planet.
    double p1x = w * 0.78, p1y = h * 0.16, p1r = h * 0.075;
    cairo_set_source_rgba(cr, 0.75, 0.45, 0.30, 0.55);
    cairo_arc(cr, p1x, p1y, p1r, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.55, 0.30, 0.18, 0.4);
    cairo_save(cr);
    cairo_translate(cr, p1x, p1y - p1r * 0.25);
    cairo_scale(cr, 1.0, 0.35);
    cairo_arc(cr, 0, 0, p1r * 0.95, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.90, 0.80, 0.60, 0.5);
    cairo_save(cr);
    cairo_translate(cr, p1x, p1y);
    cairo_rotate(cr, -0.3);
    cairo_scale(cr, 1.0, 0.30);
    cairo_set_line_width(cr, p1r * 0.18);
    cairo_arc(cr, 0, 0, p1r * 1.55, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_stroke(cr);

    // A smaller cratered moon.
    double p2x = w * 0.20, p2y = h * 0.10, p2r = h * 0.04;
    cairo_set_source_rgba(cr, 0.75, 0.75, 0.78, 0.5);
    cairo_arc(cr, p2x, p2y, p2r, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.55, 0.55, 0.60, 0.45);
    for (int k = 0; k < 3; k++) {
        double cang = ff_hash(k * 3.7) * 2 * M_PI;
        double crad = p2r * 0.55 * ff_hash(k * 5.1);
        cairo_arc(cr, p2x + cos(cang) * crad, p2y + sin(cang) * crad, p2r * 0.18, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Distant asteroid-field skyline along the floor line, standing in for
    // the usual coral-bump/hull silhouette.
    cairo_set_source_rgba(cr, 0.25, 0.22, 0.28, 0.6);
    cairo_move_to(cr, 0, base_y);
    for (double x = 0; x <= w + 1; x += w / 10.0) {
        double bump = 16.0 + 22.0 * fabs(sin(x * 0.025 + 1.4));
        cairo_line_to(cr, x, base_y - bump);
    }
    cairo_line_to(cr, w, base_y);
    cairo_close_path(cr);
    cairo_fill(cr);
}

// Static: the moon-dust base fill. No bubble_phase dependency, so this is
// the cacheable part.
void ff_draw_galaxy_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.20, 0.19, 0.24);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
}

// Scroll: drifting dust streaks, plus a scatter of small glinting asteroid
// pebbles - this theme's equivalent of the ship's coins or Atlantis's
// gems.
void ff_draw_galaxy_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    cairo_set_source_rgba(cr, 0.30, 0.28, 0.36, 0.5);
    for (double x = -fmod(bubble_phase * (h * 0.20), 22.0); x < w; x += 22.0) {
        cairo_move_to(cr, x, h - floor_h * 0.55);
        cairo_line_to(cr, x + 8, h - floor_h * 0.40);
        cairo_set_line_width(cr, 2.5);
        cairo_stroke(cr);
    }

    for (int i = 0; i < 10; i++) {
        double bx = fmod(i * 151.0 + 40.0, w);
        double by = h - floor_h * (0.22 + 0.4 * ((i * 43) % 5) / 5.0);
        double s = floor_h * 0.045 * (0.7 + 0.6 * ff_hash(i * 4.3));
        cairo_set_source_rgba(cr, 0.45, 0.42, 0.48, 0.85);
        cairo_move_to(cr, bx - s, by);
        cairo_line_to(cr, bx - s * 0.2, by - s * 0.9);
        cairo_line_to(cr, bx + s * 0.8, by - s * 0.3);
        cairo_line_to(cr, bx + s * 0.3, by + s * 0.7);
        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.85, 0.85, 0.95, 0.6);
        cairo_arc(cr, bx - s * 0.1, by - s * 0.3, s * 0.18, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// A tethered cable of space junk standing in for seaweed - the same
// three-strand sway as every other theme's decoration slot, colored dull
// metallic grey and tipped with a small blinking satellite beacon
// (alternating red/blue-white by strand) instead of a leafy tip.
void ff_draw_galaxy_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    double r = 0.45, g = 0.45, b = 0.50, a = 0.65, sway_mult = 0.60;
    int strands = 3;
    for (int i = 0; i < strands; i++) {
        double sx = x + (i - 1) * height * 0.14;
        double sh = height * (0.75 + 0.25 * (i % 2));
        double phase = t * 1.0 + i * 1.7;
        double sway1 = sin(phase) * sh * 0.16 * sway_mult;
        double sway2 = sin(phase * 0.8 + 0.6) * sh * 0.30 * sway_mult;
        double tip_x = sx + sway2 * 1.2, tip_y = base_y - sh;

        cairo_set_source_rgba(cr, r, g, b, a * alpha_mult);
        cairo_set_line_width(cr, sh * 0.045);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, sx, base_y);
        cairo_curve_to(cr,
                        sx + sway1, base_y - sh * 0.35,
                        sx + sway2, base_y - sh * 0.7,
                        tip_x, tip_y);
        cairo_stroke(cr);

        double blink = 0.5 + 0.5 * sin(t * 4.0 + i * 2.1);
        bool is_red = (i % 2 == 0);
        cairo_set_source_rgba(cr, is_red ? 1.0 : 0.35, is_red ? 0.20 : 0.85, is_red ? 0.20 : 1.0,
                               0.75 * blink * alpha_mult);
        cairo_arc(cr, tip_x, tip_y, sh * 0.07, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// One cylindrical rocket-body segment: a horizontal gradient for the
// "rounded metal tube" look, seams top/bottom, a rivet row, an occasional
// accent stripe band, and sometimes a porthole window.
static void ff_draw_rocket_segment(cairo_t *cr, double cx, double cy, double w, double h,
                                    double seed, const FFRocketPalette *pal, int idx) {
    double x0 = cx - w * 0.5, y0 = cy - h * 0.5;

    cairo_pattern_t *grad = cairo_pattern_create_linear(x0, 0, x0 + w, 0);
    cairo_pattern_add_color_stop_rgb(grad, 0.0, pal->body_r * 0.62, pal->body_g * 0.62, pal->body_b * 0.62);
    cairo_pattern_add_color_stop_rgb(grad, 0.5, fmin(1.0, pal->body_r * 1.12), fmin(1.0, pal->body_g * 1.12), fmin(1.0, pal->body_b * 1.12));
    cairo_pattern_add_color_stop_rgb(grad, 1.0, pal->body_r * 0.62, pal->body_g * 0.62, pal->body_b * 0.62);
    cairo_set_source(cr, grad);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_fill(cr);
    cairo_pattern_destroy(grad);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.30);
    cairo_set_line_width(cr, fmax(1.0, h * 0.05));
    cairo_move_to(cr, x0, y0); cairo_line_to(cr, x0 + w, y0); cairo_stroke(cr);
    cairo_move_to(cr, x0, y0 + h); cairo_line_to(cr, x0 + w, y0 + h); cairo_stroke(cr);

    if (idx % 3 == 1) {
        cairo_set_source_rgb(cr, pal->stripe_r, pal->stripe_g, pal->stripe_b);
        cairo_rectangle(cr, x0, y0 + h * 0.3, w, h * 0.4);
        cairo_fill(cr);
    }

    if (ff_hash(seed * 7.1) > 0.5) {
        double wr = w * 0.16;
        cairo_set_source_rgba(cr, pal->window_r, pal->window_g, pal->window_b, 0.9);
        cairo_arc(cr, cx, cy, wr, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.15, 0.15, 0.18, 0.8);
        cairo_set_line_width(cr, fmax(1.0, wr * 0.2));
        cairo_arc(cr, cx, cy, wr, 0, 2 * M_PI);
        cairo_stroke(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.6);
        cairo_arc(cr, cx - wr * 0.3, cy - wr * 0.3, wr * 0.3, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    cairo_set_source_rgba(cr, 0, 0, 0, 0.25);
    for (int k = 0; k < 3; k++) {
        double rx = x0 + w * (k + 0.5) / 3.0;
        cairo_arc(cr, rx, y0 + h * 0.5, fmax(0.8, w * 0.015), 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// A soft layered exhaust flame, pointing away from the rocket body -
// static art (this column is rendered once per pipe and cached), so it
// reads as a fixed glow rather than an animated burn.
static void ff_draw_rocket_flame(cairo_t *cr, double cx, double y_edge, double w, double dir_out, double seed) {
    double len = w * 0.9 * (0.8 + 0.4 * ff_hash(seed * 31.0));
    double halfw = w * 0.28;
    cairo_pattern_t *grad = cairo_pattern_create_linear(cx, y_edge, cx, y_edge + dir_out * len);
    cairo_pattern_add_color_stop_rgba(grad, 0.0, 1.0, 0.55, 0.15, 0.85);
    cairo_pattern_add_color_stop_rgba(grad, 0.5, 1.0, 0.85, 0.35, 0.65);
    cairo_pattern_add_color_stop_rgba(grad, 1.0, 1.0, 0.95, 0.75, 0.0);
    cairo_set_source(cr, grad);
    cairo_move_to(cr, cx - halfw, y_edge);
    cairo_curve_to(cr, cx - halfw * 0.5, y_edge + dir_out * len * 0.6,
                        cx - halfw * 0.15, y_edge + dir_out * len * 0.9,
                        cx, y_edge + dir_out * len);
    cairo_curve_to(cr, cx + halfw * 0.15, y_edge + dir_out * len * 0.9,
                        cx + halfw * 0.5, y_edge + dir_out * len * 0.6,
                        cx + halfw, y_edge);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_pattern_destroy(grad);
}

// Draws one obstacle column as a stack of riveted rocket-body segments,
// capped at the gap-facing tip by a nose cone with a porthole, and finished
// at the screen-edge (rooted) end with a pair of delta fins and a static
// exhaust-flame glow - a rocket standing on its tail rather than a plain
// pipe. Rectilinear like the ship/Atlantis columns, so it needs no safety
// scale-down to stay inside the collision box.
void ff_draw_galaxy_column(cairo_t *cr, double x, double y0, double y1,
                            double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFRocketPalette *pal =
        &FF_ROCKET_PALETTES[(int)(ff_hash(seed * 37.0) * 97.0) % FF_ROCKET_PALETTE_COUNT];

    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double seg_h = width * 0.62;
    int segs = (int)fmax(2.0, round(total_h / seg_h));
    seg_h = total_h / segs;

    for (int i = 0; i < segs; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        ff_draw_rocket_segment(cr, cx, seg_y, width * 0.62, seg_h * 0.94, seed + i * 5.7, pal, i);
    }

    // Nose cone at the gap-facing tip.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double cone_h = fmin(width * 0.9, total_h * 0.35);

    cairo_set_source_rgb(cr, pal->stripe_r, pal->stripe_g, pal->stripe_b);
    cairo_move_to(cr, cx - width * 0.31, tip_y);
    cairo_curve_to(cr, cx - width * 0.31, tip_y + inward * cone_h * 0.5,
                        cx - width * 0.10, tip_y + inward * cone_h * 0.9,
                        cx, tip_y + inward * cone_h);
    cairo_curve_to(cr, cx + width * 0.10, tip_y + inward * cone_h * 0.9,
                        cx + width * 0.31, tip_y + inward * cone_h * 0.5,
                        cx + width * 0.31, tip_y);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_set_line_width(cr, fmax(1.0, width * 0.02));
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, pal->window_r, pal->window_g, pal->window_b, 0.9);
    cairo_arc(cr, cx, tip_y + inward * cone_h * 0.25, width * 0.10, 0, 2 * M_PI);
    cairo_fill(cr);

    // Delta fins and exhaust flame at the rooted (screen-edge) end.
    double root_y = tip_at_y1 ? y0 : y1;
    double root_dir = tip_at_y1 ? 1.0 : -1.0; // points from root_y into the column body
    double fin_h = fmin(width * 0.5, total_h * 0.3);
    double fin_cy = root_y + root_dir * fin_h * 0.5;

    cairo_set_source_rgb(cr, pal->stripe_r, pal->stripe_g, pal->stripe_b);
    for (int side = -1; side <= 1; side += 2) {
        cairo_move_to(cr, cx + side * width * 0.31, fin_cy - root_dir * fin_h * 0.5);
        cairo_line_to(cr, cx + side * width * 0.62, fin_cy + root_dir * fin_h * 0.5);
        cairo_line_to(cr, cx + side * width * 0.31, fin_cy + root_dir * fin_h * 0.5);
        cairo_close_path(cr);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
        cairo_set_line_width(cr, fmax(1.0, width * 0.02));
        cairo_stroke(cr);
        cairo_set_source_rgb(cr, pal->stripe_r, pal->stripe_g, pal->stripe_b);
    }

    ff_draw_rocket_flame(cr, cx, root_y, width, -root_dir, seed);
}
