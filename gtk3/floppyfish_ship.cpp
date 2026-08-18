#include "floppyfish_common.h"

// --- Pirate ship theme -----------------------------------------------------

void ff_ship_sky_colors(double *top_r, double *top_g, double *top_b,
                         double *bot_r, double *bot_g, double *bot_b) {
    *top_r = 0.10; *top_g = 0.32; *top_b = 0.40;
    *bot_r = 0.16; *bot_g = 0.42; *bot_b = 0.46;
}

void ff_ship_particle_color(double *r, double *g, double *b, double *a) {
    *r = 1.0; *g = 1.0; *b = 1.0; *a = 0.25;
}

// The single biggest thing that makes the ship theme actually read as a
// shipwreck rather than "some brown blobs underwater": one large, clearly
// boat-shaped hull silhouette lying on the seabed, with rib-frame lines
// showing through broken planking, a few portholes, and a broken mast with
// a tattered sail leaning out of it.
void ff_draw_ship_backdrop(cairo_t *cr, double w, double h, double base_y) {
    double hull_w = w * 0.60;
    double hull_x = w * 0.28;
    double hull_h = h * 0.30;
    double hull_top = base_y - hull_h;

    cairo_set_source_rgba(cr, 0.05, 0.08, 0.09, 0.65);
    cairo_move_to(cr, hull_x, base_y);
    cairo_curve_to(cr, hull_x + hull_w * 0.05, hull_top + hull_h * 0.15,
                        hull_x + hull_w * 0.30, hull_top,
                        hull_x + hull_w * 0.5, hull_top);
    cairo_curve_to(cr, hull_x + hull_w * 0.70, hull_top,
                        hull_x + hull_w * 0.95, hull_top + hull_h * 0.15,
                        hull_x + hull_w, base_y);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);

    // Rib frame lines showing through broken planking - the detail that
    // most says "wrecked hull" rather than a plain rounded mound.
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_set_line_width(cr, 3.0);
    for (int i = 1; i < 7; i++) {
        double t = i / 7.0;
        double rx = hull_x + hull_w * t;
        double ry_top = hull_top + hull_h * (0.10 + 0.55 * fabs(sin(t * M_PI)));
        cairo_move_to(cr, rx, base_y);
        cairo_line_to(cr, rx, ry_top);
        cairo_stroke(cr);
    }

    // Portholes.
    cairo_set_source_rgba(cr, 0, 0, 0, 0.55);
    for (int i = 0; i < 3; i++) {
        double px = hull_x + hull_w * (0.32 + 0.18 * i);
        double py = hull_top + hull_h * 0.55;
        cairo_arc(cr, px, py, hull_h * 0.085, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_set_source_rgba(cr, 0.5, 0.75, 0.85, 0.35);
    for (int i = 0; i < 3; i++) {
        double px = hull_x + hull_w * (0.32 + 0.18 * i);
        double py = hull_top + hull_h * 0.55;
        cairo_arc(cr, px, py, hull_h * 0.05, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Broken mast leaning out of the hull, with a tattered sail.
    double mast_x = hull_x + hull_w * 0.66;
    double mast_base_y = hull_top + hull_h * 0.30;
    double mast_top_y = mast_base_y - hull_h * 1.15;
    double mast_top_x = mast_x + hull_h * 0.18;
    cairo_set_source_rgba(cr, 0.05, 0.08, 0.09, 0.65);
    cairo_set_line_width(cr, fmax(2.0, hull_h * 0.045));
    cairo_move_to(cr, mast_x, mast_base_y);
    cairo_line_to(cr, mast_top_x, mast_top_y);
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 0.15, 0.20, 0.20, 0.5);
    cairo_move_to(cr, mast_top_x, mast_top_y);
    cairo_line_to(cr, mast_top_x + hull_h * 0.48, mast_top_y + hull_h * 0.22);
    cairo_line_to(cr, mast_top_x + hull_h * 0.10, mast_top_y + hull_h * 0.50);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Faint sunbeam shafts slanting down - classic sunken-wreck lighting.
    cairo_set_source_rgba(cr, 1.0, 1.0, 0.9, 0.06);
    for (int i = 0; i < 3; i++) {
        double bx = w * (0.15 + 0.32 * i);
        cairo_move_to(cr, bx, 0);
        cairo_line_to(cr, bx + w * 0.10, 0);
        cairo_line_to(cr, bx - w * 0.05, base_y);
        cairo_line_to(cr, bx - w * 0.15, base_y);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

// Static: just the base deck fill - the one part big enough (a full-width
// rectangle) to be worth caching. Everything else below has to stay in the
// live pass so it keeps drawing on top of it in the original order.
void ff_draw_ship_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.42, 0.28, 0.16);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
}

// Scroll (really "everything drawn live on top of the cached deck"): the
// plank seams that actually scroll with bubble_phase, plus the baseline
// seam and coin scatter - kept here rather than in the static cache so they
// still land on top of the planks each frame, same as the original
// single-function draw order. All of this is cheap (a dozen strokes/arcs),
// so redrawing it live costs nothing worth caching.
void ff_draw_ship_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    cairo_set_source_rgba(cr, 0.25, 0.15, 0.08, 0.7);
    cairo_set_line_width(cr, 3.0);
    for (double x = -fmod(bubble_phase * (h * 0.34), 60.0); x < w; x += 60.0) {
        cairo_move_to(cr, x, h - floor_h);
        cairo_line_to(cr, x, h);
        cairo_stroke(cr);
    }
    cairo_set_source_rgba(cr, 0.2, 0.12, 0.06, 0.6);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, 0, h - floor_h * 0.5);
    cairo_line_to(cr, w, h - floor_h * 0.5);
    cairo_stroke(cr);

    // A scatter of half-buried gold coins/doubloons.
    for (int i = 0; i < 10; i++) {
        double cxx = fmod(i * 173.0 + 40.0, w);
        double cyy = h - floor_h * (0.25 + 0.4 * ((i * 37) % 5) / 5.0);
        double r = floor_h * 0.06;
        cairo_set_source_rgb(cr, 0.85, 0.68, 0.20);
        cairo_save(cr);
        cairo_translate(cr, cxx, cyy);
        cairo_scale(cr, 1.0, 0.55);
        cairo_arc(cr, 0, 0, r, 0, 2 * M_PI);
        cairo_restore(cr);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.55, 0.42, 0.08, 0.7);
        cairo_save(cr);
        cairo_translate(cr, cxx, cyy);
        cairo_scale(cr, 1.0, 0.55);
        cairo_arc(cr, 0, 0, r * 0.6, 0, 2 * M_PI);
        cairo_restore(cr);
        cairo_stroke(cr);
    }
}

void ff_draw_ship_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    double r = 0.30, g = 0.20, b = 0.10, a = 0.70, sway_mult = 0.8;
    int strands = 3;
    for (int i = 0; i < strands; i++) {
        double sx = x + (i - 1) * height * 0.14;
        double sh = height * (0.75 + 0.25 * (i % 2));
        double phase = t * 1.1 + i * 1.7;
        double sway1 = sin(phase) * sh * 0.16 * sway_mult;
        double sway2 = sin(phase * 0.8 + 0.6) * sh * 0.30 * sway_mult;

        cairo_set_source_rgba(cr, r, g, b, a * alpha_mult);
        cairo_set_line_width(cr, sh * 0.05);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, sx, base_y);
        cairo_curve_to(cr,
                        sx + sway1, base_y - sh * 0.35,
                        sx + sway2, base_y - sh * 0.7,
                        sx + sway2 * 1.2, base_y - sh);
        cairo_stroke(cr);
    }
}

typedef struct {
    double wood_r, wood_g, wood_b;
    double wood_dark_r, wood_dark_g, wood_dark_b;
    double band_r, band_g, band_b;
} FFShipPalette;

static const FFShipPalette FF_SHIP_PALETTES[] = {
    {0.55, 0.36, 0.18,  0.35, 0.22, 0.10,  0.18, 0.18, 0.20}, // oak / iron bands
    {0.42, 0.26, 0.14,  0.26, 0.15, 0.08,  0.55, 0.45, 0.20}, // mahogany / brass bands
    {0.48, 0.42, 0.34,  0.30, 0.26, 0.20,  0.14, 0.14, 0.16}, // weathered grey wood
};
#define FF_SHIP_PALETTE_COUNT (int)(sizeof(FF_SHIP_PALETTES) / sizeof(FF_SHIP_PALETTES[0]))

// A rectangular wooden beam segment with plank seams and grain streaks.
static void ff_draw_ship_beam(cairo_t *cr, double cx, double cy, double w, double h,
                               double seed, const FFShipPalette *pal) {
    double x0 = cx - w * 0.5, y0 = cy - h * 0.5;
    cairo_set_source_rgb(cr, pal->wood_r, pal->wood_g, pal->wood_b);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, pal->wood_dark_r, pal->wood_dark_g, pal->wood_dark_b, 0.55);
    cairo_set_line_width(cr, fmax(1.0, h * 0.05));
    for (int i = 1; i < 3; i++) {
        double py = y0 + h * i / 3.0;
        cairo_move_to(cr, x0, py);
        cairo_line_to(cr, x0 + w, py);
        cairo_stroke(cr);
    }
    cairo_set_line_width(cr, fmax(1.0, w * 0.035));
    for (int i = 0; i < 3; i++) {
        double gx = x0 + w * (0.2 + 0.3 * i) + (ff_hash(seed * 3.3 + i) - 0.5) * w * 0.08;
        cairo_move_to(cr, gx, y0 + h * 0.08);
        cairo_line_to(cr, gx, y0 + h * 0.92);
        cairo_stroke(cr);
    }

    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_stroke(cr);
}

// A barrel: an ellipse body plus two metal bands.
static void ff_draw_ship_barrel(cairo_t *cr, double cx, double cy, double w, double h,
                                 const FFShipPalette *pal) {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, w * 0.5, h * 0.5);
    cairo_set_source_rgb(cr, fmin(1.0, pal->wood_r * 1.08), fmin(1.0, pal->wood_g * 1.05), pal->wood_b);
    cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    cairo_set_line_width(cr, 2.0);
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, w * 0.5, h * 0.5);
    cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, pal->band_r, pal->band_g, pal->band_b);
    cairo_set_line_width(cr, fmax(1.0, h * 0.12));
    cairo_move_to(cr, cx - w * 0.48, cy - h * 0.22);
    cairo_line_to(cr, cx + w * 0.48, cy - h * 0.22);
    cairo_stroke(cr);
    cairo_move_to(cr, cx - w * 0.48, cy + h * 0.22);
    cairo_line_to(cr, cx + w * 0.48, cy + h * 0.22);
    cairo_stroke(cr);
}

// Draws one obstacle column as alternating wooden beams and barrels, with a
// small mast/yard-arm-and-flag cluster at the gap-facing tip in place of the
// coral head. Purely rectilinear (no random reach), so unlike coral it never
// needs a safety scale-down to stay inside the collision box - beams/barrels
// are sized directly from `width` and the yard arm/flag are hand-clamped to
// stay well inside it.
void ff_draw_ship_column(cairo_t *cr, double x, double y0, double y1,
                          double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFShipPalette *pal =
        &FF_SHIP_PALETTES[(int)(ff_hash(seed * 41.0) * 97.0) % FF_SHIP_PALETTE_COUNT];

    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double seg_h = width * 0.62;
    int segs = (int)fmax(2.0, round(total_h / seg_h));
    seg_h = total_h / segs;

    for (int i = 0; i < segs; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        bool barrel = ((i % 2) == 0) == tip_at_y1;
        double jitter = (ff_hash(seed * 7.7 + i * 1.9) - 0.5) * width * 0.06;
        double seg_w = width * 0.88;
        double seg_hh = seg_h * 0.92;
        if (barrel) {
            ff_draw_ship_barrel(cr, cx + jitter, seg_y, seg_w, seg_hh, pal);
        } else {
            ff_draw_ship_beam(cr, cx + jitter, seg_y, seg_w, seg_hh, seed + i * 3.1, pal);
        }
    }

    // Mast post + yard arm + flag right at the gap-facing tip, entirely on
    // the rooted side of the boundary so nothing pokes into the gap itself.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double post_h = fmin(width * 0.5, total_h * 0.9);
    double post_cy = tip_y + inward * post_h * 0.5;
    ff_draw_ship_beam(cr, cx, post_cy, width * 0.28, post_h, seed * 13.0, pal);

    double arm_y = tip_y + inward * width * 0.10;
    double arm_len = width * 0.30;
    cairo_set_source_rgb(cr, pal->wood_dark_r, pal->wood_dark_g, pal->wood_dark_b);
    cairo_set_line_width(cr, fmax(1.0, width * 0.08));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, cx - arm_len, arm_y);
    cairo_line_to(cr, cx + arm_len, arm_y);
    cairo_stroke(cr);

    // Jolly Roger - a black flag with a small skull-and-crossbones, the one
    // symbol that unmistakably says "pirate" at a glance.
    double flag_x = cx + arm_len;
    double flag_w = width * 0.15, flag_h = width * 0.13;
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_move_to(cr, flag_x, arm_y - flag_h * 0.5);
    cairo_line_to(cr, flag_x + flag_w, arm_y);
    cairo_line_to(cr, flag_x, arm_y + flag_h * 0.5);
    cairo_close_path(cr);
    cairo_fill(cr);

    double skx = flag_x + flag_w * 0.38, sky = arm_y;
    double skr = flag_h * 0.22;
    cairo_set_line_width(cr, fmax(1.0, flag_h * 0.07));
    cairo_set_source_rgb(cr, 0.94, 0.94, 0.90);
    cairo_move_to(cr, skx - skr, sky + skr * 0.9);
    cairo_line_to(cr, skx + skr, sky - skr * 0.9);
    cairo_stroke(cr);
    cairo_move_to(cr, skx - skr, sky - skr * 0.9);
    cairo_line_to(cr, skx + skr, sky + skr * 0.9);
    cairo_stroke(cr);
    cairo_arc(cr, skx, sky, skr, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_arc(cr, skx - skr * 0.35, sky - skr * 0.1, skr * 0.22, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, skx + skr * 0.35, sky - skr * 0.1, skr * 0.22, 0, 2 * M_PI);
    cairo_fill(cr);
}
