#ifndef BLOCKSTACK_H
#define BLOCKSTACK_H

#include "visualization.h"

#define MAX_BLOCKS_PER_COLUMN 50
#define BLOCK_COLUMNS 32

typedef enum {
    BLOCK_STATE_RISING,
    BLOCK_STATE_STACKED,
    BLOCK_STATE_FALLING
} BlockState;

typedef struct {
    double height;          // Height of this block
    double y_position;      // Current Y position
    double target_y;        // Target Y position when stacking
    double velocity;        // Falling velocity
    double lifetime;        // Time since creation
    double fade_alpha;      // Opacity for fading
    BlockState state;
    double r, g, b;         // Block color
    int intensity_level;    // Visual intensity (0-10)
} Block;

typedef struct {
    Block blocks[MAX_BLOCKS_PER_COLUMN];
    int block_count;
    double column_height;   // Total stacked height
    double spawn_timer;     // Timer for spawning new blocks
    double last_spawn_intensity;
} BlockColumn;

typedef struct {
    BlockColumn columns[BLOCK_COLUMNS];
    double beat_threshold;
    double spawn_cooldown;
    double block_width;
    double block_min_height;
    double block_max_height;
    double fall_delay;      // How long blocks stay before falling
    double volume_history[10];
    int volume_index;
} BlockStackSystem;

#endif
