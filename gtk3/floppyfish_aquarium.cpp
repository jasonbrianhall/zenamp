#include "floppyfish_common.h"

// --- Aquarium theme -------------------------------------------------------
// A bright, well-lit glass fish tank - the eighth theme zone alongside
// reef/ship/cave/atlantis/rainbow/dino/antarctic. Obstacles are towers of
// oversized glass marbles capped with a little ceramic castle turret, the
// floor is a bed of bright shiny gravel pebbles, and the seaweed slot
// becomes a stiff, glossy plastic aquarium plant. Lobsters scuttle along
// the gravel as the theme's dedicated guest (see ff_draw_lobster_group in
// floppyfish.cpp).

void ff_aquarium_sky_colors(double *top_r, double *top_g, double *top_b,
                             double *bot_r, double *bot_g, double *bot_b) {
    *top_r = 0.35; *top_g = 0.75; *top_b = 0.90;
    *bot_r = 0.68; *bot_g = 0.90; *bot_b = 0.97;
}

void ff_aquarium_particle_color(double *r, double *g, double *b, double *a) {
    // Bright, dense air-stone bubbles - this tank is well aerated.
    *r = 1.0; *g = 1.0; *b = 1.0; *a = 0.35;
}

// The handful of bright marble colors shared by the pillars and the floor
// gravel, so the "jar of marbles" palette reads consistently in both
// places.
typedef struct { double r, g, b; } FFMarbleColor;
static const FFMarbleColor FF_MARBLE_COLORS[] = {
    {0.95, 0.25, 0.32}, // red
    {0.24, 0.55, 0.95}, // blue
    {0.30, 0.85, 0.42}, // green
    {0.95, 0.78, 0.18}, // yellow
    {0.80, 0.35, 0.95}, // purple
    {0.95, 0.55, 0.14}, // orange
};
#define FF_MARBLE_COLOR_COUNT (int)(sizeof(FF_MARBLE_COLORS) / sizeof(FF_MARBLE_COLORS[0]))

// Roof/wall color pairs for the castle-turret cap.
typedef struct { double roof_r, roof_g, roof_b, wall_r, wall_g, wall_b; } FFCastlePalette;
static const FFCastlePalette FF_CASTLE_PALETTES[] = {
    {0.85, 0.20, 0.25,  0.92, 0.88, 0.75}, // red roof / cream wall
    {0.20, 0.45, 0.85,  0.90, 0.90, 0.92}, // blue roof / grey wall
    {0.75, 0.30, 0.85,  0.95, 0.90, 0.80}, // purple roof / tan wall
};
#define FF_CASTLE_PALETTE_COUNT (int)(sizeof(FF_CASTLE_PALETTES) / sizeof(FF_CASTLE_PALETTES[0]))

// Painted poster backdrop (the kind stuck to the back of a real tank),
// faint glass panel seams, a bubbling air stone, and a stuck-on
// thermometer strip - the details that most say "aquarium" at a glance,
// this theme's equivalent of the ship's hull or Atlantis's temple.
void ff_draw_aquarium_backdrop(cairo_t *cr, double w, double h, double base_y) {
    // Painted poster hills - flat and stylized, like printed tank art
    // rather than real scenery.
    cairo_set_source_rgba(cr, 0.25, 0.65, 0.55, 0.45);
    cairo_move_to(cr, 0, base_y);
    for (double x = 0; x <= w + 1; x += w / 8.0) {
        double bump = 26.0 + 20.0 * sin(x * 0.02 + 1.1);
        cairo_line_to(cr, x, base_y - bump);
    }
    cairo_line_to(cr, w, base_y);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.35, 0.75, 0.68, 0.30);
    cairo_move_to(cr, 0, base_y);
    for (double x = 0; x <= w + 1; x += w / 6.0) {
        double bump = 16.0 + 12.0 * sin(x * 0.017 + 2.4);
        cairo_line_to(cr, x, base_y - bump);
    }
    cairo_line_to(cr, w, base_y);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Faint vertical glass-panel seams - the tank walls.
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.06);
    cairo_set_line_width(cr, 3.0);
    for (int i = 1; i < 4; i++) {
        double gx = w * i / 4.0;
        cairo_move_to(cr, gx, 0);
        cairo_line_to(cr, gx, base_y);
        cairo_stroke(cr);
    }

    // An air-stone bubble column, streaming straight up near one side -
    // fixed positions since this backdrop layer is cached, not animated
    // frame to frame.
    double stone_x = w * 0.14;
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
    for (int i = 0; i < 14; i++) {
        double t = (double)i / 14.0;
        double by = base_y - t * h * 0.55;
        double bx = stone_x + sin(i * 1.7) * 6.0;
        double br = 2.0 + 2.0 * fabs(sin(i * 0.9));
        cairo_arc(cr, bx, by, br, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_set_source_rgba(cr, 0.6, 0.6, 0.6, 0.6);
    cairo_arc(cr, stone_x, base_y - 4, 6, 0, 2 * M_PI);
    cairo_fill(cr);

    // A stuck-on thermometer strip near the other edge.
    double th_x = w * 0.90;
    cairo_set_source_rgba(cr, 0.92, 0.92, 0.95, 0.4);
    cairo_rectangle(cr, th_x - 4, base_y - h * 0.30, 8, h * 0.30);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.85, 0.2, 0.25, 0.6);
    cairo_rectangle(cr, th_x - 2, base_y - h * 0.10, 4, h * 0.10);
    cairo_fill(cr);
}

// Static: the base gravel-bed fill. No bubble_phase dependency, so this is
// the cacheable part.
void ff_draw_aquarium_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.55, 0.52, 0.48);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
}

// Scroll: fine drifting gravel texture ticks, plus the theme's real
// signature - a scatter of bright, shiny, glossy pebbles catching the
// light, this theme's equivalent of the ship's coins or Atlantis's gems.
void ff_draw_aquarium_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    cairo_set_source_rgba(cr, 0.42, 0.40, 0.36, 0.5);
    for (double x = -fmod(bubble_phase * (h * 0.22), 18.0); x < w; x += 18.0) {
        cairo_move_to(cr, x, h - floor_h * 0.6);
        cairo_line_to(cr, x + 6, h - floor_h * 0.4);
        cairo_set_line_width(cr, 3.0);
        cairo_stroke(cr);
    }

    for (int i = 0; i < 14; i++) {
        double px = fmod(i * 137.0 + 30.0, w);
        double py = h - floor_h * (0.20 + 0.5 * ((i * 53) % 5) / 5.0);
        double s = floor_h * 0.045 * (0.7 + 0.6 * ff_hash(i * 3.1));
        const FFMarbleColor *col = &FF_MARBLE_COLORS[i % FF_MARBLE_COLOR_COUNT];

        cairo_save(cr);
        cairo_translate(cr, px, py);
        cairo_scale(cr, 1.0, 0.65);
        cairo_set_source_rgba(cr, col->r, col->g, col->b, 0.9);
        cairo_arc(cr, 0, 0, s, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_restore(cr);

        // Glossy glint - the "shiny" part of "bright shiny pebbles".
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.85);
        cairo_arc(cr, px - s * 0.3, py - s * 0.22, s * 0.20, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// A stiff, glossy plastic aquarium plant standing in for real seaweed -
// same three-strand slot as every other theme's decoration, but stiffer
// (less sway) and each strand carries a bright plastic color with a thin
// glossy highlight stripe down the middle.
void ff_draw_aquarium_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    double sway_mult = 0.5;
    int strands = 3;
    static const double strand_col[3][3] = {
        {0.15, 0.80, 0.35},
        {0.90, 0.30, 0.60},
        {0.20, 0.65, 0.85},
    };
    for (int i = 0; i < strands; i++) {
        double sx = x + (i - 1) * height * 0.14;
        double sh = height * (0.75 + 0.25 * (i % 2));
        double phase = t * 0.9 + i * 1.7;
        double sway1 = sin(phase) * sh * 0.12 * sway_mult;
        double sway2 = sin(phase * 0.8 + 0.6) * sh * 0.22 * sway_mult;
        double tip_x = sx + sway2 * 1.2, tip_y = base_y - sh;
        const double *col = strand_col[i % 3];

        cairo_set_source_rgba(cr, col[0], col[1], col[2], 0.75 * alpha_mult);
        cairo_set_line_width(cr, sh * 0.09);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, sx, base_y);
        cairo_curve_to(cr,
                        sx + sway1, base_y - sh * 0.35,
                        sx + sway2, base_y - sh * 0.7,
                        tip_x, tip_y);
        cairo_stroke(cr);

        // Glossy highlight stripe - the detail that reads as "plastic"
        // rather than living plant.
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.35 * alpha_mult);
        cairo_set_line_width(cr, sh * 0.025);
        cairo_move_to(cr, sx, base_y);
        cairo_curve_to(cr,
                        sx + sway1, base_y - sh * 0.35,
                        sx + sway2, base_y - sh * 0.7,
                        tip_x, tip_y);
        cairo_stroke(cr);
    }
}

// One glass marble: a radial gradient from a bright near-white highlight
// down to a dark shadowed rim, plus a small glossy glint ellipse - the
// trick that makes it read as glass rather than a flat painted circle.
static void ff_draw_aquarium_marble(cairo_t *cr, double cx, double cy, double r, double seed) {
    const FFMarbleColor *col = &FF_MARBLE_COLORS[(int)(ff_hash(seed * 17.0) * 9973.0) % FF_MARBLE_COLOR_COUNT];

    cairo_pattern_t *grad = cairo_pattern_create_radial(cx - r * 0.3, cy - r * 0.3, r * 0.1, cx, cy, r * 1.15);
    cairo_pattern_add_color_stop_rgba(grad, 0.0, fmin(1.0, col->r * 1.35), fmin(1.0, col->g * 1.35), fmin(1.0, col->b * 1.35), 0.95);
    cairo_pattern_add_color_stop_rgba(grad, 0.6, col->r, col->g, col->b, 0.95);
    cairo_pattern_add_color_stop_rgba(grad, 1.0, col->r * 0.55, col->g * 0.55, col->b * 0.55, 0.95);
    cairo_set_source(cr, grad);
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(grad);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.25);
    cairo_set_line_width(cr, fmax(1.0, r * 0.05));
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_stroke(cr);

    cairo_save(cr);
    cairo_translate(cr, cx - r * 0.35, cy - r * 0.35);
    cairo_scale(cr, 1.0, 0.6);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.75);
    cairo_arc(cr, 0, 0, r * 0.22, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_fill(cr);
}

// Draws one obstacle column as a stack of oversized glass marbles - each
// its own random color, like a jar tipped on its side - with an
// occasional plastic plant sprig poking out, capped at the gap-facing tip
// by a little ceramic castle turret. Both the marbles (circles bounded by
// their own radius) and the turret (sized directly from width) stay
// inside the collision box without any safety scale-down.
void ff_draw_aquarium_column(cairo_t *cr, double x, double y0, double y1,
                              double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double marble_d = width * 0.72;
    int n = (int)fmax(3.0, round(total_h / marble_d));
    double seg_h = total_h / n;

    for (int i = 0; i < n; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        double jitter = (ff_hash(seed * 4.4 + i * 1.3) - 0.5) * width * 0.10;
        double r = width * 0.44 * (0.85 + 0.25 * ff_hash(seed * 6.6 + i * 2.1));
        ff_draw_aquarium_marble(cr, cx + jitter, seg_y, r, seed + i * 5.7);

        // An occasional plastic plant sprig poking out to the side.
        if (ff_hash(seed * 9.9 + i * 3.3) > 0.6) {
            double side = (ff_hash(seed * 11.1 + i) > 0.5) ? 1.0 : -1.0;
            double leaf_len = width * 0.32;
            double lx0 = cx + jitter + side * r * 0.8, ly0 = seg_y;
            double lx1 = lx0 + side * leaf_len, ly1 = seg_y - leaf_len * 0.4;
            cairo_set_source_rgba(cr, 0.15, 0.75, 0.35, 0.85);
            cairo_set_line_width(cr, fmax(1.0, width * 0.05));
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_move_to(cr, lx0, ly0);
            cairo_curve_to(cr, lx0 + side * leaf_len * 0.4, ly0 - leaf_len * 0.5,
                                lx1, ly1 + leaf_len * 0.1,
                                lx1, ly1);
            cairo_stroke(cr);
        }
    }

    // Castle turret cap: a wall block, a round window, a cone roof, and a
    // tiny flag at the peak - this theme's equivalent of the coral head /
    // ship's flag / Atlantis capital.
    const FFCastlePalette *cpal =
        &FF_CASTLE_PALETTES[(int)(ff_hash(seed * 53.0) * 97.0) % FF_CASTLE_PALETTE_COUNT];
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double tower_h = fmin(width * 0.55, total_h * 0.4);
    double tower_w = width * 0.5;
    double tower_cy = tip_y + inward * tower_h * 0.5;

    cairo_set_source_rgb(cr, cpal->wall_r, cpal->wall_g, cpal->wall_b);
    cairo_rectangle(cr, cx - tower_w * 0.5, tower_cy - tower_h * 0.5, tower_w, tower_h);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.3);
    cairo_set_line_width(cr, fmax(1.0, tower_h * 0.05));
    cairo_rectangle(cr, cx - tower_w * 0.5, tower_cy - tower_h * 0.5, tower_w, tower_h);
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 0.15, 0.35, 0.55, 0.85);
    cairo_arc(cr, cx, tower_cy, tower_w * 0.16, 0, 2 * M_PI);
    cairo_fill(cr);

    double roof_y = tower_cy - inward * tower_h * 0.5;
    double roof_h = tower_h * 0.7;
    cairo_set_source_rgb(cr, cpal->roof_r, cpal->roof_g, cpal->roof_b);
    cairo_move_to(cr, cx - tower_w * 0.6, roof_y);
    cairo_line_to(cr, cx + tower_w * 0.6, roof_y);
    cairo_line_to(cr, cx, roof_y - inward * roof_h);
    cairo_close_path(cr);
    cairo_fill(cr);

    double flag_y = roof_y - inward * roof_h;
    cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
    cairo_set_line_width(cr, fmax(1.0, tower_w * 0.06));
    cairo_move_to(cr, cx, flag_y);
    cairo_line_to(cr, cx, flag_y - inward * tower_h * 0.25);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, cpal->roof_r, cpal->roof_g, cpal->roof_b);
    cairo_move_to(cr, cx, flag_y - inward * tower_h * 0.25);
    cairo_line_to(cr, cx + tower_w * 0.22, flag_y - inward * tower_h * 0.20);
    cairo_line_to(cr, cx, flag_y - inward * tower_h * 0.15);
    cairo_close_path(cr);
    cairo_fill(cr);

    // A few small bubbles rising from a hidden air stone at the very tip,
    // fixed positions (this column art is rendered once per pipe and
    // cached, not redrawn live).
    double stream_y0 = tip_y + inward * tower_h * 0.1;
    for (int k = 0; k < 3; k++) {
        double bt = fmod(seed * 3.1 + k * 0.37, 1.0);
        double by = stream_y0 - inward * bt * tower_h * 1.4;
        double bx = cx + sin(seed * 20.0 + k * 2.0) * tower_w * 0.15;
        double br = tower_w * 0.04 * (0.6 + 0.4 * ff_hash(seed * 22.0 + k));
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
        cairo_arc(cr, bx, by, br, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}
