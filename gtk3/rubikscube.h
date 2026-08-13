#ifndef RUBIKSCUBE_H
#define RUBIKSCUBE_H

#include <stdbool.h>

// "Rubik's Cube" — a self-scrambling, self-solving 3x3 cube rendered with
// the same hand-rolled Cairo perspective projection + painter's-algorithm
// depth sort used by pipes.cpp. It solves by replaying its own scramble
// backwards (guaranteed correct, no solver needed) rather than running the
// real Kociemba algorithm from rubiksolver.py.
//
// Interaction:
//   - Click-drag spins the camera around the cube, with a bit of flick
//     momentum on release (see PongGame/mouse_left_pressed for the same
//     pattern this borrows the mouse fields from).
//   - Scroll wheel zooms in/out.
//   - Left alone for a moment, it eases back into a gentle auto-orbit.
//
// Music reactivity:
//   - Layer turns are strictly beat-synced: a turn only fires when a beat
//     onset is detected in the audio, so silence/pause means the cube's
//     sections simply stop turning (camera dragging is unaffected).
//   - Each of the 6 sticker colors is tied to one VIS_FREQUENCY_BARS band
//     and gets a brightness pulse from it.
//   - Camera orbit speed follows mid-frequency energy, like pipes.cpp.

#define RUBIK_CUBIE_COUNT          27
#define RUBIK_SCRAMBLE_MOVE_COUNT  32
#define RUBIK_CUBIE_SIZE           46.0   // world units per cubie
#define RUBIK_CUBIE_GAP            0.92   // corner scale factor -> groove lines

typedef struct {
    double m[3][3]; // this cubie's accumulated orientation (local -> world)
} RubikMat3;

typedef struct {
    int gx, gy, gz;             // current grid position, each in {-1, 0, 1}
    int orig_gx, orig_gy, orig_gz; // solved-state position; fixes which
                                    // local faces carry stickers, forever
    RubikMat3 orient;
} RubikCubie;

// One quarter turn: axis (0=x,1=y,2=z), layer (-1,0,1), dir (+1 = CW, -1 = CCW,
// as seen looking down the positive axis toward the origin).
typedef struct {
    int axis;
    int layer;
    int dir;
} RubikMove;

typedef enum {
    RUBIK_PHASE_SCRAMBLING,
    RUBIK_PHASE_PAUSE,
    RUBIK_PHASE_SOLVING,
    RUBIK_PHASE_SOLVED_PAUSE
} RubikPhase;

typedef struct {
    RubikCubie cubies[RUBIK_CUBIE_COUNT];

    RubikMove scramble_moves[RUBIK_SCRAMBLE_MOVE_COUNT];
    int scramble_count;
    int move_index;   // next scramble move to play, while scrambling
    int solve_index;  // next scramble move to undo, while solving (counts down)

    RubikPhase phase;
    double phase_timer;

    bool animating;
    RubikMove current_move;
    double anim_progress;  // 0..1
    double anim_duration;

    double camera_yaw, camera_pitch, camera_distance, target_distance;

    // Mouse interaction: click-drag to spin, scroll to zoom, with a bit of
    // flick momentum and a fade back into the gentle auto-orbit once idle.
    bool dragging;
    double drag_last_mouse_x, drag_last_mouse_y;
    double yaw_velocity, pitch_velocity; // radians/sec, decays after a flick
    double idle_time;                    // seconds since the last user input
    double user_zoom;                    // persistent scroll-wheel zoom multiplier

    // Beat detection for gating layer turns.
    double beat_energy_avg;   // smoothed running energy, for onset comparison
    double beat_cooldown;     // seconds until another beat trigger is allowed

    unsigned int rng_state;
} RubiksCubeSystem;

#endif // RUBIKSCUBE_H
