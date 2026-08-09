#ifndef MAZE_GENERATOR_H
#define MAZE_GENERATOR_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_MAZE_WIDTH 100
#define MAX_MAZE_HEIGHT 100
#define MAX_SNAKE_LENGTH 10000

// Cell structure - represents a single maze cell
typedef struct {
    bool walls[4];  // 0=North, 1=East, 2=South, 3=West
    bool visited;
    bool in_path;   // For visualization of algorithm path
} MazeCell;

// Maze structure
typedef struct {
    int width;
    int height;
    MazeCell cells[MAX_MAZE_HEIGHT][MAX_MAZE_WIDTH];
    
    // Snake animation data
    int snake_x[MAX_SNAKE_LENGTH];
    int snake_y[MAX_SNAKE_LENGTH];
    int snake_length;
    double snake_progress;  // 0.0 to 1.0, for smooth animation between cells
    
    // Generation state
    int stack_x[MAX_MAZE_WIDTH * MAX_MAZE_HEIGHT];
    int stack_y[MAX_MAZE_HEIGHT * MAX_MAZE_WIDTH];
    int stack_size;
    bool generating;
    double generation_progress;  // 0.0 to 1.0
    int cells_visited;
    
    // Timing
    double cell_animation_speed;  // Cells per second
    double generation_timer;
} Maze;

// Direction enumeration for movement
typedef enum {
    DIR_NORTH = 0,
    DIR_EAST = 1,
    DIR_SOUTH = 2,
    DIR_WEST = 3
} Direction;

// Function declarations
void maze_init(Maze *maze, int width, int height);
void maze_cleanup(Maze *maze);
void maze_generate(Maze *maze);
void maze_update(Maze *maze, double dt);
void maze_reset_generation(Maze *maze);

// Helper functions
bool maze_has_unvisited_neighbor(Maze *maze, int x, int y, Direction *out_dir);
void maze_carve_passage(Maze *maze, int from_x, int from_y, Direction dir);
void maze_remove_wall(Maze *maze, int x, int y, Direction dir);
void maze_add_snake_step(Maze *maze, int x, int y);
int maze_get_opposite_direction(int dir);

#endif
