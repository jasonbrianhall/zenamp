#ifndef MAZE3D_H
#define MAZE3D_H

#define MAZE_WIDTH 16
#define MAZE_HEIGHT 16
#define MAX_PATH_LENGTH 256
#define MAX_RATS 8
#define MAX_ELEPHANTS 2

typedef enum {
    WALL_NORTH = 1,
    WALL_EAST = 2,
    WALL_SOUTH = 4,
    WALL_WEST = 8
} WallFlags;

typedef struct {
    int x, y;
} Point;

typedef struct {
    double x, y;           // Player position
    double angle;          // Viewing angle in radians
    double target_angle;   // Target angle for smooth turning
    int map_x, map_y;      // Current grid cell
    double move_speed;     // Movement speed
    double turn_speed;     // Turning speed
} Player;

typedef struct {
    double x, y, z;        // 3D position
    double rotation;       // Y-axis rotation
    double bob_offset;     // Vertical bobbing offset
    double scale;          // Size multiplier
    gboolean found;        // Has player found the penguin?
    double found_time;     // Time when penguin was found
} Penguin;

typedef struct {
    double x, y;           // Position
    double angle;          // Direction facing
    double speed;          // Movement speed
    double bob_offset;     // Animation offset
    gboolean active;       // Is this entity active?
} Creature;

typedef struct {
    unsigned char cells[MAZE_HEIGHT][MAZE_WIDTH];  // Wall flags for each cell
    Point path[MAX_PATH_LENGTH];   // Solution path
    int path_length;               // Length of solution path
    int current_path_index;        // Current position along path
    gboolean solved;               // Has maze been solved?
    
    Player player;
    Penguin penguin;
    Creature rats[MAX_RATS];       // Rats wandering the maze
    Creature elephants[MAX_ELEPHANTS];  // Elephants wandering the maze
    
    double maze_time;              // Time spent in current maze
    double audio_pulse;            // Audio-reactive pulse effect
    double wall_colors[4][3];      // RGBA colors for walls (N, E, S, W)
    
    gboolean auto_solve;           // Automatically navigate the maze
    double move_timer;             // Timer for automatic movement
    
    int exit_x, exit_y;            // Exit position (where penguin is)
} Maze3D;

void draw_rat_3d(cairo_t *cr, double screen_x, double distance, double rotation, double bob_offset, double half_height);
void draw_elephant_3d(cairo_t *cr, double screen_x, double distance, double rotation, double bob_offset, double half_height);
void draw_penguin_3d(cairo_t *cr, double screen_x, double distance, double scale, double rotation, double bob_offset, gboolean found, double pulse, double half_height);
void find_valid_spawn_position(Maze3D *maze, double *x, double *y, double radius);


#endif // MAZE3D_H
