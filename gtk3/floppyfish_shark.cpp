#include "floppyfish_common.h"

// --- Shark territory theme -------------------------------------------------
// A murky, roped-off stretch of open water - the thirteenth theme zone
// alongside reef/ship/cave/atlantis/rainbow/dino/antarctic/aquarium/galaxy/
// swamp/party/volcanic. Obstacles are weathered wooden pilings with real
// bite-shaped chunks missing from the wood and a hazard-taped warning sign
// at the gap-facing tip, the floor is churned sand scattered with shark
// teeth, and the seaweed slot becomes a strand of kelp with its own bite
// taken out of it. A pair of prowling dorsal fins (see ff_draw_shark_pack_
// group in floppyfish.cpp, reusing the existing shark silhouette) is the
// theme's dedicated guest.

void ff_shark_sky_colors(double *top_r, double *top_g, double *top_b,
                          double *bot_r, double *bot_g, double *bot_b) {
    // Murky, low-visibility water - darker and greyer than the open reef,
    // with a faint warning-red cast low down rather than a clean blue.
    *top_r = 0.03; *top_g = 0.09; *top_b = 0.11;
    *bot_r = 0.26; *bot_g = 0.28; *bot_b = 0.27;
}

void ff_shark_particle_color(double *r, double *g, double *b, double *a) {
    // Sparse, dull chum-red flecks drifting up rather than clean air
    // bubbles - the water here doesn't look inviting.
    *r = 0.55; *g = 0.16; *b = 0.14; *a = 0.22;
}

// Weathered wood tones shared by the pilings and the sunken cage/posts in
// the backdrop, so the palette reads consistently everywhere it shows up.
typedef struct {
    double wood_r, wood_g, wood_b;
    double dark_r, dark_g, dark_b;
} FFPlankColor;

static const FFPlankColor FF_PLANK_COLORS[] = {
    {0.42, 0.32, 0.22,  0.22, 0.16, 0.10}, // driftwood brown
    {0.38, 0.30, 0.24,  0.20, 0.15, 0.11}, // weathered grey-brown
    {0.46, 0.34, 0.20,  0.24, 0.17, 0.09}, // warmer plank tan
};
#define FF_PLANK_COLOR_COUNT (int)(sizeof(FF_PLANK_COLORS) / sizeof(FF_PLANK_COLORS[0]))

// A small hazard sign: a yellow diamond with a black dorsal-fin silhouette
// on it, the same icon used on the piling caps and the backdrop warning
// post - this theme's real signature detail, so it turns up everywhere the
// theme's art appears.
static void ff_draw_shark_hazard_sign(cairo_t *cr, double cx, double cy, double s) {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, s, s);

    cairo_set_source_rgba(cr, 0.95, 0.78, 0.10, 0.95);
    cairo_move_to(cr, 0, -1.3);
    cairo_line_to(cr, 1.3, 0);
    cairo_line_to(cr, 0, 1.3);
    cairo_line_to(cr, -1.3, 0);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.10, 0.08, 0.06, 0.8);
    cairo_set_line_width(cr, 0.10);
    cairo_move_to(cr, 0, -1.3);
    cairo_line_to(cr, 1.3, 0);
    cairo_line_to(cr, 0, 1.3);
    cairo_line_to(cr, -1.3, 0);
    cairo_close_path(cr);
    cairo_stroke(cr);

    // The universal "shark fin breaking the surface" icon - a shark
    // silhouette riding mostly below a wavy waterline, with just its
    // dorsal fin and tail tip poking up through it. Reads unambiguously
    // as "shark" rather than a generic caution triangle.
    double wl = 0.05; // waterline y, local to the icon

    cairo_set_source_rgba(cr, 0.08, 0.08, 0.08, 0.95);

    // Body: low and tapered, riding mostly below the waterline.
    cairo_move_to(cr, -0.85, wl + 0.20);
    cairo_curve_to(cr, -0.55, wl + 0.40, 0.30, wl + 0.44, 0.78, wl + 0.16);
    cairo_curve_to(cr, 0.45, wl + 0.32, -0.40, wl + 0.32, -0.85, wl + 0.20);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Tail fin, a small triangle at the back just grazing the surface.
    cairo_move_to(cr, 0.66, wl + 0.10);
    cairo_line_to(cr, 1.05, wl - 0.30);
    cairo_line_to(cr, 0.80, wl + 0.22);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Dorsal fin - the clear signature shape breaking the surface: a
    // steep leading edge and a swept, curved trailing edge.
    cairo_move_to(cr, -0.34, wl + 0.06);
    cairo_curve_to(cr, -0.30, wl - 0.32, -0.14, wl - 0.66, 0.04, wl - 0.85);
    cairo_curve_to(cr, 0.12, wl - 0.48, 0.22, wl - 0.16, 0.32, wl + 0.06);
    cairo_curve_to(cr, 0.10, wl + 0.14, -0.14, wl + 0.14, -0.34, wl + 0.06);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Wavy waterline drawn on top, pale so it reads as the sea surface and
    // visually separates "above" (fin, tail tip) from "below" (body).
    cairo_set_source_rgba(cr, 0.98, 0.90, 0.55, 0.95);
    cairo_set_line_width(cr, 0.10);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, -0.95, wl);
    cairo_curve_to(cr, -0.60, wl - 0.11, -0.30, wl + 0.11, 0.0, wl);
    cairo_curve_to(cr, 0.30, wl - 0.11, 0.60, wl + 0.11, 0.95, wl);
    cairo_stroke(cr);

    cairo_restore(cr);
}

// A strip of black/yellow hazard tape wrapped diagonally around a post -
// used both on the pilings and the backdrop's warning post.
static void ff_draw_shark_hazard_tape(cairo_t *cr, double x0, double y0, double w, double h) {
    cairo_save(cr);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_clip(cr);
    cairo_set_line_width(cr, h * 0.35);
    double stripe = h * 0.7;
    bool yellow = true;
    for (double sx = x0 - h; sx < x0 + w + h; sx += stripe) {
        cairo_set_source_rgba(cr, yellow ? 0.95 : 0.08, yellow ? 0.78 : 0.08, yellow ? 0.10 : 0.08, 0.9);
        cairo_move_to(cr, sx, y0 - h);
        cairo_line_to(cr, sx + h * 1.4, y0 + h * 2.0);
        cairo_stroke(cr);
        yellow = !yellow;
    }
    cairo_restore(cr);
}

// Distant dorsal fins slicing the surface along the floor line, a sunken
// diving cage dented from repeated bites, and a warning post strung with
// hazard tape and topped with the theme's sign - this theme's equivalent
// of the ship's hull or Atlantis's temple: the one backdrop that most says
// "shark territory" at a glance.
// A simple cruising shark silhouette for the backdrop "shiver" below - not
// the animated foreground/guest shark, just a flat side-profile shape
// (torpedo body, dorsal fin, tail) baked into the cached backdrop so a
// whole group of them can patrol the murk behind the pipes for free.
static void ff_draw_shark_bg_body(cairo_t *cr, double x, double y, double scale, double dir, double alpha) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, dir * scale, scale);
    cairo_set_source_rgba(cr, 0.20, 0.24, 0.26, alpha);

    // Tail fin, forked.
    cairo_move_to(cr, -26, 0);
    cairo_curve_to(cr, -33, -3, -39, -10, -46, -13);
    cairo_curve_to(cr, -39, -4, -35, -1, -30, 0);
    cairo_curve_to(cr, -35, 1, -39, 4, -46, 11);
    cairo_curve_to(cr, -39, 8, -33, 3, -26, 0);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Body.
    cairo_move_to(cr, 29, 0.5);
    cairo_curve_to(cr, 25, -7, 10, -10, -4, -8.5);
    cairo_curve_to(cr, -14, -7, -22, -4, -27, -0.5);
    cairo_curve_to(cr, -22, 4, -14, 7.5, -4, 8.5);
    cairo_curve_to(cr, 10, 10, 25, 6, 29, 0.5);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Dorsal fin.
    cairo_move_to(cr, -7, -8);
    cairo_curve_to(cr, -5, -16, 1, -19, 5, -20);
    cairo_curve_to(cr, 2, -14, 3, -10, 6, -7);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_restore(cr);
}

void ff_draw_shark_backdrop(cairo_t *cr, double w, double h, double base_y) {
    // The shiver of sharks patrolling the background actually swims - see
    // ff_draw_shark_bg_shiver_live below, called live every frame from
    // draw_floppy_fish (this backdrop layer is cached/blitted, so anything
    // drawn here would otherwise be frozen in place).

    // A row of distant dorsal fins cutting the surface, jagged like a
    // skyline rather than individually animated (this layer is cached, not
    // redrawn live) - drawn pale against the dark water so they actually
    // read at a glance instead of disappearing into the backdrop.
    cairo_set_source_rgba(cr, 0.42, 0.46, 0.48, 0.65);
    for (int i = 0; i < 6; i++) {
        double fx = w * (0.06 + 0.16 * i) + 20.0 * sin(i * 2.3);
        double fh = h * (0.045 + 0.025 * fabs(sin(i * 1.9)));
        cairo_move_to(cr, fx - fh * 0.9, base_y);
        cairo_curve_to(cr, fx - fh * 0.3, base_y - fh * 1.1, fx + fh * 0.1, base_y - fh * 1.2, fx + fh * 0.5, base_y - fh * 0.2);
        cairo_curve_to(cr, fx, base_y - fh * 0.3, fx - fh * 0.5, base_y - fh * 0.1, fx - fh * 0.9, base_y);
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    // Sunken diving cage, dented and missing a bar or two.
    double cage_x = w * 0.28, cage_w = h * 0.16, cage_h = h * 0.22;
    double cx0 = cage_x - cage_w * 0.5, cy0 = base_y - cage_h;
    cairo_set_source_rgba(cr, 0.35, 0.36, 0.38, 0.5);
    cairo_set_line_width(cr, fmax(1.5, cage_w * 0.04));
    cairo_rectangle(cr, cx0, cy0, cage_w, cage_h);
    cairo_stroke(cr);
    for (int i = 1; i < 4; i++) {
        double bx = cx0 + cage_w * i / 4.0;
        if (i == 2) continue; // a bar bitten clean out
        cairo_move_to(cr, bx, cy0);
        cairo_line_to(cr, bx, cy0 + cage_h);
        cairo_stroke(cr);
    }
    for (int i = 1; i < 3; i++) {
        double by = cy0 + cage_h * i / 3.0;
        cairo_move_to(cr, cx0, by);
        cairo_line_to(cr, cx0 + cage_w, by);
        cairo_stroke(cr);
    }
    // The dent/bite bitten into the top rail.
    cairo_set_source_rgba(cr, 0.03, 0.09, 0.11, 1.0);
    cairo_arc(cr, cx0 + cage_w * 0.5, cy0, cage_w * 0.16, 0, M_PI);
    cairo_fill(cr);

    // Warning post strung with hazard tape and topped with the sign.
    double px = w * 0.72, post_h = h * 0.16, post_w = h * 0.014;
    cairo_set_source_rgba(cr, 0.30, 0.22, 0.15, 0.6);
    cairo_rectangle(cr, px - post_w * 0.5, base_y - post_h, post_w, post_h);
    cairo_fill(cr);
    ff_draw_shark_hazard_tape(cr, px - post_w * 2.2, base_y - post_h * 0.6, post_w * 4.4, post_h * 0.16);
    ff_draw_shark_hazard_sign(cr, px, base_y - post_h * 0.85, post_h * 0.22);

    // A second, closer warning post with just the sign, no cage.
    ff_draw_shark_hazard_sign(cr, w * 0.90, base_y - h * 0.10, h * 0.026);
}

// The shiver of sharks patrolling the background, actually swimming this
// time - called live every frame from draw_floppy_fish (not part of the
// cached backdrop above, which only ever gets painted once and blitted
// after that, so anything drawn there is frozen in place). Each shark
// loops steadily across the screen and off the opposite margin using
// bubble_phase as its clock, the same way the ambient bubbles in
// ff_draw_theme_particles loop on a phase rather than a per-frame
// timestep - so their motion is smooth and independent of frame rate.
// Not part of the shared five-function theme contract (only this theme
// needs it), so it's called directly from a theme check in
// draw_floppy_fish rather than through a dispatcher.
void ff_draw_shark_bg_shiver_live(cairo_t *cr, double w, double h, double base_y, double bubble_phase) {
    for (int i = 0; i < 9; i++) {
        double by = base_y - h * (0.08 + 0.50 * ff_hash(i * 5.3 + 2.0));
        double depth = ff_hash(i * 7.1 + 3.0); // 0 = far/dim/slow, 1 = near/bold/fast
        double scale = h * (0.0011 + 0.0020 * depth);
        double alpha = 0.14 + 0.22 * depth;
        double dir = (ff_hash(i * 9.7 + 4.0) > 0.5) ? 1.0 : -1.0;
        double speed = w * (0.020 + 0.035 * depth);

        double margin = w * 0.15;
        double travel = w + margin * 2.0;
        double raw = fmod(bubble_phase * speed + i * 191.0, travel);
        double bx = (dir > 0) ? (-margin + raw) : (w + margin - raw);

        ff_draw_shark_bg_body(cr, bx, by, scale, dir, alpha);
    }
}

// Static: the churned sand base fill. No bubble_phase dependency, so this
// is the cacheable part.
void ff_draw_shark_floor_static(cairo_t *cr, double w, double h, double floor_h) {
    cairo_set_source_rgb(cr, 0.42, 0.38, 0.32);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
}

// Scroll: drifting sand-drag lines, plus a scatter of shark teeth half
// buried in the sand - this theme's equivalent of the ship's coins or the
// Atlantis floor's gems.
void ff_draw_shark_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase) {
    cairo_set_source_rgba(cr, 0.30, 0.26, 0.20, 0.5);
    for (double x = -fmod(bubble_phase * (h * 0.20), 40.0); x < w; x += 40.0) {
        cairo_move_to(cr, x, h - floor_h * 0.5);
        cairo_line_to(cr, x + 14, h - floor_h * 0.35);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
    }

    for (int i = 0; i < 8; i++) {
        double tx = fmod(i * 151.0 + 35.0, w);
        double ty = h - floor_h * (0.15 + 0.35 * ((i * 41) % 5) / 5.0);
        double s = floor_h * 0.055;
        double rot = ff_hash(i * 2.7) * 2.0 * M_PI;

        cairo_save(cr);
        cairo_translate(cr, tx, ty);
        cairo_rotate(cr, rot);
        cairo_set_source_rgba(cr, 0.92, 0.90, 0.85, 0.9);
        cairo_move_to(cr, 0, -s);
        cairo_line_to(cr, s * 0.6, s * 0.7);
        cairo_line_to(cr, -s * 0.6, s * 0.7);
        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.55, 0.50, 0.45, 0.6);
        cairo_set_line_width(cr, fmax(0.5, s * 0.1));
        cairo_move_to(cr, 0, -s);
        cairo_line_to(cr, s * 0.6, s * 0.7);
        cairo_stroke(cr);
        cairo_restore(cr);
    }
}

// A strand of kelp with a semicircular bite taken clean out of one blade -
// same three-strand slot as every other theme's decoration, but duller and
// visibly damaged rather than lush.
void ff_draw_shark_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    double r = 0.18, g = 0.28, b = 0.16, a = 0.55, sway_mult = 0.9;
    int strands = 3;
    for (int i = 0; i < strands; i++) {
        double sx = x + (i - 1) * height * 0.14;
        double sh = height * (0.75 + 0.25 * (i % 2));
        double phase = t * 1.0 + i * 1.7;
        double sway1 = sin(phase) * sh * 0.16 * sway_mult;
        double sway2 = sin(phase * 0.8 + 0.6) * sh * 0.30 * sway_mult;
        double tip_x = sx + sway2 * 1.2, tip_y = base_y - sh;

        cairo_set_source_rgba(cr, r, g, b, a * alpha_mult);
        cairo_set_line_width(cr, sh * 0.10);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, sx, base_y);
        cairo_curve_to(cr,
                        sx + sway1, base_y - sh * 0.35,
                        sx + sway2, base_y - sh * 0.7,
                        tip_x, tip_y);
        cairo_stroke(cr);
    }
}

// Draws one bite-shaped notch clean through a piling segment (real
// transparency via CAIRO_OPERATOR_CLEAR, since each pipe is rendered into
// its own alpha-backed offscreen surface - see ff_build_pipe_art_cache in
// floppyfish.cpp), plus a couple of pale scratch marks at the rim as if
// teeth dragged across the wood on the way out.
static void ff_draw_shark_bite(cairo_t *cr, double bx, double by, double r, double seed) {
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_arc(cr, bx, by, r, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_set_source_rgba(cr, 0.92, 0.90, 0.85, 0.5);
    cairo_set_line_width(cr, fmax(0.6, r * 0.10));
    for (int k = 0; k < 3; k++) {
        double ang = -0.6 + k * 0.6 + (ff_hash(seed * 5.0 + k) - 0.5) * 0.2;
        double ex = bx + cos(ang) * r * 1.15;
        double ey = by + sin(ang) * r * 1.15;
        cairo_move_to(cr, bx + cos(ang) * r * 0.6, by + sin(ang) * r * 0.6);
        cairo_line_to(cr, ex, ey);
        cairo_stroke(cr);
    }
}

// One weathered plank segment: a wood-grain rectangle with a couple of
// bite-shaped chunks missing from an edge, per the theme's real signature.
static void ff_draw_shark_plank(cairo_t *cr, double cx, double cy, double w, double h,
                                 double seed, const FFPlankColor *pal, bool bite_left, bool bite_right) {
    double x0 = cx - w * 0.5, y0 = cy - h * 0.5;
    cairo_set_source_rgb(cr, pal->wood_r, pal->wood_g, pal->wood_b);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_fill(cr);

    // Wood-grain lines.
    cairo_set_source_rgba(cr, pal->dark_r, pal->dark_g, pal->dark_b, 0.5);
    cairo_set_line_width(cr, fmax(1.0, h * 0.04));
    for (int i = 0; i < 3; i++) {
        double gy = y0 + h * (0.2 + 0.3 * i);
        cairo_move_to(cr, x0, gy + sin(seed * 3.0 + i) * h * 0.05);
        cairo_line_to(cr, x0 + w, gy - sin(seed * 4.0 + i) * h * 0.05);
        cairo_stroke(cr);
    }

    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_set_line_width(cr, fmax(1.0, w * 0.02));
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_stroke(cr);

    // Bite-shaped chunks missing from the left and/or right edge.
    if (bite_left) {
        ff_draw_shark_bite(cr, x0, cy, h * 0.42, seed);
    }
    if (bite_right) {
        ff_draw_shark_bite(cr, x0 + w, cy + h * 0.15, h * 0.36, seed * 1.7);
    }
}

// Draws one obstacle column as a stack of weathered planks with real
// bite-shaped chunks missing from their edges, wrapped partway up with a
// band of hazard tape, and capped at the gap-facing tip by the theme's
// warning sign and a small dorsal fin - a bitten piling rather than a
// straight pipe. Rectilinear like the ship/Atlantis/antarctic/volcanic
// columns, so it needs no safety scale-down to stay inside the collision
// box (the bites eat into the rendered art, not the hitbox, same as every
// other theme's decorative overhang).
void ff_draw_shark_column(cairo_t *cr, double x, double y0, double y1,
                           double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFPlankColor *pal = &FF_PLANK_COLORS[(int)(ff_hash(seed * 43.0) * 97.0) % FF_PLANK_COLOR_COUNT];

    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double seg_h = width * 0.64;
    int segs = (int)fmax(2.0, round(total_h / seg_h));
    seg_h = total_h / segs;

    int tape_seg = (int)(ff_hash(seed * 21.0) * segs);
    for (int i = 0; i < segs; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        bool bl = ff_hash(seed * 6.1 + i * 1.9) > 0.5;
        bool br = ff_hash(seed * 7.3 + i * 2.3) > 0.62;
        ff_draw_shark_plank(cr, cx, seg_y, width * 0.92, seg_h * 0.94,
                             seed + i * 5.7, pal, bl, br);
        if (i == tape_seg) {
            ff_draw_shark_hazard_tape(cr, cx - width * 0.46, seg_y - seg_h * 0.22, width * 0.92, seg_h * 0.30);
        }
    }

    // Warning sign bracket at the gap-facing tip.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double sign_r = width * 0.20;
    double sign_cy = tip_y + inward * sign_r * 1.3;

    cairo_set_source_rgba(cr, pal->dark_r, pal->dark_g, pal->dark_b, 0.9);
    cairo_set_line_width(cr, fmax(1.0, width * 0.05));
    cairo_move_to(cr, cx, tip_y);
    cairo_line_to(cr, cx, sign_cy);
    cairo_stroke(cr);
    ff_draw_shark_hazard_sign(cr, cx, sign_cy, sign_r);
}
