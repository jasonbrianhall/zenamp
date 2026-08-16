#include "visualization.h"
#include <stdlib.h>

// ---- Flappy Fish ----
// A Flappy Bird-style game: click to flap, swim between scrolling pipes,
// don't hit the coral or the sand. Fully mouse-driven, runs continuously
// like the other interactive games regardless of whether music is playing.

#define FF_MAX_PIPES 8
#define FF_BG_FISH_COUNT 7

typedef enum { FF_READY, FF_PLAYING, FF_GAME_OVER } FFState;

typedef struct {
    double x;
    double gap_center;
    bool scored;
    bool active;
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
}

static void ff_flap(Visualizer *vis) {
    s_ff_fish_vel = -vis->height * 0.62;
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

        s_ff_pipes[i].x = vis->width;
        s_ff_pipes[i].gap_center = gap_center;
        s_ff_pipes[i].scored = false;
        s_ff_pipes[i].active = true;
        return;
    }
}

void update_floppy_fish(Visualizer *vis, double dt) {
    if (vis->width <= 0 || vis->height <= 0) return;

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
            }

            // Circle-vs-rect collision against the top and bottom pipe segments.
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

// A little cluster of swaying seaweed strands rooted at the floor. Each
// strand sways on its own phase so the clump doesn't move as one rigid
// piece. Drawn as open, dim, thick-stroked curves rather than filled shapes
// so they read as thin fronds rather than solid blobs.
static void ff_draw_seaweed(cairo_t *cr, double x, double base_y, double height, double t) {
    int strands = 3;
    for (int i = 0; i < strands; i++) {
        double sx = x + (i - 1) * height * 0.14;
        double sh = height * (0.75 + 0.25 * (i % 2));
        double phase = t * 1.1 + i * 1.7;
        double sway1 = sin(phase) * sh * 0.16;
        double sway2 = sin(phase * 0.8 + 0.6) * sh * 0.30;

        cairo_set_source_rgba(cr, 0.16, 0.42, 0.28, 0.55);
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

static void ff_draw_pipe_segment(cairo_t *cr, double x, double y0, double y1, double width, bool cap_at_bottom) {
    if (y1 <= y0) return;
    double cap_h = width * 0.28;

    cairo_set_source_rgb(cr, 0.28, 0.62, 0.30);
    cairo_rectangle(cr, x, y0, width, y1 - y0);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.20, 0.48, 0.22);
    double cap_y = cap_at_bottom ? y1 - cap_h : y0;
    cairo_rectangle(cr, x - width * 0.08, cap_y, width * 1.16, cap_h);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.12, 0.30, 0.14, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, x, y0, width, y1 - y0);
    cairo_stroke(cr);
    cairo_rectangle(cr, x - width * 0.08, cap_y, width * 1.16, cap_h);
    cairo_stroke(cr);
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

void draw_floppy_fish(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;

    double w = vis->width, h = vis->height;
    double floor_h = h * 0.10;
    double pipe_width = h * 0.16;
    double fish_radius = h * 0.032;
    double fish_x = w * FF_FISH_X_FRAC;

    // Underwater gradient backdrop.
    cairo_pattern_t *sky = cairo_pattern_create_linear(0, 0, 0, h);
    cairo_pattern_add_color_stop_rgb(sky, 0.0, 0.35, 0.72, 0.85);
    cairo_pattern_add_color_stop_rgb(sky, 1.0, 0.55, 0.85, 0.88);
    cairo_set_source(cr, sky);
    cairo_paint(cr);
    cairo_pattern_destroy(sky);

    // Rising ambient bubbles.
    for (int i = 0; i < 22; i++) {
        double bx = fmod(i * 53.0 + w * 0.5, w);
        double speed = 30.0 + (i % 5) * 10.0;
        double by = h - fmod(s_ff_bubble_phase * speed + i * 71.0, h + 40.0);
        double size = 2.0 + (i % 4);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.25);
        cairo_arc(cr, bx, by, size, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Distant coral-reef "skyline" silhouette.
    cairo_set_source_rgba(cr, 0.25, 0.55, 0.5, 0.55);
    double base_y = h - floor_h;
    cairo_move_to(cr, 0, base_y);
    for (double x = 0; x <= w + 1; x += w / 10.0) {
        double bump = 18.0 + 14.0 * sin(x * 0.02 + 1.7);
        cairo_line_to(cr, x, base_y - bump);
    }
    cairo_line_to(cr, w, base_y);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Seaweed clumps swaying and drifting along the floor, behind pipes/fish.
    for (int i = 0; i < FF_SEAWEED_COUNT; i++) {
        ff_draw_seaweed(cr, s_ff_seaweed_x[i], base_y, h * 0.16 * s_ff_seaweed_height_frac[i],
                         vis->time_offset + s_ff_seaweed_phase[i]);
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

    // Pipes (coral/kelp tubes).
    for (int i = 0; i < FF_MAX_PIPES; i++) {
        if (!s_ff_pipes[i].active) continue;
        double gap = h * 0.24;
        double gc = s_ff_pipes[i].gap_center;
        ff_draw_pipe_segment(cr, s_ff_pipes[i].x, 0, gc - gap * 0.5, pipe_width, true);
        ff_draw_pipe_segment(cr, s_ff_pipes[i].x, gc + gap * 0.5, h - floor_h, pipe_width, false);
    }

    // Sandy floor.
    cairo_set_source_rgb(cr, 0.87, 0.78, 0.55);
    cairo_rectangle(cr, 0, h - floor_h, w, floor_h);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.75, 0.65, 0.35, 0.6);
    for (double x = -fmod(s_ff_bubble_phase * (h * 0.34), 24.0); x < w; x += 24.0) {
        cairo_move_to(cr, x, h - floor_h);
        cairo_line_to(cr, x + 12, h);
        cairo_set_line_width(cr, 6.0);
        cairo_stroke(cr);
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
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
        cairo_set_font_size(cr, h * 0.045);
        const char *msg = "Click to Start";
        cairo_text_extents(cr, msg, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.62);
        cairo_show_text(cr, msg);
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
        char best_text[32];
        snprintf(best_text, sizeof(best_text), "Best: %d   Click to Restart", s_ff_best_score);
        cairo_text_extents(cr, best_text, &ext);
        cairo_move_to(cr, w * 0.5 - ext.width * 0.5, h * 0.52);
        cairo_show_text(cr, best_text);
    }
}
