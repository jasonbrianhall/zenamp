#ifndef PIPES3D_H
#define PIPES3D_H

#include <stdbool.h>

// "3D Music Pipes" — a classic 3D-pipes-screensaver-style visualization
// where several glossy tubes grow through a 3D grid, turning at random,
// with growth speed, thickness, and camera motion all driven by the music.

#define PIPES_MAX_COUNT       8      // concurrent growing pipes
#define PIPES_MAX_SEGMENTS    1200   // vertex capacity per pipe (safety cap, sized for a full lifetime at max speed)
#define PIPES_GRID_HALF       13     // grid spans -13..+13 cells on each axis
#define PIPES_CELL_SIZE       34.0   // world units per grid cell
#define PIPES_PALETTE_SIZE    5
#define PIPES_LIFETIME_SECONDS 120.0 // how long a single pipe grows before it resets

typedef struct {
    double x, y, z; // grid-space vertex position, in whole cells
} PipeVertex;

typedef struct {
    bool active;

    PipeVertex verts[PIPES_MAX_SEGMENTS]; // committed joints; verts[0] = spawn point
    int vert_count;

    double dir_x, dir_y, dir_z; // current direction: one component is +-1, the rest 0
    double progress;            // 0..1 fraction through the current 1-cell leg
    double speed;                // cells/sec, audio-reactive

    double color_r, color_g, color_b;
    double base_radius;
    double radius;               // current audio-reactive radius (world units)
    int freq_band;                // which VIS_FREQUENCY_BARS band drives this pipe's pulse

    double life;                  // seconds since spawn; pipe retires at PIPES_LIFETIME_SECONDS
} Pipe3D;

typedef struct {
    Pipe3D pipes[PIPES_MAX_COUNT];

    double camera_yaw;
    double camera_pitch;
    double camera_distance;
    double target_distance;

    double spawn_cooldown[PIPES_MAX_COUNT];
    unsigned int rng_state;
} PipesSystem;

#endif // PIPES3D_H
