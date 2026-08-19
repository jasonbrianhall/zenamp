#include "visualization.h"
#include "floppyfish_common.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// All-time high-score persistence is a standalone-game feature (needs a
// writable per-user location on disk) - not used when this file is built
// into zenamp's visualizer, so it's compiled in only alongside the rest of
// the FLOPPYSOUND-gated standalone-game code.
#ifdef FLOPPYSOUND
#ifdef _WIN32
#include <direct.h>   // _mkdir
#define FF_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h> // mkdir
#define FF_MKDIR(p) mkdir(p, 0755)
#endif
#endif // FLOPPYSOUND

#ifndef M_PI
#define M_PI  3.14159265359
#endif

// ---- Flappy Fish ----
// A Flappy Bird-style game: click to flap, swim between scrolling pipes,
// don't hit the coral/wreck/rock/ruin. Fully mouse-driven, runs
// continuously like the other interactive games regardless of whether
// music is playing.
//
// All the per-theme obstacle art, sky, floor, and floor decoration live in
// floppyfish_reef.cpp / floppyfish_ship.cpp / floppyfish_cave.cpp /
// floppyfish_atlantis.cpp / floppyfish_rainbow.cpp / floppyfish_dino.cpp /
// floppyfish_antarctic.cpp / floppyfish_aquarium.cpp / floppyfish_galaxy.cpp
// (see floppyfish_common.h for the shared contract). This file owns
// everything theme-agnostic: game state, physics, collision, background
// critters, the player fish, and the UI.

#define FF_MAX_PIPES 8
#define FF_BG_FISH_COUNT 7

// Nine visual themes the run cycles through as the fish travels: coral
// reef, a sunken pirate ship, a dark cave, the ruins of Atlantis, a
// sky-high rainbow realm, a murky prehistoric bone-yard, the icy
// Antarctic, a bright glass aquarium tank, and outer space.
// s_ff_world_x is the total scroll distance covered so far (reset each run,
// paused unless actively playing) and picks which theme zone the camera is
// currently in. Zone length and the crossfade band between zones are both
// in units of vis->height, same as every other size in this file, so they
// scale with resolution consistently.
#define FF_ZONE_LEN_FRAC   18.0
#define FF_ZONE_TRANS_FRAC  4.0

static double s_ff_world_x = 0.0;

// Which theme each zone index uses, generated lazily (and randomly) the
// first time each index is reached rather than fixed in advance, then
// cached forever after so revisiting the same index is stable. Wraps after
// FF_ZONE_CACHE_MAX zones (~4 hours of continuous play at the default zone
// length) and just reuses the same random order from there rather than
// growing unboundedly.
#define FF_ZONE_CACHE_MAX 256
static int s_ff_zone_themes[FF_ZONE_CACHE_MAX];
static int s_ff_zone_cache_count = 0;

static int ff_max_theme_for_score(int score) {
    if (score < 40)  return FF_THEME_CAVE;       // 0–2
    if (score < 60)  return FF_THEME_ATLANTIS;   // 0–3
    if (score < 65)  return FF_THEME_DINO;       // 0–5
    if (score < 70) return FF_THEME_ANTARCTIC;  // 0–6
    if (score < 75) return FF_THEME_GALAXY;     // 0–8
    return FF_THEME_RAINBOW;                     // 0–9 (all)
}


static int ff_zone_theme(int idx) {
    int slot = ((idx % FF_ZONE_CACHE_MAX) + FF_ZONE_CACHE_MAX) % FF_ZONE_CACHE_MAX;
    while (s_ff_zone_cache_count <= slot) {
        int prev = (s_ff_zone_cache_count > 0) ? s_ff_zone_themes[s_ff_zone_cache_count - 1] : -1;
        int pick;
        do {
            pick = rand() % FF_THEME_COUNT;
        } while (pick == prev); // never the same theme twice in a row
        s_ff_zone_themes[s_ff_zone_cache_count] = pick;
        s_ff_zone_cache_count++;
    }
    return s_ff_zone_themes[slot];
}

// Works out which theme "zone" a world-scroll position falls in, the next
// theme it's approaching, and how far into the crossfade band toward it
// this position is (0 = fully theme_from, 1 = fully theme_to). Shared by
// the background crossfade in draw_floppy_fish and by ff_spawn_pipe (to
// decide what a newly-born obstacle looks like). The zone *order* is
// randomized (see ff_zone_theme) - only each zone's own length is fixed.
static void ff_theme_at(double world_x, double zone_len, double trans_len,
                         int *theme_from, int *theme_to, double *blend_t) {
    if (world_x < 0) world_x = 0;
    int idx = (int)floor(world_x / zone_len);
    double pos_in_zone = world_x - idx * zone_len;
    *theme_from = ff_zone_theme(idx);
    *theme_to = ff_zone_theme(idx + 1);
    *blend_t = (pos_in_zone > zone_len - trans_len)
                   ? (pos_in_zone - (zone_len - trans_len)) / trans_len
                   : 0.0;
}

typedef enum { FF_READY, FF_PLAYING, FF_GAME_OVER } FFState;

typedef struct {
    double x;
    double gap_center;
    bool scored;
    bool active;
    int theme; // which zone this obstacle was spawned in (FFTheme) - fixed for its lifetime
    // Pre-rendered top+bottom column art for this pipe. gap_center and theme
    // (the only inputs to ff_draw_obstacle_column besides x/y, which are
    // either fixed for the pipe's life or just a translation) never change
    // after spawn, so the art is rendered once here instead of redrawing the
    // coral lobes / beams / stalactites / columns from scratch every frame.
    cairo_surface_t *art_cache;
} FFPipe;

// A handful of friendly color schemes so the player's fish looks different
// each game: {highlight, mid, shadow} used for the body gradient, fin, and
// dorsal fin.
typedef struct {
    double top_r, top_g, top_b;
    double mid_r, mid_g, mid_b;
    double dark_r, dark_g, dark_b;
} FFFishPalette;

static const FFFishPalette FF_FISH_PALETTES[] = {
    {1.00, 0.75, 0.35,  0.98, 0.55, 0.15,  0.85, 0.30, 0.08}, // orange (classic)
    {0.55, 0.80, 1.00,  0.25, 0.60, 0.98,  0.10, 0.40, 0.80}, // blue
    {0.80, 0.55, 1.00,  0.60, 0.30, 0.98,  0.40, 0.12, 0.80}, // purple
    {0.60, 1.00, 0.55,  0.35, 0.85, 0.30,  0.15, 0.62, 0.15}, // green
    {1.00, 0.95, 0.40,  0.98, 0.80, 0.15,  0.85, 0.60, 0.08}, // yellow
    {1.00, 0.55, 0.80,  0.98, 0.30, 0.60,  0.80, 0.12, 0.48}, // pink
    {0.55, 1.00, 0.95,  0.20, 0.90, 0.82,  0.08, 0.65, 0.60}, // teal
};
#define FF_FISH_PALETTE_COUNT (int)(sizeof(FF_FISH_PALETTES) / sizeof(FF_FISH_PALETTES[0]))

static FFState s_ff_state = FF_READY;
static double s_ff_fish_y = 0.0;
static double s_ff_fish_vel = 0.0;
static double s_ff_rotation = 0.0;
static FFPipe s_ff_pipes[FF_MAX_PIPES];
static double s_ff_spawn_timer = 0.0;
static int s_ff_score = 0;
static int s_ff_best_score = 0;      // "today's" best - persisted, but expires at local midnight (see ff_load_daily_best)

// Score thresholds for the trinket shown on the Game Over screen - below
// sea shell is "no trinket", then each tier takes real, escalating skill.
// Underwater-flavored instead of generic sports medals: a beachcomber's
// shell, an oyster's pearl, a pirate's doubloon, and a full sunken-treasure
// chest for the very best runs.
#define FF_MEDAL_SHELL_SCORE     10
#define FF_MEDAL_PEARL_SCORE     15
#define FF_MEDAL_DOUBLOON_SCORE  30
#define FF_MEDAL_TREASURE_SCORE  40

static const char *ff_medal_for_score(int score) {
    if (score >= FF_MEDAL_TREASURE_SCORE) return "Sunken Treasure";
    if (score >= FF_MEDAL_DOUBLOON_SCORE) return "Gold Doubloon";
    if (score >= FF_MEDAL_PEARL_SCORE)    return "Pearl";
    if (score >= FF_MEDAL_SHELL_SCORE)    return "Sea Shell";
    return "None";
}

// A fan-ridged scallop shell, hinged at the bottom with wedges alternating
// two cream/pink shades for the ribbed texture, and a small hinge knob.
static void ff_draw_trinket_shell(cairo_t *cr, double cx, double cy, double radius) {
    double hinge_x = cx, hinge_y = cy + radius * 0.70;
    const int ridges = 7;
    double start_ang = -160.0 * M_PI / 180.0, end_ang = -20.0 * M_PI / 180.0;
    double px[ridges + 1], py[ridges + 1];
    for (int i = 0; i <= ridges; i++) {
        double ang = start_ang + (end_ang - start_ang) * i / ridges;
        px[i] = hinge_x + cos(ang) * radius * 1.15;
        py[i] = hinge_y + sin(ang) * radius * 1.15;
    }

    for (int i = 0; i < ridges; i++) {
        bool light = (i % 2) == 0;
        if (light) cairo_set_source_rgb(cr, 0.97, 0.90, 0.83);
        else cairo_set_source_rgb(cr, 0.90, 0.78, 0.70);
        cairo_move_to(cr, hinge_x, hinge_y);
        cairo_line_to(cr, px[i], py[i]);
        cairo_line_to(cr, px[i + 1], py[i + 1]);
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    // Ridge lines radiating from the hinge, plus the shell's outer rim.
    cairo_set_source_rgba(cr, 0.72, 0.48, 0.40, 0.8);
    cairo_set_line_width(cr, radius * 0.05);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    for (int i = 0; i <= ridges; i++) {
        cairo_move_to(cr, hinge_x, hinge_y);
        cairo_line_to(cr, px[i], py[i]);
        cairo_stroke(cr);
    }
    cairo_move_to(cr, px[0], py[0]);
    for (int i = 1; i <= ridges; i++) cairo_line_to(cr, px[i], py[i]);
    cairo_stroke(cr);

    // Hinge knob.
    cairo_set_source_rgb(cr, 0.85, 0.68, 0.60);
    cairo_arc(cr, hinge_x, hinge_y, radius * 0.14, 0, 2 * M_PI);
    cairo_fill(cr);
}

// An open clamshell cup holding a glossy pearl.
static void ff_draw_trinket_pearl(cairo_t *cr, double cx, double cy, double radius) {
    double cup_y = cy + radius * 0.30;
    cairo_set_source_rgb(cr, 0.72, 0.70, 0.76);
    cairo_save(cr);
    cairo_translate(cr, cx, cup_y);
    cairo_scale(cr, 1.0, 0.5);
    cairo_arc(cr, 0, 0, radius * 0.95, 0, M_PI);
    cairo_restore(cr);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.50, 0.48, 0.56, 0.7);
    cairo_set_line_width(cr, radius * 0.05);
    cairo_save(cr);
    cairo_translate(cr, cx, cup_y);
    cairo_scale(cr, 1.0, 0.5);
    cairo_arc(cr, 0, 0, radius * 0.95, 0, M_PI);
    cairo_restore(cr);
    cairo_stroke(cr);

    double pearl_cy = cy - radius * 0.08;
    cairo_pattern_t *pat = cairo_pattern_create_radial(
        cx - radius * 0.16, pearl_cy - radius * 0.22, radius * 0.05,
        cx, pearl_cy, radius * 0.58);
    cairo_pattern_add_color_stop_rgb(pat, 0.0, 1.00, 1.00, 1.00);
    cairo_pattern_add_color_stop_rgb(pat, 0.55, 0.93, 0.90, 0.90);
    cairo_pattern_add_color_stop_rgb(pat, 1.0, 0.74, 0.70, 0.76);
    cairo_set_source(cr, pat);
    cairo_arc(cr, cx, pearl_cy, radius * 0.58, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.7);
    cairo_arc(cr, cx - radius * 0.18, pearl_cy - radius * 0.22, radius * 0.12, 0, 2 * M_PI);
    cairo_fill(cr);
}

// A gold coin with a radial shine, a milled/reeded edge, and a simple
// stamped star at the center - a pirate's doubloon rather than a generic
// medal.
static void ff_draw_trinket_doubloon(cairo_t *cr, double cx, double cy, double radius) {
    cairo_pattern_t *pat = cairo_pattern_create_radial(
        cx - radius * 0.30, cy - radius * 0.35, radius * 0.10,
        cx, cy, radius);
    cairo_pattern_add_color_stop_rgb(pat, 0.0, 1.00, 0.97, 0.70);
    cairo_pattern_add_color_stop_rgb(pat, 0.55, 1.00, 0.84, 0.15);
    cairo_pattern_add_color_stop_rgb(pat, 1.0, 0.70, 0.50, 0.06);
    cairo_set_source(cr, pat);
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    // Reeded edge - short radial ticks around the rim.
    cairo_set_source_rgb(cr, 0.60, 0.42, 0.06);
    cairo_set_line_width(cr, fmax(1.0, radius * 0.06));
    int ticks = 24;
    for (int i = 0; i < ticks; i++) {
        double ang = 2 * M_PI * i / ticks;
        cairo_move_to(cr, cx + cos(ang) * radius * 0.90, cy + sin(ang) * radius * 0.90);
        cairo_line_to(cr, cx + cos(ang) * radius * 1.00, cy + sin(ang) * radius * 1.00);
        cairo_stroke(cr);
    }

    // Stamped star emblem.
    cairo_set_source_rgba(cr, 0.62, 0.44, 0.06, 0.85);
    int points = 5;
    double outer = radius * 0.42, inner = radius * 0.18;
    cairo_new_path(cr);
    for (int i = 0; i < points * 2; i++) {
        double ang = -M_PI / 2 + M_PI * i / points;
        double rr = (i % 2 == 0) ? outer : inner;
        double x = cx + cos(ang) * rr, y = cy + sin(ang) * rr;
        if (i == 0) cairo_move_to(cr, x, y); else cairo_line_to(cr, x, y);
    }
    cairo_close_path(cr);
    cairo_fill(cr);
}

// A small treasure chest with a domed lid, gold trim/lock, and a little
// gold and gems spilling out the top - the top-tier trinket.
static void ff_draw_trinket_chest(cairo_t *cr, double cx, double cy, double radius) {
    double w = radius * 1.85, h = radius * 0.95;
    double body_y0 = cy + radius * 0.05;
    double x0 = cx - w * 0.5, x1 = cx + w * 0.5;

    // Body.
    cairo_set_source_rgb(cr, 0.36, 0.23, 0.11);
    cairo_rectangle(cr, x0, body_y0, w, h);
    cairo_fill(cr);

    // Domed lid.
    cairo_set_source_rgb(cr, 0.44, 0.28, 0.14);
    cairo_move_to(cr, x0, body_y0);
    cairo_curve_to(cr, x0, body_y0 - h * 0.62, x1, body_y0 - h * 0.62, x1, body_y0);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Gold trim bands and lock.
    cairo_set_source_rgb(cr, 0.85, 0.68, 0.25);
    cairo_set_line_width(cr, fmax(1.0, radius * 0.09));
    cairo_move_to(cr, x0, body_y0);
    cairo_line_to(cr, x1, body_y0);
    cairo_stroke(cr);
    cairo_rectangle(cr, cx - w * 0.06, body_y0, w * 0.12, h * 0.9);
    cairo_fill(cr);
    cairo_rectangle(cr, cx - radius * 0.14, body_y0 - radius * 0.04, radius * 0.28, radius * 0.24);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_rectangle(cr, x0, body_y0, w, h);
    cairo_stroke(cr);

    // Gold coins and gems spilling out of the open lid.
    static const double gem_r[3] = {0.95, 0.55, 0.35};
    static const double gem_g[3] = {0.85, 0.90, 0.75};
    static const double gem_b[3] = {0.25, 1.00, 0.95};
    for (int i = 0; i < 5; i++) {
        double gx = cx + (i - 2) * radius * 0.24;
        double gy = body_y0 - radius * (0.30 + 0.18 * (i % 2));
        double gr = radius * 0.13;
        const double *col_r = &gem_r[i % 3], *col_g = &gem_g[i % 3], *col_b = &gem_b[i % 3];
        cairo_set_source_rgb(cr, *col_r, *col_g, *col_b);
        cairo_arc(cr, gx, gy, gr, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.6);
        cairo_arc(cr, gx - gr * 0.3, gy - gr * 0.3, gr * 0.3, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// Dispatches to the right trinket shape for the tier - the one place that
// needs to know all four exist.
static void ff_draw_medal(cairo_t *cr, double cx, double cy, double radius, const char *medal) {
    if (strcmp(medal, "Sunken Treasure") == 0) ff_draw_trinket_chest(cr, cx, cy, radius);
    else if (strcmp(medal, "Gold Doubloon") == 0) ff_draw_trinket_doubloon(cr, cx, cy, radius);
    else if (strcmp(medal, "Pearl") == 0) ff_draw_trinket_pearl(cr, cx, cy, radius);
    else ff_draw_trinket_shell(cr, cx, cy, radius); // Sea Shell
}

// All-time high-score persistence: standalone-game only (zenamp's
// visualizer build doesn't want a score file on disk, just the daily best
// above), so all of it - state, path resolution, load, save - is gated on
// FLOPPYSOUND same as the rest of the standalone-only code below.
#ifdef FLOPPYSOUND
static int s_ff_alltime_best = 0;    // persisted across runs, loaded from disk on startup

// Where the persistent high score lives. On Windows this is
// %APPDATA%\FloppyFish\highscore.txt (the standard per-user app-data
// location); everywhere else it lives in ~/.floppyfish/ (created if it
// doesn't exist yet) so the same code still works for local/dev builds.
static void ff_highscore_path(char *buf, size_t bufsize) {
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (!appdata) appdata = ".";
    char dir[400];
    snprintf(dir, sizeof(dir), "%s\\FloppyFish", appdata);
    FF_MKDIR(dir);
    snprintf(buf, bufsize, "%s\\highscore.txt", dir);
#else
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char dir[400];
    snprintf(dir, sizeof(dir), "%s/.floppyfish", home);
    FF_MKDIR(dir);
    snprintf(buf, bufsize, "%s/floppyfish_highscore", dir);
#endif
}

static void ff_load_alltime_best(void) {
    char path[512];
    ff_highscore_path(path, sizeof(path));
    s_ff_alltime_best = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%d", &s_ff_alltime_best) != 1) s_ff_alltime_best = 0;
        fclose(f);
    }
    printf("All-time high score: %d\n", s_ff_alltime_best);
}

static void ff_save_alltime_best(void) {
    char path[512];
    ff_highscore_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d\n", s_ff_alltime_best);
        fclose(f);
    }
}

// Today's best also persists now, alongside the all-time one, but expires
// at midnight instead of lasting forever. The save file stores the score
// together with a timestamp for midnight (00:00:00 local time) of the day
// it was set; loading compares that stamp against *today's* midnight, and
// only restores the score if they match. If the file is from any earlier
// day the stamp won't match and the daily best just stays at the 0 it was
// already initialized to - no separate "is it stale" flag needed, the
// timestamp comparison is the staleness check.
static void ff_daily_highscore_path(char *buf, size_t bufsize) {
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (!appdata) appdata = ".";
    char dir[400];
    snprintf(dir, sizeof(dir), "%s\\FloppyFish", appdata);
    FF_MKDIR(dir);
    snprintf(buf, bufsize, "%s\\dailyhighscore.txt", dir);
#else
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char dir[400];
    snprintf(dir, sizeof(dir), "%s/.floppyfish", home);
    FF_MKDIR(dir);
    snprintf(buf, bufsize, "%s/floppyfish_daily_highscore", dir);
#endif
}

// Midnight (00:00:00 local time) of "today". Used both to stamp a freshly
// saved daily best and, on load, to check whether a previously saved one
// still applies.
static time_t ff_today_midnight(void) {
    time_t now = time(NULL);
    struct tm tmnow;
#ifdef _WIN32
    localtime_s(&tmnow, &now);
#else
    localtime_r(&now, &tmnow);
#endif
    tmnow.tm_hour = 0;
    tmnow.tm_min = 0;
    tmnow.tm_sec = 0;
    tmnow.tm_isdst = -1; // let mktime work out DST for this date on its own
    return mktime(&tmnow);
}

static void ff_load_daily_best(void) {
    char path[512];
    ff_daily_highscore_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    long saved_midnight = 0;
    int saved_score = 0;
    bool ok = (fscanf(f, "%ld %d", &saved_midnight, &saved_score) == 2);
    fclose(f);
    if (!ok) return;

    if ((time_t)saved_midnight == ff_today_midnight()) {
        s_ff_best_score = saved_score;
        printf("Today's high score (loaded): %d\n", s_ff_best_score);
    } else {
        printf("Saved daily high score is from a previous day - resetting to 0\n");
    }
}

static void ff_save_daily_best(void) {
    char path[512];
    ff_daily_highscore_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%ld %d\n", (long)ff_today_midnight(), s_ff_best_score);
        fclose(f);
    }
}
#endif // FLOPPYSOUND
static double s_ff_bubble_phase = 0.0;
static double s_ff_flap_anim = 0.0;  // tail-flap animation clock, ticks while playing
static double s_ff_blink_timer = 1.5;     // counts down to the next blink, reseeded randomly each time
static double s_ff_blink_progress = 0.0;  // 0 = eye open, 1 = eye fully shut, drives the blink itself
static bool s_ff_blink_closing = false;   // true while mid-blink (closing then opening)
static int s_ff_fish_palette = 0;    // random new color friend each game

// Cached toy font face for the UI text - created once on first use, freed
// explicitly in shutdown_floppy_fish_system() so it doesn't show up as a
// leak at process exit (see draw_floppy_fish).
static cairo_font_face_t *s_ff_font_face = NULL;

// Per-theme cached renders of the parts of the sky/floor that never change
// frame to frame (gradient + skyline backdrop; floor base fill) - see
// ff_ensure_theme_caches. Everything that actually animates (bubbles, sand
// ripples, etc.) still gets drawn live on top of these every frame; only
// the expensive full-canvas painting gets reused instead of redone.
static cairo_surface_t *s_ff_sky_cache[FF_THEME_COUNT] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static cairo_surface_t *s_ff_floor_cache[FF_THEME_COUNT] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static double s_ff_cache_w = -1.0, s_ff_cache_h = -1.0; // canvas size the caches above were built for
static void ff_free_theme_caches(); // defined near draw_floppy_fish, used by shutdown below

// Background critters: small fish darting by at their own pace, plus an
// octopus, purely decorative and drawn behind the pipes so they never read
// as obstacles (and so they visibly vanish behind a pipe as they cross it).
static double s_ff_bgfish_x[FF_BG_FISH_COUNT];
static double s_ff_bgfish_y[FF_BG_FISH_COUNT];
static double s_ff_bgfish_vy[FF_BG_FISH_COUNT];
static double s_ff_bgfish_speed[FF_BG_FISH_COUNT];
static int s_ff_bgfish_dir[FF_BG_FISH_COUNT];  // +1 = swims right, -1 = swims left
static double s_ff_bgfish_scale[FF_BG_FISH_COUNT];
static int s_ff_bgfish_palette[FF_BG_FISH_COUNT];
static bool s_ff_bg_init = false;

static double s_ff_octopus_x = 0.0;
static double s_ff_octopus_y = 0.0;
static double s_ff_octopus_speed = 0.0;
static int s_ff_octopus_dir = -1;

// Seaweed patches drift by with the rest of the floor scenery (slower than
// the pipes) and are recycled off the left edge with fresh height/spacing.
// Each patch moves independently, but spawn placement (initial and on
// recycle) picks the most spread-out of a few random candidates so patches
// don't clump together by chance and leave the rest of the floor bare.
#define FF_SEAWEED_COUNT 6
static double s_ff_seaweed_x[FF_SEAWEED_COUNT];
static double s_ff_seaweed_height_frac[FF_SEAWEED_COUNT];
static double s_ff_seaweed_phase[FF_SEAWEED_COUNT];

// A shark that only occasionally cruises through, far behind everything
// else - not a constant presence like the octopus/small fish. When absent,
// s_ff_shark_active is false and s_ff_shark_wait counts down to its next
// appearance.
static bool s_ff_shark_active = false;
static double s_ff_shark_wait = 0.0;
static double s_ff_shark_x = 0.0;
static double s_ff_shark_y = 0.0;
static double s_ff_shark_speed = 0.0;
static int s_ff_shark_dir = -1;
static double s_ff_shark_scale = 1.0; // most passes are medium-sized; occasionally much larger

// Another rare guest, same cadence as the shark but themed to match
// whatever's currently on screen: a mermaid drifts through during the
// Atlantis zones, a unicorn during the rainbow zones, a mosasaurus during
// the bone-yard zones, a group of penguins during the Antarctic zones, a
// pair of lobsters scuttling along the gravel during the aquarium zones,
// a pair of astronauts drifting on tethers during the galaxy zones, a
// diver everywhere else (reef/ship/cave). Which one it is gets decided
// once, at spawn time (see ff_spawn_guest), and holds for that guest's
// whole pass across the tank.
typedef enum {
    FF_GUEST_DIVER = 0, FF_GUEST_MERMAID = 1, FF_GUEST_UNICORN = 2,
    FF_GUEST_MOSASAURUS = 3, FF_GUEST_PENGUINS = 4, FF_GUEST_LOBSTERS = 5,
    FF_GUEST_ASTRONAUTS = 6
} FFGuestKind;

static bool s_ff_guest_active = false;
static double s_ff_guest_wait = 0.0;
static double s_ff_guest_x = 0.0;
static double s_ff_guest_y = 0.0;
static double s_ff_guest_speed = 0.0;
static int s_ff_guest_dir = -1;
static FFGuestKind s_ff_guest_kind = FF_GUEST_DIVER;

static const double FF_FISH_X_FRAC = 0.30;

static void ff_reset(Visualizer *vis) {
    // Only the player's own state resets here - background fish and the
    // octopus keep swimming on their own independent cycles and are
    // untouched by the player dying or restarting.
    s_ff_state = FF_READY;
    s_ff_fish_y = vis->height * 0.45;
    s_ff_fish_vel = 0.0;
    s_ff_rotation = 0.0;
    s_ff_score = 0;
    s_ff_spawn_timer = 0.0;
    s_ff_world_x = 0.0;
    s_ff_zone_cache_count = 0;
    s_ff_fish_palette = rand() % FF_FISH_PALETTE_COUNT;
    for (int i = 0; i < FF_MAX_PIPES; i++) s_ff_pipes[i].active = false;
}

// (Re)launches one background fish from off-screen with a random direction,
// speed, and vertical drift - independent of the player fish entirely.
static void ff_spawn_bg_fish(Visualizer *vis, int i) {
    int dir = (rand() % 2 == 0) ? 1 : -1;
    s_ff_bgfish_dir[i] = dir;
    s_ff_bgfish_speed[i] = vis->height * (0.45 + 0.85 * ((double)rand() / RAND_MAX));
    s_ff_bgfish_vy[i] = vis->height * (-0.06 + 0.12 * ((double)rand() / RAND_MAX));
    s_ff_bgfish_y[i] = vis->height * 0.10 + (double)rand() / RAND_MAX * vis->height * 0.58;
    s_ff_bgfish_scale[i] = 0.30 + 0.35 * ((double)rand() / RAND_MAX);
    s_ff_bgfish_palette[i] = rand() % FF_FISH_PALETTE_COUNT;
    if (dir < 0) {
        s_ff_bgfish_x[i] = vis->width * (1.05 + 0.3 * ((double)rand() / RAND_MAX));
    } else {
        s_ff_bgfish_x[i] = -vis->width * (0.05 + 0.3 * ((double)rand() / RAND_MAX));
    }
}

// (Re)launches the octopus from off-screen - entirely separate cycle from
// the small background fish and from the player fish, so it never resets
// or freezes just because the player died or restarted.
static void ff_spawn_octopus(Visualizer *vis) {
    int dir = (rand() % 2 == 0) ? 1 : -1;
    s_ff_octopus_dir = dir;
    s_ff_octopus_speed = vis->height * (0.07 + 0.10 * ((double)rand() / RAND_MAX));
    s_ff_octopus_y = vis->height * (0.42 + 0.28 * ((double)rand() / RAND_MAX));
    if (dir < 0) {
        s_ff_octopus_x = vis->width * (1.05 + 0.25 * ((double)rand() / RAND_MAX));
    } else {
        s_ff_octopus_x = -vis->width * (0.05 + 0.25 * ((double)rand() / RAND_MAX));
    }
}

// Sends the shark gliding across from one side to the other, well behind
// the pipes and bigger/slower than the small background fish.
static void ff_spawn_shark(Visualizer *vis) {
    int dir = (rand() % 2 == 0) ? 1 : -1;
    s_ff_shark_dir = dir;
    s_ff_shark_speed = vis->height * (0.12 + 0.08 * ((double)rand() / RAND_MAX));
    s_ff_shark_y = vis->height * (0.15 + 0.35 * ((double)rand() / RAND_MAX));

    // Size varies pass to pass: cubing a uniform random value skews most
    // rolls toward the low end of the range (medium), while still letting
    // an occasional roll land way out near the top (very large) - "once in
    // a while, a much bigger shark".
    double roll = (double)rand() / RAND_MAX;
    s_ff_shark_scale = 0.85 + 1.75 * (roll * roll * roll);

    // Off-screen spawn margin scales with size too, so a very large shark
    // doesn't visibly pop in/out at the screen edge.
    double margin = 0.10 * s_ff_shark_scale;
    if (dir < 0) {
        s_ff_shark_x = vis->width * (1.10 + margin + 0.2 * ((double)rand() / RAND_MAX));
    } else {
        s_ff_shark_x = -vis->width * (0.10 + margin + 0.2 * ((double)rand() / RAND_MAX));
    }
    s_ff_shark_active = true;
}

// Sends the mermaid/diver/unicorn/mosasaurus/penguins/lobsters/astronauts
// guest across, same way as the shark. Which silhouette it is gets picked
// from whichever theme currently dominates the screen (s_ff_world_x, same
// lookup ff_spawn_pipe uses for obstacles), so a diver never turns up over
// the Atlantis ruins, the mermaid never turns up over a coral reef, the
// unicorn only ever turns up over the rainbow realm, the mosasaurus only
// over the bone yard, the penguins only over the Antarctic, the lobsters
// only over the aquarium gravel, and the astronauts only out in the
// galaxy.
static void ff_spawn_guest(Visualizer *vis) {
    int dir = (rand() % 2 == 0) ? 1 : -1;
    s_ff_guest_dir = dir;
    if (dir < 0) {
        s_ff_guest_x = vis->width * (1.08 + 0.2 * ((double)rand() / RAND_MAX));
    } else {
        s_ff_guest_x = -vis->width * (0.08 + 0.2 * ((double)rand() / RAND_MAX));
    }

    double zone_len = vis->height * FF_ZONE_LEN_FRAC;
    double trans_len = vis->height * FF_ZONE_TRANS_FRAC;
    int tf, tt; double bt;
    ff_theme_at(s_ff_world_x, zone_len, trans_len, &tf, &tt, &bt);
    int dominant = (bt > 0.5) ? tt : tf;
    s_ff_guest_kind = (dominant == FF_THEME_ATLANTIS) ? FF_GUEST_MERMAID
                     : (dominant == FF_THEME_RAINBOW)  ? FF_GUEST_UNICORN
                     : (dominant == FF_THEME_DINO)      ? FF_GUEST_MOSASAURUS
                     : (dominant == FF_THEME_ANTARCTIC) ? FF_GUEST_PENGUINS
                     : (dominant == FF_THEME_AQUARIUM)  ? FF_GUEST_LOBSTERS
                     : (dominant == FF_THEME_GALAXY)    ? FF_GUEST_ASTRONAUTS
                                                        : FF_GUEST_DIVER;


    // Lobsters scuttle along the gravel rather than drifting mid-tank like
    // every other guest, so they get their own slower speed and a y pinned
    // just above the floor line instead of the usual random spread.
    if (s_ff_guest_kind == FF_GUEST_LOBSTERS) {
        s_ff_guest_speed = vis->height * (0.045 + 0.03 * ((double)rand() / RAND_MAX));
        double floor_h = vis->height * 0.10;
        s_ff_guest_y = vis->height - floor_h - vis->height * (0.01 + 0.02 * ((double)rand() / RAND_MAX));
    } else {
        s_ff_guest_speed = vis->height * (0.09 + 0.09 * ((double)rand() / RAND_MAX));
        s_ff_guest_y = vis->height * (0.12 + 0.50 * ((double)rand() / RAND_MAX));
    }

    s_ff_guest_active = true;
}

// Places one seaweed patch (used both for the initial scatter and for
// recycling once a patch drifts off the left edge). Tries a handful of
// random candidate x positions and keeps whichever is farthest from every
// other current patch, so placement stays random-feeling but self-avoiding
// - no more than a couple of tries usually needed to land somewhere that
// isn't crowding an existing patch.
static void ff_spawn_seaweed(Visualizer *vis, int i, bool initial) {
    double best_x = 0.0;
    double best_min_dist = -1.0;
    for (int attempt = 0; attempt < 8; attempt++) {
        double cand;
        if (initial) {
            cand = ((double)rand() / RAND_MAX) * vis->width;
        } else {
            cand = vis->width * (1.05 + 0.45 * ((double)rand() / RAND_MAX));
        }
        double min_dist = 1e18;
        for (int j = 0; j < FF_SEAWEED_COUNT; j++) {
            if (j == i) continue;
            double d = fabs(cand - s_ff_seaweed_x[j]);
            if (d < min_dist) min_dist = d;
        }
        if (min_dist > best_min_dist) {
            best_min_dist = min_dist;
            best_x = cand;
        }
    }
    s_ff_seaweed_x[i] = best_x;
    s_ff_seaweed_height_frac[i] = 0.55 + 0.45 * ((double)rand() / RAND_MAX);
    s_ff_seaweed_phase[i] = ((double)rand() / RAND_MAX) * 6.28;
}

static void ff_init_background(Visualizer *vis) {
    if (s_ff_bg_init) return;
    // Called once at app startup before the canvas is sized, so width/height
    // can still be 0 - wait for real dimensions instead of latching in
    // zeroed-out positions and speeds that would freeze everything at (0,0).
    if (vis->width <= 0 || vis->height <= 0) return;
    for (int i = 0; i < FF_BG_FISH_COUNT; i++) {
        ff_spawn_bg_fish(vis, i);
        // Scatter the initial batch across the visible width instead of all
        // starting off-screen, so the tank doesn't look empty on launch.
        s_ff_bgfish_x[i] = (double)rand() / RAND_MAX * vis->width;
    }
    ff_spawn_octopus(vis);
    s_ff_octopus_x = (double)rand() / RAND_MAX * vis->width;
    // Shark stays off-screen for a while after launch before its first pass.
    s_ff_shark_active = false;
    s_ff_shark_wait = 12.0 + 15.0 * ((double)rand() / RAND_MAX);
    // Same for the mermaid/diver/unicorn/mosasaurus/penguins/lobsters/
    // astronauts guest, on its own independent cadence.
    s_ff_guest_active = false;
    s_ff_guest_wait = 10.0 + 14.0 * ((double)rand() / RAND_MAX);
    // Place patches one at a time so each new one can avoid the ones
    // already placed before it.
    for (int i = 0; i < FF_SEAWEED_COUNT; i++) s_ff_seaweed_x[i] = -1e9;
    for (int i = 0; i < FF_SEAWEED_COUNT; i++) ff_spawn_seaweed(vis, i, true);
    s_ff_bg_init = true;
}

void init_floppy_fish_system(Visualizer *vis) {
    s_ff_best_score = 0;
#ifdef FLOPPYSOUND
    ff_load_alltime_best();
    ff_load_daily_best(); // overrides s_ff_best_score above if today's save is still valid
#endif
    s_ff_bg_init = false;
    ff_reset(vis);
    ff_init_background(vis);
#ifdef FLOPPYSOUND
    vis->sound_flap=false;
    vis->sound_score=false;
    vis->sound_dead=false;
#endif
}

// Frees process-lifetime cairo resources cached by this file (currently
// just the UI toy font face - see draw_floppy_fish). Call once at real
// program shutdown, after the last draw_floppy_fish() call, so ASan/LSan
// don't flag it as an unreachable-at-exit leak.
void shutdown_floppy_fish_system() {
    if (s_ff_font_face) {
        cairo_font_face_destroy(s_ff_font_face);
        s_ff_font_face = NULL;
    }
    ff_free_theme_caches();
    s_ff_cache_w = -1.0;
    s_ff_cache_h = -1.0;
    for (int i = 0; i < FF_MAX_PIPES; i++) {
        if (s_ff_pipes[i].art_cache) {
            cairo_surface_destroy(s_ff_pipes[i].art_cache);
            s_ff_pipes[i].art_cache = NULL;
        }
    }
}

static void ff_flap(Visualizer *vis) {
    s_ff_fish_vel = -vis->height * 0.62;
#ifdef FLOPPYSOUND
    vis->sound_flap = true;
#endif
}

// Renders both columns of a pipe (top + bottom, given its fixed gap_center
// and theme) into a fresh offscreen surface sized to the canvas height and
// one pipe's width, at local x=0. Called once per pipe at spawn time; the
// draw loop just blits+translates this every frame instead of re-running
// the coral/beam/stalactite/column art generator per frame.
static cairo_surface_t *ff_build_pipe_art_cache(double vis_h, double pipe_width,
                                                 double gap_center, int theme) {
    cairo_surface_t *surf = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, (int)ceil(pipe_width), (int)ceil(vis_h));
    cairo_t *cr = cairo_create(surf);
    double floor_h = vis_h * 0.10;
    double gap = vis_h * 0.24;
    ff_draw_obstacle_column(theme, cr, 0, 0, gap_center - gap * 0.5, pipe_width, gap_center, true);
    ff_draw_obstacle_column(theme, cr, 0, gap_center + gap * 0.5, vis_h - floor_h, pipe_width, gap_center, false);
    cairo_destroy(cr);
    return surf;
}

static void ff_spawn_pipe(Visualizer *vis) {
    for (int i = 0; i < FF_MAX_PIPES; i++) {
        if (s_ff_pipes[i].active) continue;
        double floor_h = vis->height * 0.10;
        double gap = vis->height * 0.24;
        double margin = vis->height * 0.08;
        double lo = margin + gap * 0.5;
        double hi = vis->height - floor_h - margin - gap * 0.5;
        if (hi < lo) hi = lo;
        double gap_center = lo + (double)rand() / RAND_MAX * (hi - lo);

        // Theme is decided by where this obstacle spawns in world-space
        // (the right edge, i.e. how far the camera has traveled plus one
        // screen width ahead) so it matches whatever zone it scrolls into
        // view from, and then never changes for the rest of its lifetime.
        double zone_len = vis->height * FF_ZONE_LEN_FRAC;
        double trans_len = vis->height * FF_ZONE_TRANS_FRAC;
        int tf, tt; double bt;
        ff_theme_at(s_ff_world_x + vis->width, zone_len, trans_len, &tf, &tt, &bt);

        double pipe_width = vis->height * 0.16;

        s_ff_pipes[i].x = vis->width;
        s_ff_pipes[i].gap_center = gap_center;
        s_ff_pipes[i].scored = false;
        s_ff_pipes[i].theme = (bt > 0.5) ? tt : tf;
        s_ff_pipes[i].active = true;

        // This slot's previous occupant (if any) had a different
        // gap_center/theme, so its cached art is stale - rebuild it.
        if (s_ff_pipes[i].art_cache) {
            cairo_surface_destroy(s_ff_pipes[i].art_cache);
        }
        s_ff_pipes[i].art_cache = ff_build_pipe_art_cache(
            vis->height, pipe_width, gap_center, s_ff_pipes[i].theme);
        return;
    }
}

// --- Small geometry helpers for triangular (cave-theme) hitboxes -----------
// The cave stalactites/stalagmites are drawn as a jagged taper from full
// width at the rooted end down to a point at the gap-facing tip (see
// ff_draw_cave_column), so a plain axis-aligned rect hitbox looks noticeably
// looser than the art there. These give the cave theme a triangular hitbox
// - the un-jittered envelope of that taper - so collisions there track the
// visual shape instead.

// Squared distance from point P to segment AB.
static double ff_point_seg_dist2(double px, double py, double ax, double ay, double bx, double by) {
    double vx = bx - ax, vy = by - ay;
    double wx = px - ax, wy = py - ay;
    double len2 = vx * vx + vy * vy;
    double t = (len2 > 1e-9) ? (vx * wx + vy * wy) / len2 : 0.0;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    double cx = ax + t * vx, cy = ay + t * vy;
    double dx = px - cx, dy = py - cy;
    return dx * dx + dy * dy;
}

static bool ff_point_in_triangle(double px, double py, double ax, double ay,
                                  double bx, double by, double cx, double cy) {
    double d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
    double d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
    double d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

// Squared distance from a point to a filled triangle (0 if the point is
// inside it).
static double ff_point_triangle_dist2(double px, double py,
                                       double ax, double ay, double bx, double by,
                                       double cx, double cy) {
    if (ff_point_in_triangle(px, py, ax, ay, bx, by, cx, cy)) return 0.0;
    double d1 = ff_point_seg_dist2(px, py, ax, ay, bx, by);
    double d2 = ff_point_seg_dist2(px, py, bx, by, cx, cy);
    double d3 = ff_point_seg_dist2(px, py, cx, cy, ax, ay);
    return fmin(d1, fmin(d2, d3));
}

void update_floppy_fish(Visualizer *vis, double dt) {
    if (vis->width <= 0 || vis->height <= 0) return;

    // One-shot per-frame event flags for the host to pick up right after
    // this call returns - reset here so each flap/score is a single pulse.
#ifdef FLOPPYSOUND
    vis->sound_flap = false;
    vis->sound_score = false;
    vis->sound_dead = false;  
#endif
    ff_init_background(vis);
    s_ff_bubble_phase += dt;

    // Occasional blink, independent of game state so it keeps happening on
    // the ready screen and after game-over too.
    const double FF_BLINK_SPEED = 12.0; // full close+open cycle takes ~1/6s
    if (s_ff_blink_closing) {
        s_ff_blink_progress += dt * FF_BLINK_SPEED;
        if (s_ff_blink_progress >= 1.0) {
            s_ff_blink_progress = 1.0;
            s_ff_blink_closing = false; // now opening back up
        }
    } else if (s_ff_blink_progress > 0.0) {
        s_ff_blink_progress -= dt * FF_BLINK_SPEED;
        if (s_ff_blink_progress <= 0.0) s_ff_blink_progress = 0.0;
    } else {
        s_ff_blink_timer -= dt;
        if (s_ff_blink_timer <= 0.0) {
            s_ff_blink_closing = true;
            s_ff_blink_timer = 2.0 + (rand() % 1000) / 1000.0 * 3.0; // next blink in 2-5s
        }
    }

    // Background critters dart around independently of the player fish.
    double band_lo = vis->height * 0.08, band_hi = vis->height * 0.72;
    for (int i = 0; i < FF_BG_FISH_COUNT; i++) {
        s_ff_bgfish_x[i] += s_ff_bgfish_dir[i] * s_ff_bgfish_speed[i] * dt;
        s_ff_bgfish_y[i] += s_ff_bgfish_vy[i] * dt;

        // Gentle bounce so the vertical drift doesn't wander off the top/bottom.
        if (s_ff_bgfish_y[i] < band_lo) { s_ff_bgfish_y[i] = band_lo; s_ff_bgfish_vy[i] *= -1; }
        if (s_ff_bgfish_y[i] > band_hi) { s_ff_bgfish_y[i] = band_hi; s_ff_bgfish_vy[i] *= -1; }

        bool off_left  = s_ff_bgfish_dir[i] < 0 && s_ff_bgfish_x[i] < -vis->width * 0.12;
        bool off_right = s_ff_bgfish_dir[i] > 0 && s_ff_bgfish_x[i] > vis->width * 1.12;
        if (off_left || off_right) {
            ff_spawn_bg_fish(vis, i);
        }
    }
    s_ff_octopus_x += s_ff_octopus_dir * s_ff_octopus_speed * dt;
    bool oct_off_left  = s_ff_octopus_dir < 0 && s_ff_octopus_x < -vis->width * 0.15;
    bool oct_off_right = s_ff_octopus_dir > 0 && s_ff_octopus_x > vis->width * 1.15;
    if (oct_off_left || oct_off_right) {
        ff_spawn_octopus(vis);
    }

    // Shark: rare guest, not a constant fixture. Counts down while absent,
    // then does one slow pass across the tank before disappearing again.
    if (s_ff_shark_active) {
        s_ff_shark_x += s_ff_shark_dir * s_ff_shark_speed * dt;
        bool shark_off_left  = s_ff_shark_dir < 0 && s_ff_shark_x < -vis->width * 0.20;
        bool shark_off_right = s_ff_shark_dir > 0 && s_ff_shark_x > vis->width * 1.20;
        if (shark_off_left || shark_off_right) {
            s_ff_shark_active = false;
            s_ff_shark_wait = 18.0 + 22.0 * ((double)rand() / RAND_MAX);
        }
    } else {
        s_ff_shark_wait -= dt;
        if (s_ff_shark_wait <= 0.0) {
            ff_spawn_shark(vis);
        }
    }

    // Mermaid/diver/unicorn/mosasaurus/penguins/lobsters/astronauts guest:
    // same rare-pass pattern as the shark, independent timer so the two
    // don't line up.
    if (s_ff_guest_active) {
        s_ff_guest_x += s_ff_guest_dir * s_ff_guest_speed * dt;
        bool guest_off_left  = s_ff_guest_dir < 0 && s_ff_guest_x < -vis->width * 0.20;
        bool guest_off_right = s_ff_guest_dir > 0 && s_ff_guest_x > vis->width * 1.20;
        if (guest_off_left || guest_off_right) {
            s_ff_guest_active = false;
            s_ff_guest_wait = 16.0 + 20.0 * ((double)rand() / RAND_MAX);
        }
    } else {
        s_ff_guest_wait -= dt;
        if (s_ff_guest_wait <= 0.0) {
            ff_spawn_guest(vis);
        }
    }

    // Seaweed drifts left with the floor at a slow, constant ambient pace
    // (not gated on playing, like the bubbles/sand) and is recycled off the
    // right edge once it scrolls past the left side.
    double seaweed_speed = vis->height * 0.05;
    for (int i = 0; i < FF_SEAWEED_COUNT; i++) {
        s_ff_seaweed_x[i] -= seaweed_speed * dt;
        if (s_ff_seaweed_x[i] < -vis->width * 0.08) {
            ff_spawn_seaweed(vis, i, false);
        }
    }

    double pipe_width = vis->height * 0.16;
    double pipe_speed = vis->height * 0.34;
    double floor_h = vis->height * 0.10;
    double fish_radius = vis->height * 0.032;
    double fish_x = vis->width * FF_FISH_X_FRAC;

    // Click handling: flap, start, or restart depending on game state.
    if (vis->mouse_left_pressed || vis->mouse_right_pressed) {
        if (s_ff_state == FF_READY) {
            s_ff_state = FF_PLAYING;
            ff_flap(vis);
        } else if (s_ff_state == FF_PLAYING) {
            ff_flap(vis);
        } else { // FF_GAME_OVER
            ff_reset(vis);
            s_ff_state = FF_PLAYING;
            ff_flap(vis);
        }
        vis->mouse_left_pressed = FALSE;
    }

    if (s_ff_state == FF_READY) {
        // Gentle idle bob so it doesn't look frozen while waiting for a click.
        s_ff_fish_y = vis->height * 0.45 + sin(vis->time_offset * 2.0) * vis->height * 0.02;
        s_ff_rotation = sin(vis->time_offset * 2.0) * 0.08;
        return;
    }

    double gravity = vis->height * 2.1;
    bool playing = (s_ff_state == FF_PLAYING);

    s_ff_flap_anim += dt * (playing ? 1.0 : 0.4);

    // Middle mouse button held: swim normally - straight and level, no
    // gravity pulling the fish down and no flap impulse pushing it up.
    // Purely a control-mode override (still subject to the usual pipe/
    // floor/ceiling collisions below), not an invincibility cheat.
    bool straight_swim = playing && vis->mouse_middle_pressed && !vis->mouse_right_pressed;
    vis->mouse_right_pressed = FALSE;

    if (straight_swim) {
        s_ff_fish_vel = 0.0;
        s_ff_rotation = 0.0;
    } else {
        s_ff_fish_vel += gravity * dt;
        s_ff_fish_y += s_ff_fish_vel * dt;

        double max_up_speed = vis->height * 0.62;
        double tilt = s_ff_fish_vel / max_up_speed;
        if (tilt < -1.0) tilt = -1.0;
        if (tilt > 1.4) tilt = 1.4;
        s_ff_rotation = tilt * 0.55;
    }

    if (playing) {
        // World-scroll distance drives which theme zone we're in - paused
        // whenever the run isn't actively progressing (ready/game-over),
        // same as the pipes themselves.
        s_ff_world_x += pipe_speed * dt;

        // Scroll and recycle pipes.
        s_ff_spawn_timer -= dt;
        if (s_ff_spawn_timer <= 0.0) {
            ff_spawn_pipe(vis);
            double spacing = vis->width * 0.42;  // tighter, closer to the original game
            s_ff_spawn_timer = spacing / pipe_speed;
        }

        for (int i = 0; i < FF_MAX_PIPES; i++) {
            if (!s_ff_pipes[i].active) continue;
            s_ff_pipes[i].x -= pipe_speed * dt;
            if (s_ff_pipes[i].x + pipe_width < 0) {
                s_ff_pipes[i].active = false;
                continue;
            }

            if (!s_ff_pipes[i].scored && s_ff_pipes[i].x + pipe_width < fish_x) {
                s_ff_pipes[i].scored = true;
                s_ff_score++;
                if (s_ff_score > s_ff_best_score) {
                    s_ff_best_score = s_ff_score;
                    printf("New best score today: %d\n", s_ff_best_score);
#ifdef FLOPPYSOUND
                    ff_save_daily_best();
#endif
                }
#ifdef FLOPPYSOUND
                if (s_ff_score > s_ff_alltime_best) {
                    s_ff_alltime_best = s_ff_score;
                    ff_save_alltime_best();
                    printf("New all-time high score: %d\n", s_ff_alltime_best);
                }
                vis->sound_score = true;
#endif
            }

            // Collision against the top and bottom obstacle columns. Cave
            // stalactites/stalagmites taper to a point (see
            // ff_draw_cave_column), so they get a matching triangular
            // hitbox; every other theme keeps the simple rect, which the
            // art is drawn scaled down to always fit inside.
            double gap = vis->height * 0.24;
            double top_rect_y0 = 0, top_rect_y1 = s_ff_pipes[i].gap_center - gap * 0.5;
            double bot_rect_y0 = s_ff_pipes[i].gap_center + gap * 0.5, bot_rect_y1 = vis->height - floor_h;
            double rx0 = s_ff_pipes[i].x, rx1 = s_ff_pipes[i].x + pipe_width;
            double r2 = (fish_radius * 0.82) * (fish_radius * 0.82);

            double dtop, dbot;
            if (s_ff_pipes[i].theme == FF_THEME_CAVE) {
                double col_cx = (rx0 + rx1) * 0.5;
                // Top: rooted along the ceiling (y0), tapers to a point at
                // the gap edge (y1).
                dtop = ff_point_triangle_dist2(fish_x, s_ff_fish_y,
                                                rx0, top_rect_y0, rx1, top_rect_y0,
                                                col_cx, top_rect_y1);
                // Bottom: rooted along the floor (y1), tapers to a point at
                // the gap edge (y0).
                dbot = ff_point_triangle_dist2(fish_x, s_ff_fish_y,
                                                rx0, bot_rect_y1, rx1, bot_rect_y1,
                                                col_cx, bot_rect_y0);
            } else {
                double cx = fmax(rx0, fmin(fish_x, rx1));
                double cy_top = fmax(top_rect_y0, fmin(s_ff_fish_y, top_rect_y1));
                double cy_bot = fmax(bot_rect_y0, fmin(s_ff_fish_y, bot_rect_y1));
                double dx = fish_x - cx;
                dtop = dx * dx + (s_ff_fish_y - cy_top) * (s_ff_fish_y - cy_top);
                dbot = dx * dx + (s_ff_fish_y - cy_bot) * (s_ff_fish_y - cy_bot);
            }

            if (dtop < r2 || dbot < r2) {
                s_ff_state = FF_GAME_OVER;
            }
        }
    }

    // Hit the sandy floor - always ends the run, playing or already falling.
    if (s_ff_fish_y + fish_radius >= vis->height - floor_h) {
        s_ff_fish_y = vis->height - floor_h - fish_radius;
        if (s_ff_state == FF_PLAYING) s_ff_state = FF_GAME_OVER;
        s_ff_fish_vel = 0.0;
    }
    if (s_ff_fish_y - fish_radius < 0) {
        s_ff_fish_y = fish_radius;
        if (s_ff_fish_vel < 0) s_ff_fish_vel = 0.0;
    }

#ifdef FLOPPYSOUND
    if(s_ff_state==FF_GAME_OVER) {
        if(vis->deadcounter<2) vis->deadcounter++;
        if(vis->deadcounter==1) vis->sound_dead=true;
    }
#endif
}

static void ff_draw_fish(cairo_t *cr, double x, double y, double radius, double rotation,
                          double flap_phase, const FFFishPalette *pal, double alpha,
                          double blink_amount) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_rotate(cr, rotation);

    double tail_swing = sin(flap_phase * 9.0) * 0.5;

    // Tail fin
    cairo_set_source_rgba(cr, pal->dark_r, pal->dark_g, pal->dark_b, alpha);
    cairo_move_to(cr, -radius * 0.9, 0);
    cairo_line_to(cr, -radius * 1.7, -radius * 0.7 + tail_swing * radius * 0.4);
    cairo_line_to(cr, -radius * 1.7, radius * 0.7 + tail_swing * radius * 0.4);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Body
    cairo_pattern_t *body = cairo_pattern_create_radial(-radius * 0.2, -radius * 0.3, radius * 0.1,
                                                          0, 0, radius * 1.3);
    cairo_pattern_add_color_stop_rgba(body, 0.0, pal->top_r, pal->top_g, pal->top_b, alpha);
    cairo_pattern_add_color_stop_rgba(body, 0.6, pal->mid_r, pal->mid_g, pal->mid_b, alpha);
    cairo_pattern_add_color_stop_rgba(body, 1.0, pal->dark_r, pal->dark_g, pal->dark_b, alpha);
    cairo_set_source(cr, body);
    cairo_save(cr);
    cairo_scale(cr, 1.25, 1.0);
    cairo_arc(cr, 0, 0, radius, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_fill(cr);
    cairo_pattern_destroy(body);

    // Dorsal fin
    cairo_set_source_rgba(cr, pal->dark_r * 0.9, pal->dark_g * 0.9, pal->dark_b * 0.9, alpha);
    cairo_move_to(cr, -radius * 0.1, -radius * 0.85);
    cairo_line_to(cr, radius * 0.35, -radius * 1.35);
    cairo_line_to(cr, radius * 0.5, -radius * 0.75);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Belly highlight
    cairo_set_source_rgba(cr, 1.0, 0.95, 0.85, 0.7 * alpha);
    cairo_save(cr);
    cairo_translate(cr, radius * 0.1, radius * 0.35);
    cairo_scale(cr, 1.0, 0.55);
    cairo_arc(cr, 0, 0, radius * 0.65, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_fill(cr);

    // Eye - vertically squashed toward closed as blink_amount goes 0 -> 1,
    // so it reads as an eyelid closing over the eye rather than the eye
    // just vanishing.
    double eye_scale_y = 1.0 - 0.92 * blink_amount;
    if (eye_scale_y < 0.08) eye_scale_y = 0.08;
    cairo_save(cr);
    cairo_translate(cr, radius * 0.75, -radius * 0.15);
    cairo_scale(cr, 1.0, eye_scale_y);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha);
    cairo_arc(cr, 0, 0, radius * 0.28, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.05, alpha);
    cairo_arc(cr, radius * 0.10, 0, radius * 0.14, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);

    cairo_restore(cr);
}

// Small, dim, background-layer fish - same silhouette as the player's fish
// but simplified and translucent so they clearly read as scenery. Faces the
// direction it's actually swimming.
static void ff_draw_bg_fish(cairo_t *cr, double x, double y, double radius, double phase,
                             const FFFishPalette *pal, int dir) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, (double)dir, 1.0);
    double tail_swing = sin(phase * 6.0) * 0.5;
    double alpha = 0.5;

    cairo_set_source_rgba(cr, pal->dark_r, pal->dark_g, pal->dark_b, alpha);
    cairo_move_to(cr, -radius * 0.9, 0);
    cairo_line_to(cr, -radius * 1.6, -radius * 0.6 + tail_swing * radius * 0.35);
    cairo_line_to(cr, -radius * 1.6, radius * 0.6 + tail_swing * radius * 0.35);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, pal->mid_r, pal->mid_g, pal->mid_b, alpha);
    cairo_save(cr);
    cairo_scale(cr, 1.2, 1.0);
    cairo_arc(cr, 0, 0, radius, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha);
    cairo_arc(cr, radius * 0.7, -radius * 0.15, radius * 0.2, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.05, alpha);
    cairo_arc(cr, radius * 0.78, -radius * 0.15, radius * 0.1, 0, 2 * M_PI);
    cairo_fill(cr);

    cairo_restore(cr);
}

// A drifting octopus - round mantle plus wavy tentacles animated like the
// wing-curves in drawtrippy.cpp, drawn dim and translucent as background.
// alpha_mult fades it out as it nears/crosses the screen edges.
static void ff_draw_octopus(cairo_t *cr, double x, double y, double scale, double t, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, scale, scale);

    double alpha = 0.45 * alpha_mult;
    cairo_set_source_rgba(cr, 0.55, 0.35, 0.75, alpha);

    // Tentacles - wavy curves trailing below the mantle.
    for (int i = 0; i < 6; i++) {
        double base_x = -18 + i * 7.2;
        double sway = sin(t * 1.6 + i * 0.9) * 10.0;
        cairo_move_to(cr, base_x, 6);
        cairo_curve_to(cr,
                        base_x + sway * 0.4, 24,
                        base_x - sway * 0.6, 40,
                        base_x + sway, 56);
        cairo_set_line_width(cr, 4.5);
        cairo_stroke(cr);
    }

    // Mantle (head)
    cairo_set_source_rgba(cr, 0.62, 0.42, 0.82, alpha);
    cairo_arc(cr, 0, -10, 24, 0, 2 * M_PI);
    cairo_fill(cr);

    // Eyes
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha);
    cairo_arc(cr, -8, -14, 5, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, 8, -14, 5, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.05, alpha);
    cairo_arc(cr, -8, -14, 2.5, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, 8, -14, 2.5, 0, 2 * M_PI);
    cairo_fill(cr);

    cairo_restore(cr);
}

// A big, dim shark silhouette gliding through the far background - a single
// torpedo-shaped body path (nose to the right, tail to the left) with fins
// attached directly to its outline so they read as one animal rather than
// floating shapes. dir flips it to face the direction it's actually
// swimming.
static void ff_draw_shark(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult, double size_scale) {
    if (alpha_mult <= 0.0) return;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, (double)dir * size_scale, size_scale);

    double sway = sin(t * 1.4) * 0.05;
    cairo_rotate(cr, sway);

    double alpha = 0.32 * alpha_mult;
    cairo_set_source_rgba(cr, 0.28, 0.34, 0.40, alpha);

    double tail_swing = sin(t * 2.4) * 0.3;

    // Tail fin, forked, attached at the tail end of the body.
    cairo_move_to(cr, -52, 0);
    cairo_curve_to(cr, -66, -6, -78, -20 + tail_swing * 10, -92, -26 + tail_swing * 14);
    cairo_curve_to(cr, -78, -8, -70, -3, -60, 0);
    cairo_curve_to(cr, -70, 3, -78, 8, -92, 22 + tail_swing * 14);
    cairo_curve_to(cr, -78, 16, -66, 6, -52, 0);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Body - smooth torpedo shape, nose at +x, tapering to the tail.
    cairo_move_to(cr, 58, 1);
    cairo_curve_to(cr, 50, -14, 20, -20, -8, -17);
    cairo_curve_to(cr, -28, -14, -45, -8, -55, -1);
    cairo_curve_to(cr, -45, 8, -28, 15, -8, 17);
    cairo_curve_to(cr, 20, 20, 50, 12, 58, 1);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Dorsal fin, rooted on the body's top edge.
    cairo_move_to(cr, -14, -16);
    cairo_curve_to(cr, -10, -32, 2, -38, 10, -40);
    cairo_curve_to(cr, 4, -28, 6, -20, 12, -14);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Pectoral fin, swept back from the underside near the head.
    cairo_move_to(cr, 14, 10);
    cairo_curve_to(cr, 10, 24, 2, 34, -10, 38);
    cairo_curve_to(cr, -2, 26, 4, 16, 10, 8);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_restore(cr);
}

// The Atlantis-zone guest: a mermaid silhouette gliding by, built the same
// way as the shark (nose/nearest-swim-direction at +x, tail at -x) but with
// a humanoid torso/head/trailing hair grafted onto the fish tail instead of
// fins.
static void ff_draw_mermaid(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, (double)dir, 1.0);
    double sway = sin(t * 1.3) * 0.06;
    cairo_rotate(cr, sway);

    double alpha = 0.42 * alpha_mult;
    cairo_set_source_rgba(cr, 0.55, 0.35, 0.65, alpha);

    double tail_swing = sin(t * 3.0) * 0.35;

    // Forked tail fluke.
    cairo_move_to(cr, -40, 2);
    cairo_curve_to(cr, -52, -4, -62, -16 + tail_swing * 10, -74, -20 + tail_swing * 14);
    cairo_curve_to(cr, -62, -6, -56, -2, -48, 0);
    cairo_curve_to(cr, -56, 2, -62, 6, -74, 18 + tail_swing * 14);
    cairo_curve_to(cr, -62, 14, -52, 4, -40, 2);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Tail, tapering from the waist out to the fluke.
    cairo_move_to(cr, -6, 10);
    cairo_curve_to(cr, -16, 14, -30, 10, -40, 4);
    cairo_curve_to(cr, -30, -2, -16, -6, -6, -8);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Torso, waist to shoulders.
    cairo_move_to(cr, -6, -8);
    cairo_curve_to(cr, 0, -20, 14, -24, 22, -18);
    cairo_curve_to(cr, 26, -12, 26, 0, 20, 6);
    cairo_curve_to(cr, 12, 12, -2, 10, -6, 10);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Head.
    cairo_arc(cr, 26, -22, 8, 0, 2 * M_PI);
    cairo_fill(cr);

    // Hair, trailing behind the swim direction and swaying independently
    // of the tail beat.
    double hair_sway = sin(t * 1.8) * 8.0;
    cairo_set_line_width(cr, 5.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, 20, -26);
    cairo_curve_to(cr, 4, -24 + hair_sway * 0.3, -8, -14 + hair_sway * 0.6, -14, -4 + hair_sway);
    cairo_stroke(cr);

    // One arm, sweeping with the swim stroke.
    cairo_set_line_width(cr, 4.0);
    cairo_move_to(cr, 18, -6);
    cairo_curve_to(cr, 26, 4, 30, 14, 26, 22);
    cairo_stroke(cr);

    cairo_restore(cr);
}

// The reef/ship/cave-zone guest: a scuba diver silhouette, trailing a few
// rising bubbles from the regulator.
static void ff_draw_diver(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    double kick = sin(t * 4.0) * 0.3;

    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, (double)dir, 1.0);

    double alpha = 0.42 * alpha_mult;
    cairo_set_source_rgba(cr, 0.08, 0.10, 0.14, alpha);

    // Body, roughly horizontal, head toward the swim direction (+x).
    cairo_move_to(cr, 26, -2);
    cairo_curve_to(cr, 18, -10, 4, -10, -8, -6);
    cairo_curve_to(cr, -16, -4, -22, 0, -26, 4);
    cairo_curve_to(cr, -18, 8, -4, 10, 10, 8);
    cairo_curve_to(cr, 18, 6, 24, 2, 26, -2);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Head/mask.
    cairo_arc(cr, 30, -4, 7, 0, 2 * M_PI);
    cairo_fill(cr);

    // Air tank on the back.
    cairo_save(cr);
    cairo_translate(cr, -8, -10);
    cairo_rotate(cr, -0.15);
    cairo_rectangle(cr, -5, -10, 10, 20);
    cairo_fill(cr);
    cairo_restore(cr);

    // Fins, kicking.
    cairo_move_to(cr, -26, 4);
    cairo_line_to(cr, -42, -2 + kick * 8);
    cairo_line_to(cr, -42, 10 + kick * 8);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Trailing arm.
    cairo_set_line_width(cr, 4.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, 20, 2);
    cairo_curve_to(cr, 26, 8, 30, 14, 28, 20);
    cairo_stroke(cr);

    cairo_restore(cr);

    // A few bubbles rising from the regulator, drawn in world space (not
    // flipped/rotated with the body) so they always float straight up
    // regardless of which way the diver is facing.
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.35 * alpha_mult);
    for (int i = 0; i < 3; i++) {
        double bx = x + dir * 24.0 + sin(t * 3.0 + i) * 3.0;
        double by = y - 12.0 - fmod(t * 30.0 + i * 15.0, 40.0);
        cairo_arc(cr, bx, by, 1.5 + (i % 2), 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// The rainbow-zone guest: a galloping unicorn silhouette, built the same
// way as the shark/mermaid/diver (head/nose toward the swim direction, +x)
// but with a rainbow-striped mane and tail and a small sparkle trail in
// place of the diver's bubbles.
static void ff_draw_unicorn(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    double bob = sin(t * 3.0) * 4.0;

    cairo_save(cr);
    cairo_translate(cr, x, y + bob);
    cairo_scale(cr, (double)dir, 1.0);

    double alpha = 0.48 * alpha_mult;
    cairo_set_source_rgba(cr, 0.97, 0.97, 1.0, alpha);

    // Body, head toward the swim direction (+x).
    cairo_move_to(cr, -30, 4);
    cairo_curve_to(cr, -34, -10, -16, -18, 4, -16);
    cairo_curve_to(cr, 18, -15, 26, -8, 30, -2);
    cairo_curve_to(cr, 20, 6, -4, 10, -30, 4);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Neck and head.
    cairo_move_to(cr, 24, -10);
    cairo_curve_to(cr, 30, -20, 38, -24, 44, -20);
    cairo_curve_to(cr, 42, -14, 38, -10, 34, -8);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Horn - the one detail that unmistakably says "unicorn".
    cairo_set_source_rgba(cr, 0.95, 0.85, 0.40, alpha);
    cairo_move_to(cr, 42, -22);
    cairo_line_to(cr, 47, -35);
    cairo_line_to(cr, 44, -20);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Legs, cantering - front/back pairs offset in phase.
    double stride = sin(t * 5.0) * 10.0;
    cairo_set_source_rgba(cr, 0.97, 0.97, 1.0, alpha);
    cairo_set_line_width(cr, 4.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, 16, 6);  cairo_line_to(cr, 16 + stride, 20);        cairo_stroke(cr);
    cairo_move_to(cr, -4, 6);  cairo_line_to(cr, -4 - stride, 20);        cairo_stroke(cr);
    cairo_move_to(cr, -20, 4); cairo_line_to(cr, -20 + stride * 0.7, 18); cairo_stroke(cr);
    cairo_move_to(cr, -26, 4); cairo_line_to(cr, -26 - stride * 0.7, 18); cairo_stroke(cr);

    // Rainbow mane, trailing back from the neck, and a matching tail off
    // the rear - six colored strands each, same palette order as the
    // rainbow theme's pillars so the two visually match.
    static const double strand_r[6] = {0.90, 0.95, 0.98, 0.30, 0.26, 0.78};
    static const double strand_g[6] = {0.16, 0.55, 0.85, 0.75, 0.56, 0.38};
    static const double strand_b[6] = {0.20, 0.16, 0.22, 0.36, 0.95, 0.86};

    cairo_set_line_width(cr, 3.5);
    double mane_sway = sin(t * 2.2) * 6.0;
    for (int i = 0; i < 6; i++) {
        cairo_set_source_rgba(cr, strand_r[i], strand_g[i], strand_b[i], alpha_mult * 0.8);
        double bx = 30 + i * 2.0, by = -18 + i * 2.0;
        cairo_move_to(cr, bx, by);
        cairo_curve_to(cr, bx - 6, by + 6 + mane_sway * 0.3,
                            bx - 10, by + 14 + mane_sway * 0.6,
                            bx - 14, by + 20 + mane_sway);
        cairo_stroke(cr);
    }

    double tail_sway = sin(t * 2.5 + 1.0) * 10.0;
    for (int i = 0; i < 6; i++) {
        cairo_set_source_rgba(cr, strand_r[i], strand_g[i], strand_b[i], alpha_mult * 0.8);
        double bx = -28, by = -2 + i * 1.2;
        cairo_move_to(cr, bx, by);
        cairo_curve_to(cr, bx - 8, by + 4 + tail_sway * 0.3,
                            bx - 16, by + 10 + tail_sway * 0.6,
                            bx - 22, by + 16 + tail_sway);
        cairo_stroke(cr);
    }

    cairo_restore(cr);

    // A few golden sparkles trailing behind, drawn in world space (not
    // flipped with the body) so they read the same regardless of facing.
    cairo_set_source_rgba(cr, 1.0, 0.95, 0.60, 0.5 * alpha_mult);
    for (int i = 0; i < 4; i++) {
        double sx = x - dir * (28.0 + i * 8.0);
        double sy = y + bob - 6.0 + sin(t * 4.0 + i) * 6.0;
        double s = 1.2 + (i % 2);
        cairo_arc(cr, sx, sy, s, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// The bone-yard-zone guest: a mosasaurus - a long, thick-bodied marine
// reptile gliding through the murk, built the same way as the shark (nose
// at +x, tail at -x) but longer, with paddle-like flippers instead of fins
// and a snapping jaw with visible teeth in place of the shark's blunt
// snout. Drawn dim and slow, like something glimpsed rather than seen
// clearly.
static void ff_draw_mosasaurus(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, (double)dir, 1.0);

    double sway = sin(t * 0.9) * 0.06;
    cairo_rotate(cr, sway);

    double alpha = 0.34 * alpha_mult;
    cairo_set_source_rgba(cr, 0.16, 0.22, 0.16, alpha);

    double tail_swing = sin(t * 1.6) * 0.3;

    // Tail, long and undulating, with a fin at the very tip.
    cairo_move_to(cr, -60, 0);
    cairo_curve_to(cr, -80, -4 + tail_swing * 6, -100, -8 + tail_swing * 14, -118, -6 + tail_swing * 20);
    cairo_curve_to(cr, -128, -14 + tail_swing * 20, -136, -22 + tail_swing * 24, -150, -30 + tail_swing * 26);
    cairo_curve_to(cr, -138, -10 + tail_swing * 20, -128, 4 + tail_swing * 14, -118, 6 + tail_swing * 20);
    cairo_curve_to(cr, -100, 10 + tail_swing * 14, -80, 6 + tail_swing * 6, -60, 0);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Body - long, thick, torpedo-shaped, nose at +x.
    cairo_move_to(cr, 62, 2);
    cairo_curve_to(cr, 52, -18, 10, -24, -30, -19);
    cairo_curve_to(cr, -45, -17, -55, -10, -62, -2);
    cairo_curve_to(cr, -55, 10, -45, 16, -30, 18);
    cairo_curve_to(cr, 10, 22, 52, 16, 62, 2);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Elongated jaw, extending past the body's nose.
    cairo_move_to(cr, 58, -6);
    cairo_curve_to(cr, 70, -10, 84, -8, 96, -2);
    cairo_curve_to(cr, 84, 2, 70, 4, 58, 4);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Teeth along the jaw line - a handful of small triangles, the detail
    // that most says "predator" at a glance.
    cairo_set_source_rgba(cr, 0.85, 0.85, 0.80, alpha_mult * 0.7);
    for (int k = 0; k < 4; k++) {
        double tx = 62 + k * 8.5;
        cairo_move_to(cr, tx, 2);
        cairo_line_to(cr, tx + 4, 2);
        cairo_line_to(cr, tx + 2, 8);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
    cairo_set_source_rgba(cr, 0.16, 0.22, 0.16, alpha);

    // Two paddle-like flippers, fore and aft on the underside.
    cairo_move_to(cr, 22, 12);
    cairo_curve_to(cr, 18, 26, 8, 36, -8, 40);
    cairo_curve_to(cr, 0, 28, 6, 18, 14, 10);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_move_to(cr, -32, 14);
    cairo_curve_to(cr, -36, 26, -44, 34, -56, 36);
    cairo_curve_to(cr, -50, 26, -44, 18, -38, 12);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Small dorsal ridge along the back.
    cairo_set_source_rgba(cr, 0.12, 0.18, 0.12, alpha);
    for (int i = 0; i < 4; i++) {
        double rx = 30 - i * 22.0;
        cairo_move_to(cr, rx - 6, -18);
        cairo_line_to(cr, rx, -26);
        cairo_line_to(cr, rx + 6, -18);
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    // A small pale eye, the one bright point in an otherwise murky
    // silhouette.
    cairo_set_source_rgba(cr, 0.85, 0.90, 0.75, alpha_mult * 0.5);
    cairo_arc(cr, 44, -12, 2.5, 0, 2 * M_PI);
    cairo_fill(cr);

    cairo_restore(cr);
}

// A single swimming penguin: a compact torpedo body, black back, white
// belly, orange beak, and a pair of flippers flapping like a fast
// underwater "flight" - head toward the swim direction (+x). Used three
// times by ff_draw_penguin_group below, since a lone penguin doesn't say
// "Antarctic" nearly as clearly as a little waddle of them does.
static void ff_draw_penguin_single(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult, double scale) {
    if (alpha_mult <= 0.0) return;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, (double)dir * scale, scale);

    double flap = sin(t * 6.0) * 0.3;
    double alpha = 0.55 * alpha_mult;

    // Back (black).
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.07, alpha);
    cairo_move_to(cr, 18, 0);
    cairo_curve_to(cr, 14, -10, -4, -12, -16, -6);
    cairo_curve_to(cr, -20, -3, -20, 3, -16, 6);
    cairo_curve_to(cr, -4, 12, 14, 10, 18, 0);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Belly (white), inset.
    cairo_set_source_rgba(cr, 0.96, 0.96, 0.98, alpha);
    cairo_move_to(cr, 12, 0);
    cairo_curve_to(cr, 9, -5, -2, -6, -11, -3);
    cairo_curve_to(cr, -13, -1, -13, 1, -11, 3);
    cairo_curve_to(cr, -2, 6, 9, 5, 12, 0);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Flippers, flapping in a fast wingbeat.
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.07, alpha);
    cairo_move_to(cr, 2, -4);
    cairo_line_to(cr, -2, -14 + flap * 8);
    cairo_line_to(cr, 6, -6);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_move_to(cr, 2, 4);
    cairo_line_to(cr, -2, 14 - flap * 8);
    cairo_line_to(cr, 6, 6);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Head and beak.
    cairo_arc(cr, 17, -1, 5, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.95, 0.55, 0.15, alpha_mult * 0.85);
    cairo_move_to(cr, 21, -1);
    cairo_line_to(cr, 26, 0);
    cairo_line_to(cr, 21, 1);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_restore(cr);
}

// The Antarctic-zone guest: a little waddle of three penguins swimming
// past together, each with its own bob/phase so they read as a loose
// group rather than three copies of one animal. Positions are given in
// pre-flip local space (negative x trails behind the lead penguin) and
// scaled by dir so the whole group still trails correctly however it's
// facing.
static void ff_draw_penguin_group(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    static const double off_x[3]  = {0.0, -34.0, -30.0};
    static const double off_y[3]  = {0.0, -16.0, 15.0};
    static const double scale[3]  = {1.0, 0.82, 0.86};
    static const double phase[3]  = {0.0, 0.6, 1.3};

    for (int i = 0; i < 3; i++) {
        double bob = sin(t * 2.0 + phase[i] * 3.0) * 5.0;
        double px = x + dir * off_x[i];
        double py = y + off_y[i] + bob;
        ff_draw_penguin_single(cr, px, py, t + phase[i], dir, alpha_mult, scale[i]);
    }
}

// A single scuttling lobster: a segmented tail curling under a thorax,
// snapping claws, whip-thin antennae, and a few walking legs cycling out
// of phase - drawn close to the gravel rather than mid-tank like every
// other guest (see ff_spawn_guest). Head/claws toward the direction of
// travel (+x).
static void ff_draw_lobster_single(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult, double scale) {
    if (alpha_mult <= 0.0) return;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, (double)dir * scale, scale);

    double alpha = 0.62 * alpha_mult;
    cairo_set_source_rgba(cr, 0.85, 0.25, 0.15, alpha);

    // Segmented tail, curling under the body.
    cairo_move_to(cr, -14, 4);
    cairo_curve_to(cr, -22, 8, -30, 10, -36, 6);
    cairo_curve_to(cr, -30, 2, -22, 0, -14, -2);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Tail fan.
    cairo_move_to(cr, -34, 4);
    cairo_line_to(cr, -42, 10);
    cairo_line_to(cr, -40, 2);
    cairo_line_to(cr, -42, -6);
    cairo_line_to(cr, -34, 0);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Thorax.
    cairo_move_to(cr, -14, -2);
    cairo_curve_to(cr, -10, -12, 4, -14, 14, -10);
    cairo_curve_to(cr, 20, -8, 22, -2, 20, 4);
    cairo_curve_to(cr, 8, 10, -8, 8, -14, 4);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Whip-thin antennae.
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, 18, -6);
    cairo_curve_to(cr, 26, -14, 34, -18, 42, -16);
    cairo_stroke(cr);
    cairo_move_to(cr, 18, -2);
    cairo_curve_to(cr, 26, -6, 34, -4, 40, 4);
    cairo_stroke(cr);

    // Claws, snapping open and shut with the walk cycle.
    double snap = 0.4 + 0.3 * sin(t * 5.0);
    for (int side = -1; side <= 1; side += 2) {
        cairo_save(cr);
        cairo_translate(cr, 16, side * 8);
        cairo_rotate(cr, side * 0.3);
        cairo_move_to(cr, 0, 0);
        cairo_curve_to(cr, 6, -4, 14, -4, 18, -side * snap * 4);
        cairo_curve_to(cr, 14, 4, 6, 4, 0, 0);
        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_move_to(cr, 14, -side * snap * 3);
        cairo_line_to(cr, 20, -side * snap * 6);
        cairo_line_to(cr, 16, side * snap * 1);
        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    // Walking legs, alternating.
    cairo_set_line_width(cr, 2.0);
    for (int i = 0; i < 3; i++) {
        double lx = -4 - i * 6.0;
        double lift = sin(t * 8.0 + i * 2.0) * 3.0;
        cairo_move_to(cr, lx, 6);
        cairo_line_to(cr, lx - 4, 12 + lift);
        cairo_stroke(cr);
    }

    cairo_restore(cr);
}

// The aquarium-zone guest: a pair of lobsters scuttling along the gravel
// together, so "lobsters" reads as plural rather than a lone straggler -
// same idea as the penguin waddle.
static void ff_draw_lobster_group(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    static const double off_x[2] = {0.0, -50.0};
    static const double off_y[2] = {0.0, 5.0};
    static const double scale[2] = {1.0, 0.85};

    for (int i = 0; i < 2; i++) {
        double px = x + dir * off_x[i];
        double py = y + off_y[i];
        ff_draw_lobster_single(cr, px, py, t + i * 0.4, dir, alpha_mult, scale[i]);
    }
}

// A single drifting astronaut: a bulky suit torso, a helmet with a
// reflective visor, a backpack (PLSS), limbs swaying loosely as if in
// zero-g, a tether cable trailing behind, and a couple of small thruster
// puffs - facing/moving toward the swim direction (+x).
static void ff_draw_astronaut_single(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult, double scale) {
    if (alpha_mult <= 0.0) return;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, (double)dir * scale, scale);
    cairo_rotate(cr, sin(t * 1.2) * 0.08);

    double alpha = 0.55 * alpha_mult;

    // Suit torso.
    cairo_set_source_rgba(cr, 0.92, 0.92, 0.94, alpha);
    cairo_move_to(cr, -10, -6);
    cairo_curve_to(cr, -14, 4, -12, 14, -4, 18);
    cairo_curve_to(cr, 4, 20, 12, 16, 12, 6);
    cairo_curve_to(cr, 12, -4, 4, -10, -10, -6);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Backpack (life-support pack), peeking out behind the torso.
    cairo_set_source_rgba(cr, 0.72, 0.72, 0.78, alpha);
    cairo_rectangle(cr, -16, -4, 7, 16);
    cairo_fill(cr);

    // Helmet and reflective visor.
    cairo_set_source_rgba(cr, 0.92, 0.92, 0.94, alpha);
    cairo_arc(cr, 6, -14, 9, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.15, 0.55, 0.85, alpha_mult * 0.7);
    cairo_arc(cr, 8, -14, 6, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5 * alpha_mult);
    cairo_arc(cr, 6, -17, 2, 0, 2 * M_PI);
    cairo_fill(cr);

    // Arms and legs, drifting loosely rather than swimming with purpose.
    double armswing = sin(t * 1.5) * 10.0;
    double legswing = sin(t * 1.3 + 1.0) * 8.0;
    cairo_set_source_rgba(cr, 0.90, 0.90, 0.92, alpha);
    cairo_set_line_width(cr, 5.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, -6, 0);
    cairo_curve_to(cr, -14, -2 + armswing * 0.3, -20, 4 + armswing * 0.6, -24, 10 + armswing);
    cairo_stroke(cr);
    cairo_move_to(cr, 8, 4);
    cairo_curve_to(cr, 16, 8 - armswing * 0.3, 20, 14 - armswing * 0.6, 22, 20 - armswing);
    cairo_stroke(cr);
    cairo_move_to(cr, -6, 16);
    cairo_curve_to(cr, -10, 22, -12, 28 + legswing * 0.5, -14, 34 + legswing);
    cairo_stroke(cr);
    cairo_move_to(cr, 2, 18);
    cairo_curve_to(cr, 6, 24, 8, 30 - legswing * 0.5, 10, 36 - legswing);
    cairo_stroke(cr);

    cairo_restore(cr);

    // Tether cable and thruster puffs, drawn in world space (not
    // flipped/rotated with the body) so they trail correctly however the
    // astronaut is facing.
    cairo_set_source_rgba(cr, 0.85, 0.85, 0.30, 0.4 * alpha_mult);
    cairo_set_line_width(cr, 1.5);
    double tx = x - dir * 26.0, ty = y + 10.0;
    cairo_move_to(cr, x - dir * 10.0, y + 4.0);
    cairo_curve_to(cr, x - dir * 18.0, y + 8.0 + sin(t * 2.0) * 4.0,
                        x - dir * 24.0, y + 2.0 + sin(t * 2.3) * 5.0,
                        tx, ty);
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.3 * alpha_mult);
    for (int k = 0; k < 2; k++) {
        double px = x - dir * (14.0 + k * 5.0);
        double py = y + 6.0 + sin(t * 3.0 + k) * 3.0;
        cairo_arc(cr, px, py, 1.5 + k * 0.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// The galaxy-zone guest: a pair of astronauts drifting past on their
// tethers together, buddy-system style, rather than a lone figure.
static void ff_draw_astronaut_group(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    static const double off_x[2] = {0.0, -46.0};
    static const double off_y[2] = {0.0, 20.0};
    static const double scale[2] = {1.0, 0.85};

    for (int i = 0; i < 2; i++) {
        double px = x + dir * off_x[i];
        double py = y + off_y[i];
        ff_draw_astronaut_single(cr, px, py, t + i * 0.5, dir, alpha_mult, scale[i]);
    }
}

// (Re)builds the cached static-layer surfaces for all nine themes if they
// haven't been built yet, or if the canvas size has changed since they
// were (this file's canvas is normally a fixed GAME_W x GAME_H, but it's
// also reused as-is by zenamp's visualizer, so this is a size check rather
// than a one-shot flag). Cheap to call every frame once built - it's just
// two float comparisons in the common case.
static void ff_free_theme_caches() {
    for (int t = 0; t < FF_THEME_COUNT; t++) {
        if (s_ff_sky_cache[t])   { cairo_surface_destroy(s_ff_sky_cache[t]);   s_ff_sky_cache[t] = NULL; }
        if (s_ff_floor_cache[t]) { cairo_surface_destroy(s_ff_floor_cache[t]); s_ff_floor_cache[t] = NULL; }
    }
}

static void ff_ensure_theme_caches(double w, double h, double floor_h) {
    if (s_ff_sky_cache[0] && w == s_ff_cache_w && h == s_ff_cache_h) return;
    ff_free_theme_caches();
    for (int t = 0; t < FF_THEME_COUNT; t++) {
        cairo_surface_t *sky = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)w, (int)h);
        cairo_t *skc = cairo_create(sky);
        ff_draw_theme_sky_static(skc, t, w, h);
        cairo_destroy(skc);
        s_ff_sky_cache[t] = sky;

        // Floor cache only needs to be as tall as the floor band itself -
        // painted back at the right y-offset below - so blitting it doesn't
        // touch the ~90% of the canvas above the floor.
        cairo_surface_t *floor = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)w, (int)ceil(floor_h));
        cairo_t *flc = cairo_create(floor);
        ff_draw_theme_floor_static(flc, t, w, floor_h, floor_h);
        cairo_destroy(flc);
        s_ff_floor_cache[t] = floor;
    }
    s_ff_cache_w = w;
    s_ff_cache_h = h;
}

void draw_floppy_fish(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;

    double w = vis->width, h = vis->height;
    double floor_h = h * 0.10;
    double pipe_width = h * 0.16;
    double fish_radius = h * 0.032;
    double fish_x = w * FF_FISH_X_FRAC;

    // Which theme zone the camera is in right now, the next one it's
    // approaching, and how far into the crossfade toward it we are. This
    // one calculation drives every themed layer drawn below, so the fish
    // travels between reef/ship/cave/atlantis as one continuous blend
    // rather than a set of independently-changing pieces.
    double zone_len = h * FF_ZONE_LEN_FRAC;
    double trans_len = h * FF_ZONE_TRANS_FRAC;
    int theme_from, theme_to;
    double blend_t;
    ff_theme_at(s_ff_world_x, zone_len, trans_len, &theme_from, &theme_to, &blend_t);
    double base_y = h - floor_h;

    ff_ensure_theme_caches(w, h, floor_h);

    // Sky/backdrop, crossfaded as a single composited layer so its internal
    // bubbles/skyline never double-blend against each other mid-transition.
    // The gradient+skyline part comes from the per-theme cache built above
    // (a cheap blit); only the bubbles are actually redrawn live.
    cairo_set_source_surface(cr, s_ff_sky_cache[theme_from], 0, 0);
    cairo_paint(cr);
    ff_draw_theme_particles(cr, theme_from, w, h, s_ff_bubble_phase);
    if (blend_t > 0.0) {
        cairo_push_group(cr);
        cairo_set_source_surface(cr, s_ff_sky_cache[theme_to], 0, 0);
        cairo_paint(cr);
        ff_draw_theme_particles(cr, theme_to, w, h, s_ff_bubble_phase);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, blend_t);
    }

    // Floor decoration - reef seaweed, ship rope/plank debris, glowing cave
    // crystal shards, or glowing Atlantean kelp - fades in/out with how
    // "present" each theme currently is, so it blends right along with the
    // sky/floor around it.
    double theme_alpha[FF_THEME_COUNT] = {0};
    theme_alpha[theme_from] += (1.0 - blend_t);
    theme_alpha[theme_to] += blend_t;
    for (int i = 0; i < FF_SEAWEED_COUNT; i++) {
        double t_phase = vis->time_offset + s_ff_seaweed_phase[i];
        double sh = h * 0.16 * s_ff_seaweed_height_frac[i];
        for (int th = 0; th < FF_THEME_COUNT; th++) {
            if (theme_alpha[th] > 0.001) {
                ff_draw_seaweed(cr, s_ff_seaweed_x[i], base_y, sh, t_phase, th, theme_alpha[th]);
            }
        }
    }

    // Little background friends - fish and an octopus drifting behind the
    // pipes, purely decorative. The shark is a rare guest, drawn dim and
    // furthest back of all so it never competes with the pipes/fish.
    if (s_ff_shark_active) {
        ff_draw_shark(cr, s_ff_shark_x, s_ff_shark_y, vis->time_offset, s_ff_shark_dir,
                       ff_edge_fade(s_ff_shark_x, w), s_ff_shark_scale);
    }
    if (s_ff_guest_active) {
        double guest_alpha = ff_edge_fade(s_ff_guest_x, w);
        switch (s_ff_guest_kind) {
            case FF_GUEST_MERMAID:
                ff_draw_mermaid(cr, s_ff_guest_x, s_ff_guest_y, vis->time_offset, s_ff_guest_dir, guest_alpha);
                break;
            case FF_GUEST_UNICORN:
                ff_draw_unicorn(cr, s_ff_guest_x, s_ff_guest_y, vis->time_offset, s_ff_guest_dir, guest_alpha);
                break;
            case FF_GUEST_MOSASAURUS:
                ff_draw_mosasaurus(cr, s_ff_guest_x, s_ff_guest_y, vis->time_offset, s_ff_guest_dir, guest_alpha);
                break;
            case FF_GUEST_PENGUINS:
                ff_draw_penguin_group(cr, s_ff_guest_x, s_ff_guest_y, vis->time_offset, s_ff_guest_dir, guest_alpha);
                break;
            case FF_GUEST_LOBSTERS:
                ff_draw_lobster_group(cr, s_ff_guest_x, s_ff_guest_y, vis->time_offset, s_ff_guest_dir, guest_alpha);
                break;
            case FF_GUEST_ASTRONAUTS:
                ff_draw_astronaut_group(cr, s_ff_guest_x, s_ff_guest_y, vis->time_offset, s_ff_guest_dir, guest_alpha);
                break;
            default:
                ff_draw_diver(cr, s_ff_guest_x, s_ff_guest_y, vis->time_offset, s_ff_guest_dir, guest_alpha);
                break;
        }
    }
    ff_draw_octopus(cr, s_ff_octopus_x, s_ff_octopus_y, 1.0, vis->time_offset,
                     ff_edge_fade(s_ff_octopus_x, w));
    for (int i = 0; i < FF_BG_FISH_COUNT; i++) {
        const FFFishPalette *bg_pal = &FF_FISH_PALETTES[s_ff_bgfish_palette[i]];
        double bg_radius = fish_radius * s_ff_bgfish_scale[i];
        ff_draw_bg_fish(cr, s_ff_bgfish_x[i], s_ff_bgfish_y[i], bg_radius,
                         vis->time_offset + i * 1.3, bg_pal, s_ff_bgfish_dir[i]);
    }

    // Obstacles: each one keeps whichever theme's look it was born with, so
    // individual columns don't morph mid-flight - it's the mix on screen
    // that shifts smoothly as newly-spawned ones start matching the new zone.
    for (int i = 0; i < FF_MAX_PIPES; i++) {
        if (!s_ff_pipes[i].active || !s_ff_pipes[i].art_cache) continue;
        cairo_set_source_surface(cr, s_ff_pipes[i].art_cache, s_ff_pipes[i].x, 0);
        cairo_paint(cr);
    }

    // Floor, crossfaded the same way as the sky - cached base fill blitted,
    // then the live ripples/planks/tiles (and whatever decoration has to
    // sit on top of them) drawn on top.
    cairo_set_source_surface(cr, s_ff_floor_cache[theme_from], 0, base_y);
    cairo_paint(cr);
    ff_draw_theme_floor_scroll(cr, theme_from, w, h, floor_h, s_ff_bubble_phase);
    if (blend_t > 0.0) {
        cairo_push_group(cr);
        cairo_set_source_surface(cr, s_ff_floor_cache[theme_to], 0, base_y);
        cairo_paint(cr);
        ff_draw_theme_floor_scroll(cr, theme_to, w, h, floor_h, s_ff_bubble_phase);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, blend_t);
    }

    // Fish
    ff_draw_fish(cr, fish_x, s_ff_fish_y, fish_radius, s_ff_rotation, s_ff_flap_anim,
                 &FF_FISH_PALETTES[s_ff_fish_palette], 1.0, s_ff_blink_progress);

    // Score
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    // A rounder, bouncier face for all the game's UI text, matching the
    // playful "floppy fish" tone better than a plain sans font. Cairo's toy
    // font API falls back to a default sans face if this name isn't
    // installed, so this degrades gracefully rather than failing.
    if (!s_ff_font_face) {
        s_ff_font_face = cairo_toy_font_face_create(
            "Comic Sans MS", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    }
    cairo_set_font_face(cr, s_ff_font_face);
    cairo_set_font_size(cr, h * 0.08);
    char score_text[16];
    snprintf(score_text, sizeof(score_text), "%d", s_ff_score);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, score_text, &ext);
    double sx = w * 0.5 - ext.width * 0.5;
    double sy = h * 0.12;
    cairo_move_to(cr, sx + 2, sy + 2);
    cairo_show_text(cr, score_text);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, sx, sy);
    cairo_show_text(cr, score_text);

    if (s_ff_state == FF_READY) {
        // Welcome to Floppy Fish
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
        cairo_set_font_size(cr, h * 0.045);
        const char *msg = "Welcome to Floppy Fish";
        cairo_text_extents(cr, msg, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.62);
        cairo_show_text(cr, msg);

        // Help Text
#ifdef FLOPPYSOUND
        cairo_set_font_size(cr, h * 0.02);
        const char *msg3 = "Press F11 to Toggle Fullscreen or S to toggle sound";
        cairo_text_extents(cr, msg3, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.74);
        cairo_show_text(cr, msg3);
#endif

        cairo_set_font_size(cr, h * 0.02);
        const char *msg4 = "Keep Clicking to Swim Up and Avoid Obstacles; Not Clicking causes Floppy Fish to Sink (middle click swims normally)";
        cairo_text_extents(cr, msg4, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.78);
        cairo_show_text(cr, msg4);

        // Click to Start Text
        cairo_set_font_size(cr, h * 0.045);
        cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.95);
#ifdef FLOPPYSOUND
        const char *msg2 = "Click to Start or Escape to Exit";
#else
        const char *msg2 = "Click to Start";
#endif
        cairo_text_extents(cr, msg2, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.68);
        cairo_show_text(cr, msg2);




    } else if (s_ff_state == FF_GAME_OVER) {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.45);
        cairo_rectangle(cr, w * 0.12, h * 0.32, w * 0.76, h * 0.30);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_set_font_size(cr, h * 0.05);
        const char *msg = "Game Over";
        cairo_text_extents(cr, msg, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.40);
        cairo_show_text(cr, msg);

        cairo_set_font_size(cr, h * 0.032);
        char best_text[64];
        const char *medal = ff_medal_for_score(s_ff_score);
        double score_y = h * 0.48;
        if (strcmp(medal, "None") == 0) {
            snprintf(best_text, sizeof(best_text), "Score: %d", s_ff_score);
            cairo_text_extents(cr, best_text, &ext);
            cairo_move_to(cr, w * 0.5 - ext.width * 0.5, score_y);
            cairo_show_text(cr, best_text);
        } else {
            snprintf(best_text, sizeof(best_text), "Score: %d", s_ff_score);
            cairo_text_extents(cr, best_text, &ext);
            double medal_radius = h * 0.026;
            double gap = h * 0.02;
            double total_w = ext.width + gap + medal_radius * 2.0;
            double start_x = w * 0.5 - total_w * 0.5;
            cairo_move_to(cr, start_x, score_y);
            cairo_show_text(cr, best_text);
            ff_draw_medal(cr, start_x + ext.width + gap + medal_radius,
                          score_y - ext.height * 0.42, medal_radius, medal);
        }

#ifdef FLOPPYSOUND
        snprintf(best_text, sizeof(best_text), "Today's Best: %d   All-Time Best: %d",
                 s_ff_best_score, s_ff_alltime_best);
#else
        snprintf(best_text, sizeof(best_text), "Today's Best: %d", s_ff_best_score);
#endif
        cairo_text_extents(cr, best_text, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.54);
        cairo_show_text(cr, best_text);

#ifdef FLOPPYSOUND
        snprintf(best_text, sizeof(best_text), "Left Click to Restart or Escape to Quit");
#else
        snprintf(best_text, sizeof(best_text), "Left Click to Restart");
#endif
        cairo_text_extents(cr, best_text, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.60);
        cairo_show_text(cr, best_text);


    }
}
