#include "floppyfish_common.h"

#ifndef M_PI
#define M_PI  3.14159265359
#endif

double ff_hash(double n) {
    double x = sin(n * 127.1) * 43758.5453;
    return x - floor(x);
}

double ff_edge_fade(double x, double w) {
    double fade_zone = w * 0.15;
    double fade = 1.0;
    if (x < fade_zone) fade = x / fade_zone;
    else if (x > w - fade_zone) fade = (w - x) / fade_zone;
    if (fade < 0.0) fade = 0.0;
    if (fade > 1.0) fade = 1.0;
    return fade;
}

// Picks which theme's obstacle art to draw - the one piece of code that
// needs to know all four themes exist.
void ff_draw_obstacle_column(int theme, cairo_t *cr, double x, double y0, double y1,
                              double width, double seed, bool tip_at_y1) {
    switch (theme) {
        case FF_THEME_SHIP:     ff_draw_ship_column(cr, x, y0, y1, width, seed, tip_at_y1); break;
        case FF_THEME_CAVE:     ff_draw_cave_column(cr, x, y0, y1, width, seed, tip_at_y1); break;
        case FF_THEME_ATLANTIS: ff_draw_atlantis_column(cr, x, y0, y1, width, seed, tip_at_y1); break;
        default:                ff_draw_coral_column(cr, x, y0, y1, width, seed, tip_at_y1); break;
    }
}

// Static half of the sky: the gradient and the distant skyline silhouette.
// Neither depends on bubble_phase, so this is the part draw_floppy_fish
// caches into an offscreen surface per theme instead of repainting a
// full-canvas gradient (and re-stroking the backdrop shapes) every frame.
void ff_draw_theme_sky_static(cairo_t *cr, int theme, double w, double h) {
    double top_r, top_g, top_b, bot_r, bot_g, bot_b;
    switch (theme) {
        case FF_THEME_SHIP:     ff_ship_sky_colors(&top_r, &top_g, &top_b, &bot_r, &bot_g, &bot_b); break;
        case FF_THEME_CAVE:     ff_cave_sky_colors(&top_r, &top_g, &top_b, &bot_r, &bot_g, &bot_b); break;
        case FF_THEME_ATLANTIS: ff_atlantis_sky_colors(&top_r, &top_g, &top_b, &bot_r, &bot_g, &bot_b); break;
        default:                ff_reef_sky_colors(&top_r, &top_g, &top_b, &bot_r, &bot_g, &bot_b); break;
    }

    cairo_pattern_t *sky = cairo_pattern_create_linear(0, 0, 0, h);
    cairo_pattern_add_color_stop_rgb(sky, 0.0, top_r, top_g, top_b);
    cairo_pattern_add_color_stop_rgb(sky, 1.0, bot_r, bot_g, bot_b);
    cairo_set_source(cr, sky);
    cairo_paint(cr);
    cairo_pattern_destroy(sky);

    double floor_h = h * 0.10;
    double base_y = h - floor_h;
    switch (theme) {
        case FF_THEME_SHIP:     ff_draw_ship_backdrop(cr, w, h, base_y); break;
        case FF_THEME_CAVE:     ff_draw_cave_backdrop(cr, w, h, base_y); break;
        case FF_THEME_ATLANTIS: ff_draw_atlantis_backdrop(cr, w, h, base_y); break;
        default:                ff_draw_reef_backdrop(cr, w, h, base_y); break;
    }
}

// Animated half of the sky: the rising ambient particles/bubbles. Only 22
// small arcs, so it's cheap enough to redraw live every frame on top of the
// cached static blit above.
void ff_draw_theme_particles(cairo_t *cr, int theme, double w, double h, double bubble_phase) {
    double pr, pg, pb, pa;
    switch (theme) {
        case FF_THEME_SHIP:     ff_ship_particle_color(&pr, &pg, &pb, &pa); break;
        case FF_THEME_CAVE:     ff_cave_particle_color(&pr, &pg, &pb, &pa); break;
        case FF_THEME_ATLANTIS: ff_atlantis_particle_color(&pr, &pg, &pb, &pa); break;
        default:                ff_reef_particle_color(&pr, &pg, &pb, &pa); break;
    }

    for (int i = 0; i < 22; i++) {
        double bx = fmod(i * 53.0 + w * 0.5, w);
        double speed = 30.0 + (i % 5) * 10.0;
        double by = h - fmod(bubble_phase * speed + i * 71.0, h + 40.0);
        double size = 2.0 + (i % 4);
        cairo_set_source_rgba(cr, pr, pg, pb, pa);
        cairo_arc(cr, bx, by, size, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// Sky gradient, ambient particles, and a distant skyline silhouette for one
// theme, as a single call. Just the static+animated halves above back to
// back - kept for anyone who wants the simple one-call version; the actual
// game draws them separately so the static half can be cached (see
// draw_floppy_fish).
void ff_draw_theme_sky(cairo_t *cr, int theme, double w, double h, double bubble_phase) {
    ff_draw_theme_sky_static(cr, theme, w, h);
    ff_draw_theme_particles(cr, theme, w, h, bubble_phase);
}

// Static half of the floor: whatever part of the theme's floor doesn't move
// with bubble_phase. Every theme's static half currently boils down to just
// the base fill rectangle (the one part big enough to be worth caching) -
// see each theme file's own comment for why the rest of its decoration
// stays in the live/scroll half instead.
void ff_draw_theme_floor_static(cairo_t *cr, int theme, double w, double h, double floor_h) {
    switch (theme) {
        case FF_THEME_SHIP:     ff_draw_ship_floor_static(cr, w, h, floor_h); break;
        case FF_THEME_CAVE:     ff_draw_cave_floor_static(cr, w, h, floor_h); break;
        case FF_THEME_ATLANTIS: ff_draw_atlantis_floor_static(cr, w, h, floor_h); break;
        default:                ff_draw_reef_floor_static(cr, w, h, floor_h); break;
    }
}

// Animated half of the floor: sand ripples, plank seams, mosaic tiles, plus
// any static decoration that has to render on top of those (coins, gems,
// baseline seams) to keep the original draw order. Cheap, meant to be
// redrawn live every frame on top of the cached static blit above.
void ff_draw_theme_floor_scroll(cairo_t *cr, int theme, double w, double h, double floor_h, double bubble_phase) {
    switch (theme) {
        case FF_THEME_SHIP:     ff_draw_ship_floor_scroll(cr, w, h, floor_h, bubble_phase); break;
        case FF_THEME_CAVE:     ff_draw_cave_floor_scroll(cr, w, h, floor_h, bubble_phase); break;
        case FF_THEME_ATLANTIS: ff_draw_atlantis_floor_scroll(cr, w, h, floor_h, bubble_phase); break;
        default:                ff_draw_reef_floor_scroll(cr, w, h, floor_h, bubble_phase); break;
    }
}

// Floor for one theme: sand, ship-deck planking, dark rock, or Atlantean
// mosaic, as a single call. Just the static+scroll halves above back to
// back - kept for anyone who wants the simple one-call version; the actual
// game draws them separately so the static half can be cached (see
// draw_floppy_fish).
void ff_draw_theme_floor(cairo_t *cr, int theme, double w, double h, double floor_h, double bubble_phase) {
    ff_draw_theme_floor_static(cr, theme, w, h, floor_h);
    ff_draw_theme_floor_scroll(cr, theme, w, h, floor_h, bubble_phase);
}

void ff_draw_seaweed(cairo_t *cr, double x, double base_y, double height, double t, int theme, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    switch (theme) {
        case FF_THEME_SHIP:     ff_draw_ship_seaweed(cr, x, base_y, height, t, alpha_mult); break;
        case FF_THEME_CAVE:     ff_draw_cave_seaweed(cr, x, base_y, height, t, alpha_mult); break;
        case FF_THEME_ATLANTIS: ff_draw_atlantis_seaweed(cr, x, base_y, height, t, alpha_mult); break;
        default:                ff_draw_reef_seaweed(cr, x, base_y, height, t, alpha_mult); break;
    }
}
