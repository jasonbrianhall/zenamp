#ifndef BOUNCINGCIRCLE_H
#define BOUNCINGCIRCLE_H

#include <cairo.h>

#define MAX_TRAIL_POINTS 500
#define INITIAL_BALL_RADIUS_FACTOR 0.015  // Ball radius as fraction of container size
#define CONTAINER_CIRCLE_RADIUS 150.0

typedef struct {
    double x;
    double y;
    double r;
    double g;
    double b;
    double a;
} TrailPoint;

typedef struct {
    // Ball physics
    double pos_x, pos_y;
    double vel_x, vel_y;
    double radius;
    
    // Trail
    TrailPoint trail[MAX_TRAIL_POINTS];
    int trail_count;
    
    // Animation state
    double bounce_counter;  // Counts bounces
    int total_bounces;      // Total bounces so far
    double max_radius;      // Maximum radius before reset
    
    // Colors for trail
    double hue_offset;
    
    // Beat-responsive parameters
    double current_gravity;         // Gravity affected by beat
    double current_growth_rate;     // Growth rate affected by beat
    double beat_magnitude;          // Current beat strength (0.0 to 1.0)
    double beat_decay;              // Decay the beat over time
} BouncingCircleState;

// Function declarations
void init_bouncing_circle_system(void *vis_ptr);
void update_bouncing_circle(void *vis_ptr, double dt);
void draw_bouncing_circle(void *vis_ptr, cairo_t *cr);
void update_bouncing_circle_beat(void *vis_ptr, double beat_magnitude);

#endif
