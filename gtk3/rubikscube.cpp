// "Rubik's Cube" visualization — a self-scrambling, self-solving 3x3 cube.
// Rendered in Cairo with the same hand-rolled perspective projection and
// per-frame painter's-algorithm depth sort as pipes.cpp (no OpenGL, and no
// dependency on the real Kociemba solver in rubiksolver.py — it solves by
// simply replaying its own scramble in reverse, which is always correct and
// reads just as well visually).
//
// The screen shows the cube alongside a "permutation" diagram: 9 rings (one
// per layer) and 54 dots (one per sticker). When a layer turns, its ring
// lights up and its stickers slide around it in sync with the cube.
//
// Interaction: click-drag spins the camera, scroll zooms — both work at any
// time, even paused/no audio. Layer turns are a separate concern: they only
// fire on a detected beat, so no music genuinely means no turning.

#include "visualization.h"
#include "audio_player.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
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

static void rubik_draw_cube_3d(Visualizer *vis, cairo_t *cr) {
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

// --- Permutation view (what the visualization actually shows) ------------

#define RPERM_C             0.30  // ring spacing on the sticker sphere
#define RPERM_K             0.80  // 1.0 = pure stereographic; lower = more even
#define RPERM_RING_SAMPLES  360
#define RPERM_STICKERS      54
#define RPERM_HISTORY       6

static const double RPERM_ACCENT[3] = {0.62, 0.52, 0.98}; // title / current move

// --- Small math ------------------------------------------------------------

static void rperm_rotate(int axis, double ang, double v[3]) {
    double c = cos(ang), s = sin(ang);
    int a = (axis + 1) % 3, b = (axis + 2) % 3;
    double va = v[a], vb = v[b];
    v[a] = c * va - s * vb;
    v[b] = s * va + c * vb;
}

// View basis for the flat map: the projection pole sits off a cube corner
// (+x+y+z), well clear of every ring and dot, so nothing blows up.
static void rperm_basis(double b[3][3]) {
    const double r3 = 1.0 / sqrt(3.0), r2 = 1.0 / sqrt(2.0);
    double e3[3] = {r3, r3, r3};
    double e1[3] = {-r2, r2, 0.0};
    double e2[3] = {e3[1]*e1[2] - e3[2]*e1[1],
                    e3[2]*e1[0] - e3[0]*e1[2],
                    e3[0]*e1[1] - e3[1]*e1[0]};
    memcpy(b[0], e1, sizeof e1);
    memcpy(b[1], e2, sizeof e2);
    memcpy(b[2], e3, sizeof e3);
}

// Softened stereographic projection from the pole (b[2]).
static void rperm_map(const double b[3][3], const double q[3], double *x, double *y) {
    double e0 = b[0][0]*q[0] + b[0][1]*q[1] + b[0][2]*q[2];
    double e1 = b[1][0]*q[0] + b[1][1]*q[1] + b[1][2]*q[2];
    double e2 = b[2][0]*q[0] + b[2][1]*q[1] + b[2][2]*q[2];
    double th = acos(fmax(-1.0, fmin(1.0, -e2)));  // angle from view center
    double r = tan(th * 0.5 * RPERM_K);
    double d = hypot(e0, e1);
    if (d < 1e-9) { *x = 0.0; *y = 0.0; return; }
    *x = e0 / d * r;
    *y = e1 / d * r;
}

// --- Move sequence helpers --------------------------------------------------

// Whole cycle as one list: scramble moves, then their inverses in reverse.
static RubikMove rperm_step_move(const RubiksCubeSystem *sys, int step) {
    int n = sys->scramble_count;
    if (step < n) return sys->scramble_moves[step];
    RubikMove m = sys->scramble_moves[2 * n - 1 - step];
    m.dir = -m.dir;
    return m;
}

static int rperm_steps_done(const RubiksCubeSystem *sys) {
    int n = sys->scramble_count;
    switch (sys->phase) {
        case RUBIK_PHASE_SCRAMBLING: return sys->move_index;
        case RUBIK_PHASE_PAUSE:      return n;
        case RUBIK_PHASE_SOLVING:    return n + (n - 1 - sys->solve_index);
        default:                     return 2 * n;
    }
}

// Singmaster notation in the cube's world frame. dir +1 is a right-handed
// (counter-clockwise from +axis) quarter turn, so R/U/F/S are dir -1 and
// L/D/B/M/E are dir +1.
static void rperm_move_name(RubikMove m, char *buf, size_t len) {
    static const char *names[3][3] = {{"L", "M", "R"}, {"D", "E", "U"}, {"B", "S", "F"}};
    bool cw_is_negative = (m.layer == 1) || (m.axis == 2 && m.layer == 0);
    bool prime = cw_is_negative ? (m.dir == 1) : (m.dir == -1);
    snprintf(buf, len, "%s%s", names[m.axis][m.layer + 1], prime ? "\xe2\x80\xb2" : "");
}

// --- Text helpers -----------------------------------------------------------

static double rperm_text_width(cairo_t *cr, const char *s, double size) {
    cairo_text_extents_t ext;
    cairo_set_font_size(cr, size);
    cairo_text_extents(cr, s, &ext);
    return ext.x_advance;
}

// align: 0 = left edge at x, 0.5 = centered on x.
static double rperm_text(cairo_t *cr, double x, double y, const char *s, double size,
                         double r, double g, double b, double a, double align) {
    double w = rperm_text_width(cr, s, size);
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_move_to(cr, x - w * align, y);
    cairo_show_text(cr, s);
    return w;
}

// --- Layout -------------------------------------------------------------

typedef struct {
    double fs;                          // base font size
    double title_y;
    double cube_cx, cube_cy, cube_size;
    double label_x, label_y;
    double diag_cx, diag_cy, diag_size;
    double hist_x, hist_y;
} RpermLayout;

static RpermLayout rperm_layout(Visualizer *vis) {
    double w = vis->width, h = vis->height;
    RpermLayout L;
    L.fs = fmin(w, h) * 0.035;
    if (w < h * 1.1) {
        // Portrait: stacked like the reel.
        L.title_y = h * 0.07;
        L.cube_cx = w * 0.45; L.cube_cy = h * 0.25; L.cube_size = fmin(h * 0.26, w * 0.55);
        L.label_x = w * 0.80; L.label_y = h * 0.24;
        L.diag_cx = w * 0.50; L.diag_cy = h * 0.64; L.diag_size = fmin(w * 0.88, h * 0.44);
        L.hist_x = w * 0.08;  L.hist_y = h * 0.94;
    } else {
        // Landscape: cube left, diagram right.
        L.title_y = h * 0.08;
        L.cube_cx = w * 0.25; L.cube_cy = h * 0.42; L.cube_size = fmin(h * 0.42, w * 0.34);
        L.label_x = w * 0.25; L.label_y = h * 0.76;
        L.diag_cx = w * 0.66; L.diag_cy = h * 0.54; L.diag_size = fmin(w * 0.58, h * 0.80);
        L.hist_x = w * 0.05;  L.hist_y = h * 0.93;
    }
    return L;
}

// --- Draw -------------------------------------------------------------------

typedef struct { double x, y; int color; bool moving; } RpermDot;

static double g_rperm_rings[9][RPERM_RING_SAMPLES][2];
static RpermDot g_rperm_dots[RPERM_STICKERS];

void draw_rubiks_cube_system(Visualizer *vis, cairo_t *cr) {
    RubiksCubeSystem *sys = &vis->rubiks_cube;
    RpermLayout L = rperm_layout(vis);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    // ---- The 3D cube, scaled into its slot. Its own painter does a full
    //      cairo_paint, so it's clipped to a box around the slot.
    {
        // On-screen width of the cube at default zoom, seen corner-on.
        double native = 1.65 * 3.0 * RUBIK_CUBIE_SIZE * vis->height * 1.05 / (RUBIK_CUBIE_SIZE * 8.0);
        double s = L.cube_size / native;
        cairo_save(cr);
        cairo_rectangle(cr, L.cube_cx - L.cube_size, L.cube_cy - L.cube_size * 0.9,
                        L.cube_size * 2.0, L.cube_size * 1.8);
        cairo_clip(cr);
        cairo_translate(cr, L.cube_cx, L.cube_cy);
        cairo_scale(cr, s, s);
        cairo_translate(cr, -vis->width / 2.0, -vis->height / 2.0);
        rubik_draw_cube_3d(vis, cr);
        cairo_restore(cr);
    }

    // ---- Which move is current / last.
    int done = rperm_steps_done(sys);
    int total = 2 * sys->scramble_count;
    int cur = sys->animating ? done : done - 1;   // step to highlight
    bool have_cur = cur >= 0 && cur < total;
    RubikMove cur_move = have_cur ? rperm_step_move(sys, cur) : RubikMove{0, 0, 0};
    int active_ring = have_cur ? cur_move.axis * 3 + (cur_move.layer + 1) : -1;
    double partial = sys->animating
        ? sys->current_move.dir * (M_PI / 2.0) * sys->anim_progress : 0.0;

    // ---- Build the flat diagram (in projection units).
    double basis[3][3];
    rperm_basis(basis);

    double minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
    for (int a = 0; a < 3; a++)
        for (int l = -1; l <= 1; l++) {
            int ri = a * 3 + (l + 1);
            double h = RPERM_C * l, rho = sqrt(1.0 - h * h);
            for (int k = 0; k < RPERM_RING_SAMPLES; k++) {
                double t = 2.0 * M_PI * k / RPERM_RING_SAMPLES;
                double q[3];
                q[a] = h;
                q[(a + 1) % 3] = rho * cos(t);
                q[(a + 2) % 3] = rho * sin(t);
                double x, y;
                rperm_map(basis, q, &x, &y);
                g_rperm_rings[ri][k][0] = x;
                g_rperm_rings[ri][k][1] = y;
                minx = fmin(minx, x); maxx = fmax(maxx, x);
                miny = fmin(miny, y); maxy = fmax(maxy, y);
            }
        }

    int nd = 0;
    for (int i = 0; i < RUBIK_CUBIE_COUNT; i++) {
        RubikCubie *c = &sys->cubies[i];
        int orig[3] = {c->orig_gx, c->orig_gy, c->orig_gz};
        double pos[3] = {(double)c->gx, (double)c->gy, (double)c->gz};
        int grid[3] = {c->gx, c->gy, c->gz};
        bool moving = sys->animating &&
            grid[sys->current_move.axis] == sys->current_move.layer;

        for (int a = 0; a < 3; a++)
            for (int s = -1; s <= 1; s += 2) {
                if (orig[a] != s) continue;          // not a sticker face
                double ln[3] = {0, 0, 0}, n[3];
                ln[a] = s;
                rubik_mat3_apply(&c->orient, ln[0], ln[1], ln[2], &n[0], &n[1], &n[2]);
                for (int k = 0; k < 3; k++) n[k] = (double)lround(n[k]);

                double pn = pos[0]*n[0] + pos[1]*n[1] + pos[2]*n[2];
                double perp[3], pp = 0.0;
                for (int k = 0; k < 3; k++) { perp[k] = pos[k] - pn * n[k]; pp += perp[k] * perp[k]; }
                double out = sqrt(fmax(0.0, 1.0 - RPERM_C * RPERM_C * pp));
                double q[3];
                for (int k = 0; k < 3; k++) q[k] = RPERM_C * perp[k] + out * n[k];

                // Mid-turn: rotate on the sphere, which slides it along its ring.
                if (moving) rperm_rotate(sys->current_move.axis, partial, q);

                if (nd < RPERM_STICKERS) {
                    RpermDot *d = &g_rperm_dots[nd++];
                    rperm_map(basis, q, &d->x, &d->y);
                    d->color = a * 2 + (s < 0);
                    d->moving = moving;
                }
            }
    }

    // ---- Fit to the diagram slot (gently "breathing" with volume).
    double extent = fmax(maxx - minx, maxy - miny);
    double scale = L.diag_size * (1.0 + vis->volume_level * 0.03) / extent;
    double ox = L.diag_cx - (minx + maxx) * 0.5 * scale;
    double oy = L.diag_cy - (miny + maxy) * 0.5 * scale;

    double mid = 0.0;
    for (int i = VIS_FREQUENCY_BARS / 4; i < 3 * VIS_FREQUENCY_BARS / 4; i++)
        mid += vis->frequency_bands[i];
    mid /= (VIS_FREQUENCY_BARS / 2);

    // ---- Rings: dim gray, the active layer's ring bright white on top.
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    for (int pass = 0; pass < 2; pass++)
        for (int ri = 0; ri < 9; ri++) {
            bool active = ri == active_ring;
            if (active != (pass == 1)) continue;
            cairo_move_to(cr, ox + g_rperm_rings[ri][0][0] * scale, oy + g_rperm_rings[ri][0][1] * scale);
            for (int k = 1; k < RPERM_RING_SAMPLES; k++)
                cairo_line_to(cr, ox + g_rperm_rings[ri][k][0] * scale, oy + g_rperm_rings[ri][k][1] * scale);
            cairo_close_path(cr);
            if (active) {
                double a = sys->animating ? 1.0 : 0.75;
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, a);
                cairo_set_line_width(cr, fmax(2.0, L.diag_size * 0.0065));
            } else {
                cairo_set_source_rgba(cr, 0.58, 0.58, 0.66, fmin(0.85, 0.42 + mid * 0.35));
                cairo_set_line_width(cr, fmax(1.0, L.diag_size * 0.0032));
            }
            cairo_stroke(cr);
        }

    // ---- Sticker dots (moving ones drawn last so they ride on top).
    double dot_r = L.diag_size * 0.008;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < nd; i++) {
            RpermDot *d = &g_rperm_dots[i];
            if (d->moving != (pass == 1)) continue;
            int band = (d->color * VIS_FREQUENCY_BARS) / 6;
            double e = vis->frequency_bands[band];
            double r = dot_r * (1.0 + e * 0.35) * (d->moving ? 1.12 : 1.0);
            const double *col = RUBIK_COLOR_RGB[d->color];
            double g = 1.0 + e * 0.4;
            cairo_new_path(cr);
            cairo_arc(cr, ox + d->x * scale, oy + d->y * scale, r, 0, 2.0 * M_PI);
            cairo_set_source_rgb(cr, fmin(1.0, col[0] * g), fmin(1.0, col[1] * g), fmin(1.0, col[2] * g));
            cairo_fill(cr);
        }

    // ---- Text (serif, like the reel's LaTeX look).
    cairo_select_font_face(cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    double fs = L.fs;

    // Title.
    rperm_text(cr, vis->width * 0.5, L.title_y, "Rubik\xe2\x80\x99s Cube", fs * 1.3,
               RPERM_ACCENT[0], RPERM_ACCENT[1], RPERM_ACCENT[2], 1.0, 0.5);

    char name[16];

    // Big current-move label + "k / N" counter + phase.
    {
        int n = sys->scramble_count;
        bool solving = sys->phase == RUBIK_PHASE_SOLVING || sys->phase == RUBIK_PHASE_SOLVED_PAUSE;
        int shown = have_cur ? (cur < n ? cur + 1 : cur - n + 1) : 0;
        if (have_cur) {
            rperm_move_name(cur_move, name, sizeof name);
            rperm_text(cr, L.label_x, L.label_y, name, fs * 2.6,
                       RPERM_ACCENT[0], RPERM_ACCENT[1], RPERM_ACCENT[2], 1.0, 0.5);
        }
        char counter[32];
        snprintf(counter, sizeof counter, "%d / %d", shown, n);
        rperm_text(cr, L.label_x, L.label_y + fs * 1.2, counter, fs * 0.7, 0.7, 0.7, 0.75, 0.9, 0.5);
        const char *phase =
            sys->phase == RUBIK_PHASE_SOLVED_PAUSE ? "solved" : (solving ? "solving" : "scrambling");
        rperm_text(cr, L.label_x, L.label_y + fs * 2.1, phase, fs * 0.6, 0.55, 0.55, 0.6, 0.8, 0.5);
    }

    // Small tag on the left edge of the active ring.
    if (active_ring >= 0) {
        int best = 0;
        for (int k = 1; k < RPERM_RING_SAMPLES; k++)
            if (g_rperm_rings[active_ring][k][0] < g_rperm_rings[active_ring][best][0]) best = k;
        double tx = ox + g_rperm_rings[active_ring][best][0] * scale - fs * 1.4;
        double ty = oy + g_rperm_rings[active_ring][best][1] * scale;
        cairo_new_path(cr);  // drop the current point left by show_text
        cairo_arc(cr, tx, ty, fs * 0.85, 0, 2.0 * M_PI);
        cairo_set_source_rgba(cr, 0.08, 0.08, 0.1, 0.9);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.6, 0.6, 0.66, 0.6);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
        rperm_move_name(cur_move, name, sizeof name);
        rperm_text(cr, tx, ty + fs * 0.32, name, fs * 0.9, 1.0, 1.0, 1.0, 1.0, 0.5);
    }

    // Recent move history, current one in the accent color.
    if (have_cur) {
        double x = L.hist_x;
        int first = cur - (RPERM_HISTORY - 1);
        if (first < 0) first = 0;
        for (int st = first; st <= cur; st++) {
            rperm_move_name(rperm_step_move(sys, st), name, sizeof name);
            bool is_cur = st == cur;
            double fade = 0.45 + 0.55 * (double)(st - first + 1) / (cur - first + 1);
            x += rperm_text(cr, x, L.hist_y, name, fs,
                            is_cur ? RPERM_ACCENT[0] : 1.0,
                            is_cur ? RPERM_ACCENT[1] : 1.0,
                            is_cur ? RPERM_ACCENT[2] : 1.0,
                            is_cur ? 1.0 : fade * 0.8, 0.0);
            x += fs * 0.45;
        }
    }
}
