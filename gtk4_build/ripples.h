// Ripples
#define MAX_RIPPLES 20

typedef struct {
    double center_x, center_y;    // Ripple center position
    double radius;                // Current radius
    double max_radius;            // Maximum radius before fading out
    double speed;                 // Expansion speed
    double intensity;             // Audio intensity that created it
    double life;                  // Life remaining (1.0 to 0.0)
    double hue;                   // Color hue
    double thickness;             // Ring thickness
    gboolean active;              // Is ripple active
    int frequency_band;           // Which frequency band triggered it
} Ripple;
