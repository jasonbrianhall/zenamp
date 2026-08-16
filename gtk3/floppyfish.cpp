#include "visualization.h"
#include <stdlib.h>

// ---- Flappy Fish ----
// A Flappy Bird-style game: click to flap, swim between scrolling pipes,
// don't hit the coral or the sand. Fully mouse-driven, runs continuously
// like the other interactive games regardless of whether music is playing.

#define FF_MAX_PIPES 8
#define FF_BG_FISH_COUNT 7

// The coral's lobes/jags are generated somewhat larger than the obstacle's
// actual pipe_width column (that's what gives the jags their reach), then
// drawn at this fraction of that size. Chosen so that even the analytical
// worst case (max radius, max jitter, a jag angled dead-on and at max
// length) still lands exactly at the pipe_width edge - so nothing needs to
// be clipped, and there's no flat pipe-like crop line, but a visible jag
// is still guaranteed to be within the collision box.
#define FF_CORAL_SCALE 0.55

// Three visual themes the run cycles through as the fish travels: coral
// reef, a sunken pirate ship, and a dark cave. s_ff_world_x is the total
// scroll distance covered so far (reset each run, paused unless actively
// playing) and picks which theme zone the camera is currently in. Zone
// length and the crossfade band between zones are both in units of
// vis->height, same as every other size in this file, so they scale with
// resolution consistently.
typedef enum { FF_THEME_REEF = 0, FF_THEME_SHIP = 1, FF_THEME_CAVE = 2, FF_THEME_COUNT = 3 } FFTheme;
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
static int s_ff_best_score = 0;
static double s_ff_bubble_phase = 0.0;
static double s_ff_flap_anim = 0.0;  // tail-flap animation clock, ticks while playing
static int s_ff_fish_palette = 0;    // random new color friend each game

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
    if (dir < 0) {
        s_ff_shark_x = vis->width * (1.10 + 0.2 * ((double)rand() / RAND_MAX));
    } else {
        s_ff_shark_x = -vis->width * (0.10 + 0.2 * ((double)rand() / RAND_MAX));
    }
    s_ff_shark_active = true;
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
    // Place patches one at a time so each new one can avoid the ones
    // already placed before it.
    for (int i = 0; i < FF_SEAWEED_COUNT; i++) s_ff_seaweed_x[i] = -1e9;
    for (int i = 0; i < FF_SEAWEED_COUNT; i++) ff_spawn_seaweed(vis, i, true);
    s_ff_bg_init = true;
}

void init_floppy_fish_system(Visualizer *vis) {
    s_ff_best_score = 0;
    s_ff_bg_init = false;
    ff_reset(vis);
    ff_init_background(vis);
#ifdef FLOPPYSOUND
    vis->sound_flap=false;
    vis->sound_score=false;
    vis->sound_dead=false;
#endif
}

static void ff_flap(Visualizer *vis) {
    s_ff_fish_vel = -vis->height * 0.62;
#ifdef FLOPPYSOUND
    vis->sound_flap = true;
#endif
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

        s_ff_pipes[i].x = vis->width;
        s_ff_pipes[i].gap_center = gap_center;
        s_ff_pipes[i].scored = false;
        s_ff_pipes[i].theme = (bt > 0.5) ? tt : tf;
        s_ff_pipes[i].active = true;
        return;
    }
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
    if (vis->mouse_left_pressed) {
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

    s_ff_fish_vel += gravity * dt;
    s_ff_fish_y += s_ff_fish_vel * dt;

    double max_up_speed = vis->height * 0.62;
    double tilt = s_ff_fish_vel / max_up_speed;
    if (tilt < -1.0) tilt = -1.0;
    if (tilt > 1.4) tilt = 1.4;
    s_ff_rotation = tilt * 0.55;

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
                if (s_ff_score > s_ff_best_score) s_ff_best_score = s_ff_score;
#ifdef FLOPPYSOUND
                vis->sound_score = true;
#endif
            }

            // Circle-vs-rect collision against the top and bottom coral
            // columns. The corals is drawn scaled down to guarantee its jags
            // never reach past this exact box, so no padding is needed here.
            double gap = vis->height * 0.24;
            double top_rect_y0 = 0, top_rect_y1 = s_ff_pipes[i].gap_center - gap * 0.5;
            double bot_rect_y0 = s_ff_pipes[i].gap_center + gap * 0.5, bot_rect_y1 = vis->height - floor_h;
            double rx0 = s_ff_pipes[i].x, rx1 = s_ff_pipes[i].x + pipe_width;

            double cx = fmax(rx0, fmin(fish_x, rx1));
            double cy_top = fmax(top_rect_y0, fmin(s_ff_fish_y, top_rect_y1));
            double cy_bot = fmax(bot_rect_y0, fmin(s_ff_fish_y, bot_rect_y1));

            double dx = fish_x - cx;
            double dtop = dx * dx + (s_ff_fish_y - cy_top) * (s_ff_fish_y - cy_top);
            double dbot = dx * dx + (s_ff_fish_y - cy_bot) * (s_ff_fish_y - cy_bot);
            double r2 = (fish_radius * 0.82) * (fish_radius * 0.82);

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
                          double flap_phase, const FFFishPalette *pal, double alpha) {
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

    // Eye
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha);
    cairo_arc(cr, radius * 0.75, -radius * 0.15, radius * 0.28, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.05, alpha);
    cairo_arc(cr, radius * 0.85, -radius * 0.15, radius * 0.14, 0, 2 * M_PI);
    cairo_fill(cr);

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
static void ff_draw_shark(cairo_t *cr, double x, double y, double t, int dir, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, (double)dir, 1.0);

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

// A little cluster of swaying floor decoration strands, rooted at the
// floor - reef seaweed fronds, ship rope/plank debris, or glowing cave
// crystal shards depending on `theme`. Each strand sways on its own phase
// so the clump doesn't move as one rigid piece (cave crystals sway only
// slightly, reading as more rigid). `alpha_mult` lets the same slot draw
// more than one theme's decoration at once, faded, to crossfade smoothly
// as the zone changes.
static void ff_draw_seaweed(cairo_t *cr, double x, double base_y, double height, double t,
                             int theme, double alpha_mult) {
    if (alpha_mult <= 0.0) return;
    double r, g, b, a, sway_mult;
    if (theme == FF_THEME_SHIP) { r = 0.30; g = 0.20; b = 0.10; a = 0.70; sway_mult = 0.8; }
    else if (theme == FF_THEME_CAVE) { r = 0.55; g = 0.80; b = 0.95; a = 0.55; sway_mult = 0.25; }
    else { r = 0.16; g = 0.42; b = 0.28; a = 0.55; sway_mult = 1.0; }

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

// Cheap deterministic pseudo-random hash - same input always gives the same
// output, so coral shapes stay put frame to frame instead of flickering.
static double ff_coral_hash(double n) {
    double x = sin(n * 127.1) * 43758.5453;
    return x - floor(x);
}

// Reef color families - warm coral tones instead of the old pipe green.
// Picked per obstacle so each one reads as a single coherent coral head.
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
        double rr = radius * (0.68 + 0.55 * ff_coral_hash(seed * 9.1 + i * 3.3));
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
    int grooves = 2 + (int)(ff_coral_hash(seed * 4.4) * 2);
    for (int gi = 0; gi < grooves; gi++) {
        double gang = ff_coral_hash(seed * 6.6 + gi * 2.7) * 2 * M_PI;
        double gx = cx + cos(gang) * radius * 0.5;
        double gy = cy + sin(gang) * radius * 0.5;
        double bend = (ff_coral_hash(seed * 8.8 + gi * 1.9) - 0.5) * radius * 0.9;
        cairo_move_to(cr, gx - radius * 0.55, gy - bend * 0.3);
        cairo_curve_to(cr, gx - radius * 0.2, gy + bend,
                            gx + radius * 0.2, gy - bend,
                            gx + radius * 0.55, gy + bend * 0.3);
        cairo_stroke(cr);
    }
    cairo_restore(cr);

    // Polyp flecks scattered near the surface.
    int polyps = 3 + (int)(ff_coral_hash(seed * 2.2) * 3);
    for (int p = 0; p < polyps; p++) {
        double pang = ff_coral_hash(seed * 11.3 + p * 1.9) * 2 * M_PI;
        double prad = radius * (0.45 + 0.35 * ff_coral_hash(seed * 13.7 + p * 2.4));
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
    if (ff_coral_hash(seed * 15.5) > 0.45) {
        double bang = (ff_coral_hash(seed * 19.3) - 0.5) * M_PI * 1.3 - M_PI * 0.5;
        double blen = radius * (0.35 + 0.35 * ff_coral_hash(seed * 21.1));
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
static void ff_draw_coral_column(cairo_t *cr, double x, double y0, double y1,
                                  double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFCoralPalette *pal =
        &FF_CORAL_PALETTES[(int)(ff_coral_hash(seed) * 97.0) % FF_CORAL_PALETTE_COUNT];

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

        double h1 = ff_coral_hash(seed * 3.7 + i * 1.31);
        double h2 = ff_coral_hash(seed * 5.9 + i * 2.03);
        double radius = shape_w * 0.5 * (0.78 + 0.32 * h1);
        double jitter_x = (h2 - 0.5) * shape_w * 0.20;

        ff_draw_coral_lobe(cr, cx + jitter_x, lobe_y, radius, seed * 1.3 + i * 7.1, pal, tip_t);
    }

    // A fuller three-lobe cluster right at the gap-facing tip, like a coral
    // head opening up, instead of the old flared pipe cap.
    double tip_y = tip_at_y1 ? y1 : y0;
    for (int k = -1; k <= 1; k++) {
        double h3 = ff_coral_hash(seed * 17.0 + k * 2.2);
        double head_r = shape_w * 0.30 * (0.85 + 0.3 * h3);
        double head_x = cx + k * shape_w * 0.30;
        double head_y = tip_y + (tip_at_y1 ? -head_r * 0.4 : head_r * 0.4);
        ff_draw_coral_lobe(cr, head_x, head_y, head_r, seed * 23.0 + k * 5.5, pal, 1.0);
    }
}

// --- Pirate ship theme -----------------------------------------------------

typedef struct {
    double wood_r, wood_g, wood_b;
    double wood_dark_r, wood_dark_g, wood_dark_b;
    double band_r, band_g, band_b;
} FFShipPalette;

static const FFShipPalette FF_SHIP_PALETTES[] = {
    {0.55, 0.36, 0.18,  0.35, 0.22, 0.10,  0.18, 0.18, 0.20}, // oak / iron bands
    {0.42, 0.26, 0.14,  0.26, 0.15, 0.08,  0.55, 0.45, 0.20}, // mahogany / brass bands
    {0.48, 0.42, 0.34,  0.30, 0.26, 0.20,  0.14, 0.14, 0.16}, // weathered grey wood
};
#define FF_SHIP_PALETTE_COUNT (int)(sizeof(FF_SHIP_PALETTES) / sizeof(FF_SHIP_PALETTES[0]))

// A rectangular wooden beam segment with plank seams and grain streaks.
static void ff_draw_ship_beam(cairo_t *cr, double cx, double cy, double w, double h,
                               double seed, const FFShipPalette *pal) {
    double x0 = cx - w * 0.5, y0 = cy - h * 0.5;
    cairo_set_source_rgb(cr, pal->wood_r, pal->wood_g, pal->wood_b);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, pal->wood_dark_r, pal->wood_dark_g, pal->wood_dark_b, 0.55);
    cairo_set_line_width(cr, fmax(1.0, h * 0.05));
    for (int i = 1; i < 3; i++) {
        double py = y0 + h * i / 3.0;
        cairo_move_to(cr, x0, py);
        cairo_line_to(cr, x0 + w, py);
        cairo_stroke(cr);
    }
    cairo_set_line_width(cr, fmax(1.0, w * 0.035));
    for (int i = 0; i < 3; i++) {
        double gx = x0 + w * (0.2 + 0.3 * i) + (ff_coral_hash(seed * 3.3 + i) - 0.5) * w * 0.08;
        cairo_move_to(cr, gx, y0 + h * 0.08);
        cairo_line_to(cr, gx, y0 + h * 0.92);
        cairo_stroke(cr);
    }

    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, x0, y0, w, h);
    cairo_stroke(cr);
}

// A barrel: an ellipse body plus two metal bands.
static void ff_draw_ship_barrel(cairo_t *cr, double cx, double cy, double w, double h,
                                 const FFShipPalette *pal) {
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, w * 0.5, h * 0.5);
    cairo_set_source_rgb(cr, fmin(1.0, pal->wood_r * 1.08), fmin(1.0, pal->wood_g * 1.05), pal->wood_b);
    cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);

    cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
    cairo_set_line_width(cr, 2.0);
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, w * 0.5, h * 0.5);
    cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, pal->band_r, pal->band_g, pal->band_b);
    cairo_set_line_width(cr, fmax(1.0, h * 0.12));
    cairo_move_to(cr, cx - w * 0.48, cy - h * 0.22);
    cairo_line_to(cr, cx + w * 0.48, cy - h * 0.22);
    cairo_stroke(cr);
    cairo_move_to(cr, cx - w * 0.48, cy + h * 0.22);
    cairo_line_to(cr, cx + w * 0.48, cy + h * 0.22);
    cairo_stroke(cr);
}

// Draws one obstacle column as alternating wooden beams and barrels, with a
// small mast/yard-arm-and-flag cluster at the gap-facing tip in place of the
// coral head. Purely rectilinear (no random reach), so unlike coral it never
// needs a safety scale-down to stay inside the collision box - beams/barrels
// are sized directly from `width` and the yard arm/flag are hand-clamped to
// stay well inside it.
static void ff_draw_ship_column(cairo_t *cr, double x, double y0, double y1,
                                 double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFShipPalette *pal =
        &FF_SHIP_PALETTES[(int)(ff_coral_hash(seed * 41.0) * 97.0) % FF_SHIP_PALETTE_COUNT];

    double cx = x + width * 0.5;
    double total_h = y1 - y0;
    double seg_h = width * 0.62;
    int segs = (int)fmax(2.0, round(total_h / seg_h));
    seg_h = total_h / segs;

    for (int i = 0; i < segs; i++) {
        double seg_y = y0 + (i + 0.5) * seg_h;
        bool barrel = ((i % 2) == 0) == tip_at_y1;
        double jitter = (ff_coral_hash(seed * 7.7 + i * 1.9) - 0.5) * width * 0.06;
        double seg_w = width * 0.88;
        double seg_hh = seg_h * 0.92;
        if (barrel) {
            ff_draw_ship_barrel(cr, cx + jitter, seg_y, seg_w, seg_hh, pal);
        } else {
            ff_draw_ship_beam(cr, cx + jitter, seg_y, seg_w, seg_hh, seed + i * 3.1, pal);
        }
    }

    // Mast post + yard arm + flag right at the gap-facing tip, entirely on
    // the rooted side of the boundary so nothing pokes into the gap itself.
    double tip_y = tip_at_y1 ? y1 : y0;
    double inward = tip_at_y1 ? -1.0 : 1.0;
    double post_h = fmin(width * 0.5, total_h * 0.9);
    double post_cy = tip_y + inward * post_h * 0.5;
    ff_draw_ship_beam(cr, cx, post_cy, width * 0.28, post_h, seed * 13.0, pal);

    double arm_y = tip_y + inward * width * 0.10;
    double arm_len = width * 0.30;
    cairo_set_source_rgb(cr, pal->wood_dark_r, pal->wood_dark_g, pal->wood_dark_b);
    cairo_set_line_width(cr, fmax(1.0, width * 0.08));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, cx - arm_len, arm_y);
    cairo_line_to(cr, cx + arm_len, arm_y);
    cairo_stroke(cr);

    // Jolly Roger - a black flag with a small skull-and-crossbones, the one
    // symbol that unmistakably says "pirate" at a glance.
    double flag_x = cx + arm_len;
    double flag_w = width * 0.15, flag_h = width * 0.13;
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_move_to(cr, flag_x, arm_y - flag_h * 0.5);
    cairo_line_to(cr, flag_x + flag_w, arm_y);
    cairo_line_to(cr, flag_x, arm_y + flag_h * 0.5);
    cairo_close_path(cr);
    cairo_fill(cr);

    double skx = flag_x + flag_w * 0.38, sky = arm_y;
    double skr = flag_h * 0.22;
    cairo_set_line_width(cr, fmax(1.0, flag_h * 0.07));
    cairo_set_source_rgb(cr, 0.94, 0.94, 0.90);
    cairo_move_to(cr, skx - skr, sky + skr * 0.9);
    cairo_line_to(cr, skx + skr, sky - skr * 0.9);
    cairo_stroke(cr);
    cairo_move_to(cr, skx - skr, sky - skr * 0.9);
    cairo_line_to(cr, skx + skr, sky + skr * 0.9);
    cairo_stroke(cr);
    cairo_arc(cr, skx, sky, skr, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_arc(cr, skx - skr * 0.35, sky - skr * 0.1, skr * 0.22, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, skx + skr * 0.35, sky - skr * 0.1, skr * 0.22, 0, 2 * M_PI);
    cairo_fill(cr);
}

// --- Dark cave theme --------------------------------------------------------

typedef struct {
    double rock_r, rock_g, rock_b;
    double crystal_r, crystal_g, crystal_b;
} FFCavePalette;

static const FFCavePalette FF_CAVE_PALETTES[] = {
    {0.16, 0.15, 0.21,  0.45, 0.85, 0.95}, // dark slate / cyan crystal
    {0.14, 0.11, 0.17,  0.75, 0.45, 0.95}, // near-black / purple crystal
    {0.19, 0.13, 0.13,  0.95, 0.55, 0.30}, // dark red rock / amber crystal
    {0.11, 0.16, 0.15,  0.50, 0.95, 0.65}, // dark green rock / green crystal
};
#define FF_CAVE_PALETTE_COUNT (int)(sizeof(FF_CAVE_PALETTES) / sizeof(FF_CAVE_PALETTES[0]))

// A jagged stalactite/stalagmite: a polygon tapering from full width at the
// rooted end down to a point at the gap-facing tip, with per-segment jitter
// scaled to that segment's own remaining half-width - so however jagged it
// gets, it can never reach past the pipe_width edges it started from, no
// clipping or scale-down needed. A handful of small glowing crystal flecks
// are studded along it for color and to sell the "cave" read.
static void ff_draw_cave_column(cairo_t *cr, double x, double y0, double y1,
                                 double width, double seed, bool tip_at_y1) {
    if (y1 <= y0) return;
    const FFCavePalette *pal =
        &FF_CAVE_PALETTES[(int)(ff_coral_hash(seed * 53.0) * 97.0) % FF_CAVE_PALETTE_COUNT];

    double cx = x + width * 0.5;
    const int segs = 7;
    double leftx[segs + 1], rightx[segs + 1], ys[segs + 1], halfs[segs + 1];
    for (int i = 0; i <= segs; i++) {
        double t = (double)i / segs;
        double y = y0 + t * (y1 - y0);
        double tip_t = tip_at_y1 ? t : 1.0 - t; // 0 at rooted end, 1 at the point
        double half = width * 0.5 * (1.0 - tip_t);
        double jl = 0.70 + 0.30 * ff_coral_hash(seed * 5.1 + i * 1.7);
        double jr = 0.70 + 0.30 * ff_coral_hash(seed * 6.3 + i * 2.1);
        leftx[i] = cx - half * jl;
        rightx[i] = cx + half * jr;
        ys[i] = y;
        halfs[i] = half;
    }

    cairo_new_path(cr);
    cairo_move_to(cr, leftx[0], ys[0]);
    for (int i = 1; i <= segs; i++) cairo_line_to(cr, leftx[i], ys[i]);
    for (int i = segs; i >= 0; i--) cairo_line_to(cr, rightx[i], ys[i]);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, pal->rock_r, pal->rock_g, pal->rock_b);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
    cairo_set_line_width(cr, fmax(1.0, width * 0.03));
    cairo_stroke(cr);

    // Crack texture.
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_set_line_width(cr, fmax(1.0, width * 0.025));
    double seg_span = (y1 - y0) / segs;
    for (int i = 1; i < segs; i += 2) {
        double midx = (leftx[i] + rightx[i]) * 0.5;
        cairo_move_to(cr, midx - halfs[i] * 0.3, ys[i] - seg_span * 0.3);
        cairo_line_to(cr, midx + halfs[i] * 0.25, ys[i] + seg_span * 0.3);
        cairo_stroke(cr);
    }

    // Glowing crystal flecks, kept well inside this segment's own bounds.
    for (int i = 1; i < segs; i++) {
        if (ff_coral_hash(seed * 8.8 + i * 2.3) < 0.55) continue;
        if (halfs[i] < width * 0.04) continue;
        double cxx = cx + (ff_coral_hash(seed * 9.9 + i * 3.1) - 0.5) * halfs[i];
        double crad = width * 0.04 + width * 0.025 * ff_coral_hash(seed * 10.5 + i);
        cairo_set_source_rgba(cr, pal->crystal_r, pal->crystal_g, pal->crystal_b, 0.25);
        cairo_arc(cr, cxx, ys[i], crad * 2.0, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, pal->crystal_r, pal->crystal_g, pal->crystal_b, 0.95);
        cairo_arc(cr, cxx, ys[i], crad, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// Picks which theme's obstacle art to draw - the one piece of code that
// needs to know all three exist.
static void ff_draw_obstacle_column(int theme, cairo_t *cr, double x, double y0, double y1,
                                     double width, double seed, bool tip_at_y1) {
    switch (theme) {
        case FF_THEME_SHIP: ff_draw_ship_column(cr, x, y0, y1, width, seed, tip_at_y1); break;
        case FF_THEME_CAVE: ff_draw_cave_column(cr, x, y0, y1, width, seed, tip_at_y1); break;
        default: ff_draw_coral_column(cr, x, y0, y1, width, seed, tip_at_y1); break;
    }
}

// Smoothly fades a critter to transparent as it nears/crosses either screen
// edge, so it visibly dissolves instead of popping in/out at the boundary.
static double ff_edge_fade(double x, double w) {
    double fade_zone = w * 0.15;
    double fade = 1.0;
    if (x < fade_zone) fade = x / fade_zone;
    else if (x > w - fade_zone) fade = (w - x) / fade_zone;
    if (fade < 0.0) fade = 0.0;
    if (fade > 1.0) fade = 1.0;
    return fade;
}

// The single biggest thing that makes the ship theme actually read as a
// shipwreck rather than "some brown blobs underwater": one large, clearly
// boat-shaped hull silhouette lying on the seabed, with rib-frame lines
// showing through broken planking, a few portholes, and a broken mast with
// a tattered sail leaning out of it.
static void ff_draw_ship_hull_backdrop(cairo_t *cr, double w, double h, double base_y) {
    double hull_w = w * 0.60;
    double hull_x = w * 0.28;
    double hull_h = h * 0.30;
    double hull_top = base_y - hull_h;

    cairo_set_source_rgba(cr, 0.05, 0.08, 0.09, 0.65);
    cairo_move_to(cr, hull_x, base_y);
    cairo_curve_to(cr, hull_x + hull_w * 0.05, hull_top + hull_h * 0.15,
                        hull_x + hull_w * 0.30, hull_top,
                        hull_x + hull_w * 0.5, hull_top);
    cairo_curve_to(cr, hull_x + hull_w * 0.70, hull_top,
                        hull_x + hull_w * 0.95, hull_top + hull_h * 0.15,
                        hull_x + hull_w, base_y);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);

    // Rib frame lines showing through broken planking - the detail that
    // most says "wrecked hull" rather than a plain rounded mound.
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_set_line_width(cr, 3.0);
    for (int i = 1; i < 7; i++) {
        double t = i / 7.0;
        double rx = hull_x + hull_w * t;
        double ry_top = hull_top + hull_h * (0.10 + 0.55 * fabs(sin(t * M_PI)));
        cairo_move_to(cr, rx, base_y);
        cairo_line_to(cr, rx, ry_top);
        cairo_stroke(cr);
    }

    // Portholes.
    cairo_set_source_rgba(cr, 0, 0, 0, 0.55);
    for (int i = 0; i < 3; i++) {
        double px = hull_x + hull_w * (0.32 + 0.18 * i);
        double py = hull_top + hull_h * 0.55;
        cairo_arc(cr, px, py, hull_h * 0.085, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_set_source_rgba(cr, 0.5, 0.75, 0.85, 0.35);
    for (int i = 0; i < 3; i++) {
        double px = hull_x + hull_w * (0.32 + 0.18 * i);
        double py = hull_top + hull_h * 0.55;
        cairo_arc(cr, px, py, hull_h * 0.05, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Broken mast leaning out of the hull, with a tattered sail.
    double mast_x = hull_x + hull_w * 0.66;
    double mast_base_y = hull_top + hull_h * 0.30;
    double mast_top_y = mast_base_y - hull_h * 1.15;
    double mast_top_x = mast_x + hull_h * 0.18;
    cairo_set_source_rgba(cr, 0.05, 0.08, 0.09, 0.65);
    cairo_set_line_width(cr, fmax(2.0, hull_h * 0.045));
    cairo_move_to(cr, mast_x, mast_base_y);
    cairo_line_to(cr, mast_top_x, mast_top_y);
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 0.15, 0.20, 0.20, 0.5);
    cairo_move_to(cr, mast_top_x, mast_top_y);
    cairo_line_to(cr, mast_top_x + hull_h * 0.48, mast_top_y + hull_h * 0.22);
    cairo_line_to(cr, mast_top_x + hull_h * 0.10, mast_top_y + hull_h * 0.50);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Faint sunbeam shafts slanting down - classic sunken-wreck lighting.
    cairo_set_source_rgba(cr, 1.0, 1.0, 0.9, 0.06);
    for (int i = 0; i < 3; i++) {
        double bx = w * (0.15 + 0.32 * i);
        cairo_move_to(cr, bx, 0);
        cairo_line_to(cr, bx + w * 0.10, 0);
        cairo_line_to(cr, bx - w * 0.05, base_y);
        cairo_line_to(cr, bx - w * 0.15, base_y);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

// Sky gradient, ambient particles, and a distant skyline silhouette for one
// theme. Meant to be composited via cairo_push_group/paint_with_alpha at
// the call site to crossfade between two themes, so everything in here
// just paints at full opacity as if it were the only thing on screen.
static void ff_draw_theme_sky(cairo_t *cr, int theme, double w, double h, double bubble_phase) {
    cairo_pattern_t *sky = cairo_pattern_create_linear(0, 0, 0, h);
    if (theme == FF_THEME_SHIP) {
        cairo_pattern_add_color_stop_rgb(sky, 0.0, 0.10, 0.32, 0.40);
        cairo_pattern_add_color_stop_rgb(sky, 1.0, 0.16, 0.42, 0.46);
    } else if (theme == FF_THEME_CAVE) {
        cairo_pattern_add_color_stop_rgb(sky, 0.0, 0.04, 0.04, 0.09);
        cairo_pattern_add_color_stop_rgb(sky, 1.0, 0.11, 0.09, 0.17);
    } else {
        cairo_pattern_add_color_stop_rgb(sky, 0.0, 0.35, 0.72, 0.85);
        cairo_pattern_add_color_stop_rgb(sky, 1.0, 0.55, 0.85, 0.88);
    }
    cairo_set_source(cr, sky);
    cairo_paint(cr);
    cairo_pattern_destroy(sky);

    // Ambient particles: rising bubbles for reef/ship, dim drifting motes
    // for the cave.
    for (int i = 0; i < 22; i++) {
        double bx = fmod(i * 53.0 + w * 0.5, w);
        double speed = 30.0 + (i % 5) * 10.0;
        double by = h - fmod(bubble_phase * speed + i * 71.0, h + 40.0);
        double size = 2.0 + (i % 4);
        if (theme == FF_THEME_CAVE) cairo_set_source_rgba(cr, 0.55, 0.75, 0.95, 0.18);
        else cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.25);
        cairo_arc(cr, bx, by, size, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Distant skyline silhouette: coral bumps, a real sunken-hull wreck, or
    // jagged cave rock.
    double floor_h = h * 0.10;
    double base_y = h - floor_h;
    if (theme == FF_THEME_SHIP) {
        ff_draw_ship_hull_backdrop(cr, w, h, base_y);
        return;
    }
    if (theme == FF_THEME_CAVE) cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.6);
    else cairo_set_source_rgba(cr, 0.25, 0.55, 0.5, 0.55);

    cairo_move_to(cr, 0, base_y);
    if (theme == FF_THEME_CAVE) {
        for (double x = 0; x <= w + 1; x += w / 18.0) {
            double bump = 24.0 + 30.0 * fabs(sin(x * 0.045 + 2.3));
            cairo_line_to(cr, x, base_y - bump);
        }
    } else {
        for (double x = 0; x <= w + 1; x += w / 10.0) {
            double bump = 18.0 + 14.0 * sin(x * 0.02 + 1.7);
            cairo_line_to(cr, x, base_y - bump);
        }
    }
    cairo_line_to(cr, w, base_y);
    cairo_close_path(cr);
    cairo_fill(cr);
}

// Floor for one theme: sand, ship-deck planking, or dark rock with glowing
// flecks. Same crossfade-via-compositing contract as ff_draw_theme_sky.
static void ff_draw_theme_floor(cairo_t *cr, int theme, double w, double h, double floor_h,
                                 double bubble_phase) {
    if (theme == FF_THEME_SHIP) {
        cairo_set_source_rgb(cr, 0.42, 0.28, 0.16);
        cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.25, 0.15, 0.08, 0.7);
        cairo_set_line_width(cr, 3.0);
        for (double x = -fmod(bubble_phase * (h * 0.34), 60.0); x < w; x += 60.0) {
            cairo_move_to(cr, x, h - floor_h);
            cairo_line_to(cr, x, h);
            cairo_stroke(cr);
        }
        cairo_set_source_rgba(cr, 0.2, 0.12, 0.06, 0.6);
        cairo_set_line_width(cr, 2.0);
        cairo_move_to(cr, 0, h - floor_h * 0.5);
        cairo_line_to(cr, w, h - floor_h * 0.5);
        cairo_stroke(cr);

        // A scatter of half-buried gold coins/doubloons.
        for (int i = 0; i < 10; i++) {
            double cxx = fmod(i * 173.0 + 40.0, w);
            double cyy = h - floor_h * (0.25 + 0.4 * ((i * 37) % 5) / 5.0);
            double r = floor_h * 0.06;
            cairo_set_source_rgb(cr, 0.85, 0.68, 0.20);
            cairo_save(cr);
            cairo_translate(cr, cxx, cyy);
            cairo_scale(cr, 1.0, 0.55);
            cairo_arc(cr, 0, 0, r, 0, 2 * M_PI);
            cairo_restore(cr);
            cairo_fill(cr);
            cairo_set_source_rgba(cr, 0.55, 0.42, 0.08, 0.7);
            cairo_save(cr);
            cairo_translate(cr, cxx, cyy);
            cairo_scale(cr, 1.0, 0.55);
            cairo_arc(cr, 0, 0, r * 0.6, 0, 2 * M_PI);
            cairo_restore(cr);
            cairo_stroke(cr);
        }
    } else if (theme == FF_THEME_CAVE) {
        cairo_set_source_rgb(cr, 0.10, 0.09, 0.13);
        cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.16, 0.14, 0.20);
        cairo_move_to(cr, 0, h - floor_h);
        for (double x = 0; x <= w + 1; x += w / 24.0) {
            double bump = 4.0 + 8.0 * fabs(sin(x * 0.09 + 1.1));
            cairo_line_to(cr, x, h - floor_h - bump);
        }
        cairo_line_to(cr, w, h - floor_h);
        cairo_close_path(cr);
        cairo_fill(cr);
        for (int i = 0; i < 14; i++) {
            double fx = fmod(i * 137.0, w);
            double fy = h - floor_h * 0.4 + (i % 3) * floor_h * 0.15;
            cairo_set_source_rgba(cr, 0.5, 0.8, 0.95, 0.5);
            cairo_arc(cr, fx, fy, 2.0, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    } else {
        cairo_set_source_rgb(cr, 0.87, 0.78, 0.55);
        cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.75, 0.65, 0.35, 0.6);
        for (double x = -fmod(bubble_phase * (h * 0.34), 24.0); x < w; x += 24.0) {
            cairo_move_to(cr, x, h - floor_h);
            cairo_line_to(cr, x + 12, h);
            cairo_set_line_width(cr, 6.0);
            cairo_stroke(cr);
        }
    }
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
    // travels between reef/ship/cave as one continuous blend rather than a
    // set of independently-changing pieces.
    double zone_len = h * FF_ZONE_LEN_FRAC;
    double trans_len = h * FF_ZONE_TRANS_FRAC;
    int theme_from, theme_to;
    double blend_t;
    ff_theme_at(s_ff_world_x, zone_len, trans_len, &theme_from, &theme_to, &blend_t);
    double base_y = h - floor_h;

    // Sky/backdrop, crossfaded as a single composited layer so its internal
    // bubbles/skyline never double-blend against each other mid-transition.
    ff_draw_theme_sky(cr, theme_from, w, h, s_ff_bubble_phase);
    if (blend_t > 0.0) {
        cairo_push_group(cr);
        ff_draw_theme_sky(cr, theme_to, w, h, s_ff_bubble_phase);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, blend_t);
    }

    // Floor decoration - reef seaweed, ship rope/plank debris, or glowing
    // cave crystal shards - fades in/out with how "present" each theme
    // currently is, so it blends right along with the sky/floor around it.
    double reef_a = (theme_from == FF_THEME_REEF ? (1.0 - blend_t) : 0.0) + (theme_to == FF_THEME_REEF ? blend_t : 0.0);
    double ship_a = (theme_from == FF_THEME_SHIP ? (1.0 - blend_t) : 0.0) + (theme_to == FF_THEME_SHIP ? blend_t : 0.0);
    double cave_a = (theme_from == FF_THEME_CAVE ? (1.0 - blend_t) : 0.0) + (theme_to == FF_THEME_CAVE ? blend_t : 0.0);
    for (int i = 0; i < FF_SEAWEED_COUNT; i++) {
        double t_phase = vis->time_offset + s_ff_seaweed_phase[i];
        double sh = h * 0.16 * s_ff_seaweed_height_frac[i];
        if (reef_a > 0.001) ff_draw_seaweed(cr, s_ff_seaweed_x[i], base_y, sh, t_phase, FF_THEME_REEF, reef_a);
        if (ship_a > 0.001) ff_draw_seaweed(cr, s_ff_seaweed_x[i], base_y, sh, t_phase, FF_THEME_SHIP, ship_a);
        if (cave_a > 0.001) ff_draw_seaweed(cr, s_ff_seaweed_x[i], base_y, sh, t_phase, FF_THEME_CAVE, cave_a);
    }

    // Little background friends - fish and an octopus drifting behind the
    // pipes, purely decorative. The shark is a rare guest, drawn dim and
    // furthest back of all so it never competes with the pipes/fish.
    if (s_ff_shark_active) {
        ff_draw_shark(cr, s_ff_shark_x, s_ff_shark_y, vis->time_offset, s_ff_shark_dir,
                       ff_edge_fade(s_ff_shark_x, w));
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
        if (!s_ff_pipes[i].active) continue;
        double gap = h * 0.24;
        double gc = s_ff_pipes[i].gap_center;
        ff_draw_obstacle_column(s_ff_pipes[i].theme, cr, s_ff_pipes[i].x, 0, gc - gap * 0.5, pipe_width, gc, true);
        ff_draw_obstacle_column(s_ff_pipes[i].theme, cr, s_ff_pipes[i].x, gc + gap * 0.5, h - floor_h, pipe_width, gc, false);
    }

    // Floor, crossfaded the same way as the sky.
    ff_draw_theme_floor(cr, theme_from, w, h, floor_h, s_ff_bubble_phase);
    if (blend_t > 0.0) {
        cairo_push_group(cr);
        ff_draw_theme_floor(cr, theme_to, w, h, floor_h, s_ff_bubble_phase);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, blend_t);
    }

    // Fish
    ff_draw_fish(cr, fish_x, s_ff_fish_y, fish_radius, s_ff_rotation, s_ff_flap_anim,
                 &FF_FISH_PALETTES[s_ff_fish_palette], 1.0);

    // Score
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
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
        const char *msg4 = "Keep Clicking to Swim Up and Avoid Obstacles; Not Clicking causes Floppy Fish to Sink";
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
        cairo_rectangle(cr, w * 0.12, h * 0.35, w * 0.76, h * 0.24);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_set_font_size(cr, h * 0.05);
        const char *msg = "Game Over";
        cairo_text_extents(cr, msg, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.42);
        cairo_show_text(cr, msg);

        cairo_set_font_size(cr, h * 0.032);
        char best_text[50];
        snprintf(best_text, sizeof(best_text), "Best: %d", s_ff_best_score);
        cairo_text_extents(cr, best_text, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.52);
        cairo_show_text(cr, best_text);

#ifdef FLOPPYSOUND
        snprintf(best_text, sizeof(best_text), "Left Click to Restart or Escape to Quit");
#else
        snprintf(best_text, sizeof(best_text), "Left Click to Restart");
#endif
        cairo_text_extents(cr, best_text, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.56);
        cairo_show_text(cr, best_text);


    }
}
