#ifndef FLOPPYFISH_COMMON_H
#define FLOPPYFISH_COMMON_H

// Shared types/utilities/prototypes so the game can be split across one file
// per visual theme instead of one 1800-line floppyfish.cpp. Every theme file
// (floppyfish_reef.cpp, floppyfish_ship.cpp, floppyfish_cave.cpp,
// floppyfish_atlantis.cpp, floppyfish_rainbow.cpp, floppyfish_dino.cpp,
// floppyfish_antarctic.cpp, floppyfish_aquarium.cpp, floppyfish_galaxy.cpp)
// includes this and implements the same five-function contract below;
// floppyfish_common.cpp holds the small dispatchers that are the only code
// allowed to know all themes exist, and floppyfish.cpp (the core) drives
// state/physics/UI and calls into the dispatchers.

#include <cairo.h>
#include <math.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI  3.14159265359
#endif

// Nine visual themes the run cycles through as the fish travels: coral
// reef, a sunken pirate ship, a dark cave, the ruins of Atlantis, a
// sky-high rainbow realm, a murky prehistoric bone-yard, the icy
// Antarctic, a bright glass aquarium tank, and outer space.
typedef enum {
    FF_THEME_REEF = 0, FF_THEME_SHIP = 1, FF_THEME_CAVE = 2, FF_THEME_ATLANTIS = 3,
    FF_THEME_RAINBOW = 4, FF_THEME_DINO = 5, FF_THEME_ANTARCTIC = 6, FF_THEME_AQUARIUM = 7,
    FF_THEME_GALAXY = 8, FF_THEME_COUNT = 9
} FFTheme;

// Cheap deterministic pseudo-random hash - same input always gives the same
// output, so procedural shapes stay put frame to frame instead of flickering.
double ff_hash(double n);

// Smoothly fades a critter/decoration to transparent as it nears/crosses
// either screen edge, so it visibly dissolves instead of popping in/out.
double ff_edge_fade(double x, double w);

// --- Per-theme obstacle column art -----------------------------------------
// Draws one obstacle column (from y0 to y1, at world x, given pipe `width`).
// `seed` is fixed per pipe so the column's look holds steady while it
// scrolls. `tip_at_y1` is true for the column whose gap-facing end is at y1
// (i.e. it grows downward from the ceiling), false for the one growing
// upward from the floor.
void ff_draw_coral_column(cairo_t *cr, double x, double y0, double y1, double width, double seed, bool tip_at_y1);
void ff_draw_ship_column(cairo_t *cr, double x, double y0, double y1, double width, double seed, bool tip_at_y1);
void ff_draw_cave_column(cairo_t *cr, double x, double y0, double y1, double width, double seed, bool tip_at_y1);
void ff_draw_atlantis_column(cairo_t *cr, double x, double y0, double y1, double width, double seed, bool tip_at_y1);
void ff_draw_rainbow_column(cairo_t *cr, double x, double y0, double y1, double width, double seed, bool tip_at_y1);
void ff_draw_dino_column(cairo_t *cr, double x, double y0, double y1, double width, double seed, bool tip_at_y1);
void ff_draw_antarctic_column(cairo_t *cr, double x, double y0, double y1, double width, double seed, bool tip_at_y1);
void ff_draw_aquarium_column(cairo_t *cr, double x, double y0, double y1, double width, double seed, bool tip_at_y1);
void ff_draw_galaxy_column(cairo_t *cr, double x, double y0, double y1, double width, double seed, bool tip_at_y1);

// --- Per-theme sky --------------------------------------------------------
// Gradient top/bottom color and ambient-particle color+alpha for the theme.
// One pair of these per theme file (not a single switch) since each theme
// owns its own palette.
void ff_reef_sky_colors(double *top_r, double *top_g, double *top_b, double *bot_r, double *bot_g, double *bot_b);
void ff_ship_sky_colors(double *top_r, double *top_g, double *top_b, double *bot_r, double *bot_g, double *bot_b);
void ff_cave_sky_colors(double *top_r, double *top_g, double *top_b, double *bot_r, double *bot_g, double *bot_b);
void ff_atlantis_sky_colors(double *top_r, double *top_g, double *top_b, double *bot_r, double *bot_g, double *bot_b);
void ff_rainbow_sky_colors(double *top_r, double *top_g, double *top_b, double *bot_r, double *bot_g, double *bot_b);
void ff_dino_sky_colors(double *top_r, double *top_g, double *top_b, double *bot_r, double *bot_g, double *bot_b);
void ff_antarctic_sky_colors(double *top_r, double *top_g, double *top_b, double *bot_r, double *bot_g, double *bot_b);
void ff_aquarium_sky_colors(double *top_r, double *top_g, double *top_b, double *bot_r, double *bot_g, double *bot_b);
void ff_galaxy_sky_colors(double *top_r, double *top_g, double *top_b, double *bot_r, double *bot_g, double *bot_b);

void ff_reef_particle_color(double *r, double *g, double *b, double *a);
void ff_ship_particle_color(double *r, double *g, double *b, double *a);
void ff_cave_particle_color(double *r, double *g, double *b, double *a);
void ff_atlantis_particle_color(double *r, double *g, double *b, double *a);
void ff_rainbow_particle_color(double *r, double *g, double *b, double *a);
void ff_dino_particle_color(double *r, double *g, double *b, double *a);
void ff_antarctic_particle_color(double *r, double *g, double *b, double *a);
void ff_aquarium_particle_color(double *r, double *g, double *b, double *a);
void ff_galaxy_particle_color(double *r, double *g, double *b, double *a);

// Distant skyline/backdrop silhouette drawn along the floor line (base_y).
void ff_draw_reef_backdrop(cairo_t *cr, double w, double h, double base_y);
void ff_draw_ship_backdrop(cairo_t *cr, double w, double h, double base_y);
void ff_draw_cave_backdrop(cairo_t *cr, double w, double h, double base_y);
void ff_draw_atlantis_backdrop(cairo_t *cr, double w, double h, double base_y);
void ff_draw_rainbow_backdrop(cairo_t *cr, double w, double h, double base_y);
void ff_draw_dino_backdrop(cairo_t *cr, double w, double h, double base_y);
void ff_draw_antarctic_backdrop(cairo_t *cr, double w, double h, double base_y);
void ff_draw_aquarium_backdrop(cairo_t *cr, double w, double h, double base_y);
void ff_draw_galaxy_backdrop(cairo_t *cr, double w, double h, double base_y);

// --- Per-theme floor --------------------------------------------------------
// Split into a static part (base fill plus any non-scrolling decoration -
// coins, gems, crystal flecks, baseline seam) and a scroll part (whatever
// actually moves with bubble_phase - sand ripples, plank seams, mosaic
// tiles). The static part doesn't depend on bubble_phase at all, so callers
// can render it once per theme into an offscreen surface and just blit that
// every frame instead of repainting the whole floor rect from scratch; the
// scroll part is cheap (a handful of lines/tiles) and meant to be redrawn
// live on top of that cached blit each frame.
void ff_draw_reef_floor_static(cairo_t *cr, double w, double h, double floor_h);
void ff_draw_ship_floor_static(cairo_t *cr, double w, double h, double floor_h);
void ff_draw_cave_floor_static(cairo_t *cr, double w, double h, double floor_h);
void ff_draw_atlantis_floor_static(cairo_t *cr, double w, double h, double floor_h);
void ff_draw_rainbow_floor_static(cairo_t *cr, double w, double h, double floor_h);
void ff_draw_dino_floor_static(cairo_t *cr, double w, double h, double floor_h);
void ff_draw_antarctic_floor_static(cairo_t *cr, double w, double h, double floor_h);
void ff_draw_aquarium_floor_static(cairo_t *cr, double w, double h, double floor_h);
void ff_draw_galaxy_floor_static(cairo_t *cr, double w, double h, double floor_h);

void ff_draw_reef_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase);
void ff_draw_ship_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase);
void ff_draw_cave_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase);
void ff_draw_atlantis_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase);
void ff_draw_rainbow_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase);
void ff_draw_dino_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase);
void ff_draw_antarctic_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase);
void ff_draw_aquarium_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase);
void ff_draw_galaxy_floor_scroll(cairo_t *cr, double w, double h, double floor_h, double bubble_phase);

// --- Per-theme floor decoration clump (seaweed/rope/crystals/kelp) --------
// x/base_y/height/t match the slot shared across themes so the same patch
// can crossfade between two themes' decoration as the zone changes.
void ff_draw_reef_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult);
void ff_draw_ship_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult);
void ff_draw_cave_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult);
void ff_draw_atlantis_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult);
void ff_draw_rainbow_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult);
void ff_draw_dino_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult);
void ff_draw_antarctic_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult);
void ff_draw_aquarium_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult);
void ff_draw_galaxy_seaweed(cairo_t *cr, double x, double base_y, double height, double t, double alpha_mult);

// --- Dispatchers (floppyfish_common.cpp) -----------------------------------
// The only functions that need to know all FF_THEME_COUNT themes exist.
void ff_draw_obstacle_column(int theme, cairo_t *cr, double x, double y0, double y1,
                              double width, double seed, bool tip_at_y1);

// Full sky/floor draws (static + animated together), kept for anyone that
// wants the simple one-call version. draw_floppy_fish itself uses the split
// static/animated calls below instead, so it can cache the static part.
void ff_draw_theme_sky(cairo_t *cr, int theme, double w, double h, double bubble_phase);
void ff_draw_theme_floor(cairo_t *cr, int theme, double w, double h, double floor_h, double bubble_phase);

// Static half of the sky: gradient + distant skyline backdrop, no
// bubble_phase dependency - safe to render once per theme into an offscreen
// surface and blit every frame.
void ff_draw_theme_sky_static(cairo_t *cr, int theme, double w, double h);
// Animated half of the sky: the rising ambient particles/bubbles. Cheap, so
// this is meant to be redrawn live on top of the cached static blit above.
void ff_draw_theme_particles(cairo_t *cr, int theme, double w, double h, double bubble_phase);

// Static/animated split for the floor, same idea as the sky above.
void ff_draw_theme_floor_static(cairo_t *cr, int theme, double w, double h, double floor_h);
void ff_draw_theme_floor_scroll(cairo_t *cr, int theme, double w, double h, double floor_h, double bubble_phase);

void ff_draw_seaweed(cairo_t *cr, double x, double base_y, double height, double t, int theme, double alpha_mult);

#endif
