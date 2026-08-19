#include "floppyfish_common.h"

// --- Rainbow theme -------------------------------------------------------
// A bright, whimsical sky-realm - the fifth theme zone alongside
// reef/ship/cave/atlantis. Obstacles are candy-striped rainbow pillars
// capped with fluffy cloud puffs, the "sea floor" is a bank of clouds
// instead of sand/planks/rock/mosaic, and the seaweed slot becomes a
// cotton-candy tuft swaying on a stalk.

void ff_rainbow_sky_colors(double *top_r, double *top_g, double *top_b,
                            double *bot_r, double *bot_g, double *bot_b) {
    *top_r = 0.45; *top_g = 0.68; *top_b = 0.96;
    *bot_r = 0.80; *bot_g = 0.90; *bot_b = 1.0;
}

void ff_rainbow_particle_color(double *r, double *g, double *b, double *a) {
    // Warm golden sparkle/glitter drifting up, distinct from every other
    // theme's cooler bubble tones.
    *r = 1.0; *g = 0.93; *b = 0.65; *a = 0.30;
}

// The seven ROYGBIV bands, shared by the backdrop arc, the pillar stripes,
// the floor sparkles, and the guest unicorn's mane/tail so the "rainbow"
// palette reads consistently everywhere it shows up.
typedef struct { double r, g, b; } FFRainbowBand;
static const FFRainbowBand FF_RAINBOW_BANDS[] = {
    {0.90, 0.16, 0.20}, // red
    {0.95, 0.55, 0.16}, // orange
    {0.98, 0.85, 0.22}, // yellow
    {0.30, 0.75, 0.36}, // green
    {0.26, 0.56, 0.95}, // blue
    {0.46, 0.32, 0.86}, // indigo
    {0.78, 0.38, 0.86}, // violet
};
#define FF_RAINBOW_BAND_COUNT (int)(sizeof(FF_RAINBOW_BANDS) / sizeof(FF_RAINBOW_BANDS[0]))

// Distant fluffy cloud-bump horizon, echoing the reef's coral-bump skyline
// but soft and white, plus the one big decorative rainbow arc that makes
// this theme unmistakable at a glance - this theme's equivalent of the
// ship's hull or Atlantis's temple.
void ff_draw_rainbow_backdrop(cairo_t *cr, double w, double h, double base_y) {
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.55);
    cairo_move_to(cr, 0, base_y);
    for (double x = 0; x <= w + 1; x += w / 12.0) {
        double bump = 20.0 + 16.0 * fabs(sin(x * 0.018 + 0.9));
        cairo_line_to(cr, x, base_y - bump);
    }
    cairo_line_to(cr, w, base_y);
    cairo_close_path(cr);
    cairo_fill(cr);

    // A handful of small drifting cloud puffs higher up, independent of the
    // bump silhouette below.
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.35);
    for (int i = 0; i < 5; i++) {
        double px = w * (0.08 + 0.20 * i);
        double py = h * (0.10 + 0.10 * ((i * 37) % 4) / 4.0);
        double pr = h * 0.035;
        cairo_arc(cr, px, py, pr, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, px + pr * 1.2, py + pr * 0.2, pr * 0.75, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, px - pr * 1.1, py + pr * 0.3, pr * 0.65, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // The big rainbow arc, centered low so only its top half shows above
    // the horizon, same silhouette trick as a real rainbow.
    double arc_cx = w * 0.5, arc_cy = base_y + h * 0.32;
    double outer_r = h * 0.62;
    double band_w = outer_r * 0.045;
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    for (int i = 0; i < FF_RAINBOW_BAND_COUNT; i++) {
        double r = outer_r - i * band_w;
        const FFRainbowBand *band = &FF_RAINBOW_BANDS[i];
        cairo_set_source_rgba(cr, band->r, band->g, band->b, 0.50);
        cairo_set_line_width(cr, band_w * 1.05);
        cairo_arc(cr, arc_cx, arc_cy, r, M_PI, 2 * M_PI);
        cairo_stroke(cr);
    }
}

// Static: the pale cloud-bank fill plus its scalloped, fluffy top edge - a
// row of overlapping puff caps in place of the other themes' flat or
// jagged floor line. No bubble_phase dependency, so this is the cacheable
// part.
void ff_draw_rainbow_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.90, 0.93, 0.99);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);

    double puff_r = floor_h * 0.30;
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    for (double x = -puff_r; x < w + puff_r; x += puff_r * 1.3) {
        cairo_arc(cr, x, h - floor_h, puff_r, M_PI, 2 * M_PI);
        cairo_fill(cr);
    }

    // A soft shaded band partway down the bank, so it doesn't read as flat.
    cairo_set_source_rgba(cr, 0.72, 0.78, 0.92, 0.45);
    cairo_rectangle(cr, 0, h - floor_h * 0.60, w, floor_h * 0.16);
    cairo_fill(cr);
}

// Scroll: soft puff-shadows drifting with bubble_phase, plus a scatter of
// tiny rainbow-colored sparkles - this theme's equivalent of the ship's
// coins or the Atlantis floor's gems.
void ff_draw_rainbow_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    double step = floor_h * 1.6;
    double scroll = fmod(bubble_phase * (h * 0.05), step);
    cairo_set_source_rgba(cr, 0.80, 0.85, 0.97, 0.40);
    for (double x = -step - scroll; x < w + step; x += step) {
        cairo_arc(cr, x, h - floor_h * 0.35, floor_h * 0.20, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    for (int i = 0; i < 9; i++) {
        double gx = fmod(i * 191.0 + 50.0, w);
        double gy = h - floor_h * (0.25 + 0.4 * ((i * 47) % 5) / 5.0);
        const FFRainbowBand *band = &FF_RAINBOW_BANDS[i % FF_RAINBOW_BAND_COUNT];
        cairo_set_source_rgba(cr, band->r, band->g, band->b, 0.85);
        double s = floor_h * 0.045;
        cairo_move_to(cr, gx, gy - s);
        cairo_line_to(cr, gx + s, gy);
        cairo_line_to(cr, gx, gy + s);
        cairo_line_to(cr, gx - s, gy);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

// Cotton-candy tuft standing in for kelp/seaweed: the same three-strand
// sway as every other theme's decoration slot, but each stalk is topped
// with a small pastel cloud puff instead of a leafy tip.
void ff_draw_rainbow_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    double r = 0.85, g = 0.80, b = 0.95, a = 0.55, sway_mult = 0.9;
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

        const FFRainbowBand *band = &FF_RAINBOW_BANDS[(i * 2) % FF_RAINBOW_BAND_COUNT];
        cairo_set_source_rgba(cr, band->r, band->g, band->b, 0.6 * alpha_mult);
        cairo_arc(cr, tip_x, tip_y, sh * 0.11, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.7 * alpha_mult);
        cairo_arc(cr, tip_x - sh * 0.05, tip_y - sh * 0.03, sh * 0.05, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// A small cluster of overlapping white circles reading as one fluffy cloud
// puff - shared by the pillar's cap below.
static void ff_draw_rainbow_cloud_puff(cairo_t *cr, double cx, double cy, double w, double h, double seed) {
    int n = 5;
    for (int i = 0; i < n; i++) {
        double t = (double)i / (n - 1);
        double px = cx + (t - 0.5) * w * 0.85;
        double py = cy + (0.5 - fabs(t - 0.5) * 2.0) * -h * 0.25 + h * 0.10;
        double pr = h * 0.34 * (0.75 + 0.4 * ff_hash(seed * 7.3 + i * 2.1));
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
        cairo_arc(cr, px, py, pr, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_set_source_rgba(cr, 0, 0, 0, 0.10);
    for (int i = 0; i < n; i++) {
        double t = (double)i / (n - 1);
        double px = cx + (t - 0.5) * w * 0.85;
        double py = cy + (0.5 - fabs(t - 0.5) * 2.0) * -h * 0.25 + h * 0.10;
        double pr = h * 0.34 * (0.75 + 0.4 * ff_hash(seed * 7.3 + i * 2.1));
        cairo_arc(cr, px, py, pr, 0, 2 * M_PI);
        cairo_stroke(cr);
    }
}

// A small pulsing sparkle star, right at the very tip of the pillar - this
// theme's equivalent of Atlantis's glowing rune orb or the ship's flag.
static void ff_draw_rainbow_star(cairo_t *cr, double cx, double cy, double r, double seed) {
    double pulse = 0.6 + 0.4 * sin(seed * 40.0);
    cairo_pattern_t *glow = cairo_pattern_create_radial(cx, cy, 0, cx, cy, r * 2.6);
    cairo_pattern_add_color_stop_rgba(glow, 0.0, 1.0, 0.95, 0.55, 0.5 * pulse);
    cairo_pattern_add_color_stop_rgba(glow, 1.0, 1.0, 0.95, 0.55, 0.0);
    cairo_set_source(cr, glow);
    cairo_arc(cr, cx, cy, r * 2.6, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(glow);

    cairo_set_source_rgba(cr, 1.0, 0.95, 0.55, 0.95);
    int points = 5;
    cairo_new_path(cr);
    for (int i = 0; i < points * 2; i++) {
        double ang = M_PI * 0.5 + i * M_PI / points;
        double rad = (i % 2 == 0) ? r : r * 0.42;
        double px = cx + cos(ang) * rad, py = cy - sin(ang) * rad;
        if (i == 0) cairo_move_to(cr, px, py); else cairo_line_to(cr, px, py);
    }
    cairo_close_path(cr);
    cairo_fill(cr);
}

// Draws one obstacle column as a candy-striped rainbow pillar (stacked
// ROYGBIV bands, each with a thin glossy highlight down the middle) capped
// at the gap-facing tip by a fluffy cloud puff and a sparkling star. Purely
// rectilinear like the ship/Atlantis columns, so it needs no safety
// scale-down to stay inside the collision box.
void ff_draw_rainbow_column(cairo_t *cr, double x, double y0, double y1,
                             double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double band_h = width * 0.30;
    int bands = (int)fmax(3.0, round(total_h / band_h));
    band_h = total_h / bands;
    int start_band = (int)(ff_hash(seed * 31.0) * 7919.0) % FF_RAINBOW_BAND_COUNT;

    for (int i = 0; i < bands; i++) {
        double by = y0 + i * band_h;
        const FFRainbowBand *band = &FF_RAINBOW_BANDS[(start_band + i) % FF_RAINBOW_BAND_COUNT];
        cairo_set_source_rgb(cr, band->r, band->g, band->b);
        // The +0.75 overlap keeps antialiasing from leaving hairline gaps
        // between adjacent bands.
        cairo_rectangle(cr, cx - width * 0.44, by - 0.5, width * 0.88, band_h + 0.75);
        cairo_fill(cr);

        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.18);
        cairo_rectangle(cr, cx - width * 0.08, by - 0.5, width * 0.10, band_h + 0.75);
        cairo_fill(cr);
    }

    cairo_set_source_rgba(cr, 0, 0, 0, 0.15);
    cairo_set_line_width(cr, fmax(1.0, width * 0.02));
    cairo_rectangle(cr, cx - width * 0.44, y0, width * 0.88, total_h);
    cairo_stroke(cr);

    // Fluffy cloud cap plus sparkling star, right at the tip that faces
    // the gap.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double cap_h = fmin(width * 0.5, total_h * 0.4);
    double cap_cy = tip_y + inward * cap_h * 0.35;
    ff_draw_rainbow_cloud_puff(cr, cx, cap_cy, width * 1.15, cap_h, seed);

    double star_y = tip_y + inward * cap_h * 0.1;
    ff_draw_rainbow_star(cr, cx, star_y, width * 0.16, seed);
}
