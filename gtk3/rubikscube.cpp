// "Rubik's Cube" visualization — a self-scrambling, self-solving 3x3 cube.
// Rendered in Cairo with the same hand-rolled perspective projection and
// per-frame painter's-algorithm depth sort as pipes.cpp (no OpenGL, and no
// dependency on the real Kociemba solver in rubiksolver.py — it solves by
// simply replaying its own scramble in reverse, which is always correct and
// reads just as well visually).
//
// Interaction: click-drag spins the camera, scroll zooms — both work at any
// time, even paused/no audio. Layer turns are a separate concern: they only
// fire on a detected beat, so no music genuinely means no turning.

#include "visualization.h"
#include "audio_player.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// --- Small self-contained RNG (mirrors pipes.cpp's) ------------------------

static unsigned int rubik_rand(unsigned int *state) {
    unsigned int x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static double rubik_rand_double(unsigned int *state) {
    return (rubik_rand(state) & 0xFFFFFF) / (double)0x1000000;
}

// --- 3x3 rotation matrices --------------------------------------------------

static RubikMat3 rubik_mat3_identity(void) {
    RubikMat3 m;
    memset(&m, 0, sizeof(m));
    m.m[0][0] = m.m[1][1] = m.m[2][2] = 1.0;
    return m;
}

// Rotation by `angle_rad` about one of the 3 principal axes.
static RubikMat3 rubik_axis_rotation(int axis, double angle_rad) {
    double c = cos(angle_rad), s = sin(angle_rad);
    RubikMat3 m = rubik_mat3_identity();
    if (axis == 0) { // x
        m.m[1][1] = c; m.m[1][2] = -s;
        m.m[2][1] = s; m.m[2][2] = c;
    } else if (axis == 1) { // y
        m.m[0][0] = c; m.m[0][2] = s;
        m.m[2][0] = -s; m.m[2][2] = c;
    } else { // z
        m.m[0][0] = c; m.m[0][1] = -s;
        m.m[1][0] = s; m.m[1][1] = c;
    }
    return m;
}

static RubikMat3 rubik_mat3_mul(const RubikMat3 *a, const RubikMat3 *b) {
    RubikMat3 r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            double sum = 0.0;
            for (int k = 0; k < 3; k++) sum += a->m[i][k] * b->m[k][j];
            r.m[i][j] = sum;
        }
    return r;
}

static void rubik_mat3_apply(const RubikMat3 *m, double x, double y, double z,
                              double *ox, double *oy, double *oz) {
    *ox = m->m[0][0]*x + m->m[0][1]*y + m->m[0][2]*z;
    *oy = m->m[1][0]*x + m->m[1][1]*y + m->m[1][2]*z;
    *oz = m->m[2][0]*x + m->m[2][1]*y + m->m[2][2]*z;
}

// --- Canonical sticker colors (fixed to each cubie's original hull faces) --

typedef enum { RC_BLUE, RC_GREEN, RC_WHITE, RC_YELLOW, RC_RED, RC_ORANGE } RubikColorId;

static const double RUBIK_COLOR_RGB[6][3] = {
    {0.10, 0.35, 0.90}, // BLUE   (+x)
    {0.10, 0.75, 0.25}, // GREEN  (-x)
    {0.95, 0.95, 0.95}, // WHITE  (+y)
    {0.95, 0.85, 0.10}, // YELLOW (-y)
    {0.90, 0.10, 0.10}, // RED    (+z)
    {0.95, 0.55, 0.05}, // ORANGE (-z)
};

// Dark plastic body color used on faces that never carry a sticker
// (interior faces, or hull faces mid-cube hasn't reached yet). Drawing
// these too — instead of skipping them — is what keeps the cube reading as
// a solid block instead of a hollow wireframe while a layer is mid-turn and
// cubies visually separate from their neighbors.
static const double RUBIK_BODY_RGB[3] = {0.06, 0.06, 0.07};
// across the spectrum so the six colors visibly react to different parts
// of the mix.
static int rubik_color_band(int color_id) {
    return (color_id * VIS_FREQUENCY_BARS) / 6;
}

// Local face table: normal axis (0/1/2), normal sign, and the 4 local unit
// corners (indices into the 8-corner cube below) in winding order.
static const int RUBIK_CORNERS[8][3] = {
    {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
    {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1},
};
typedef struct { int axis; int sign; int corners[4]; int color_id; } RubikFaceDef;
static const RubikFaceDef RUBIK_FACES[6] = {
    {0,  1, {1,2,6,5}, RC_BLUE},
    {0, -1, {0,3,7,4}, RC_GREEN},
    {1,  1, {3,2,6,7}, RC_WHITE},
    {1, -1, {0,1,5,4}, RC_YELLOW},
    {2,  1, {4,5,6,7}, RC_RED},
    {2, -1, {0,1,2,3}, RC_ORANGE},
};

// --- Grid / move mechanics --------------------------------------------------

// Rotates an integer grid vector by a quarter turn; exact because cos/sin
// of 90-degree multiples land exactly on {-1,0,1}.
static void rubik_rotate_grid(int axis, int dir, int x, int y, int z,
                               int *ox, int *oy, int *oz) {
    RubikMat3 r = rubik_axis_rotation(axis, dir * M_PI / 2.0);
    double fx, fy, fz;
    rubik_mat3_apply(&r, (double)x, (double)y, (double)z, &fx, &fy, &fz);
    *ox = (int)lround(fx);
    *oy = (int)lround(fy);
    *oz = (int)lround(fz);
}

static int rubik_cubie_grid_axis(RubikCubie *c, int axis) {
    return axis == 0 ? c->gx : (axis == 1 ? c->gy : c->gz);
}

// Commits a full quarter turn immediately: updates grid position and
// accumulated orientation for every cubie in the affected layer.
static void rubik_apply_move_instant(RubiksCubeSystem *sys, RubikMove move) {
    RubikMat3 r = rubik_axis_rotation(move.axis, move.dir * M_PI / 2.0);
    for (int i = 0; i < RUBIK_CUBIE_COUNT; i++) {
        RubikCubie *c = &sys->cubies[i];
        if (rubik_cubie_grid_axis(c, move.axis) != move.layer) continue;

        int nx, ny, nz;
        rubik_rotate_grid(move.axis, move.dir, c->gx, c->gy, c->gz, &nx, &ny, &nz);
        c->gx = nx; c->gy = ny; c->gz = nz;
        c->orient = rubik_mat3_mul(&r, &c->orient);
    }
}

static void rubik_generate_scramble(RubiksCubeSystem *sys) {
    int last_axis = -1, last_layer = -2;
    for (int i = 0; i < RUBIK_SCRAMBLE_MOVE_COUNT; i++) {
        int axis, layer;
        do {
            axis = rubik_rand(&sys->rng_state) % 3;
            layer = (int)(rubik_rand(&sys->rng_state) % 3) - 1;
        } while (axis == last_axis && layer == last_layer); // avoid a no-op pair
        int dir = (rubik_rand(&sys->rng_state) % 2) ? 1 : -1;

        sys->scramble_moves[i].axis = axis;
        sys->scramble_moves[i].layer = layer;
        sys->scramble_moves[i].dir = dir;
        last_axis = axis;
        last_layer = layer;
    }
    sys->scramble_count = RUBIK_SCRAMBLE_MOVE_COUNT;
}

static void rubik_reset_cube(RubiksCubeSystem *sys) {
    int idx = 0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            for (int z = -1; z <= 1; z++) {
                RubikCubie *c = &sys->cubies[idx++];
                c->gx = c->orig_gx = x;
                c->gy = c->orig_gy = y;
                c->gz = c->orig_gz = z;
                c->orient = rubik_mat3_identity();
            }
    rubik_generate_scramble(sys);
    sys->move_index = 0;
    sys->solve_index = -1;
    sys->phase = RUBIK_PHASE_SCRAMBLING;
    sys->phase_timer = 0.0;
    sys->animating = false;
    sys->anim_progress = 0.0;
}

// Beat/loudness trigger, in the same spirit as pong_detect_beat/blockstack's
// beat detectors, but edge-triggered with a cooldown so a single sustained
// loud passage doesn't fire a dozen turns back to back. Also hard-gated on
// actual playback state, since stale frequency data can otherwise linger
// briefly right at pause/stop.
//
// Two ways to trigger a turn:
//   - onset: energy jumps noticeably above its own recent average (catches
//     rhythmic beats/kicks).
//   - loud: energy (or overall volume) is just flat-out loud, regardless of
//     whether it jumped — catches sustained loud passages/noise bursts that
//     an onset check alone would miss.
static bool rubik_detect_beat(Visualizer *vis, RubiksCubeSystem *sys, double dt) {
    if (sys->beat_cooldown > 0.0) sys->beat_cooldown -= dt;

    bool music_playing = player && player->is_playing && !player->is_paused;
    if (!music_playing) {
        sys->beat_energy_avg = 0.0;
        return false;
    }

    double energy = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) energy += vis->frequency_bands[i];
    energy /= VIS_FREQUENCY_BARS;

    bool onset = energy > 0.28 && energy > sys->beat_energy_avg * 1.25;
    bool loud = energy > 0.5 || vis->volume_level > 0.6;
    sys->beat_energy_avg += (energy - sys->beat_energy_avg) * 0.2;

    if ((onset || loud) && sys->beat_cooldown <= 0.0) {
        // A flat-out loud passage should feel busier than a subtle onset,
        // so give it a shorter cooldown rather than reusing the same one.
        sys->beat_cooldown = loud && !onset ? 0.12 : 0.18;
        return true;
    }
    return false;
}

void init_rubiks_cube_system(Visualizer *vis) {
    RubiksCubeSystem *sys = &vis->rubiks_cube;
    memset(sys, 0, sizeof(*sys));
    sys->rng_state = 0xB5297A4Du ^ (unsigned int)(size_t)vis;
    sys->camera_yaw = 0.5;
    sys->camera_pitch = 0.35;
    sys->camera_distance = RUBIK_CUBIE_SIZE * 8.0;
    sys->target_distance = sys->camera_distance;
    sys->user_zoom = 1.0;
    sys->dragging = false;
    sys->yaw_velocity = sys->pitch_velocity = 0.0;
    sys->idle_time = 999.0; // start already "settled" into auto-orbit
    rubik_reset_cube(sys);
}

// --- Update: phase machine + audio-reactive pacing/camera -------------------

void update_rubiks_cube_system(Visualizer *vis, double dt) {
    RubiksCubeSystem *sys = &vis->rubiks_cube;

    double mid_energy = 0.0;
    for (int i = VIS_FREQUENCY_BARS / 4; i < 3 * VIS_FREQUENCY_BARS / 4; i++)
        mid_energy += vis->frequency_bands[i];
    mid_energy /= (VIS_FREQUENCY_BARS / 2);

    // --- Mouse drag to manually spin the cube -------------------------
    if (vis->mouse_left_pressed) {
        if (!sys->dragging) {
            sys->dragging = true;
            sys->drag_last_mouse_x = vis->mouse_x;
            sys->drag_last_mouse_y = vis->mouse_y;
        } else {
            double dx = vis->mouse_x - sys->drag_last_mouse_x;
            double dy = vis->mouse_y - sys->drag_last_mouse_y;
            const double DRAG_SENSITIVITY = 0.008;
            double dyaw = dx * DRAG_SENSITIVITY;
            double dpitch = dy * DRAG_SENSITIVITY;

            sys->camera_yaw += dyaw;
            sys->camera_pitch += dpitch;

            if (dt > 1e-4) {
                sys->yaw_velocity = dyaw / dt;
                sys->pitch_velocity = dpitch / dt;
            }
            sys->drag_last_mouse_x = vis->mouse_x;
            sys->drag_last_mouse_y = vis->mouse_y;
        }
        sys->idle_time = 0.0;
    } else {
        sys->dragging = false;
    }

    // Scroll wheel zoom (a persistent multiplier layered under the
    // volume-driven "breathing" distance below, so it survives frame to
    // frame instead of being overwritten every tick).
    if (vis->scroll_direction != 0) {
        sys->user_zoom *= (1.0 - vis->scroll_direction * 0.08);
        if (sys->user_zoom < 0.4) sys->user_zoom = 0.4;
        if (sys->user_zoom > 2.5) sys->user_zoom = 2.5;
        sys->idle_time = 0.0;
    }

    if (!sys->dragging) {
        // Flick momentum: keep spinning briefly after release, decaying.
        sys->camera_yaw += sys->yaw_velocity * dt;
        sys->camera_pitch += sys->pitch_velocity * dt;
        double decay = pow(0.02, dt);
        sys->yaw_velocity *= decay;
        sys->pitch_velocity *= decay;
        if (fabs(sys->yaw_velocity) < 0.001) sys->yaw_velocity = 0.0;
        if (fabs(sys->pitch_velocity) < 0.001) sys->pitch_velocity = 0.0;

        sys->idle_time += dt;

        // Only resume the gentle automatic orbit once any flick has settled
        // and it's been idle a moment, easing in rather than snapping.
        if (sys->yaw_velocity == 0.0 && sys->pitch_velocity == 0.0 && sys->idle_time > 1.5) {
            double auto_blend = fmin(1.0, (sys->idle_time - 1.5) * 0.5);
            sys->camera_yaw += dt * (0.15 + mid_energy * 0.5) * auto_blend;
            double resting_pitch = 0.35 + sin(sys->camera_yaw * 0.31) * 0.15;
            sys->camera_pitch += (resting_pitch - sys->camera_pitch) * fmin(1.0, dt * 0.5) * auto_blend;
        }
    }

    if (sys->camera_pitch > 1.45) sys->camera_pitch = 1.45;
    if (sys->camera_pitch < -1.45) sys->camera_pitch = -1.45;

    sys->target_distance = RUBIK_CUBIE_SIZE * (8.0 - vis->volume_level * 1.2) * sys->user_zoom;
    sys->camera_distance += (sys->target_distance - sys->camera_distance) * fmin(1.0, dt * 2.5);

    // Beat-synced layer turns: call once per frame regardless of phase so
    // the cooldown/energy tracking stays consistent even during pauses.
    bool beat_now = rubik_detect_beat(vis, sys, dt);

    // Once a turn is triggered, how fast it visually completes still scales
    // with volume (louder = snappier), it's only the *start* of each turn
    // that's gated on the beat above.
    double base_duration = 0.42;
    double duration = base_duration / (0.7 + vis->volume_level * 1.6);
    if (duration < 0.14) duration = 0.14;
    if (duration > 0.55) duration = 0.55;

    if (sys->animating) {
        sys->anim_progress += dt / sys->anim_duration;
        if (sys->anim_progress >= 1.0) {
            rubik_apply_move_instant(sys, sys->current_move);
            sys->animating = false;
            sys->anim_progress = 0.0;

            if (sys->phase == RUBIK_PHASE_SCRAMBLING) {
                sys->move_index++;
            } else if (sys->phase == RUBIK_PHASE_SOLVING) {
                sys->solve_index--;
            }
        }
        return;
    }

    switch (sys->phase) {
        case RUBIK_PHASE_SCRAMBLING:
            if (sys->move_index < sys->scramble_count) {
                if (beat_now) {
                    sys->current_move = sys->scramble_moves[sys->move_index];
                    sys->anim_duration = duration;
                    sys->animating = true;
                }
                // else: no beat yet this frame — hold here. With no music
                // playing, beat_now is never true, so the sections simply
                // stop turning (camera dragging above is unaffected).
            } else {
                sys->phase = RUBIK_PHASE_PAUSE;
                sys->phase_timer = 1.2;
            }
            break;

        case RUBIK_PHASE_PAUSE:
            sys->phase_timer -= dt;
            if (sys->phase_timer <= 0.0) {
                sys->phase = RUBIK_PHASE_SOLVING;
                sys->solve_index = sys->scramble_count - 1;
            }
            break;

        case RUBIK_PHASE_SOLVING:
            if (sys->solve_index >= 0) {
                if (beat_now) {
                    RubikMove m = sys->scramble_moves[sys->solve_index];
                    m.dir = -m.dir; // undo, in reverse order
                    sys->current_move = m;
                    sys->anim_duration = duration;
                    sys->animating = true;
                }
            } else {
                sys->phase = RUBIK_PHASE_SOLVED_PAUSE;
                sys->phase_timer = 2.0;
            }
            break;

        case RUBIK_PHASE_SOLVED_PAUSE:
            sys->phase_timer -= dt;
            if (sys->phase_timer <= 0.0) {
                rubik_reset_cube(sys);
            }
            break;
    }
}

// --- Rendering ---------------------------------------------------------

typedef struct { double x, y, depth; } RubikProj;

static RubikProj rubik_project(Visualizer *vis, RubiksCubeSystem *sys, double wx, double wy, double wz) {
    double cy = cos(sys->camera_yaw), sy = sin(sys->camera_yaw);
    double x1 = wx * cy - wz * sy;
    double z1 = wx * sy + wz * cy;

    double cp = cos(sys->camera_pitch), sp = sin(sys->camera_pitch);
    double y2 = wy * cp - z1 * sp;
    double z2 = wy * sp + z1 * cp;

    double cam_z = z2 + sys->camera_distance;
    if (cam_z < 40.0) cam_z = 40.0;

    double focal = vis->height * 1.05;
    double scale = focal / cam_z;

    RubikProj p;
    p.x = vis->width / 2.0 + x1 * scale;
    p.y = vis->height / 2.0 + y2 * scale;
    p.depth = cam_z;
    return p;
}

typedef struct {
    double depth;
    double px[4], py[4];
    double cr, cg, cb;
} RubikQuad;

#define RUBIK_MAX_QUADS (RUBIK_CUBIE_COUNT * 6)
static RubikQuad g_rubik_quads[RUBIK_MAX_QUADS];

static int rubik_quad_cmp(const void *a, const void *b) {
    const RubikQuad *qa = (const RubikQuad*)a;
    const RubikQuad *qb = (const RubikQuad*)b;
    if (qa->depth > qb->depth) return -1; // farther first
    if (qa->depth < qb->depth) return 1;
    return 0;
}

void draw_rubiks_cube_system(Visualizer *vis, cairo_t *cr) {
    RubiksCubeSystem *sys = &vis->rubiks_cube;

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    const double half = 0.5 * RUBIK_CUBIE_GAP * RUBIK_CUBIE_SIZE;
    const double spacing = RUBIK_CUBIE_SIZE;
    const double light_x = -0.4, light_y = -0.6, light_z = 0.5;

    // Per-color glow multiplier, driven by that color's assigned band.
    double glow[6];
    for (int i = 0; i < 6; i++)
        glow[i] = 1.0 + vis->frequency_bands[rubik_color_band(i)] * 0.5;

    RubikMat3 partial;
    bool have_partial = false;
    if (sys->animating) {
        double angle = sys->current_move.dir * (M_PI / 2.0) * sys->anim_progress;
        partial = rubik_axis_rotation(sys->current_move.axis, angle);
        have_partial = true;
    }

    int quad_count = 0;

    for (int i = 0; i < RUBIK_CUBIE_COUNT; i++) {
        RubikCubie *c = &sys->cubies[i];

        bool in_moving_layer = have_partial &&
            rubik_cubie_grid_axis(c, sys->current_move.axis) == sys->current_move.layer;

        RubikMat3 eff_orient = c->orient;
        double wcx = c->gx * spacing, wcy = c->gy * spacing, wcz = c->gz * spacing;

        if (in_moving_layer) {
            eff_orient = rubik_mat3_mul(&partial, &c->orient);
            rubik_mat3_apply(&partial, wcx, wcy, wcz, &wcx, &wcy, &wcz);
        }

        for (int f = 0; f < 6; f++) {
            const RubikFaceDef *fd = &RUBIK_FACES[f];
            // Determine face color: the real sticker color on hull faces,
            // otherwise the dark body color. All 6 faces are drawn (not
            // just hull ones) so the cube never looks hollow mid-turn.
            int orig_axis_val = fd->axis == 0 ? c->orig_gx : (fd->axis == 1 ? c->orig_gy : c->orig_gz);
            bool is_sticker = (orig_axis_val == fd->sign);

            double local_n[3] = {0,0,0};
            local_n[fd->axis] = fd->sign;
            double nx, ny, nz;
            rubik_mat3_apply(&eff_orient, local_n[0], local_n[1], local_n[2], &nx, &ny, &nz);

            RubikQuad q;
            double sumz = 0.0;
            for (int k = 0; k < 4; k++) {
                const int *corner = RUBIK_CORNERS[fd->corners[k]];
                double lx = corner[0]*half, ly = corner[1]*half, lz = corner[2]*half;
                double rx, ry, rz;
                rubik_mat3_apply(&eff_orient, lx, ly, lz, &rx, &ry, &rz);
                RubikProj p = rubik_project(vis, sys, wcx + rx, wcy + ry, wcz + rz);
                q.px[k] = p.x; q.py[k] = p.y;
                sumz += p.depth;
            }
            q.depth = sumz / 4.0;

            double light_dot = fabs(nx*light_x + ny*light_y + nz*light_z);
            double shade = 0.55 + 0.45 * light_dot;

            if (is_sticker) {
                double g = glow[fd->color_id];
                const double *base = RUBIK_COLOR_RGB[fd->color_id];
                q.cr = fmin(1.0, base[0] * shade * g);
                q.cg = fmin(1.0, base[1] * shade * g);
                q.cb = fmin(1.0, base[2] * shade * g);
            } else {
                q.cr = RUBIK_BODY_RGB[0] * shade;
                q.cg = RUBIK_BODY_RGB[1] * shade;
                q.cb = RUBIK_BODY_RGB[2] * shade;
            }

            if (quad_count < RUBIK_MAX_QUADS) g_rubik_quads[quad_count++] = q;
        }
    }

    qsort(g_rubik_quads, quad_count, sizeof(RubikQuad), rubik_quad_cmp);

    for (int i = 0; i < quad_count; i++) {
        RubikQuad *q = &g_rubik_quads[i];
        cairo_move_to(cr, q->px[0], q->py[0]);
        for (int k = 1; k < 4; k++) cairo_line_to(cr, q->px[k], q->py[k]);
        cairo_close_path(cr);

        cairo_set_source_rgb(cr, q->cr, q->cg, q->cb);
        cairo_fill_preserve(cr);

        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.85);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
    }
}
