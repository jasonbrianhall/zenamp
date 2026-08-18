#include "floppyfish_common.h"

// --- Coral reef theme --------------------------------------------------

// The coral's lobes/jags are generated somewhat larger than the obstacle's
// actual pipe_width column (that's what gives the jags their reach), then
// drawn at this fraction of that size. Chosen so that even the analytical
// worst case (max radius, max jitter, a jag angled dead-on and at max
// length) still lands exactly at the pipe_width edge - so nothing needs to
// be clipped, and there's no flat pipe-like crop line, but a visible jag
// is still guaranteed to be within the collision box.
#define FF_CORAL_SCALE 0.55

void ff_reef_sky_colors(double *top_r, double *top_g, double *top_b,
                         double *bot_r, double *bot_g, double *bot_b) {
    *top_r = 0.35; *top_g = 0.72; *top_b = 0.85;
    *bot_r = 0.55; *bot_g = 0.85; *bot_b = 0.88;
}

void ff_reef_particle_color(double *r, double *g, double *b, double *a) {
    *r = 1.0; *g = 1.0; *b = 1.0; *a = 0.25;
}

// Distant coral-bump skyline silhouette.
void ff_draw_reef_backdrop(cairo_t *cr, double w, double h, double base_y) {
    cairo_set_source_rgba(cr, 0.25, 0.55, 0.5, 0.55);
    cairo_move_to(cr, 0, base_y);
    for (double x = 0; x <= w + 1; x += w / 10.0) {
        double bump = 18.0 + 14.0 * sin(x * 0.02 + 1.7);
        cairo_line_to(cr, x, base_y - bump);
    }
    cairo_line_to(cr, w, base_y);
    cairo_close_path(cr);
    cairo_fill(cr);
}

// Static: the sand fill itself. No bubble_phase dependency, so this is the
// cacheable part.
void ff_draw_reef_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.87, 0.78, 0.55);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
}

// Scroll: the diagonal sand-ripple ticks, which drift with bubble_phase and
// so need to be redrawn live every frame on top of the cached fill above.
void ff_draw_reef_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    cairo_set_source_rgba(cr, 0.75, 0.65, 0.35, 0.6);
    for (double x = -fmod(bubble_phase * (h * 0.34), 24.0); x < w; x += 24.0) {
        cairo_move_to(cr, x, h - floor_h);
        cairo_line_to(cr, x + 12, h);
        cairo_set_line_width(cr, 6.0);
        cairo_stroke(cr);
    }
}

void ff_draw_reef_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    double r = 0.16, g = 0.42, b = 0.28, a = 0.55, sway_mult = 1.0;
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

// Reef color families - warm coral tones. Picked per obstacle so each one
// reads as a single coherent coral head.
typedef struct {
    double base_r, base_g, base_b; // rooted/shaded end
    double tip_r, tip_g, tip_b;    // toward the gap opening, brighter
    double polyp_r, polyp_g, polyp_b;
    double groove_r, groove_g, groove_b;
} FFCoralPalette;

static const FFCoralPalette FF_CORAL_PALETTES[] = {
    {0.62, 0.14, 0.20,  1.00, 0.50, 0.46,  1.00, 0.90, 0.62,  0.40, 0.06, 0.12}, // salmon/red
    {0.66, 0.28, 0.06,  1.00, 0.62, 0.28,  1.00, 0.92, 0.50,  0.42, 0.16, 0.02}, // orange
    {0.40, 0.12, 0.46,  0.92, 0.50, 0.92,  1.00, 0.88, 0.98,  0.24, 0.06, 0.30}, // magenta/purple
    {0.14, 0.40, 0.36,  0.50, 0.90, 0.78,  0.95, 1.00, 0.72,  0.06, 0.24, 0.20}, // sea-green
    {0.64, 0.42, 0.04,  1.00, 0.80, 0.32,  1.00, 0.98, 0.75,  0.38, 0.24, 0.02}, // golden
    {0.55, 0.20, 0.42,  0.95, 0.62, 0.78,  1.00, 0.92, 0.90,  0.30, 0.08, 0.24}, // pink/brain-coral
};
#define FF_CORAL_PALETTE_COUNT (int)(sizeof(FF_CORAL_PALETTES) / sizeof(FF_CORAL_PALETTES[0]))

// Fills (and optionally strokes) a lumpy, non-circular blob by running a
// smooth closed curve through points scattered around a circle at jittered
// radii - this single trick is what keeps coral lobes from reading as plain
// bubbles/balloons the way perfect cairo_arc circles do.
static void ff_coral_blob_path(cairo_t *cr, double cx, double cy, double radius, double seed) {
    const int n = 8;
    double px[8], py[8];
    for (int i = 0; i < n; i++) {
        double ang = (2 * M_PI * i) / n;
        double rr = radius * (0.68 + 0.55 * ff_hash(seed * 9.1 + i * 3.3));
        px[i] = cx + cos(ang) * rr;
        py[i] = cy + sin(ang) * rr;
    }
    cairo_new_path(cr);
    cairo_move_to(cr, (px[0] + px[n - 1]) * 0.5, (py[0] + py[n - 1]) * 0.5);
    for (int i = 0; i < n; i++) {
        int ni = (i + 1) % n;
        double mx = (px[i] + px[ni]) * 0.5, my = (py[i] + py[ni]) * 0.5;
        cairo_curve_to(cr, px[i], py[i], px[i], py[i], mx, my);
    }
    cairo_close_path(cr);
}

// One knobby coral lobe: an irregular blob, a couple of squiggly darker
// grooves carved across it (the brain-coral texture that reads as coral
// rather than a smooth balloon), a few small polyp flecks, and - on some
// lobes - a slim tapered finger branch poking out to the side.
static void ff_draw_coral_lobe(cairo_t *cr, double cx, double cy, double radius,
                                double seed, const FFCoralPalette *pal, double tip_t) {
    double r = pal->base_r + (pal->tip_r - pal->base_r) * tip_t;
    double g = pal->base_g + (pal->tip_g - pal->base_g) * tip_t;
    double b = pal->base_b + (pal->tip_b - pal->base_b) * tip_t;

    ff_coral_blob_path(cr, cx, cy, radius, seed);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, r * 0.5, g * 0.5, b * 0.5, 0.8);
    cairo_set_line_width(cr, radius * 0.09);
    cairo_stroke(cr);

    // Squiggly grooves - short wavy strokes, clipped to the blob so they
    // never spill outside its silhouette.
    cairo_save(cr);
    ff_coral_blob_path(cr, cx, cy, radius, seed);
    cairo_clip(cr);
    cairo_set_source_rgba(cr, pal->groove_r, pal->groove_g, pal->groove_b, 0.55);
    cairo_set_line_width(cr, radius * 0.06);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    int grooves = 2 + (int)(ff_hash(seed * 4.4) * 2);
    for (int gi = 0; gi < grooves; gi++) {
        double gang = ff_hash(seed * 6.6 + gi * 2.7) * 2 * M_PI;
        double gx = cx + cos(gang) * radius * 0.5;
        double gy = cy + sin(gang) * radius * 0.5;
        double bend = (ff_hash(seed * 8.8 + gi * 1.9) - 0.5) * radius * 0.9;
        cairo_move_to(cr, gx - radius * 0.55, gy - bend * 0.3);
        cairo_curve_to(cr, gx - radius * 0.2, gy + bend,
                            gx + radius * 0.2, gy - bend,
                            gx + radius * 0.55, gy + bend * 0.3);
        cairo_stroke(cr);
    }
    cairo_restore(cr);

    // Polyp flecks scattered near the surface.
    int polyps = 3 + (int)(ff_hash(seed * 2.2) * 3);
    for (int p = 0; p < polyps; p++) {
        double pang = ff_hash(seed * 11.3 + p * 1.9) * 2 * M_PI;
        double prad = radius * (0.45 + 0.35 * ff_hash(seed * 13.7 + p * 2.4));
        double px = cx + cos(pang) * prad;
        double py = cy + sin(pang) * prad;
        cairo_set_source_rgba(cr, pal->polyp_r, pal->polyp_g, pal->polyp_b, 0.9);
        cairo_arc(cr, px, py, radius * 0.09, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // A slim tapered jag on roughly half the lobes, angled off to one side -
    // the spiky detail that most says "coral reef" at a glance. Kept short
    // and anchored close to the lobe so it lands inside the padded hazard
    // box rather than relying on the clip to hide an overshoot.
    if (ff_hash(seed * 15.5) > 0.45) {
        double bang = (ff_hash(seed * 19.3) - 0.5) * M_PI * 1.3 - M_PI * 0.5;
        double blen = radius * (0.35 + 0.35 * ff_hash(seed * 21.1));
        double bx0 = cx + cos(bang) * radius * 0.6;
        double by0 = cy + sin(bang) * radius * 0.6;
        double bx1 = cx + cos(bang) * (radius * 0.6 + blen);
        double by1 = cy + sin(bang) * (radius * 0.6 + blen);
        double perp = bang + M_PI * 0.5;
        double w0 = radius * 0.32, w1 = radius * 0.12;

        cairo_new_path(cr);
        cairo_move_to(cr, bx0 + cos(perp) * w0, by0 + sin(perp) * w0);
        cairo_curve_to(cr, bx0 + cos(perp) * w0 * 0.6 + cos(bang) * blen * 0.5,
                            by0 + sin(perp) * w0 * 0.6 + sin(bang) * blen * 0.5,
                            bx1 + cos(perp) * w1, by1 + sin(perp) * w1,
                            bx1, by1);
        cairo_curve_to(cr, bx1 - cos(perp) * w1, by1 - sin(perp) * w1,
                            bx0 - cos(perp) * w0 * 0.6 + cos(bang) * blen * 0.5,
                            by0 - sin(perp) * w0 * 0.6 + sin(bang) * blen * 0.5,
                            bx0 - cos(perp) * w0, by0 - sin(perp) * w0);
        cairo_close_path(cr);
        cairo_set_source_rgb(cr, r, g, b);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, r * 0.5, g * 0.5, b * 0.5, 0.8);
        cairo_set_line_width(cr, radius * 0.06);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, pal->tip_r, pal->tip_g, pal->tip_b, 0.9);
        cairo_arc(cr, bx1, by1, w1 * 0.9, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// Draws one obstacle column as a stack of lumpy, textured coral lobes with a
// fuller polyp-covered head at the end that faces the gap - a reef tower,
// not a straight-sided pipe with a flared cap. `seed` is fixed per pipe (its
// gap center), so this column's palette/bump layout hold steady while it
// scrolls (only `x` changes frame to frame), and the top/bottom column of
// the same pipe share one seed so they read as a matching pair.
void ff_draw_coral_column(cairo_t *cr, double x, double y0, double y1,
                           double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFCoralPalette *pal =
        &FF_CORAL_PALETTES[(int)(ff_hash(seed) * 97.0) % FF_CORAL_PALETTE_COUNT];

    double cx = x + width * 0.5;
    // Every lobe/jag size and offset below is derived from shape_w rather
    // than the full pipe_width - shrinking the whole coral so its natural
    // silhouette (jags included) tops out right at the pipe_width edge
    // instead of needing to be clipped there.
    double shape_w = width * FF_CORAL_SCALE;
    double total_h = y1 - y0;
    double lobe_h = shape_w * 0.56;
    int lobes = (int)fmax(2.0, round(total_h / lobe_h));
    lobe_h = total_h / lobes;

    for (int i = 0; i < lobes; i++) {
        double lobe_y = y0 + (i + 0.5) * lobe_h;
        double denom = lobes > 1 ? (double)(lobes - 1) : 1.0;
        // 0 at the rooted end (sand/screen edge), 1 at the gap-facing tip.
        double tip_t = tip_at_y1 ? i / denom : 1.0 - i / denom;

        double h1 = ff_hash(seed * 3.7 + i * 1.31);
        double h2 = ff_hash(seed * 5.9 + i * 2.03);
        double radius = shape_w * 0.5 * (0.78 + 0.32 * h1);
        double jitter_x = (h2 - 0.5) * shape_w * 0.20;

        ff_draw_coral_lobe(cr, cx + jitter_x, lobe_y, radius, seed * 1.3 + i * 7.1, pal, tip_t);
    }

    // A fuller three-lobe cluster right at the gap-facing tip, like a coral
    // head opening up, instead of the old flared pipe cap.
    double tip_y = tip_at_y1 ? y1 : y0;
    for (int k = -1; k <= 1; k++) {
        double h3 = ff_hash(seed * 17.0 + k * 2.2);
        double head_r = shape_w * 0.30 * (0.85 + 0.3 * h3);
        double head_x = cx + k * shape_w * 0.30;
        double head_y = tip_y + (tip_at_y1 ? -head_r * 0.4 : head_r * 0.4);
        ff_draw_coral_lobe(cr, head_x, head_y, head_r, seed * 23.0 + k * 5.5, pal, 1.0);
    }
}
