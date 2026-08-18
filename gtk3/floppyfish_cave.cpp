#include "floppyfish_common.h"

// --- Dark cave theme --------------------------------------------------------

void ff_cave_sky_colors(double *top_r, double *top_g, double *top_b,
                         double *bot_r, double *bot_g, double *bot_b) {
    *top_r = 0.04; *top_g = 0.04; *top_b = 0.09;
    *bot_r = 0.11; *bot_g = 0.09; *bot_b = 0.17;
}

void ff_cave_particle_color(double *r, double *g, double *b, double *a) {
    *r = 0.55; *g = 0.75; *b = 0.95; *a = 0.18;
}

void ff_draw_cave_backdrop(cairo_t *cr, double w, double h, double base_y) {
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.6);
    cairo_move_to(cr, 0, base_y);
    for (double x = 0; x <= w + 1; x += w / 18.0) {
        double bump = 24.0 + 30.0 * fabs(sin(x * 0.045 + 2.3));
        cairo_line_to(cr, x, base_y - bump);
    }
    cairo_line_to(cr, w, base_y);
    cairo_close_path(cr);
    cairo_fill(cr);
}

// Static: this floor never had any bubble_phase-driven motion (the ridge
// and glowing flecks are all fixed positions), so the whole thing is
// cacheable and there's nothing left over for the scroll half below.
void ff_draw_cave_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.10, 0.09, 0.13);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.16, 0.14, 0.20);
    cairo_move_to(cr, 0, h - floor_h);
    for (double x = 0; x <= w + 1; x += w / 24.0) {
        double bump = 4.0 + 8.0 * fabs(sin(x * 0.09 + 1.1));
        cairo_line_to(cr, x, h - floor_h - bump);
    }
    cairo_line_to(cr, w, h - floor_h);
    cairo_close_path(cr);
    cairo_fill(cr);
    for (int i = 0; i < 14; i++) {
        double fx = fmod(i * 137.0, w);
        double fy = h - floor_h * 0.4 + (i % 3) * floor_h * 0.15;
        cairo_set_source_rgba(cr, 0.5, 0.8, 0.95, 0.5);
        cairo_arc(cr, fx, fy, 2.0, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// Scroll: nothing to animate here - kept only so the cave theme satisfies
// the same static/scroll contract as the other three.
void ff_draw_cave_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    (void)cr; (void)w; (void)h; (void)floor_h; (void)bubble_phase;
}

void ff_draw_cave_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    double r = 0.55, g = 0.80, b = 0.95, a = 0.55, sway_mult = 0.25;
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

// --- Small geometry helper shared only by this file's collision-matching
// taper shape (see ff_point_triangle_dist2 in the core file for the actual
// hit test against it).

typedef struct {
    double rock_r, rock_g, rock_b;
    double crystal_r, crystal_g, crystal_b;
} FFCavePalette;

static const FFCavePalette FF_CAVE_PALETTES[] = {
    {0.16, 0.15, 0.21,  0.45, 0.85, 0.95}, // dark slate / cyan crystal
    {0.14, 0.11, 0.17,  0.75, 0.45, 0.95}, // near-black / purple crystal
    {0.19, 0.13, 0.13,  0.95, 0.55, 0.30}, // dark red rock / amber crystal
    {0.11, 0.16, 0.15,  0.50, 0.95, 0.65}, // dark green rock / green crystal
};
#define FF_CAVE_PALETTE_COUNT (int)(sizeof(FF_CAVE_PALETTES) / sizeof(FF_CAVE_PALETTES[0]))

// A jagged stalactite/stalagmite: a polygon tapering from full width at the
// rooted end down to a point at the gap-facing tip, with per-segment jitter
// scaled to that segment's own remaining half-width - so however jagged it
// gets, it can never reach past the pipe_width edges it started from, no
// clipping or scale-down needed. A handful of small glowing crystal flecks
// are studded along it for color and to sell the "cave" read.
void ff_draw_cave_column(cairo_t *cr, double x, double y0, double y1,
                          double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFCavePalette *pal =
        &FF_CAVE_PALETTES[(int)(ff_hash(seed * 53.0) * 97.0) % FF_CAVE_PALETTE_COUNT];

    double cx = x + width * 0.5;
    const int segs = 7;
    double leftx[segs + 1], rightx[segs + 1], ys[segs + 1], halfs[segs + 1];
    for (int i = 0; i <= segs; i++) {
        double t = (double)i / segs;
        double y = y0 + t * (y1 - y0);
        double tip_t = tip_at_y1 ? t : 1.0 - t; // 0 at rooted end, 1 at the point
        double half = width * 0.5 * (1.0 - tip_t);
        double jl = 0.70 + 0.30 * ff_hash(seed * 5.1 + i * 1.7);
        double jr = 0.70 + 0.30 * ff_hash(seed * 6.3 + i * 2.1);
        leftx[i] = cx - half * jl;
        rightx[i] = cx + half * jr;
        ys[i] = y;
        halfs[i] = half;
    }

    cairo_new_path(cr);
    cairo_move_to(cr, leftx[0], ys[0]);
    for (int i = 1; i <= segs; i++) cairo_line_to(cr, leftx[i], ys[i]);
    for (int i = segs; i >= 0; i--) cairo_line_to(cr, rightx[i], ys[i]);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, pal->rock_r, pal->rock_g, pal->rock_b);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
    cairo_set_line_width(cr, fmax(1.0, width * 0.03));
    cairo_stroke(cr);

    // Crack texture.
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_set_line_width(cr, fmax(1.0, width * 0.025));
    double seg_span = (y1 - y0) / segs;
    for (int i = 1; i < segs; i += 2) {
        double midx = (leftx[i] + rightx[i]) * 0.5;
        cairo_move_to(cr, midx - halfs[i] * 0.3, ys[i] - seg_span * 0.3);
        cairo_line_to(cr, midx + halfs[i] * 0.25, ys[i] + seg_span * 0.3);
        cairo_stroke(cr);
    }

    // Glowing crystal flecks, kept well inside this segment's own bounds.
    for (int i = 1; i < segs; i++) {
        if (ff_hash(seed * 8.8 + i * 2.3) < 0.55) continue;
        if (halfs[i] < width * 0.04) continue;
        double cxx = cx + (ff_hash(seed * 9.9 + i * 3.1) - 0.5) * halfs[i];
        double crad = width * 0.04 + width * 0.025 * ff_hash(seed * 10.5 + i);
        cairo_set_source_rgba(cr, pal->crystal_r, pal->crystal_g, pal->crystal_b, 0.25);
        cairo_arc(cr, cxx, ys[i], crad * 2.0, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, pal->crystal_r, pal->crystal_g, pal->crystal_b, 0.95);
        cairo_arc(cr, cxx, ys[i], crad, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}
