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
} MinesweeperGame;

#endif
