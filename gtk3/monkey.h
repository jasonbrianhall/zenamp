#ifndef MONKEY_H
#define MONKEY_H

typedef struct {
    double left_arm_angle;      // Left arm/stick swing angle
    double right_arm_angle;     // Right arm/stick swing angle
    double left_stick_height;   // Left drumstick tip height (negative = raised)
    double right_stick_height;  // Right drumstick tip height (negative = raised)
    gboolean next_arm_left;     // Alternates which arm strikes next
    double last_left_hit_time;  // Time of last left drum hit
    double last_right_hit_time; // Time of last right drum hit
    gboolean left_hit_active;   // Left drum currently reacting to a hit
    gboolean right_hit_active;  // Right drum currently reacting to a hit
    double head_bob_offset;     // Head bobbing animation
    double body_bounce;         // Body bounce offset
    double mouth_open;          // 0.0 to 1.0, screech/chatter
    double blink_timer[3];      // Independent blink timer per monkey
    gboolean eye_closed[3];     // Independent eye state per monkey
    double ear_wiggle[3];       // Independent ear wiggle per monkey
    double tail_sway;           // Tail sway amount
    double chest_scale;         // Chest breathing scale
    double glow_intensity;      // Glow effect intensity
    double last_beat_time;      // Last detected beat time (drives hits + particles)
} MonkeyState;

#endif
