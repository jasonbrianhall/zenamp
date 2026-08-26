#include "floppyfish_common.h"

// --- Mystical Swamp theme --------------------------------------------------
// A fog-hung mangrove swamp at dusk - the tenth theme zone alongside
// reef/ship/cave/atlantis/rainbow/dino/antarctic/aquarium/galaxy. Obstacles
// are knotted mangrove-root pillars wrapped in hanging moss and capped with
// a floating will-o'-wisp lantern light, the floor is dark peaty mud dotted
// with lily pads and sunken logs, and the backdrop carries a row of twisted
// mangrove silhouettes, trailing curtains of Spanish moss, and a few more
// wisps drifting low over the water - alligators are the theme's real
// signature guest, though (see ff_draw_alligator_group in floppyfish.cpp).

void ff_swamp_sky_colors(double *top_r, double *top_g, double *top_b,
                          double *bot_r, double *bot_g, double *bot_b) {
    // Deep indigo-green dusk overhead, fading down into murky, fog-lit teal
    // near the waterline.
    *top_r = 0.07; *top_g = 0.10; *top_b = 0.11;
    *bot_r = 0.16; *bot_g = 0.30; *bot_b = 0.26;
}

void ff_swamp_particle_color(double *r, double *g, double *b, double *a) {
    // Drifting will-o'-wisp motes - pale sickly green-yellow, the theme's
    // ambient "magic in the air" glow.
    *r = 0.78; *g = 0.95; *b = 0.55; *a = 0.35;
}

// Root/moss tones per obstacle/backdrop element, plus a soft wisp-glow
// shade - shared by the root pillars and the distant mangrove row so the
// palette reads consistently everywhere it shows up.
typedef struct {
    double root_r, root_g, root_b;
    double moss_r, moss_g, moss_b;
    double glow_r, glow_g, glow_b;
} FFRootPalette;

static const FFRootPalette FF_ROOT_PALETTES[] = {
    {0.32, 0.22, 0.15,  0.25, 0.42, 0.22,  0.75, 0.95, 0.55}, // warm brown root / mossy green / pale wisp
    {0.28, 0.20, 0.16,  0.30, 0.48, 0.28,  0.60, 0.90, 0.65}, // darker root / brighter moss / cool wisp
    {0.36, 0.26, 0.18,  0.20, 0.35, 0.20,  0.85, 0.92, 0.45}, // pale root / olive moss / amber-green wisp
    {0.24, 0.17, 0.13,  0.28, 0.45, 0.26,  0.65, 0.98, 0.70}, // near-black root / deep moss / bright wisp
};
#define FF_ROOT_PALETTE_COUNT (int)(sizeof(FF_ROOT_PALETTES) / sizeof(FF_ROOT_PALETTES[0]))

// A small floating will-o'-wisp: a soft glowing core with a couple of
// fading halo rings - this theme's real signature detail, so it appears
// atop every root pillar the same way the penguin does for Antarctic.
static void ff_draw_swamp_wisp(cairo_t *cr, double cx, double cy, double s, double t,
                                const FFRootPalette *pal) {
    double bob = sin(t * 1.6) * s * 0.35;
    double flicker = 0.75 + 0.25 * sin(t * 9.0);
    cairo_save(cr);
    cairo_translate(cr, cx, cy + bob);

    for (int ring = 2; ring >= 0; ring--) {
        double rr = s * (0.55 + ring * 0.55);
        double ra = (0.30 - ring * 0.09) * flicker;
        cairo_set_source_rgba(cr, pal->glow_r, pal->glow_g, pal->glow_b, fmax(0.0, ra));
        cairo_arc(cr, 0, 0, rr, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_set_source_rgba(cr, 0.98, 1.0, 0.92, 0.9 * flicker);
    cairo_arc(cr, 0, 0, s * 0.32, 0, 2 * M_PI);
    cairo_fill(cr);

    cairo_restore(cr);
}

// A curtain of Spanish moss, trailing down in a few uneven strands - drawn
// once into the backdrop, and also used at the base of every root pillar.
static void ff_draw_swamp_moss_curtain(cairo_t *cr, double x, double top_y, double length,
                                        double width, const FFRootPalette *pal, double alpha) {
    int strands = 4;
    for (int i = 0; i < strands; i++) {
        double sx = x + (i - (strands - 1) * 0.5) * (width / strands);
        double sl = length * (0.6 + 0.4 * ff_hash(sx * 0.7 + i));
        cairo_set_source_rgba(cr, pal->moss_r, pal->moss_g, pal->moss_b, alpha);
        cairo_set_line_width(cr, fmax(1.0, width * 0.06));
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, sx, top_y);
        cairo_curve_to(cr, sx - width * 0.06, top_y + sl * 0.4,
                            sx + width * 0.06, top_y + sl * 0.7,
                            sx, top_y + sl);
        cairo_stroke(cr);
    }
}

// A pair of glowing eyes just breaking the surface, far back in the fog -
// a hint of something lurking without competing with the real alligator
// guest that occasionally swims across (see ff_draw_alligator_group in
// floppyfish.cpp).
static void ff_draw_swamp_distant_eyes(cairo_t *cr, double x, double y, double s, double t) {
    double blink = (fmod(t * 0.13 + x * 0.001, 6.0) < 0.15) ? 0.15 : 1.0;
    cairo_set_source_rgba(cr, 0.85, 0.92, 0.25, 0.55 * blink);
    cairo_arc(cr, x - s, y, s * 0.35, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, x + s, y, s * 0.35, 0, 2 * M_PI);
    cairo_fill(cr);
}

// Distant twisted mangrove trunks rising out of the fog, trailing moss,
// with a couple of low wisps and a pair of half-submerged eyes lurking
// near the waterline - this theme's equivalent of the ship's hull or
// Atlantis's temple: the one backdrop that most says "mystical swamp" at
// a glance.
void ff_draw_swamp_backdrop(cairo_t *cr, double w, double h, double base_y) {
    // Low fog bank, flat and pale, sitting right at the waterline.
    cairo_set_source_rgba(cr, 0.55, 0.65, 0.58, 0.16);
    cairo_rectangle(cr, 0, base_y - h * 0.10, w, h * 0.10);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.10, 0.14, 0.11, 0.6);
    for (int i = 0; i < 6; i++) {
        double tx = w * (0.05 + 0.16 * i) + 20.0 * sin(i * 3.1);
        double th = h * (0.30 + 0.18 * fabs(sin(i * 1.7)));
        double tw = h * 0.05;
        const FFRootPalette *pal = &FF_ROOT_PALETTES[i % FF_ROOT_PALETTE_COUNT];

        // Gnarled trunk, tapering and kinked rather than a straight pole.
        cairo_move_to(cr, tx - tw * 0.5, base_y);
        cairo_curve_to(cr, tx - tw * 0.7, base_y - th * 0.35,
                            tx + tw * 0.4, base_y - th * 0.55,
                            tx - tw * 0.15, base_y - th * 0.78);
        cairo_curve_to(cr, tx - tw * 0.4, base_y - th * 0.9,
                            tx + tw * 0.1, base_y - th * 0.97,
                            tx, base_y - th);
        cairo_line_to(cr, tx + tw * 0.35, base_y - th * 0.96);
        cairo_curve_to(cr, tx + tw * 0.55, base_y - th * 0.7,
                            tx + tw * 0.1, base_y - th * 0.5,
                            tx + tw * 0.6, base_y - th * 0.3);
        cairo_line_to(cr, tx + tw * 0.5, base_y);
        cairo_close_path(cr);
        cairo_fill(cr);

        ff_draw_swamp_moss_curtain(cr, tx + tw * 0.1, base_y - th * 0.7, th * 0.5, tw * 1.4, pal, 0.4);
    }

    ff_draw_swamp_distant_eyes(cr, w * 0.62, base_y - h * 0.006, h * 0.006, 0.0);

    const FFRootPalette *wpal = &FF_ROOT_PALETTES[1];
    ff_draw_swamp_wisp(cr, w * 0.24, base_y - h * 0.22, h * 0.012, 0.0, wpal);
    ff_draw_swamp_wisp(cr, w * 0.80, base_y - h * 0.30, h * 0.010, 1.4, &FF_ROOT_PALETTES[2]);
}

// Static: the base peaty-mud fill. No bubble_phase dependency, so this is
// the cacheable part.
void ff_draw_swamp_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.16, 0.14, 0.10);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
}

// Scroll: drifting silt ripples, plus a scatter of round lily pads and a
// couple of half-buried sunken logs - this theme's equivalent of the
// ship's coins or the Atlantis floor's gems.
void ff_draw_swamp_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    cairo_set_source_rgba(cr, 0.26, 0.22, 0.14, 0.5);
    for (double x = -fmod(bubble_phase * (h * 0.20), 46.0); x < w; x += 46.0) {
        cairo_move_to(cr, x, h - floor_h * 0.7);
        cairo_line_to(cr, x + 12, h - floor_h * 0.5);
        cairo_set_line_width(cr, 2.5);
        cairo_stroke(cr);
    }

    // Sunken logs.
    for (int i = 0; i < 3; i++) {
        double lx = fmod(i * 260.0 + 90.0, w);
        double ly = h - floor_h * (0.15 + 0.15 * (i % 2));
        double lw = floor_h * 1.3, lh = floor_h * 0.16;
        cairo_set_source_rgba(cr, 0.20, 0.14, 0.10, 0.85);
        cairo_save(cr);
        cairo_translate(cr, lx, ly);
        cairo_scale(cr, 1.0, 0.6);
        cairo_arc(cr, 0, 0, lw * 0.5, 0, 2 * M_PI);
        cairo_restore(cr);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.32, 0.24, 0.16, 0.7);
        cairo_save(cr);
        cairo_translate(cr, lx, ly - lh * 0.3);
        cairo_scale(cr, 1.0, 0.55);
        cairo_arc(cr, 0, 0, lw * 0.42, 0, 2 * M_PI);
        cairo_restore(cr);
        cairo_fill(cr);
    }

    // Lily pads with a small notch, scattered across the mud.
    for (int i = 0; i < 8; i++) {
        double px = fmod(i * 151.0 + 40.0, w);
        double py = h - floor_h * (0.30 + 0.5 * ((i * 37) % 5) / 5.0);
        double s = floor_h * 0.10 * (0.7 + 0.4 * ff_hash(i * 2.3));
        cairo_set_source_rgba(cr, 0.18, 0.42, 0.20, 0.85);
        cairo_save(cr);
        cairo_translate(cr, px, py);
        cairo_scale(cr, 1.0, 0.55);
        double notch = 0.35;
        cairo_arc(cr, 0, 0, s, notch, 2 * M_PI - notch);
        cairo_line_to(cr, 0, 0);
        cairo_close_path(cr);
        cairo_restore(cr);
        cairo_fill(cr);
    }
}

// A cluster of gnarled root tendrils standing in for seaweed - the same
// three-strand slot as every other theme's decoration, colored dark
// mossy brown-green and tipped with a tiny glowing wisp instead of a
// leafy tip.
void ff_draw_swamp_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    double r = 0.22, g = 0.28, b = 0.15, a = 0.6, sway_mult = 0.7;
    int strands = 3;
    for (int i = 0; i < strands; i++) {
        double sx = x + (i - 1) * height * 0.14;
        double sh = height * (0.75 + 0.25 * (i % 2));
        double phase = t * 0.85 + i * 1.7;
        double sway1 = sin(phase) * sh * 0.14 * sway_mult;
        double sway2 = sin(phase * 0.8 + 0.6) * sh * 0.26 * sway_mult;
        double tip_x = sx + sway2 * 1.2, tip_y = base_y - sh;

        cairo_set_source_rgba(cr, r, g, b, a * alpha_mult);
        cairo_set_line_width(cr, sh * 0.06);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, sx, base_y);
        cairo_curve_to(cr,
                        sx + sway1, base_y - sh * 0.35,
                        sx + sway2, base_y - sh * 0.7,
                        tip_x, tip_y);
        cairo_stroke(cr);

        double gs = sh * 0.10;
        double flicker = 0.6 + 0.4 * sin(t * 6.0 + i * 2.1);
        cairo_set_source_rgba(cr, 0.78, 0.95, 0.55, 0.55 * alpha_mult * flicker);
        cairo_arc(cr, tip_x, tip_y, gs * 0.5, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.78, 0.95, 0.55, 0.22 * alpha_mult * flicker);
        cairo_arc(cr, tip_x, tip_y, gs, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// One knobby root segment: a lumpy, irregular blob rather than a clean
// rectangle, in the root base color, with a darker underside shadow and a
// lighter mossy patch draped over the top - the detail that keeps it from
// reading as a stack of plain boxes.
static void ff_draw_swamp_knot(cairo_t *cr, double cx, double cy, double w, double h,
                                double seed, const FFRootPalette *pal) {
    double x0 = cx - w * 0.5, y0 = cy - h * 0.5;

    cairo_set_source_rgb(cr, pal->root_r, pal->root_g, pal->root_b);
    cairo_move_to(cr, x0, y0 + h * 0.2);
    cairo_curve_to(cr, x0 - w * 0.08, y0 + h * 0.6, x0 - w * 0.04, y0 + h * 0.95, x0 + w * 0.15, y0 + h);
    cairo_line_to(cr, x0 + w * 0.85, y0 + h);
    cairo_curve_to(cr, x0 + w * 1.05, y0 + h * 0.9, x0 + w * 1.08, y0 + h * 0.55, x0 + w, y0 + h * 0.25);
    cairo_curve_to(cr, x0 + w * 1.04, y0 - h * 0.05, x0 + w * 0.8, y0 - h * 0.08, x0 + w * 0.6, y0);
    cairo_line_to(cr, x0 + w * 0.3, y0);
    cairo_curve_to(cr, x0 + w * 0.1, y0 - h * 0.06, x0 - w * 0.05, y0 - h * 0.02, x0, y0 + h * 0.2);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Underside shadow.
    cairo_set_source_rgba(cr, pal->root_r * 0.45, pal->root_g * 0.45, pal->root_b * 0.45, 0.55);
    cairo_move_to(cr, x0 + w * 0.15, y0 + h);
    cairo_curve_to(cr, x0 + w * 0.4, y0 + h * 0.82, x0 + w * 0.7, y0 + h * 0.82, x0 + w * 0.85, y0 + h);
    cairo_curve_to(cr, x0 + w * 0.7, y0 + h * 0.92, x0 + w * 0.3, y0 + h * 0.92, x0 + w * 0.15, y0 + h);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Mossy patch draped over the top.
    cairo_set_source_rgba(cr, pal->moss_r, pal->moss_g, pal->moss_b, 0.75);
    cairo_move_to(cr, x0 + w * 0.05, y0 + h * 0.15);
    cairo_curve_to(cr, x0 + w * 0.25, y0 - h * 0.08, x0 + w * 0.65, y0 - h * 0.08, x0 + w * 0.85, y0 + h * 0.1);
    cairo_curve_to(cr, x0 + w * 0.6, y0 + h * 0.2, x0 + w * 0.3, y0 + h * 0.2, x0 + w * 0.05, y0 + h * 0.15);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, pal->root_r * 0.6, pal->root_g * 0.6, pal->root_b * 0.6, 0.5);
    cairo_set_line_width(cr, fmax(1.0, w * 0.02));
    cairo_move_to(cr, x0, y0 + h * 0.2);
    cairo_curve_to(cr, x0 - w * 0.08, y0 + h * 0.6, x0 - w * 0.04, y0 + h * 0.95, x0 + w * 0.15, y0 + h);
    cairo_stroke(cr);

    // A couple of tiny knot-hole flecks.
    for (int i = 0; i < 2; i++) {
        if (ff_hash(seed * 6.6 + i * 2.2) < 0.5) continue;
        double fx = x0 + w * ff_hash(seed * 7.7 + i * 1.4);
        double fy = y0 + h * ff_hash(seed * 8.8 + i * 1.9);
        cairo_set_source_rgba(cr, pal->root_r * 0.3, pal->root_g * 0.3, pal->root_b * 0.3, 0.6);
        cairo_arc(cr, fx, fy, w * 0.035, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// Draws one obstacle column as a stack of knobby, moss-draped mangrove
// root knots capped, at the gap-facing tip, by a trailing moss curtain
// with a floating will-o'-wisp lantern light hovering just above it - a
// living root pillar rather than a straight pipe. Rectilinear-enough
// bounding like the ship/Atlantis columns, so it needs no safety
// scale-down to stay inside the collision box.
void ff_draw_swamp_column(cairo_t *cr, double x, double y0, double y1,
                           double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFRootPalette *pal =
        &FF_ROOT_PALETTES[(int)(ff_hash(seed * 43.0) * 97.0) % FF_ROOT_PALETTE_COUNT];

    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double seg_h = width * 0.62;
    int segs = (int)fmax(2.0, round(total_h / seg_h));
    seg_h = total_h / segs;

    for (int i = 0; i < segs; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        double jitter = (ff_hash(seed * 4.4 + i * 1.3) - 0.5) * width * 0.08;
        ff_draw_swamp_knot(cr, cx + jitter, seg_y, width * 0.88, seg_h * 0.94,
                            seed + i * 5.7, pal);
    }

    // Trailing moss curtain at the gap-facing tip.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double moss_len = fmin(width * 0.5, total_h * 0.3);
    if (tip_at_y1) {
        ff_draw_swamp_moss_curtain(cr, cx, tip_y - moss_len, moss_len, width * 0.8, pal, 0.75);
    } else {
        // Growing up from the floor, so the "curtain" reads as trailing
        // roots draped back down toward where it emerged.
        ff_draw_swamp_moss_curtain(cr, cx, tip_y - moss_len * 0.15, moss_len * 0.9, width * 0.8, pal, 0.6);
    }

    // The will-o'-wisp, hovering just clear of the tip - this theme's real
    // signature, so it appears on every single pillar.
    double wisp_y = tip_y + inward * width * 0.30;
    ff_draw_swamp_wisp(cr, cx, wisp_y, width * 0.10, seed * 3.0, pal);
}
