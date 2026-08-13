// "3D Music Pipes" visualization — a classic 3D-pipes-screensaver-style
// growing tube maze, rendered in Cairo with a hand-rolled perspective
// projection and a per-frame painter's-algorithm depth sort (no OpenGL
// dependency, consistent with the rest of this project's 2D-cairo
// visualizations like dna.cpp).
//
// Music reactivity:
//   - Each pipe's thickness pulses with its own assigned frequency band.
//   - Overall volume speeds up growth and gives the camera a gentle zoom.
//   - Bass energy raises the chance of a turn at each grid cell, so the
//     structure gets visibly busier during heavier moments.
//   - Mid frequency energy speeds up the slow automatic camera orbit.

#include "visualization.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Curated palette echoing the classic look (green / gold / soft pink),
// plus a couple of extras so several concurrent pipes read as distinct.
static const double PIPES_PALETTE[PIPES_PALETTE_SIZE][3] = {
    {0.20, 0.80, 0.25}, // green
    {0.95, 0.75, 0.15}, // gold
    {0.95, 0.85, 0.90}, // soft pink/white
    {0.25, 0.55, 0.95}, // blue
    {0.90, 0.30, 0.30}, // red
};

// Small self-contained xorshift RNG so this visualization's randomness
// doesn't disturb the app's global rand() stream (used elsewhere for audio
// and other visualizations).
static unsigned int pipes_rand(unsigned int *state) {
    unsigned int x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static double pipes_rand_double(unsigned int *state) {
    return (pipes_rand(state) & 0xFFFFFF) / (double)0x1000000;
}

// One of the 6 axis-aligned unit directions.
static void pipes_random_direction(unsigned int *state, double *dx, double *dy, double *dz) {
    int axis = pipes_rand(state) % 3;
    double sign = (pipes_rand(state) % 2) ? 1.0 : -1.0;
    *dx = *dy = *dz = 0.0;
    if (axis == 0) *dx = sign;
    else if (axis == 1) *dy = sign;
    else *dz = sign;
}

// Given the pipe is at `from` moving in (cur_dx,cur_dy,cur_dz), picks one of
// the 6 axis-aligned directions that stays inside the grid — preferring one
// that isn't a straight reversal. Used to steer a pipe back inward when it
// reaches a wall, instead of ending the pipe there.
static void pick_bounded_direction(unsigned int *state, PipeVertex *from,
                                    double cur_dx, double cur_dy, double cur_dz,
                                    double *out_dx, double *out_dy, double *out_dz) {
    static const double candidates[6][3] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1},
    };

    int valid_no_reverse[6], n_no_reverse = 0;
    int valid_any[6], n_valid_any = 0;

    for (int i = 0; i < 6; i++) {
        double nx = from->x + candidates[i][0];
        double ny = from->y + candidates[i][1];
        double nz = from->z + candidates[i][2];
        bool inbound = fabs(nx) <= PIPES_GRID_HALF && fabs(ny) <= PIPES_GRID_HALF && fabs(nz) <= PIPES_GRID_HALF;
        if (!inbound) continue;

        valid_any[n_valid_any++] = i;
        bool is_reverse = (candidates[i][0] == -cur_dx && candidates[i][1] == -cur_dy && candidates[i][2] == -cur_dz);
        if (!is_reverse) valid_no_reverse[n_no_reverse++] = i;
    }

    const int *pool = n_no_reverse > 0 ? valid_no_reverse : valid_any;
    int pool_n = n_no_reverse > 0 ? n_no_reverse : n_valid_any;

    if (pool_n == 0) {
        // Grid half is always >= 1 in practice, so this shouldn't happen —
        // but fall back to holding direction rather than crashing.
        *out_dx = cur_dx; *out_dy = cur_dy; *out_dz = cur_dz;
        return;
    }

    int pick = pool[pipes_rand(state) % pool_n];
    *out_dx = candidates[pick][0];
    *out_dy = candidates[pick][1];
    *out_dz = candidates[pick][2];
}

static void spawn_pipe(PipesSystem *sys, int idx) {
    Pipe3D *p = &sys->pipes[idx];
    memset(p, 0, sizeof(*p));
    p->active = true;
    p->vert_count = 1;

    double half = PIPES_GRID_HALF;
    p->verts[0].x = (pipes_rand_double(&sys->rng_state) * 2.0 - 1.0) * half;
    p->verts[0].y = (pipes_rand_double(&sys->rng_state) * 2.0 - 1.0) * half;
    p->verts[0].z = (pipes_rand_double(&sys->rng_state) * 2.0 - 1.0) * half;

    pipes_random_direction(&sys->rng_state, &p->dir_x, &p->dir_y, &p->dir_z);

    int palette_idx = idx % PIPES_PALETTE_SIZE;
    if (pipes_rand(&sys->rng_state) % 3 == 0) {
        palette_idx = pipes_rand(&sys->rng_state) % PIPES_PALETTE_SIZE;
    }
    p->color_r = PIPES_PALETTE[palette_idx][0];
    p->color_g = PIPES_PALETTE[palette_idx][1];
    p->color_b = PIPES_PALETTE[palette_idx][2];

    p->base_radius = 10.0 + pipes_rand_double(&sys->rng_state) * 6.0;
    p->radius = p->base_radius;
    p->freq_band = pipes_rand(&sys->rng_state) % VIS_FREQUENCY_BARS;
    p->speed = 1.6 + pipes_rand_double(&sys->rng_state) * 0.8;
}

void init_pipes_system(Visualizer *vis) {
    PipesSystem *sys = &vis->pipes3d;
    memset(sys, 0, sizeof(*sys));
    sys->rng_state = 0x9E3779B9u ^ (unsigned int)(size_t)vis;
    sys->camera_yaw = 0.6;
    sys->camera_pitch = 0.35;
    sys->camera_distance = PIPES_GRID_HALF * PIPES_CELL_SIZE * 2.6;
    sys->target_distance = sys->camera_distance;

    for (int i = 0; i < PIPES_MAX_COUNT; i++) {
        spawn_pipe(sys, i);
        sys->spawn_cooldown[i] = 0.0;
    }
}

// Advances one pipe's growing head by `speed` cells/sec worth of `dt`.
// Commits a new joint (and optionally turns) each time a full cell is
// crossed. If the current direction would carry it outside the grid, it's
// steered to an inbound direction instead of retiring — that's what lets a
// pipe keep growing for its whole PIPES_LIFETIME_SECONDS rather than dying
// the moment it reaches a wall. Retirement itself is handled by the caller
// via the pipe's `life` timer.
static void advance_pipe(PipesSystem *sys, Pipe3D *p, double dt, double turn_bias) {
    p->progress += p->speed * dt;

    while (p->progress >= 1.0) {
        p->progress -= 1.0;

        PipeVertex *last = &p->verts[p->vert_count - 1];

        double step_x = p->dir_x, step_y = p->dir_y, step_z = p->dir_z;
        double test_x = last->x + step_x, test_y = last->y + step_y, test_z = last->z + step_z;
        bool would_leave_grid = fabs(test_x) > PIPES_GRID_HALF ||
                                 fabs(test_y) > PIPES_GRID_HALF ||
                                 fabs(test_z) > PIPES_GRID_HALF;

        if (would_leave_grid) {
            pick_bounded_direction(&sys->rng_state, last, p->dir_x, p->dir_y, p->dir_z,
                                    &step_x, &step_y, &step_z);
            p->dir_x = step_x; p->dir_y = step_y; p->dir_z = step_z;
        }

        if (p->vert_count >= PIPES_MAX_SEGMENTS) {
            // Hit the safety cap — extremely rare given how it's sized for a
            // full lifetime at max speed. Stop adding geometry but leave the
            // pipe visible and active; it retires naturally once `life`
            // reaches PIPES_LIFETIME_SECONDS instead of popping out early.
            return;
        }

        PipeVertex next;
        next.x = last->x + step_x;
        next.y = last->y + step_y;
        next.z = last->z + step_z;
        p->verts[p->vert_count++] = next;

        // Decide whether to keep going straight or turn. `turn_bias` (driven
        // by bass/volume) raises the turn chance during louder moments, so
        // the pipework visibly gets more intricate with the music.
        if (pipes_rand_double(&sys->rng_state) < turn_bias) {
            double nx, ny, nz;
            // Re-roll until it's not a straight reversal of the current
            // direction — a pipe folding back on itself looks broken.
            do {
                pipes_random_direction(&sys->rng_state, &nx, &ny, &nz);
            } while (nx == -p->dir_x && ny == -p->dir_y && nz == -p->dir_z);
            p->dir_x = nx; p->dir_y = ny; p->dir_z = nz;
        }
    }
}

void update_pipes_system(Visualizer *vis, double dt) {
    PipesSystem *sys = &vis->pipes3d;

    double mid_energy = 0.0;
    for (int i = VIS_FREQUENCY_BARS / 4; i < 3 * VIS_FREQUENCY_BARS / 4; i++)
        mid_energy += vis->frequency_bands[i];
    mid_energy /= (VIS_FREQUENCY_BARS / 2);

    // Slow automatic orbit; mid/high energy speeds the spin up a touch.
    sys->camera_yaw += dt * (0.12 + mid_energy * 0.35);
    sys->camera_pitch = 0.35 + sin(sys->camera_yaw * 0.37) * 0.12;

    // Gentle "breathing" zoom tied to overall loudness.
    sys->target_distance = PIPES_GRID_HALF * PIPES_CELL_SIZE * (2.6 - vis->volume_level * 0.5);
    sys->camera_distance += (sys->target_distance - sys->camera_distance) * fmin(1.0, dt * 2.0);

    double low_energy = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS / 4; i++)
        low_energy += vis->frequency_bands[i];
    low_energy /= (VIS_FREQUENCY_BARS / 4);

    double turn_bias = 0.18 + low_energy * 0.5 + vis->volume_level * 0.25;
    if (turn_bias > 0.9) turn_bias = 0.9;

    for (int i = 0; i < PIPES_MAX_COUNT; i++) {
        Pipe3D *p = &sys->pipes[i];

        if (!p->active) {
            sys->spawn_cooldown[i] -= dt;
            if (sys->spawn_cooldown[i] <= 0.0) {
                spawn_pipe(sys, i);
                sys->spawn_cooldown[i] = 0.15 + pipes_rand_double(&sys->rng_state) * 0.5;
            }
            continue;
        }

        p->life += dt;
        if (p->life >= PIPES_LIFETIME_SECONDS) {
            p->active = false;
            continue;
        }

        // Each pipe's thickness (and growth speed) pulses with its own
        // assigned frequency band, so the structure visibly reacts to
        // different parts of the mix rather than moving as one blob.
        double band = vis->frequency_bands[p->freq_band];
        p->radius = p->base_radius * (0.75 + band * 1.3);
        p->speed = (2.2 + band * 1.8) * (0.9 + vis->volume_level * 1.1);

        advance_pipe(sys, p, dt, turn_bias);
    }
}

// --- Rendering ---------------------------------------------------------

typedef struct {
    double x, y, depth, scale;
} PipeProj;

static PipeProj pipes_project(Visualizer *vis, PipesSystem *sys, double gx, double gy, double gz) {
    double wx = gx * PIPES_CELL_SIZE;
    double wy = gy * PIPES_CELL_SIZE;
    double wz = gz * PIPES_CELL_SIZE;

    // Orbit camera: yaw around Y, then pitch around X.
    double cy = cos(sys->camera_yaw), sy = sin(sys->camera_yaw);
    double x1 = wx * cy - wz * sy;
    double z1 = wx * sy + wz * cy;

    double cp = cos(sys->camera_pitch), sp = sin(sys->camera_pitch);
    double y2 = wy * cp - z1 * sp;
    double z2 = wy * sp + z1 * cp;

    double cam_z = z2 + sys->camera_distance;
    if (cam_z < 60.0) cam_z = 60.0; // keep the perspective divide well-behaved

    double focal = vis->height * 0.95;
    double scale = focal / cam_z;

    PipeProj proj;
    proj.x = vis->width / 2.0 + x1 * scale;
    proj.y = vis->height / 2.0 + y2 * scale;
    proj.depth = cam_z;
    proj.scale = scale;
    return proj;
}

typedef struct {
    double depth;    // sort key: camera-space distance, farthest drawn first
    bool is_segment; // true = tube segment, false = joint sphere
    double x1, y1, r1;
    double x2, y2;   // only used when is_segment
    double cr, cg, cb;
} PipeDrawItem;

// Sized to comfortably hold every segment + joint + head cap across all
// concurrent pipes at their full PIPES_MAX_SEGMENTS capacity.
#define PIPES_MAX_DRAW_ITEMS (PIPES_MAX_COUNT * (PIPES_MAX_SEGMENTS * 2 + 4))
static PipeDrawItem g_pipe_draw_items[PIPES_MAX_DRAW_ITEMS];

static int pipe_draw_item_cmp(const void *a, const void *b) {
    const PipeDrawItem *ia = (const PipeDrawItem*)a;
    const PipeDrawItem *ib = (const PipeDrawItem*)b;
    if (ia->depth > ib->depth) return -1; // farther first
    if (ia->depth < ib->depth) return 1;
    return 0;
}

static void push_draw_item(int *count, PipeDrawItem item) {
    if (*count < PIPES_MAX_DRAW_ITEMS) {
        g_pipe_draw_items[(*count)++] = item;
    }
}

void draw_pipes_system(Visualizer *vis, cairo_t *cr) {
    PipesSystem *sys = &vis->pipes3d;

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    int item_count = 0;
    const double light_x = -0.4, light_y = -0.6, light_z = 0.5;

    for (int i = 0; i < PIPES_MAX_COUNT; i++) {
        Pipe3D *p = &sys->pipes[i];
        if (!p->active || p->vert_count < 1) continue;

        // Committed straight-line segments between joints.
        for (int v = 0; v < p->vert_count - 1; v++) {
            PipeVertex *a = &p->verts[v];
            PipeVertex *b = &p->verts[v + 1];

            PipeProj pa = pipes_project(vis, sys, a->x, a->y, a->z);
            PipeProj pb = pipes_project(vis, sys, b->x, b->y, b->z);

            double dx = b->x - a->x, dy = b->y - a->y, dz = b->z - a->z;
            double len = sqrt(dx*dx + dy*dy + dz*dz);
            if (len < 1e-6) len = 1e-6;
            double light_dot = fabs(dx/len*light_x + dy/len*light_y + dz/len*light_z);
            double shade = 0.55 + 0.45 * (1.0 - light_dot);

            PipeDrawItem item;
            item.depth = (pa.depth + pb.depth) * 0.5;
            item.is_segment = true;
            item.x1 = pa.x; item.y1 = pa.y; item.r1 = p->radius * pa.scale;
            item.x2 = pb.x; item.y2 = pb.y;
            item.cr = p->color_r * shade;
            item.cg = p->color_g * shade;
            item.cb = p->color_b * shade;
            push_draw_item(&item_count, item);
        }

        // Elbow-cap spheres at every committed joint.
        for (int v = 0; v < p->vert_count; v++) {
            PipeVertex *vert = &p->verts[v];
            PipeProj pv = pipes_project(vis, sys, vert->x, vert->y, vert->z);

            PipeDrawItem item;
            item.depth = pv.depth;
            item.is_segment = false;
            item.x1 = pv.x; item.y1 = pv.y; item.r1 = p->radius * pv.scale * 1.08;
            item.cr = p->color_r; item.cg = p->color_g; item.cb = p->color_b;
            push_draw_item(&item_count, item);
        }

        // Growing head: interpolate the last committed vertex toward the
        // next cell so the tip visibly extends rather than popping in.
        PipeVertex *last = &p->verts[p->vert_count - 1];
        double hx = last->x + p->dir_x * p->progress;
        double hy = last->y + p->dir_y * p->progress;
        double hz = last->z + p->dir_z * p->progress;

        PipeProj plast = pipes_project(vis, sys, last->x, last->y, last->z);
        PipeProj phead = pipes_project(vis, sys, hx, hy, hz);

        PipeDrawItem seg;
        seg.depth = (plast.depth + phead.depth) * 0.5;
        seg.is_segment = true;
        seg.x1 = plast.x; seg.y1 = plast.y; seg.r1 = p->radius * plast.scale;
        seg.x2 = phead.x; seg.y2 = phead.y;
        seg.cr = p->color_r * 0.85; seg.cg = p->color_g * 0.85; seg.cb = p->color_b * 0.85;
        push_draw_item(&item_count, seg);

        PipeDrawItem cap;
        cap.depth = phead.depth;
        cap.is_segment = false;
        cap.x1 = phead.x; cap.y1 = phead.y; cap.r1 = p->radius * phead.scale * 1.05;
        cap.cr = p->color_r; cap.cg = p->color_g; cap.cb = p->color_b;
        push_draw_item(&item_count, cap);
    }

    qsort(g_pipe_draw_items, item_count, sizeof(PipeDrawItem), pipe_draw_item_cmp);

    for (int i = 0; i < item_count; i++) {
        PipeDrawItem *it = &g_pipe_draw_items[i];

        if (it->is_segment) {
            double w = it->r1 * 2.0;
            if (w < 1.0) w = 1.0;

            cairo_set_line_width(cr, w);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_source_rgb(cr, it->cr, it->cg, it->cb);
            cairo_move_to(cr, it->x1, it->y1);
            cairo_line_to(cr, it->x2, it->y2);
            cairo_stroke(cr);

            // Thin glossy highlight stripe for a plastic/metal pipe look.
            cairo_set_line_width(cr, w * 0.28);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.35);
            cairo_move_to(cr, it->x1, it->y1);
            cairo_line_to(cr, it->x2, it->y2);
            cairo_stroke(cr);
        } else {
            double r = it->r1;
            if (r < 0.5) continue;

            cairo_pattern_t *grad = cairo_pattern_create_radial(
                it->x1 - r * 0.35, it->y1 - r * 0.35, r * 0.1,
                it->x1, it->y1, r);
            cairo_pattern_add_color_stop_rgba(grad, 0.0, 1.0, 1.0, 1.0, 0.85);
            cairo_pattern_add_color_stop_rgba(grad, 0.35, it->cr, it->cg, it->cb, 1.0);
            cairo_pattern_add_color_stop_rgba(grad, 1.0, it->cr * 0.5, it->cg * 0.5, it->cb * 0.5, 1.0);

            cairo_set_source(cr, grad);
            cairo_arc(cr, it->x1, it->y1, r, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_pattern_destroy(grad);
        }
    }
}
