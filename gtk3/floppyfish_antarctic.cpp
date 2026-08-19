#include "floppyfish_common.h"

// --- Antarctic theme ------------------------------------------------------
// Icy polar water - the seventh theme zone alongside
// reef/ship/cave/atlantis/rainbow/dino. Obstacles are faceted glacier
// blocks capped with a snow crown and a tiny penguin standing lookout, the
// floor is packed ice scattered with bergy bits, and the backdrop carries
// a row of icebergs, a sea lion hauled out on a floe, and a whale cruising
// the deep background - penguins are the theme's real signature, though,
// so they also get the dedicated swimming guest (see ff_draw_penguin_group
// in floppyfish.cpp).

void ff_antarctic_sky_colors(double *top_r, double *top_g, double *top_b,
                              double *bot_r, double *bot_g, double *bot_b) {
    *top_r = 0.08; *top_g = 0.22; *top_b = 0.36;
    *bot_r = 0.42; *bot_g = 0.62; *bot_b = 0.72;
}

void ff_antarctic_particle_color(double *r, double *g, double *b, double *a) {
    // Drifting snow/ice flecks - brighter and cooler than any other
    // theme's ambient particles.
    *r = 0.95; *g = 0.98; *b = 1.0; *a = 0.30;
}

// Ice tones per obstacle/backdrop element, plus a crevasse-shadow shade -
// shared by the glacier blocks and the distant iceberg row so the palette
// reads consistently everywhere it shows up.
typedef struct {
    double ice_r, ice_g, ice_b;
    double shadow_r, shadow_g, shadow_b;
    double crystal_r, crystal_g, crystal_b;
} FFIcePalette;

static const FFIcePalette FF_ICE_PALETTES[] = {
    {0.80, 0.90, 0.98,  0.45, 0.62, 0.78,  0.55, 0.85, 1.00}, // pale blue-white / cyan sparkle
    {0.72, 0.88, 0.92,  0.38, 0.58, 0.68,  0.45, 0.90, 0.95}, // teal-tinted ice / aqua sparkle
    {0.85, 0.92, 0.98,  0.55, 0.68, 0.80,  0.70, 0.90, 1.00}, // near-white / pale blue sparkle
    {0.65, 0.80, 0.92,  0.30, 0.48, 0.65,  0.40, 0.75, 0.98}, // deep glacier blue / bright sparkle
};
#define FF_ICE_PALETTE_COUNT (int)(sizeof(FF_ICE_PALETTES) / sizeof(FF_ICE_PALETTES[0]))

// A tiny penguin silhouette standing lookout, used at the tip of every
// glacier pillar - this theme's real signature detail, so it shows up
// everywhere the theme's art appears, not just on the rare swimming guest.
static void ff_draw_antarctic_tip_penguin(cairo_t *cr, double cx, double cy, double s, double seed) {
    double flip = (ff_hash(seed * 13.0) > 0.5) ? 1.0 : -1.0;
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, flip * s, s);

    // Body, standing upright.
    cairo_set_source_rgba(cr, 0.06, 0.06, 0.08, 0.95);
    cairo_move_to(cr, 0, -1.6);
    cairo_curve_to(cr, 0.9, -1.6, 1.1, -0.4, 1.0, 0.6);
    cairo_curve_to(cr, 0.9, 1.4, 0.4, 1.7, 0, 1.7);
    cairo_curve_to(cr, -0.4, 1.7, -0.9, 1.4, -1.0, 0.6);
    cairo_curve_to(cr, -1.1, -0.4, -0.9, -1.6, 0, -1.6);
    cairo_close_path(cr);
    cairo_fill(cr);

    // White belly patch.
    cairo_set_source_rgba(cr, 0.97, 0.97, 1.0, 0.95);
    cairo_move_to(cr, 0, -0.6);
    cairo_curve_to(cr, 0.4, -0.6, 0.55, 0.1, 0.5, 0.9);
    cairo_curve_to(cr, 0.45, 1.3, 0.2, 1.4, 0, 1.4);
    cairo_curve_to(cr, -0.2, 1.4, -0.45, 1.3, -0.5, 0.9);
    cairo_curve_to(cr, -0.55, 0.1, -0.4, -0.6, 0, -0.6);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Head and beak.
    cairo_set_source_rgba(cr, 0.06, 0.06, 0.08, 0.95);
    cairo_arc(cr, 0, -2.0, 0.6, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.95, 0.55, 0.15, 0.9);
    cairo_move_to(cr, 0.4, -2.0);
    cairo_line_to(cr, 1.0, -1.9);
    cairo_line_to(cr, 0.4, -1.75);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Feet.
    cairo_set_source_rgba(cr, 0.95, 0.55, 0.15, 0.9);
    cairo_move_to(cr, -0.5, 1.7); cairo_line_to(cr, -0.2, 1.7); cairo_line_to(cr, -0.35, 2.1); cairo_close_path(cr); cairo_fill(cr);
    cairo_move_to(cr, 0.2, 1.7);  cairo_line_to(cr, 0.5, 1.7);  cairo_line_to(cr, 0.35, 2.1);  cairo_close_path(cr); cairo_fill(cr);

    cairo_restore(cr);
}

// A drowsy sea lion, hauled out on a low ice ledge - a curved lump of a
// body with a raised head and a couple of stubby flippers, drawn once
// into the backdrop rather than as a moving critter.
static void ff_draw_antarctic_sea_lion(cairo_t *cr, double x, double y, double s) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, s, s);

    cairo_set_source_rgba(cr, 0.28, 0.22, 0.18, 0.55);
    cairo_move_to(cr, -22, 4);
    cairo_curve_to(cr, -20, -6, -8, -10, 4, -8);
    cairo_curve_to(cr, 12, -7, 16, -2, 14, 4);
    cairo_curve_to(cr, 6, 8, -12, 8, -22, 4);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Raised head/neck.
    cairo_move_to(cr, 10, -6);
    cairo_curve_to(cr, 14, -14, 22, -16, 26, -12);
    cairo_curve_to(cr, 24, -8, 20, -6, 16, -4);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Front flipper.
    cairo_move_to(cr, -4, 3);
    cairo_curve_to(cr, -2, 8, 2, 11, 8, 11);
    cairo_curve_to(cr, 4, 6, 0, 3, -4, 3);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_restore(cr);
}

// A whale cruising the deep background - drawn faint and far back so it
// reads as a glimpse rather than competing with the pillars/fish.
static void ff_draw_antarctic_whale(cairo_t *cr, double w, double h, double base_y) {
    double wx = w * 0.72, wy = base_y - h * 0.42;
    double scale = h * 0.0032;

    cairo_save(cr);
    cairo_translate(cr, wx, wy);
    cairo_scale(cr, scale, scale);
    cairo_set_source_rgba(cr, 0.10, 0.20, 0.28, 0.35);

    // Body.
    cairo_move_to(cr, 60, 0);
    cairo_curve_to(cr, 50, -20, 10, -24, -40, -16);
    cairo_curve_to(cr, -60, -12, -75, -4, -80, 4);
    cairo_curve_to(cr, -60, 14, -10, 20, 40, 16);
    cairo_curve_to(cr, 52, 14, 58, 8, 60, 0);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Fluke.
    cairo_move_to(cr, -78, 2);
    cairo_curve_to(cr, -96, -6, -112, -14, -120, -18);
    cairo_curve_to(cr, -110, -2, -110, 2, -120, 18);
    cairo_curve_to(cr, -112, 14, -96, 8, -78, 2);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Small dorsal ridge.
    cairo_move_to(cr, -6, -18);
    cairo_curve_to(cr, -2, -26, 6, -28, 12, -26);
    cairo_curve_to(cr, 8, -22, 2, -18, -2, -16);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_restore(cr);
}

// Distant iceberg row - a jagged white/blue skyline along the floor line -
// plus a sea lion on a low ledge and a whale further out. This theme's
// equivalent of the ship's hull or Atlantis's temple: the one backdrop
// that most says "Antarctic" at a glance.
void ff_draw_antarctic_backdrop(cairo_t *cr, double w, double h, double base_y) {
    ff_draw_antarctic_whale(cr, w, h, base_y);

    cairo_set_source_rgba(cr, 0.78, 0.90, 0.97, 0.55);
    cairo_move_to(cr, 0, base_y);
    for (double x = 0; x <= w + 1; x += w / 9.0) {
        double peak = 22.0 + 30.0 * fabs(sin(x * 0.03 + 0.6));
        cairo_line_to(cr, x, base_y - peak);
    }
    cairo_line_to(cr, w, base_y);
    cairo_close_path(cr);
    cairo_fill(cr);

    // A few darker crevasse cracks on the iceberg faces.
    cairo_set_source_rgba(cr, 0.35, 0.55, 0.68, 0.4);
    cairo_set_line_width(cr, 2.0);
    for (int i = 0; i < 5; i++) {
        double cx = w * (0.08 + 0.18 * i);
        double peak = 22.0 + 30.0 * fabs(sin(cx * 0.03 + 0.6));
        cairo_move_to(cr, cx, base_y - peak * 0.3);
        cairo_line_to(cr, cx + 6, base_y - peak * 0.75);
        cairo_stroke(cr);
    }

    ff_draw_antarctic_sea_lion(cr, w * 0.30, base_y - h * 0.015, h * 0.0022);
}

// Static: the packed-ice base fill. No bubble_phase dependency, so this is
// the cacheable part.
void ff_draw_antarctic_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.72, 0.84, 0.92);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
}

// Scroll: drifting frost cracks, plus a scatter of small angular bergy
// bits half-buried in the ice - this theme's equivalent of the ship's
// coins or the Atlantis floor's gems.
void ff_draw_antarctic_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    cairo_set_source_rgba(cr, 0.50, 0.68, 0.80, 0.55);
    for (double x = -fmod(bubble_phase * (h * 0.30), 50.0); x < w; x += 50.0) {
        cairo_move_to(cr, x, h - floor_h);
        cairo_line_to(cr, x + 10, h);
        cairo_set_line_width(cr, 3.0);
        cairo_stroke(cr);
    }

    for (int i = 0; i < 9; i++) {
        double bx = fmod(i * 179.0 + 55.0, w);
        double by = h - floor_h * (0.22 + 0.4 * ((i * 43) % 5) / 5.0);
        double s = floor_h * 0.06;
        const FFIcePalette *pal = &FF_ICE_PALETTES[i % FF_ICE_PALETTE_COUNT];
        cairo_set_source_rgba(cr, pal->ice_r, pal->ice_g, pal->ice_b, 0.9);
        cairo_move_to(cr, bx - s, by);
        cairo_line_to(cr, bx, by - s * 0.8);
        cairo_line_to(cr, bx + s, by);
        cairo_line_to(cr, bx + s * 0.3, by + s * 0.6);
        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, pal->shadow_r, pal->shadow_g, pal->shadow_b, 0.6);
        cairo_move_to(cr, bx, by - s * 0.8);
        cairo_line_to(cr, bx + s, by);
        cairo_line_to(cr, bx + s * 0.3, by + s * 0.6);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

// Frost-tipped kelp standing in for seaweed - the same three-strand sway
// as every other theme's decoration slot, colored cold teal-green and
// tipped with a small icy crystal instead of a leafy tip.
void ff_draw_antarctic_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    double r = 0.14, g = 0.36, b = 0.34, a = 0.55, sway_mult = 0.85;
    int strands = 3;
    for (int i = 0; i < strands; i++) {
        double sx = x + (i - 1) * height * 0.14;
        double sh = height * (0.75 + 0.25 * (i % 2));
        double phase = t * 1.1 + i * 1.7;
        double sway1 = sin(phase) * sh * 0.16 * sway_mult;
        double sway2 = sin(phase * 0.8 + 0.6) * sh * 0.30 * sway_mult;
        double tip_x = sx + sway2 * 1.2, tip_y = base_y - sh;

        cairo_set_source_rgba(cr, r, g, b, a * alpha_mult);
        cairo_set_line_width(cr, sh * 0.05);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, sx, base_y);
        cairo_curve_to(cr,
                        sx + sway1, base_y - sh * 0.35,
                        sx + sway2, base_y - sh * 0.7,
                        tip_x, tip_y);
        cairo_stroke(cr);

        double cs = sh * 0.08;
        cairo_set_source_rgba(cr, 0.75, 0.92, 1.0, 0.7 * alpha_mult);
        cairo_move_to(cr, tip_x, tip_y - cs);
        cairo_line_to(cr, tip_x + cs, tip_y);
        cairo_line_to(cr, tip_x, tip_y + cs);
        cairo_line_to(cr, tip_x - cs, tip_y);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

// One faceted glacier block: a rectangle in the ice base color, with a
// lighter diagonal facet on one side (the "catching the light" look that
// keeps it from reading as flat) and a darker crevasse-shadow facet on
// the other, plus a scatter of tiny sparkle flecks.
static void ff_draw_antarctic_block(cairo_t *cr, double cx, double cy, double w, double h,
                                     double seed, const FFIcePalette *pal) {
    double x0 = cx - w * 0.5, y0 = cy - h * 0.5;
    cairo_set_source_rgb(cr, pal->ice_r, pal->ice_g, pal->ice_b);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_fill(cr);

    // Darker crevasse-shadow facet along one diagonal half.
    cairo_move_to(cr, x0, y0 + h);
    cairo_line_to(cr, x0 + w, y0);
    cairo_line_to(cr, x0 + w, y0 + h);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, pal->shadow_r, pal->shadow_g, pal->shadow_b, 0.45);
    cairo_fill(cr);

    // Lighter highlight sliver along the opposite edge.
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.30);
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x0 + w * 0.25, y0);
    cairo_line_to(cr, x0, y0 + h * 0.6);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, pal->shadow_r * 0.7, pal->shadow_g * 0.7, pal->shadow_b * 0.7, 0.6);
    cairo_set_line_width(cr, fmax(1.0, w * 0.02));
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_stroke(cr);

    // Sparkle flecks.
    for (int i = 0; i < 3; i++) {
        if (ff_hash(seed * 6.6 + i * 2.2) < 0.5) continue;
        double fx = x0 + w * ff_hash(seed * 7.7 + i * 1.4);
        double fy = y0 + h * ff_hash(seed * 8.8 + i * 1.9);
        cairo_set_source_rgba(cr, pal->crystal_r, pal->crystal_g, pal->crystal_b, 0.7);
        cairo_arc(cr, fx, fy, w * 0.03, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// Draws one obstacle column as a stack of faceted glacier blocks capped,
// at the gap-facing tip, by a jagged snow crown with a tiny penguin
// standing lookout on top - an iceberg pillar rather than a straight pipe.
// Rectilinear like the ship/Atlantis columns, so it needs no safety
// scale-down to stay inside the collision box.
void ff_draw_antarctic_column(cairo_t *cr, double x, double y0, double y1,
                               double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFIcePalette *pal =
        &FF_ICE_PALETTES[(int)(ff_hash(seed * 43.0) * 97.0) % FF_ICE_PALETTE_COUNT];

    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double seg_h = width * 0.60;
    int segs = (int)fmax(2.0, round(total_h / seg_h));
    seg_h = total_h / segs;

    for (int i = 0; i < segs; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        double jitter = (ff_hash(seed * 4.4 + i * 1.3) - 0.5) * width * 0.05;
        ff_draw_antarctic_block(cr, cx + jitter, seg_y, width * 0.90, seg_h * 0.92,
                                 seed + i * 5.7, pal);
    }

    // Jagged snow crown at the gap-facing tip.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double crown_h = fmin(width * 0.42, total_h * 0.35);
    double crown_base_y = tip_y;

    cairo_set_source_rgba(cr, 0.97, 0.98, 1.0, 0.95);
    cairo_move_to(cr, cx - width * 0.46, crown_base_y);
    int peaks = 4;
    for (int k = 0; k <= peaks; k++) {
        double t = (double)k / peaks;
        double px = cx - width * 0.46 + width * 0.92 * t;
        double ph = crown_h * (0.5 + 0.5 * ff_hash(seed * 15.0 + k * 3.1));
        cairo_line_to(cr, px, crown_base_y + inward * ph);
    }
    cairo_line_to(cr, cx + width * 0.46, crown_base_y);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, pal->shadow_r, pal->shadow_g, pal->shadow_b, 0.5);
    cairo_set_line_width(cr, fmax(1.0, width * 0.02));
    cairo_stroke(cr);

    // The penguin, standing right at the peak of the crown - this theme's
    // real signature, so it appears on every single pillar.
    double penguin_y = crown_base_y + inward * crown_h * 0.55;
    ff_draw_antarctic_tip_penguin(cr, cx, penguin_y, width * 0.11, seed);
}
