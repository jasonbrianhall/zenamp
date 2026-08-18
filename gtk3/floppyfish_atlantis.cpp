#include "floppyfish_common.h"

// --- Sunken Atlantis theme ---------------------------------------------
// Ruined marble colonnades instead of pipes, lit from within by faintly
// pulsing runes - the fourth theme zone, alongside reef/ship/cave.

void ff_atlantis_sky_colors(double *top_r, double *top_g, double *top_b,
                             double *bot_r, double *bot_g, double *bot_b) {
    *top_r = 0.05; *top_g = 0.16; *top_b = 0.30;
    *bot_r = 0.10; *bot_g = 0.28; *bot_b = 0.42;
}

void ff_atlantis_particle_color(double *r, double *g, double *b, double *a) {
    // Warm golden motes drifting up past the ruins, distinct from the cool
    // white/cyan bubbles of the other three themes.
    *r = 1.0; *g = 0.85; *b = 0.45; *a = 0.22;
}

// A soft radial glow behind a rune/orb - shared by the backdrop, the
// columns' rune medallions, and the floor gems, so the "glowing Atlantean
// light" look reads consistently everywhere it appears.
static void ff_draw_glow_dot(cairo_t *cr, double cx, double cy, double radius,
                              double r, double g, double b, double alpha) {
    cairo_pattern_t *glow = cairo_pattern_create_radial(cx, cy, 0, cx, cy, radius * 2.2);
    cairo_pattern_add_color_stop_rgba(glow, 0.0, r, g, b, alpha);
    cairo_pattern_add_color_stop_rgba(glow, 1.0, r, g, b, 0.0);
    cairo_set_source(cr, glow);
    cairo_arc(cr, cx, cy, radius * 2.2, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(glow);

    cairo_set_source_rgba(cr, r, g, b, fmin(1.0, alpha * 1.6));
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_fill(cr);
}

// Distant skyline: a ruined Atlantean temple - a row of fluted columns
// holding up a broken pediment, with one large glowing rune at its center
// and faint golden sunbeams, echoing the ship theme's hull backdrop but
// upright and ornamental rather than a fallen wreck.
void ff_draw_atlantis_backdrop(cairo_t *cr, double w, double h, double base_y) {
    double temple_w = w * 0.58;
    double temple_x = w * 0.21;
    double col_h = h * 0.24;
    int n_cols = 6;
    double col_w = temple_w / (n_cols * 1.8);

    cairo_set_source_rgba(cr, 0.10, 0.20, 0.28, 0.55);
    for (int i = 0; i < n_cols; i++) {
        double cx = temple_x + temple_w * (i + 0.5) / n_cols;
        cairo_rectangle(cr, cx - col_w * 0.5, base_y - col_h, col_w, col_h);
        cairo_fill(cr);
    }

    // Broken pediment roofline - not a clean unbroken triangle, so it still
    // reads as "ruin" rather than an intact building.
    cairo_move_to(cr, temple_x - temple_w * 0.03, base_y - col_h);
    cairo_line_to(cr, temple_x + temple_w * 0.46, base_y - col_h - h * 0.09);
    cairo_line_to(cr, temple_x + temple_w * 0.63, base_y - col_h - h * 0.045);
    cairo_line_to(cr, temple_x + temple_w * 1.03, base_y - col_h);
    cairo_close_path(cr);
    cairo_fill(cr);

    // The one big glowing rune at the heart of the pediment - the detail
    // that most says "Atlantis" at a glance, same as the Jolly Roger does
    // for the ship theme.
    ff_draw_glow_dot(cr, temple_x + temple_w * 0.5, base_y - col_h - h * 0.05,
                      h * 0.028, 0.45, 0.85, 1.0, 0.5);

    // Faint golden sunbeam shafts, echoing the ship wreck's lighting but
    // warmer.
    cairo_set_source_rgba(cr, 1.0, 0.9, 0.6, 0.05);
    for (int i = 0; i < 3; i++) {
        double bx = w * (0.18 + 0.30 * i);
        cairo_move_to(cr, bx, 0);
        cairo_line_to(cr, bx + w * 0.09, 0);
        cairo_line_to(cr, bx - w * 0.04, base_y);
        cairo_line_to(cr, bx - w * 0.13, base_y);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

// Mosaic tile floor - alternating teal/gold tiles with glowing seams,
// scattered with a few loose gems instead of the ship theme's coins.
// Static: just the base fill - the one part big enough (a full-width
// rectangle) to be worth caching. Everything else below has to stay in the
// live pass so it keeps drawing on top of it in the original order.
void ff_draw_atlantis_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.14, 0.22, 0.28);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
}

// Scroll (really "everything drawn live on top of the cached fill"): the
// mosaic tiles that actually scroll with bubble_phase, plus the baseline
// seam and gem scatter - kept here rather than in the static cache so they
// still land on top of the tiles each frame, same as the original
// single-function draw order. All of this is cheap, so redrawing it live
// costs nothing worth caching.
void ff_draw_atlantis_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    double tile = floor_h * 0.6;
    double scroll = fmod(bubble_phase * (h * 0.04), tile * 2.0);
    for (double x = -tile * 2.0 - scroll; x < w + tile; x += tile) {
        bool alt = ((int)round(x / tile)) % 2 == 0;
        if (alt) cairo_set_source_rgba(cr, 0.20, 0.32, 0.36, 0.9);
        else cairo_set_source_rgba(cr, 0.62, 0.50, 0.22, 0.55);
        cairo_rectangle(cr, x, h - floor_h, tile, floor_h);
        cairo_fill(cr);
    }

    cairo_set_source_rgba(cr, 0.45, 0.85, 1.0, 0.35);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, 0, h - floor_h * 0.5);
    cairo_line_to(cr, w, h - floor_h * 0.5);
    cairo_stroke(cr);

    // A scatter of small glowing gems half-sunk in the mosaic.
    for (int i = 0; i < 8; i++) {
        double gx = fmod(i * 211.0 + 60.0, w);
        double gy = h - floor_h * (0.3 + 0.35 * ((i * 53) % 5) / 5.0);
        double r = floor_h * 0.05;
        double gr = 0.4 + 0.5 * ff_hash(i * 3.7);
        double gg = 0.7 + 0.3 * ff_hash(i * 5.1);
        double gb = 1.0;
        ff_draw_glow_dot(cr, gx, gy, r, gr, gg, gb, 0.55);
    }
}

// Glowing kelp - teal fronds tipped with a faint pulsing rune-mote, in
// place of the reef's plain seaweed or the cave's crystal shards.
void ff_draw_atlantis_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    double r = 0.20, g = 0.55, b = 0.60, a = 0.60, sway_mult = 0.75;
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

        double pulse = 0.5 + 0.5 * sin(phase * 1.6);
        ff_draw_glow_dot(cr, tip_x, tip_y, sh * 0.05, 0.5, 0.9, 1.0, 0.35 * pulse * alpha_mult);
    }
}

// Marble tones per ruined column, plus the color its runes glow in.
typedef struct {
    double marble_r, marble_g, marble_b;
    double marble_dark_r, marble_dark_g, marble_dark_b;
    double rune_r, rune_g, rune_b;
    double gold_r, gold_g, gold_b;
} FFAtlantisPalette;

static const FFAtlantisPalette FF_ATLANTIS_PALETTES[] = {
    {0.80, 0.83, 0.90,  0.52, 0.56, 0.66,  0.35, 0.85, 1.00,  0.85, 0.68, 0.25}, // white-blue marble / cyan glow
    {0.58, 0.72, 0.66,  0.32, 0.46, 0.42,  1.00, 0.85, 0.35,  0.80, 0.65, 0.30}, // sea-green marble / gold glow
    {0.72, 0.55, 0.60,  0.44, 0.30, 0.36,  0.75, 0.45, 0.95,  0.85, 0.68, 0.25}, // rose marble / violet glow
    {0.38, 0.55, 0.60,  0.20, 0.32, 0.36,  1.00, 0.95, 0.75,  0.85, 0.68, 0.25}, // deep teal marble / warm glow
};
#define FF_ATLANTIS_PALETTE_COUNT (int)(sizeof(FF_ATLANTIS_PALETTES) / sizeof(FF_ATLANTIS_PALETTES[0]))

// One fluted marble drum segment: a block with alternating light/dark
// vertical flutes (the classic Greek-column texture) and a dark joint seam
// top and bottom. Occasionally carries a glowing rune medallion.
static void ff_draw_atlantis_drum(cairo_t *cr, double cx, double cy, double w, double h,
                                   double seed, const FFAtlantisPalette *pal) {
    double x0 = cx - w * 0.5, y0 = cy - h * 0.5;
    cairo_set_source_rgb(cr, pal->marble_r, pal->marble_g, pal->marble_b);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_fill(cr);

    int flutes = 6;
    for (int i = 0; i < flutes; i++) {
        double fx = x0 + w * (i + 0.5) / flutes;
        bool light = (i % 2) == 0;
        if (light) cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.18);
        else cairo_set_source_rgba(cr, pal->marble_dark_r, pal->marble_dark_g, pal->marble_dark_b, 0.55);
        cairo_set_line_width(cr, fmax(1.0, w / flutes * 0.5));
        cairo_move_to(cr, fx, y0 + h * 0.05);
        cairo_line_to(cr, fx, y0 + h * 0.95);
        cairo_stroke(cr);
    }

    // Dark joint seams top/bottom, like stacked column drums.
    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    cairo_set_line_width(cr, fmax(1.0, h * 0.06));
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x0 + w, y0);
    cairo_stroke(cr);
    cairo_move_to(cr, x0, y0 + h);
    cairo_line_to(cr, x0 + w, y0 + h);
    cairo_stroke(cr);

    // A faintly pulsing rune medallion on roughly a third of the drums.
    if (ff_hash(seed * 6.6) > 0.66) {
        double pulse = 0.6 + 0.4 * sin(seed * 50.0);
        ff_draw_glow_dot(cr, cx, cy, h * 0.16, pal->rune_r, pal->rune_g, pal->rune_b, 0.5 * pulse);
    }
}

// Draws one obstacle column as a stack of fluted marble drums capped, at
// the gap-facing tip, by an ornate capital with volute scrolls and a bright
// glowing orb - a broken temple pillar rather than a straight pipe.
// Rectilinear like the ship column, so it needs no safety scale-down to
// stay inside the collision box.
void ff_draw_atlantis_column(cairo_t *cr, double x, double y0, double y1,
                              double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFAtlantisPalette *pal =
        &FF_ATLANTIS_PALETTES[(int)(ff_hash(seed * 61.0) * 97.0) % FF_ATLANTIS_PALETTE_COUNT];

    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double seg_h = width * 0.58;
    int segs = (int)fmax(2.0, round(total_h / seg_h));
    seg_h = total_h / segs;

    for (int i = 0; i < segs; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        double jitter = (ff_hash(seed * 4.4 + i * 1.3) - 0.5) * width * 0.03;
        ff_draw_atlantis_drum(cr, cx + jitter, seg_y, width * 0.90, seg_h * 0.94,
                               seed + i * 5.7, pal);
    }

    // Ornate capital at the gap-facing tip: a wider abacus block, two
    // volute scrolls curling off its corners, and a bright glowing orb -
    // this theme's equivalent of the coral head / ship's flag.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double cap_h = fmin(width * 0.34, total_h * 0.5);
    double cap_cy = tip_y + inward * cap_h * 0.5;
    double cap_w = width * 1.06;

    cairo_set_source_rgb(cr, pal->gold_r, pal->gold_g, pal->gold_b);
    cairo_rectangle(cr, cx - cap_w * 0.5, cap_cy - cap_h * 0.5, cap_w, cap_h);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_set_line_width(cr, fmax(1.0, cap_h * 0.08));
    cairo_rectangle(cr, cx - cap_w * 0.5, cap_cy - cap_h * 0.5, cap_w, cap_h);
    cairo_stroke(cr);

    // Volute scrolls: a small spiral circle at each top corner of the
    // capital, hand-clamped inside cap_w so they never poke past the pipe
    // edge.
    double volute_r = cap_h * 0.32;
    double volute_y = cap_cy - inward * cap_h * 0.5;
    for (int side = -1; side <= 1; side += 2) {
        double vx = cx + side * (cap_w * 0.5 - volute_r * 1.05);
        cairo_set_source_rgb(cr, pal->gold_r * 0.85, pal->gold_g * 0.85, pal->gold_b * 0.85);
        cairo_arc(cr, vx, volute_y, volute_r, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
        cairo_set_line_width(cr, fmax(1.0, volute_r * 0.18));
        cairo_arc(cr, vx, volute_y, volute_r * 0.55, 0.3, 5.5);
        cairo_stroke(cr);
    }

    // Bright glowing orb right at the tip, floating just off the capital
    // face and into the gap - the "eye of Atlantis".
    double orb_y = tip_y + inward * cap_h * 0.15;
    double pulse = 0.65 + 0.35 * sin(seed * 33.0);
    ff_draw_glow_dot(cr, cx, orb_y, cap_h * 0.20, pal->rune_r, pal->rune_g, pal->rune_b, 0.75 * pulse);
}
