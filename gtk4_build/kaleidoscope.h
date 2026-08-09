// Kaleidoscope
#define MAX_KALEIDOSCOPE_SHAPES 20
#define KALEIDOSCOPE_MIRRORS 6  // Number of mirror segments (creates 6-fold symmetry)

typedef struct {
    double x, y;               // Position in the base triangle
    double vx, vy;             // Velocity
    double rotation;           // Current rotation angle
    double rotation_speed;     // Rotation speed
    double scale;              // Size scale
    double scale_speed;        // Scale pulsing speed
    double hue;                // Color hue
    double saturation;         // Color saturation
    double brightness;         // Current brightness
    double base_brightness;    // Base brightness level
    int shape_type;            // 0=circle, 1=triangle, 2=square, 3=star, 4=hexagon
    double life;               // Shape lifetime (for fading)
    double pulse_phase;        // Phase for pulsing effects
    int frequency_band;        // Which frequency band controls this shape
    gboolean active;           // Is shape active
} KaleidoscopeShape;
