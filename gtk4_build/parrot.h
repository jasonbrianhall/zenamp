#ifndef PARROT_H
#define PARROT_H

typedef struct {
    double mouth_open;        // 0.0 to 1.0
    double blink_timer;       // Timer for blinking
    gboolean eye_closed;      // Is eye currently closed
    double head_bob_offset;   // Head bobbing animation
    double body_bounce;       // Body bounce offset
    double wing_flap_angle;   // Wing flapping angle
    double tail_sway;         // Tail feather sway amount
    double pupil_x;           // Pupil X position offset
    double pupil_y;           // Pupil Y position offset
    double chest_scale;       // Chest breathing scale
    double foot_tap;          // Left foot tapping offset
    double right_foot_tap;    // Right foot tapping offset
    double glow_intensity;    // Glow effect intensity
    double last_beat_time;    // Last detected beat time
} ParrotState;

#endif
