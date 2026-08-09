// Ball physics constants
#define MAX_RADIAL_BALLS 128
#define BALL_RADIUS 4.0
#define GRAVITY 400.0
#define BOUNCE_DAMPING 0.75
#define FRICTION 0.98
#define BALL_SPAWN_THRESHOLD 0.3

// Ball structure
typedef struct {
    double x, y;              // Current position
    double vx, vy;            // Velocity
    double radius;
    double lifetime;
    double max_lifetime;
    double r, g, b;           // Color
    int source_bar;           // Which bar it came from
    gboolean active;
} RadialBall;

typedef struct {
    RadialBall balls[MAX_RADIAL_BALLS];
    int ball_count;
    double center_x, center_y;
    double inner_radius;
    double outer_radius;
    double spawn_timer;
} RadialBarsSystem;
