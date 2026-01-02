#define ROBOT_CHASER_MAZE_WIDTH 25
#define ROBOT_CHASER_MAZE_HEIGHT 15
#define MAX_ROBOT_CHASER_PELLETS 300
#define MAX_ROBOT_CHASER_ROBOTS 6
#define ROBOT_CHASER_ROBOT_COLORS 4
#define robot_chaser_maze_template robot_chaser_level_1

typedef enum {
    GAME_PLAYING = 0,
    GAME_PLAYER_DIED = 1,
    GAME_LEVEL_COMPLETE = 2,
    GAME_GAME_OVER = 3
} GameState;

typedef enum {
    CHASER_WALL = 1,
    CHASER_PELLET = 2,
    CHASER_POWER_PELLET = 3,
    CHASER_EMPTY = 0
} ChaserCellType;

typedef enum {
    CHASER_UP = 0,
    CHASER_DOWN = 1,
    CHASER_LEFT = 2,
    CHASER_RIGHT = 3
} ChaserDirection;

typedef struct {
    double x, y;              // Smooth position (can be fractional)
    int grid_x, grid_y;       // Grid position
    ChaserDirection direction; // Current direction
    ChaserDirection next_direction; // Queued direction change
    double mouth_angle;       // For animation
    double size_multiplier;   // Audio-reactive size
    gboolean moving;          // Is currently moving
    double speed;             // Movement speed
    double beat_pulse;        // Pulse effect on beats
} ChaserPlayer;

typedef struct {
    double x, y;              // Smooth position
    int grid_x, grid_y;       // Grid position
    ChaserDirection direction; // Current direction
    int color_index;          // 0=red, 1=pink, 2=cyan, 3=orange
    double hue;               // Current color hue
    double target_hue;        // Target color based on audio
    double size_multiplier;   // Audio-reactive size
    double speed;             // Movement speed
    double scared_timer;      // How long scared state lasts
    gboolean scared;          // Is in scared state
    gboolean visible;         // Is currently visible
    double blink_timer;       // For blinking effect
    int frequency_band;       // Which frequency band controls this ROBOT
    double audio_intensity;   // Current audio level affecting this ROBOT
} ChaserRobot;

typedef struct {
    int grid_x, grid_y;       // Grid position
    gboolean active;          // Is pellet still there
    double pulse_phase;       // For pulsing animation
    double size_multiplier;   // Audio-reactive size
    gboolean is_power_pellet; // Is this a power pellet
    double hue;               // Color hue
    int frequency_band;       // Which frequency affects this pellet
} ChaserPellet;

#define ROBOT_CHASER_NUM_LEVELS 5

// Level 1 - Original (Balanced)
static const int robot_chaser_level_1[ROBOT_CHASER_MAZE_HEIGHT][ROBOT_CHASER_MAZE_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,3,1,1,1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,2,1,1,1,3,1},
    {0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0},
    {1,2,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,2,1,2,1,1,1,2,1},
    {1,2,2,2,2,2,1,2,2,2,2,2,1,2,2,2,2,2,1,2,2,2,2,2,1},
    {1,1,1,1,1,2,1,1,1,1,1,0,1,0,1,1,1,1,1,2,1,1,1,1,1},
    {0,0,0,0,1,2,1,0,0,0,0,0,0,0,0,0,0,0,1,2,1,0,0,0,0},
    {1,1,1,1,1,2,1,1,1,1,1,0,1,0,1,1,1,1,1,2,1,1,1,1,1},
    {1,2,2,2,2,2,1,2,2,2,2,2,1,2,2,2,2,2,1,2,2,2,2,2,1},
    {1,2,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,2,1,2,1,1,1,2,1},
    {0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0},
    {1,3,1,1,1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,2,1,1,1,3,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Level 2 - The Corridors (Long narrow passages)
static const int robot_chaser_level_2[ROBOT_CHASER_MAZE_HEIGHT][ROBOT_CHASER_MAZE_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,2,1},
    {1,2,1,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,2,1,2,1},
    {1,2,1,2,1,1,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,2,1,2,1},
    {1,2,1,2,1,3,2,2,2,2,1,2,1,2,1,2,2,2,2,3,1,2,1,2,1},
    {1,2,1,2,1,1,1,1,1,2,1,2,1,2,1,2,1,1,1,1,1,2,1,2,1},
    {2,2,2,2,2,2,2,2,2,2,1,2,0,2,1,2,2,2,2,2,2,2,2,2,2},
    {1,2,1,2,1,1,1,1,1,2,1,2,1,2,1,2,1,1,1,1,1,2,1,2,1},
    {1,2,1,2,1,3,2,2,2,2,1,2,1,2,1,2,2,2,2,3,1,2,1,2,1},
    {1,2,1,2,1,1,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,2,1,2,1},
    {1,2,1,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,2,1,2,1},
    {1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Level 3 - The Cross (Central cross pattern)
static const int robot_chaser_level_3[ROBOT_CHASER_MAZE_HEIGHT][ROBOT_CHASER_MAZE_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,2,1,2,2,2,1,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,1,1,1,1,1,2,1,2,2,2,1,2,1,1,1,1,1,1,1,2,1},
    {1,2,1,3,2,2,2,2,1,2,1,2,2,2,1,2,1,2,2,2,2,3,1,2,1},
    {1,2,1,1,1,1,1,2,1,2,1,2,2,2,1,2,1,2,1,1,1,1,1,2,1},
    {1,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,1},
    {1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1},
    {0,2,2,2,2,2,2,2,2,2,2,2,0,2,2,2,2,2,2,2,2,2,2,2,0},
    {1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1},
    {1,2,2,2,2,2,2,2,1,2,1,2,2,2,1,2,1,2,2,2,2,2,2,2,1},
    {1,2,1,1,1,1,1,2,1,2,1,2,2,2,1,2,1,2,1,1,1,1,1,2,1},
    {1,2,1,3,2,2,2,2,1,2,1,2,2,2,1,2,1,2,2,2,2,3,1,2,1},
    {1,2,1,1,1,1,1,1,1,2,2,2,2,2,2,2,1,1,1,1,1,1,1,2,1},
    {1,2,2,2,2,2,2,2,2,2,1,2,2,2,1,2,2,2,2,2,2,2,2,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Level 4 - The Spiral (Spiral pattern, very challenging)
static const int robot_chaser_level_4[ROBOT_CHASER_MAZE_HEIGHT][ROBOT_CHASER_MAZE_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1},
    {1,2,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,2,1},
    {1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,2,1},
    {1,3,1,2,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,2,1,3,1},
    {1,2,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,2,1,2,1},
    {0,2,1,2,1,2,1,2,2,2,2,2,0,2,2,2,2,2,1,2,1,2,1,2,0},
    {1,2,1,2,1,2,1,2,1,1,1,1,1,1,1,1,1,2,1,2,1,2,1,2,1},
    {1,2,1,2,1,2,1,2,1,3,2,2,2,2,2,3,1,2,1,2,1,2,1,2,1},
    {1,2,1,2,1,2,1,2,1,1,1,1,1,1,1,1,1,2,1,2,1,2,1,2,1},
    {1,2,1,2,1,2,1,2,2,2,2,2,2,2,2,2,2,2,1,2,1,2,1,2,1},
    {1,2,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,2,1,2,1},
    {1,2,1,2,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,2,1,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Level 5 - The Arena (Open center with scattered walls)
static const int robot_chaser_level_5[ROBOT_CHASER_MAZE_HEIGHT][ROBOT_CHASER_MAZE_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,1,2,2,2,2,2,2,2,2,2,2,2,1,1,2,1,1,2,1},
    {1,2,1,1,2,1,1,2,2,2,2,2,2,2,2,2,2,2,1,1,2,1,1,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,3,2,2,2,2,2,2,2,1,1,2,2,2,1,1,2,2,2,2,2,2,2,3,1},
    {1,2,2,2,2,2,2,2,2,1,1,2,2,2,1,1,2,2,2,2,2,2,2,2,1},
    {0,2,2,2,2,2,2,2,2,2,2,2,0,2,2,2,2,2,2,2,2,2,2,2,0},
    {1,2,2,2,2,2,2,2,2,1,1,2,2,2,1,1,2,2,2,2,2,2,2,2,1},
    {1,3,2,2,2,2,2,2,2,1,1,2,2,2,1,1,2,2,2,2,2,2,2,3,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,1,2,2,2,2,2,2,2,2,2,2,2,1,1,2,1,1,2,1},
    {1,2,1,1,2,1,1,2,2,2,2,2,2,2,2,2,2,2,1,1,2,1,1,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// Array of all levels
static const int (*robot_chaser_levels[ROBOT_CHASER_NUM_LEVELS])[ROBOT_CHASER_MAZE_WIDTH] = {
    robot_chaser_level_1,
    robot_chaser_level_2,
    robot_chaser_level_3,
    robot_chaser_level_4,
    robot_chaser_level_5
};


