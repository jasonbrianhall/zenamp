#include "floppyfish_common.h"

// --- Party theme ------------------------------------------------------
// A black-lit underwater dance party - the eleventh theme zone alongside
// reef/ship/cave/atlantis/rainbow/dino/antarctic/aquarium/galaxy/swamp.
// Obstacles are poles wrapped in spiral paper streamers, EXCEPT for the
// one segment closest to the gap, which drops the streamers entirely and
// is just a plain pillar - crowned with a little gold party crown. The
// sky is party-black, lit by a hanging disco ball, sweeping colored
// spotlight beams, a string-light garland, and a couple of silhouetted
// fish caught mid dance-move.

void ff_party_sky_colors(double *top_r, double *top_g, double *top_b,
                          double *bot_r, double *bot_g, double *bot_b) {
    // Near-black overhead, the faintest warm glow low down where the
    // spotlights and dance floor lights bounce off the water.
    *top_r = 0.02; *top_g = 0.02; *top_b = 0.04;
    *bot_r = 0.10; *bot_g = 0.05; *bot_b = 0.14;
}

void ff_party_particle_color(double *r, double *g, double *b, double *a) {
    // Drifting confetti motes - bright and warm so they actually read
    // against the black sky.
    *r = 1.0; *g = 0.92; *b = 0.55; *a = 0.55;
}

// Two streamer stripe colors plus the plain pillar color, all sharing one
// "party palette" so a given column's streamers, plain segment, and
// seaweed-slot ribbon read as one coordinated color scheme.
typedef struct {
    double s1_r, s1_g, s1_b;
    double s2_r, s2_g, s2_b;
    double plain_r, plain_g, plain_b;
} FFPartyPalette;

static const FFPartyPalette FF_PARTY_PALETTES[] = {
    {0.98, 0.20, 0.55,  0.20, 0.85, 0.95,  0.16, 0.06, 0.20}, // hot pink / cyan / deep purple
    {0.98, 0.80, 0.15,  0.85, 0.20, 0.90,  0.10, 0.08, 0.22}, // gold / magenta / navy
    {0.98, 0.45, 0.10,  0.55, 0.95, 0.25,  0.20, 0.05, 0.10}, // orange / lime / maroon
    {0.25, 0.90, 0.95,  0.98, 0.85, 0.15,  0.06, 0.10, 0.20}, // cyan / gold / charcoal-blue
};
#define FF_PARTY_PALETTE_COUNT (int)(sizeof(FF_PARTY_PALETTES) / sizeof(FF_PARTY_PALETTES[0]))

static const double FF_CROWN_GOLD_R = 1.0, FF_CROWN_GOLD_G = 0.85, FF_CROWN_GOLD_B = 0.25;

// One rotating spotlight beam - a soft triangular gradient fanning down
// from a fixed point, like a light rig up in the rafters.
static void ff_draw_party_spotlight(cairo_t *cr, double ox, double oy, double angle, double len,
                                     double spread, double r, double g, double b, double alpha) {
    double a0 = angle - spread * 0.5, a1 = angle + spread * 0.5;
    double x1 = ox + cos(a0) * len, y1 = oy + sin(a0) * len;
    double x2 = ox + cos(a1) * len, y2 = oy + sin(a1) * len;

    cairo_pattern_t *grad = cairo_pattern_create_linear(ox, oy, ox + cos(angle) * len, oy + sin(angle) * len);
    cairo_pattern_add_color_stop_rgba(grad, 0.0, r, g, b, alpha);
    cairo_pattern_add_color_stop_rgba(grad, 1.0, r, g, b, 0.0);
    cairo_set_source(cr, grad);
    cairo_move_to(cr, ox, oy);
    cairo_line_to(cr, x1, y1);
    cairo_line_to(cr, x2, y2);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_pattern_destroy(grad);
}

// A hanging disco ball: a shaded sphere with a facet grid and a few bright
// glints, on a thin cable up into the dark - the light source the
// spotlight beams below are implicitly meant to be coming from.
static void ff_draw_party_discoball(cairo_t *cr, double cx, double cy, double r) {
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.55, 0.6);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, cx, 0);
    cairo_line_to(cr, cx, cy - r);
    cairo_stroke(cr);

    cairo_pattern_t *grad = cairo_pattern_create_radial(cx - r * 0.3, cy - r * 0.3, r * 0.1, cx, cy, r * 1.1);
    cairo_pattern_add_color_stop_rgba(grad, 0.0, 0.95, 0.95, 1.0, 0.9);
    cairo_pattern_add_color_stop_rgba(grad, 0.6, 0.65, 0.68, 0.75, 0.85);
    cairo_pattern_add_color_stop_rgba(grad, 1.0, 0.25, 0.27, 0.32, 0.85);
    cairo_set_source(cr, grad);
    cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(grad);

    // Facet grid - a handful of chords across the disc, standing in for
    // the mirrored tile lines.
    cairo_set_source_rgba(cr, 0.15, 0.15, 0.18, 0.5);
    cairo_set_line_width(cr, fmax(1.0, r * 0.06));
    for (int i = -2; i <= 2; i++) {
        double fx = i * r * 0.35;
        cairo_move_to(cr, cx + fx, cy - sqrt(fmax(0.0, r * r - fx * fx)));
        cairo_line_to(cr, cx + fx, cy + sqrt(fmax(0.0, r * r - fx * fx)));
        cairo_stroke(cr);
    }
    for (int i = -2; i <= 2; i++) {
        double fy = i * r * 0.35;
        cairo_move_to(cr, cx - sqrt(fmax(0.0, r * r - fy * fy)), cy + fy);
        cairo_line_to(cr, cx + sqrt(fmax(0.0, r * r - fy * fy)), cy + fy);
        cairo_stroke(cr);
    }

    // A few bright glints catching the light.
    static const double gx[4] = {-0.35, 0.25, -0.1, 0.4};
    static const double gy[4] = {-0.3, -0.15, 0.35, 0.2};
    for (int i = 0; i < 4; i++) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.85);
        cairo_arc(cr, cx + gx[i] * r, cy + gy[i] * r, r * 0.06, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// A garland of string lights swagged across the top of the scene - a
// scalloped cable with small colored bulbs at even intervals.
static void ff_draw_party_string_lights(cairo_t *cr, double w, double h) {
    static const double bulb_r_col[6][3] = {
        {0.98, 0.2, 0.3}, {0.98, 0.75, 0.15}, {0.3, 0.95, 0.4},
        {0.25, 0.7, 0.98}, {0.85, 0.25, 0.95}, {0.98, 0.5, 0.15},
    };
    int swags = 5;
    double top_y = h * 0.03;
    double sag = h * 0.05;

    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.5);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, 0, top_y);
    for (int i = 0; i <= swags; i++) {
        double x = w * i / (double)swags;
        double y = top_y + ((i % 2 == 0) ? 0 : sag);
        cairo_curve_to(cr, x - w / swags * 0.5, y + sag, x - w / swags * 0.5, y + sag, x, y);
    }
    cairo_stroke(cr);

    int bulbs_per_swag = 6;
    int idx = 0;
    for (int i = 0; i < swags; i++) {
        double x0 = w * i / (double)swags, x1 = w * (i + 1) / (double)swags;
        double y0 = top_y + ((i % 2 == 0) ? 0 : sag), y1 = top_y + ((i % 2 == 0) ? sag : 0);
        for (int k = 1; k < bulbs_per_swag; k++) {
            double t = (double)k / bulbs_per_swag;
            double bx = x0 + (x1 - x0) * t;
            double by = y0 + (y1 - y0) * sin(t * M_PI * 0.5) * 1.0 + (y1 - y0) * 0.0;
            // Approximate the sag with a simple parabola instead of chasing
            // the bezier above exactly - close enough for small bulbs.
            by = y0 + (y1 - y0) * t + sag * 0.5 * sin(t * M_PI);
            const double *col = bulb_r_col[idx % 6];
            cairo_set_source_rgba(cr, col[0], col[1], col[2], 0.85);
            cairo_arc(cr, bx, by, h * 0.006, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_set_source_rgba(cr, col[0], col[1], col[2], 0.25);
            cairo_arc(cr, bx, by, h * 0.013, 0, 2 * M_PI);
            cairo_fill(cr);
            idx++;
        }
    }
}

// One silhouetted fish frozen mid dance-move: a chunky body, a raised
// "arm" fin thrown up, and a kicked-out tail - filled dark so it reads as
// a silhouette against the black sky, with a bright neon outline instead
// of the usual shaded body since there's no ambient light to shade it by.
static void ff_draw_party_dancing_fish(cairo_t *cr, double x, double y, double s, double lean,
                                        double r, double g, double b) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, s, s);
    cairo_rotate(cr, lean);

    cairo_set_source_rgba(cr, 0.03, 0.02, 0.05, 0.92);

    // Body.
    cairo_move_to(cr, -14, 0);
    cairo_curve_to(cr, -10, -14, 10, -14, 18, -4);
    cairo_curve_to(cr, 22, 0, 22, 2, 18, 6);
    cairo_curve_to(cr, 10, 15, -10, 13, -14, 0);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, r, g, b, 0.85);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);

    // Tail, kicked out to one side mid-move.
    cairo_set_source_rgba(cr, 0.03, 0.02, 0.05, 0.92);
    cairo_move_to(cr, -14, 2);
    cairo_curve_to(cr, -22, -6, -30, 4, -34, 16);
    cairo_curve_to(cr, -24, 12, -18, 8, -14, 2);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, r, g, b, 0.8);
    cairo_set_line_width(cr, 1.3);
    cairo_stroke(cr);

    // Raised "arm" fin, thrown straight up like it's caught mid dance-move.
    cairo_set_source_rgba(cr, 0.03, 0.02, 0.05, 0.92);
    cairo_move_to(cr, 2, -8);
    cairo_curve_to(cr, 4, -22, 8, -32, 12, -38);
    cairo_curve_to(cr, 14, -30, 12, -20, 8, -8);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, r, g, b, 0.85);
    cairo_set_line_width(cr, 1.3);
    cairo_stroke(cr);

    // Eye.
    cairo_set_source_rgba(cr, r, g, b, 0.9);
    cairo_arc(cr, 10, -2, 1.6, 0, 2 * M_PI);
    cairo_fill(cr);

    cairo_restore(cr);
}

// The full black sky: a hanging disco ball, three sweeping spotlight
// beams in different colors, a string-light garland, and a little knot of
// fish caught mid dance-move - this theme's equivalent of the ship's hull
// or Atlantis's temple: the one backdrop that most says "party" at a
// glance. Fixed angles/positions since this backdrop layer is cached, not
// animated frame to frame (see ff_draw_theme_sky_static).
void ff_draw_party_backdrop(cairo_t *cr, double w, double h, double base_y) {
    ff_draw_party_spotlight(cr, w * 0.20, 0, M_PI * 0.36, h * 1.1, 0.55, 0.98, 0.2, 0.5, 0.16);
    ff_draw_party_spotlight(cr, w * 0.50, 0, M_PI * 0.5,  h * 1.1, 0.50, 0.25, 0.85, 0.98, 0.14);
    ff_draw_party_spotlight(cr, w * 0.80, 0, M_PI * 0.64, h * 1.1, 0.55, 0.98, 0.85, 0.2, 0.16);

    ff_draw_party_discoball(cr, w * 0.5, h * 0.09, h * 0.028);
    ff_draw_party_string_lights(cr, w, h);

    ff_draw_party_dancing_fish(cr, w * 0.28, base_y - h * 0.16, h * 0.0032, -0.15, 0.98, 0.25, 0.55);
    ff_draw_party_dancing_fish(cr, w * 0.40, base_y - h * 0.11, h * 0.0026,  0.22, 0.25, 0.9, 0.98);
    ff_draw_party_dancing_fish(cr, w * 0.72, base_y - h * 0.14, h * 0.0030,  0.12, 0.98, 0.85, 0.2);
}

// Static: the base dance-floor fill (a dark checkerboard of light-up
// tiles). No bubble_phase dependency, so this is the cacheable part.
void ff_draw_party_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.05, 0.04, 0.07);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);

    double tile = floor_h * 0.5;
    int cols = (int)ceil(w / tile) + 1;
    int rows = 2;
    for (int cRow = 0; cRow < rows; cRow++) {
        for (int cCol = 0; cCol < cols; cCol++) {
            if ((cRow + cCol) % 2 != 0) continue;
            const FFPartyPalette *pal = &FF_PARTY_PALETTES[(cCol * 7 + cRow * 3) % FF_PARTY_PALETTE_COUNT];
            cairo_set_source_rgba(cr, pal->s1_r, pal->s1_g, pal->s1_b, 0.18);
            cairo_rectangle(cr, cCol * tile, h - floor_h + cRow * tile, tile, tile);
            cairo_fill(cr);
        }
    }
}

// Scroll: pulsing tile-edge glow lines, plus a scatter of drifting
// confetti flecks - this theme's equivalent of the ship's coins or the
// Atlantis floor's gems.
void ff_draw_party_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    double tile = floor_h * 0.5;
    double pulse = 0.5 + 0.5 * sin(bubble_phase * 3.0);
    cairo_set_source_rgba(cr, 0.9, 0.85, 0.5, 0.15 + 0.20 * pulse);
    cairo_set_line_width(cr, 1.5);
    for (double x = -fmod(bubble_phase * (h * 0.10), tile); x < w; x += tile) {
        cairo_move_to(cr, x, h - floor_h);
        cairo_line_to(cr, x, h);
        cairo_stroke(cr);
    }

    for (int i = 0; i < 16; i++) {
        double cx = fmod(i * 97.0 - fmod(bubble_phase * (h * 0.05), w), w);
        if (cx < 0) cx += w;
        double cy = h - floor_h * (0.15 + 0.7 * ((i * 61) % 7) / 7.0);
        const FFPartyPalette *pal = &FF_PARTY_PALETTES[i % FF_PARTY_PALETTE_COUNT];
        double s = floor_h * 0.035;
        cairo_save(cr);
        cairo_translate(cr, cx, cy);
        cairo_rotate(cr, i * 1.7 + bubble_phase * 0.5);
        cairo_set_source_rgba(cr, (i % 2 == 0) ? pal->s1_r : pal->s2_r,
                                    (i % 2 == 0) ? pal->s1_g : pal->s2_g,
                                    (i % 2 == 0) ? pal->s1_b : pal->s2_b, 0.85);
        cairo_rectangle(cr, -s * 0.5, -s * 0.5, s, s);
        cairo_fill(cr);
        cairo_restore(cr);
    }
}

// A cluster of curly paper-ribbon streamers standing in for seaweed - the
// same three-strand slot as every other theme's decoration, drawn as
// tightly curled party-ribbon spirals in bright colors and tipped with a
// tiny confetti-burst instead of a leafy tip.
void ff_draw_party_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    int strands = 3;
    for (int i = 0; i < strands; i++) {
        const FFPartyPalette *pal = &FF_PARTY_PALETTES[i % FF_PARTY_PALETTE_COUNT];
        double sx = x + (i - 1) * height * 0.16;
        double sh = height * (0.7 + 0.3 * (i % 2));
        double phase = t * 1.2 + i * 1.9;
        double sway = sin(phase) * sh * 0.18;

        cairo_set_source_rgba(cr, (i % 2 == 0) ? pal->s1_r : pal->s2_r,
                                    (i % 2 == 0) ? pal->s1_g : pal->s2_g,
                                    (i % 2 == 0) ? pal->s1_b : pal->s2_b, 0.8 * alpha_mult);
        cairo_set_line_width(cr, sh * 0.05);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        // Tightly curled ribbon spiral rather than a simple bent strand.
        double px = sx, py = base_y;
        cairo_move_to(cr, px, py);
        int coils = 4;
        double tip_x = sx, tip_y = base_y;
        for (int c = 0; c < coils; c++) {
            double cy = base_y - sh * (c + 1) / (double)coils;
            double cx = sx + sin(phase + c * 1.4) * sh * 0.10 + ((c % 2 == 0) ? sh * 0.09 : -sh * 0.09) + sway * (c / (double)coils);
            cairo_curve_to(cr, px + sh * 0.06, py - sh * 0.06 / coils * 4,
                                cx - sh * 0.06, cy + sh * 0.06 / coils * 4,
                                cx, cy);
            px = cx; py = cy;
            tip_x = cx; tip_y = cy;
        }
        cairo_stroke(cr);

        double flicker = 0.6 + 0.4 * sin(t * 7.0 + i * 2.3);
        double gs = sh * 0.10;
        cairo_set_source_rgba(cr, 1.0, 0.95, 0.6, 0.7 * alpha_mult * flicker);
        cairo_arc(cr, tip_x, tip_y, gs * 0.4, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 1.0, 0.95, 0.6, 0.25 * alpha_mult * flicker);
        cairo_arc(cr, tip_x, tip_y, gs, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// One streamer-wrapped pole segment: the base color of the plain pillar
// (dark, so the wrap pops), a diagonal candy-stripe spiral in the two
// palette colors, and a thin dark seam where the wrap overlaps - the
// detail that sells "wound streamer paper" instead of a painted stripe.
static void ff_draw_party_streamer_segment(cairo_t *cr, double cx, double cy, double w, double h,
                                            double seed, const FFPartyPalette *pal) {
    double x0 = cx - w * 0.5, y0 = cy - h * 0.5;

    cairo_set_source_rgb(cr, pal->plain_r, pal->plain_g, pal->plain_b);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_fill(cr);

    cairo_save(cr);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_clip(cr);

    double stripe_w = w * 0.6;
    double dir = (ff_hash(seed * 9.1) > 0.5) ? 1.0 : -1.0;
    bool toggle = false;
    for (double sx = x0 - h * 1.5; sx < x0 + w + h * 1.5; sx += stripe_w) {
        cairo_set_source_rgba(cr, toggle ? pal->s1_r : pal->s2_r,
                                    toggle ? pal->s1_g : pal->s2_g,
                                    toggle ? pal->s1_b : pal->s2_b, 0.92);
        cairo_move_to(cr, sx, y0);
        cairo_line_to(cr, sx + stripe_w * 0.5, y0);
        cairo_line_to(cr, sx + stripe_w * 0.5 + dir * h, y0 + h);
        cairo_line_to(cr, sx + dir * h, y0 + h);
        cairo_close_path(cr);
        cairo_fill(cr);
        toggle = !toggle;
    }
    cairo_restore(cr);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.3);
    cairo_set_line_width(cr, fmax(1.0, w * 0.02));
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_stroke(cr);

    // An occasional loose streamer tail flicking out to the side.
    if (ff_hash(seed * 12.3) > 0.65) {
        double side = (ff_hash(seed * 13.7) > 0.5) ? 1.0 : -1.0;
        double tail_len = w * 0.5;
        double lx0 = cx + side * w * 0.42, ly0 = cy;
        cairo_set_source_rgba(cr, pal->s1_r, pal->s1_g, pal->s1_b, 0.85);
        cairo_set_line_width(cr, fmax(1.0, w * 0.06));
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, lx0, ly0);
        cairo_curve_to(cr, lx0 + side * tail_len * 0.4, ly0 - tail_len * 0.3,
                            lx0 + side * tail_len * 0.7, ly0 + tail_len * 0.3,
                            lx0 + side * tail_len, ly0);
        cairo_stroke(cr);
    }
}

// The one plain segment of the column - no streamer wrap at all, just a
// flat-shaded pillar block with a light-catching edge, standing in
// deliberate contrast to every streamer segment around it.
static void ff_draw_party_plain_segment(cairo_t *cr, double cx, double cy, double w, double h,
                                         const FFPartyPalette *pal) {
    double x0 = cx - w * 0.5, y0 = cy - h * 0.5;
    cairo_set_source_rgb(cr, pal->plain_r, pal->plain_g, pal->plain_b);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
    cairo_rectangle(cr, x0, y0, w * 0.25, h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_set_line_width(cr, fmax(1.0, w * 0.02));
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_stroke(cr);
}

// The little gold party crown capping the plain tip segment - a scalloped
// band with three points, each finished with a small colored jewel, plus
// one larger jewel set in the center point.
static void ff_draw_party_crown(cairo_t *cr, double cx, double tip_y, double inward, double width, double seed) {
    double crown_w = width * 0.62;
    double crown_h = width * 0.5;
    double band_h = crown_h * 0.28;

    // Band.
    cairo_set_source_rgb(cr, FF_CROWN_GOLD_R, FF_CROWN_GOLD_G, FF_CROWN_GOLD_B);
    cairo_rectangle(cr, cx - crown_w * 0.5, tip_y, crown_w, inward * band_h);
    cairo_fill(cr);

    // Three points rising off the band.
    double base_y2 = tip_y + inward * band_h;
    int points = 3;
    for (int p = 0; p < points; p++) {
        double t0 = (double)p / points, t1 = (double)(p + 1) / points;
        double px0 = cx - crown_w * 0.5 + crown_w * t0;
        double px1 = cx - crown_w * 0.5 + crown_w * t1;
        double pxm = (px0 + px1) * 0.5;
        double ph = crown_h * (p == points / 2 ? 0.85 : 0.62);
        cairo_move_to(cr, px0, base_y2);
        cairo_line_to(cr, pxm, base_y2 + inward * ph);
        cairo_line_to(cr, px1, base_y2);
        cairo_close_path(cr);
        cairo_fill(cr);

        // Jewel at the tip of each point.
        static const double jewel_cols[3][3] = {
            {0.92, 0.15, 0.20}, {0.15, 0.55, 0.92}, {0.20, 0.85, 0.35},
        };
        const double *jc = jewel_cols[(p + (int)(ff_hash(seed * 5.0) * 3.0)) % 3];
        cairo_set_source_rgba(cr, jc[0], jc[1], jc[2], 0.95);
        cairo_arc(cr, pxm, base_y2 + inward * ph, width * 0.045, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, FF_CROWN_GOLD_R, FF_CROWN_GOLD_G, FF_CROWN_GOLD_B);
    }

    // A row of small bead jewels along the band.
    for (int i = 0; i < 4; i++) {
        double bx = cx - crown_w * 0.5 + crown_w * (i + 0.5) / 4.0;
        cairo_set_source_rgba(cr, 0.95, 0.95, 0.85, 0.9);
        cairo_arc(cr, bx, tip_y + inward * band_h * 0.5, width * 0.025, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    cairo_set_source_rgba(cr, 0.55, 0.40, 0.05, 0.6);
    cairo_set_line_width(cr, fmax(1.0, width * 0.02));
    cairo_rectangle(cr, cx - crown_w * 0.5, tip_y, crown_w, inward * band_h);
    cairo_stroke(cr);
}

// Draws one obstacle column as a pole wrapped in spiral paper streamers -
// except for the single segment closest to the gap-facing tip, which
// drops the streamers and is drawn as a plain pillar, topped with a
// little gold party crown. Rectilinear like the ship/Atlantis columns, so
// it needs no safety scale-down to stay inside the collision box.
void ff_draw_party_column(cairo_t *cr, double x, double y0, double y1,
                           double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFPartyPalette *pal =
        &FF_PARTY_PALETTES[(int)(ff_hash(seed * 43.0) * 97.0) % FF_PARTY_PALETTE_COUNT];

    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double seg_h = width * 0.55;
    int segs = (int)fmax(2.0, round(total_h / seg_h));
    seg_h = total_h / segs;

    int tip_idx = tip_at_y1 ? (segs - 1) : 0;

    for (int i = 0; i < segs; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        if (i == tip_idx) {
            ff_draw_party_plain_segment(cr, cx, seg_y, width * 0.92, seg_h * 0.96, pal);
        } else {
            ff_draw_party_streamer_segment(cr, cx, seg_y, width * 0.92, seg_h * 0.96,
                                            seed + i * 5.7, pal);
        }
    }

    // The crown, sitting right on the plain tip segment - this theme's
    // real signature, so it appears on every single pillar.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    ff_draw_party_crown(cr, cx, tip_y, inward, width, seed);
}
