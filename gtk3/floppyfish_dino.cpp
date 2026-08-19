#include "floppyfish_common.h"

// --- Prehistoric bone-yard theme -----------------------------------------
// Murky, silt-clouded water over a seabed of fossilized bone - the sixth
// theme zone alongside reef/ship/cave/atlantis/rainbow. Obstacles are
// stacked bone fragments capped with a toothy skull, the floor is packed
// silt scattered with buried bone shards, and the seaweed slot becomes a
// murky strand of algae fouling an old rib.

void ff_dino_sky_colors(double *top_r, double *top_g, double *top_b,
                         double *bot_r, double *bot_g, double *bot_b) {
    *top_r = 0.10; *top_g = 0.16; *top_b = 0.12;
    *bot_r = 0.20; *bot_g = 0.28; *bot_b = 0.20;
}

void ff_dino_particle_color(double *r, double *g, double *b, double *a) {
    // Drifting silt/muck motes, murkier and dimmer than any other theme's
    // ambient particles.
    *r = 0.55; *g = 0.58; *b = 0.42; *a = 0.20;
}

// Bone tones per obstacle, plus the algae-stain tint scattered across it -
// shared by the drum segments and the skull cap so one pillar reads as a
// single coherent fossil.
typedef struct {
    double bone_r, bone_g, bone_b;
    double bone_dark_r, bone_dark_g, bone_dark_b;
    double moss_r, moss_g, moss_b;
} FFDinoPalette;

static const FFDinoPalette FF_DINO_PALETTES[] = {
    {0.80, 0.74, 0.56,  0.42, 0.36, 0.24,  0.30, 0.42, 0.26}, // tan bone / olive algae
    {0.72, 0.70, 0.66,  0.36, 0.34, 0.32,  0.24, 0.38, 0.30}, // grey bone / dark green algae
    {0.75, 0.62, 0.44,  0.40, 0.30, 0.18,  0.34, 0.40, 0.20}, // ochre bone / mustard algae
    {0.68, 0.68, 0.60,  0.32, 0.32, 0.26,  0.22, 0.34, 0.24}, // weathered white bone / moss
};
#define FF_DINO_PALETTE_COUNT (int)(sizeof(FF_DINO_PALETTES) / sizeof(FF_DINO_PALETTES[0]))

// Distant silhouette on the murky seabed: a half-silted rib cage arch with
// a big skull slumped beside it - the detail that most says "bone yard"
// rather than just "dark water", echoing the ship theme's hull but built
// from bone instead of timber.
void ff_draw_dino_backdrop(cairo_t *cr, double w, double h, double base_y) {
    double cage_w = w * 0.46, cage_x = w * 0.10;
    double cage_h = h * 0.20;
    double cage_top = base_y - cage_h;

    cairo_set_source_rgba(cr, 0.30, 0.30, 0.24, 0.45);
    int n_ribs = 6;
    for (int i = 0; i < n_ribs; i++) {
        double t = (double)i / (n_ribs - 1);
        double rx = cage_x + cage_w * t;
        double rise = cage_h * (0.35 + 0.65 * sin(t * M_PI));
        cairo_set_line_width(cr, fmax(2.0, cage_h * 0.045));
        cairo_move_to(cr, rx, base_y);
        cairo_curve_to(cr, rx - cage_w * 0.04, base_y - rise * 0.6,
                            rx + cage_w * 0.02, base_y - rise * 0.9,
                            rx, base_y - rise);
        cairo_stroke(cr);
    }
    // Spine along the top of the ribs.
    cairo_move_to(cr, cage_x, base_y - cage_h * 0.35);
    cairo_curve_to(cr, cage_x + cage_w * 0.3, cage_top,
                        cage_x + cage_w * 0.7, cage_top,
                        cage_x + cage_w, base_y - cage_h * 0.35);
    cairo_set_line_width(cr, fmax(2.0, cage_h * 0.06));
    cairo_stroke(cr);

    // A big half-buried skull slumped beside the rib cage.
    double sk_x = w * 0.66, sk_w = w * 0.30, sk_h = h * 0.16;
    double sk_y = base_y - sk_h * 0.55;
    cairo_set_source_rgba(cr, 0.34, 0.32, 0.24, 0.55);
    cairo_move_to(cr, sk_x, base_y);
    cairo_curve_to(cr, sk_x - sk_w * 0.05, sk_y - sk_h * 0.5,
                        sk_x + sk_w * 0.25, sk_y - sk_h,
                        sk_x + sk_w * 0.55, sk_y - sk_h * 0.85);
    cairo_curve_to(cr, sk_x + sk_w * 0.85, sk_y - sk_h * 0.7,
                        sk_x + sk_w, sk_y - sk_h * 0.2,
                        sk_x + sk_w * 0.95, base_y);
    cairo_close_path(cr);
    cairo_fill(cr);
    // Eye socket.
    cairo_set_source_rgba(cr, 0.06, 0.07, 0.05, 0.6);
    cairo_arc(cr, sk_x + sk_w * 0.55, sk_y - sk_h * 0.45, sk_h * 0.14, 0, 2 * M_PI);
    cairo_fill(cr);

    // Faint murky light shafts, dimmer and greener than the other themes'.
    cairo_set_source_rgba(cr, 0.55, 0.65, 0.45, 0.045);
    for (int i = 0; i < 3; i++) {
        double bx = w * (0.20 + 0.30 * i);
        cairo_move_to(cr, bx, 0);
        cairo_line_to(cr, bx + w * 0.08, 0);
        cairo_line_to(cr, bx - 0.04 * w, base_y);
        cairo_line_to(cr, bx - 0.12 * w, base_y);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

// Static: the packed-silt base fill. No bubble_phase dependency, so this
// is the cacheable part.
void ff_draw_dino_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.22, 0.20, 0.15);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
}

// Scroll: drifting silt streaks, plus a scatter of small bone-shard
// fragments half-buried in the muck - this theme's equivalent of the
// ship's coins or Atlantis's gems.
void ff_draw_dino_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    double step = 40.0;
    double scroll = fmod(bubble_phase * (h * 0.03), step);
    cairo_set_source_rgba(cr, 0.30, 0.28, 0.20, 0.5);
    for (double x = -step - scroll; x < w + step; x += step) {
        cairo_move_to(cr, x, h - floor_h * 0.7);
        cairo_line_to(cr, x + step * 0.5, h - floor_h * 0.85);
        cairo_set_line_width(cr, 3.0);
        cairo_stroke(cr);
    }

    for (int i = 0; i < 10; i++) {
        double bx = fmod(i * 163.0 + 45.0, w);
        double by = h - floor_h * (0.20 + 0.4 * ((i * 41) % 5) / 5.0);
        double s = floor_h * 0.05;
        double jag = (ff_hash(i * 3.3) - 0.5) * s * 0.6;
        cairo_set_source_rgba(cr, 0.78, 0.74, 0.60, 0.75);
        cairo_move_to(cr, bx - s, by + jag);
        cairo_line_to(cr, bx - s * 0.2, by - s * 0.7);
        cairo_line_to(cr, bx + s * 0.9, by - s * 0.1);
        cairo_line_to(cr, bx + s * 0.3, by + s * 0.6);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

// A murky strand of algae fouling an old bone splinter, standing in for
// kelp/seaweed - the same three-strand sway as every other theme's
// decoration slot, but duller and slower, tipped with a small pale bone
// chip instead of a leafy tip.
void ff_draw_dino_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    double r = 0.22, g = 0.32, b = 0.18, a = 0.60, sway_mult = 0.55;
    int strands = 3;
    for (int i = 0; i < strands; i++) {
        double sx = x + (i - 1) * height * 0.14;
        double sh = height * (0.75 + 0.25 * (i % 2));
        double phase = t * 0.8 + i * 1.7;
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

        cairo_set_source_rgba(cr, 0.78, 0.74, 0.60, 0.7 * alpha_mult);
        cairo_arc(cr, tip_x, tip_y, sh * 0.06, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// One "dog bone" fragment: two rounded knobs joined by a shaft, the
// silhouette that reads as a bone at a glance rather than a plain
// rectangle - plus a couple of dark hairline cracks across it.
static void ff_draw_dino_bone(cairo_t *cr, double cx, double cy, double w, double h,
                               double seed, const FFDinoPalette *pal) {
    double knob_r = w * 0.46;
    double shaft_h = fmax(0.0, h - knob_r * 1.1);

    cairo_set_source_rgb(cr, pal->bone_r, pal->bone_g, pal->bone_b);
    cairo_rectangle(cr, cx - w * 0.20, cy - shaft_h * 0.5, w * 0.40, shaft_h);
    cairo_fill(cr);
    cairo_arc(cr, cx, cy - h * 0.5 + knob_r * 0.55, knob_r, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, cx, cy + h * 0.5 - knob_r * 0.55, knob_r, 0, 2 * M_PI);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.3);
    cairo_set_line_width(cr, fmax(1.0, w * 0.02));
    cairo_arc(cr, cx, cy - h * 0.5 + knob_r * 0.55, knob_r, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_arc(cr, cx, cy + h * 0.5 - knob_r * 0.55, knob_r, 0, 2 * M_PI);
    cairo_stroke(cr);

    // Hairline cracks.
    cairo_set_source_rgba(cr, pal->bone_dark_r, pal->bone_dark_g, pal->bone_dark_b, 0.6);
    cairo_set_line_width(cr, fmax(1.0, w * 0.02));
    for (int i = 0; i < 2; i++) {
        double cx0 = cx + (ff_hash(seed * 4.1 + i) - 0.5) * w * 0.2;
        double cy0 = cy - shaft_h * 0.3 + shaft_h * 0.6 * ff_hash(seed * 5.7 + i);
        double ang = ff_hash(seed * 6.3 + i) * 2 * M_PI;
        cairo_move_to(cr, cx0, cy0);
        cairo_line_to(cr, cx0 + cos(ang) * w * 0.18, cy0 + sin(ang) * w * 0.18);
        cairo_stroke(cr);
    }

    // A patch of algae staining, so the bone doesn't read as clean/new.
    if (ff_hash(seed * 9.1) > 0.5) {
        double mx = cx + (ff_hash(seed * 10.3) - 0.5) * w * 0.3;
        double my = cy + (ff_hash(seed * 11.7) - 0.5) * h * 0.3;
        cairo_set_source_rgba(cr, pal->moss_r, pal->moss_g, pal->moss_b, 0.35);
        cairo_arc(cr, mx, my, w * 0.16, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// Draws one obstacle column as a stack of dog-bone fragments capped, at
// the gap-facing tip, by a toothy skull with a dark eye socket and a
// crack running across the braincase - a fossil pillar rather than a
// straight pipe. Rectilinear/circle-based like the ship/Atlantis columns,
// so it needs no safety scale-down to stay inside the collision box.
void ff_draw_dino_column(cairo_t *cr, double x, double y0, double y1,
                          double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFDinoPalette *pal =
        &FF_DINO_PALETTES[(int)(ff_hash(seed * 47.0) * 97.0) % FF_DINO_PALETTE_COUNT];

    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double seg_h = width * 0.56;
    int segs = (int)fmax(2.0, round(total_h / seg_h));
    seg_h = total_h / segs;

    for (int i = 0; i < segs; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        double jitter = (ff_hash(seed * 4.4 + i * 1.3) - 0.5) * width * 0.04;
        double rot = (ff_hash(seed * 8.1 + i * 2.7) - 0.5) * 0.5;
        cairo_save(cr);
        cairo_translate(cr, cx + jitter, seg_y);
        cairo_rotate(cr, rot);
        ff_draw_dino_bone(cr, 0, 0, width * 0.62, seg_h * 0.90, seed + i * 5.7, pal);
        cairo_restore(cr);
    }

    // Skull at the gap-facing tip: a rounded braincase, an elongated snout
    // with a row of triangular teeth, and a dark eye socket - this theme's
    // equivalent of the coral head / ship's flag / Atlantis capital.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double skull_h = fmin(width * 0.85, total_h * 0.55);
    double skull_cy = tip_y + inward * skull_h * 0.5;
    double skull_w = width * 0.98;

    cairo_set_source_rgb(cr, pal->bone_r, pal->bone_g, pal->bone_b);
    cairo_move_to(cr, cx - skull_w * 0.42, skull_cy - inward * skull_h * 0.15);
    cairo_curve_to(cr, cx - skull_w * 0.50, skull_cy - inward * skull_h * 0.55,
                        cx - skull_w * 0.20, skull_cy - inward * skull_h * 0.62,
                        cx, skull_cy - inward * skull_h * 0.58);
    cairo_curve_to(cr, cx + skull_w * 0.24, skull_cy - inward * skull_h * 0.60,
                        cx + skull_w * 0.50, skull_cy - inward * skull_h * 0.45,
                        cx + skull_w * 0.50, skull_cy - inward * skull_h * 0.05);
    cairo_curve_to(cr, cx + skull_w * 0.50, skull_cy + inward * skull_h * 0.30,
                        cx + skull_w * 0.20, skull_cy + inward * skull_h * 0.40,
                        cx, skull_cy + inward * skull_h * 0.36);
    cairo_curve_to(cr, cx - skull_w * 0.22, skull_cy + inward * skull_h * 0.40,
                        cx - skull_w * 0.42, skull_cy + inward * skull_h * 0.25,
                        cx - skull_w * 0.42, skull_cy - inward * skull_h * 0.15);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_set_line_width(cr, fmax(1.0, skull_h * 0.03));
    cairo_stroke(cr);

    // Eye socket.
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.04, 0.9);
    cairo_arc(cr, cx - skull_w * 0.10, skull_cy - inward * skull_h * 0.18, skull_h * 0.13, 0, 2 * M_PI);
    cairo_fill(cr);

    // A crack across the braincase.
    cairo_set_source_rgba(cr, pal->bone_dark_r, pal->bone_dark_g, pal->bone_dark_b, 0.7);
    cairo_set_line_width(cr, fmax(1.0, skull_h * 0.025));
    cairo_move_to(cr, cx - skull_w * 0.05, skull_cy - inward * skull_h * 0.55);
    cairo_line_to(cr, cx + skull_w * 0.08, skull_cy - inward * skull_h * 0.30);
    cairo_stroke(cr);

    // Snout with teeth, extending toward the gap.
    double snout_len = skull_w * 0.52;
    double snout_x1 = cx + skull_w * 0.50;
    double snout_x2 = snout_x1 + snout_len;
    double jaw_y = skull_cy + inward * skull_h * 0.08;
    cairo_set_source_rgb(cr, pal->bone_r, pal->bone_g, pal->bone_b);
    cairo_move_to(cr, snout_x1, jaw_y - inward * skull_h * 0.20);
    cairo_line_to(cr, snout_x2, jaw_y - inward * skull_h * 0.06);
    cairo_line_to(cr, snout_x2, jaw_y + inward * skull_h * 0.18);
    cairo_line_to(cr, snout_x1, jaw_y + inward * skull_h * 0.20);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_stroke(cr);

    int teeth = 4;
    cairo_set_source_rgba(cr, 0.95, 0.93, 0.85, 0.95);
    for (int k = 0; k < teeth; k++) {
        double tt = (double)k / teeth;
        double tx = snout_x1 + (snout_x2 - snout_x1) * tt;
        double tw = snout_len / teeth * 0.7;
        double base_yy = jaw_y + inward * skull_h * 0.19;
        double tip_yy = base_yy + inward * skull_h * 0.13;
        cairo_move_to(cr, tx, base_yy);
        cairo_line_to(cr, tx + tw * 0.5, base_yy);
        cairo_line_to(cr, tx + tw * 0.25, tip_yy);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}
