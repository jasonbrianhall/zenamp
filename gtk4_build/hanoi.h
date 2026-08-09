#ifndef HANOI_H
#define HANOI_H

#define MAX_HANOI_DISKS 25
#define HANOI_NUM_PEGS 3

typedef struct {
    int disk_size;      // 1 = smallest, MAX_HANOI_DISKS = largest
    double y_offset;    // For animation
    double glow;        // Pulse effect
} HanoiDisk;

typedef struct {
    HanoiDisk disks[MAX_HANOI_DISKS];
    int disk_count;
    double x_position;
} HanoiPeg;

typedef struct {
    int from_peg;
    int to_peg;
    int disk_size;
    double progress;    // 0.0 to 1.0 animation progress
    bool active;
} HanoiMove;

typedef struct {
    HanoiPeg pegs[HANOI_NUM_PEGS];
    HanoiMove current_move;
    int total_disks;
    int move_count;
    double move_timer;
    double beat_threshold;
    bool waiting_for_beat;
    double volume_history[10];
    int volume_index;
    
    // Solution state - just track where we are
    int total_moves_needed;  // 2^n - 1
    int current_solution_step;
    bool puzzle_complete;
    double completion_glow;
} HanoiSystem;

#define MAX_HANOI_PARTICLES 200

typedef struct {
    double x, y;
    double vx, vy;
    double life;
    double r, g, b;
    double size;
} HanoiParticle;

#endif
