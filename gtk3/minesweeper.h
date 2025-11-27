#ifndef MINESWEEPER_H
#define MINESWEEPER_H

#include <gtk/gtk.h>
#include <cairo.h>
#include <stdbool.h>

// Difficulty levels
typedef enum {
    DIFFICULTY_EASY,      // 8x8, 10 mines
    DIFFICULTY_MEDIUM,    // 10x10, 20 mines
    DIFFICULTY_HARD       // 12x12, 40 mines
} MineSweepDifficulty;

// Button areas for difficulty selection
typedef struct {
    double x, y, width, height;
    bool hovered;
    MineSweepDifficulty difficulty;
} DifficultyButton;

// Dynamic grid sizing based on difficulty
#define MINESWEEPER_MAX_GRID_SIZE 12
#define MINESWEEPER_CELL_PADDING 2

// Particle system
#define MAX_EXPLOSION_PARTICLES 500

typedef enum {
    PARTICLE_SPARK,
    PARTICLE_DEBRIS,
    PARTICLE_SMOKE
} ParticleType;

typedef struct {
    double x, y;
    double vx, vy;  // velocity
    double life;    // 0.0 to 1.0
    ParticleType type;
} ExplosionParticle;

typedef struct {
    ExplosionParticle particles[MAX_EXPLOSION_PARTICLES];
    int particle_count;
} ExplosionSystem;

typedef enum {
    CELL_HIDDEN,
    CELL_REVEALED_EMPTY,
    CELL_REVEALED_MINE,
    CELL_FLAGGED,
    CELL_REVEALED_NUMBER
} CellState;

typedef struct {
    CellState state;
    bool is_mine;
    int adjacent_mines;
    double reveal_animation;  // 0.0 to 1.0
    double pulse_intensity;   // Beat reactivity
    double beat_phase;        // For beat-based wave effects
    double distance_glow;     // Glow based on distance from epicenter
    int dist_to_revealed;     // Distance to nearest revealed cell (for hint delay)
} MinesweeperCell;

typedef struct {
    MinesweeperCell grid[MINESWEEPER_MAX_GRID_SIZE][MINESWEEPER_MAX_GRID_SIZE];
    bool game_over;
    bool game_won;
    int flags_placed;
    int cells_revealed;
    double game_over_time;
    double beat_glow;
    int last_revealed_x;
    int last_revealed_y;
    
    // Difficulty settings
    MineSweepDifficulty current_difficulty;
    int grid_size;
    int total_mines;
    int total_cells;
    
    // Difficulty buttons
    DifficultyButton difficulty_buttons[3];
    bool show_difficulty_menu;
    
    // First click tracking
    bool first_click_made;
    
    // Beat reactivity
    double beat_magnitude;      // Current beat strength (0-1)
    double bass_energy;         // Low frequency energy
    double mid_energy;          // Mid frequency energy
    double high_energy;         // High frequency energy
    double beat_time;           // Time since last beat
    int last_beat_x, last_beat_y;  // Epicenter of last beat
    double wave_expansion;      // For expanding beat waves
    
    // Background explosion system
    ExplosionSystem explosion_system;
    
    // Timer
    double elapsed_time;        // Elapsed time in seconds
    
    // Idle hint system
    double idle_time;           // Time since last move
    double idle_threshold;      // Seconds before showing hints (3 seconds)
    double hint_intensity;      // 0.0 to 1.0, how red the mines glow
} MinesweeperGame;

#endif
