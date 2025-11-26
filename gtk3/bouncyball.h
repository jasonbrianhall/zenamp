#define MAX_BOUNCY_BALLS 15

typedef struct {
    double x, y;               // Position
    double vx, vy;             // Velocity
    double radius;             // Ball size
    double base_radius;        // Base size when no audio
    double bounce_damping;     // Energy loss on bounce (0.0-1.0)
    double gravity;            // Downward acceleration
    double hue;                // Color hue
    double saturation;         // Color saturation  
    double brightness;         // Current brightness
    double audio_intensity;    // Current audio influence
    double trail_x[20];        // Position trail for motion blur
    double trail_y[20];        // Position trail for motion blur
    int trail_index;           // Current trail position
    int frequency_band;        // Which frequency band affects this ball
    gboolean active;           // Is ball active
    double spawn_time;         // When ball was created
    double last_bounce_time;   // Last wall/floor bounce
    double energy;             // Current kinetic energy level
    gboolean user_created;     // TRUE if created by mouse click
    int click_type;            // 0=audio, 1=left click (cyan), 2=right click (magenta), 3=middle click (orange)
} BouncyBall;
