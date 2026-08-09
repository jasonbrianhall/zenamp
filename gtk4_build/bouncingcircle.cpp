#include "bouncingcircle.h"
#include "visualization.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>


#define BASE_GRAVITY 500.0
#define DAMPING 0.99
#define CONTAINER_MARGIN 20.0
#define BASE_GROWTH_RATE 2.0
#define BEAT_DECAY_RATE 3.0
#define TRAIL_LINE_WIDTH 4.0

void init_bouncing_circle_system(void *vis_ptr) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    if (!vis) return;
    
    memset(&vis->bouncing_circle_state, 0, sizeof(BouncingCircleState));
    vis->bouncing_circle_state.pos_x = vis->width / 2.0;
    vis->bouncing_circle_state.pos_y = vis->height / 2.0;
    vis->bouncing_circle_state.vel_x = 300.0;  // Initial velocity
    vis->bouncing_circle_state.vel_y = 200.0;
    
    // Calculate initial ball radius based on visualization dimensions
    double container_radius = fmin(vis->width, vis->height) * 0.4;
    vis->bouncing_circle_state.radius = container_radius * INITIAL_BALL_RADIUS_FACTOR;
    
    vis->bouncing_circle_state.hue_offset = 0.0;
    vis->bouncing_circle_state.trail_count = 0;
    vis->bouncing_circle_state.bounce_counter = 0;
    vis->bouncing_circle_state.total_bounces = 0;
    vis->bouncing_circle_state.max_radius = 0;
    
    // Initialize beat-responsive parameters
    vis->bouncing_circle_state.current_gravity = BASE_GRAVITY;
    vis->bouncing_circle_state.current_growth_rate = BASE_GROWTH_RATE;
    vis->bouncing_circle_state.beat_magnitude = 0.0;
    vis->bouncing_circle_state.beat_decay = 0.0;
}

void update_bouncing_circle_beat(void *vis_ptr, double beat_magnitude) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    if (!vis) return;
    
    BouncingCircleState *state = &vis->bouncing_circle_state;
    
    // Clamp beat_magnitude to 0-1 range
    if (beat_magnitude < 0.0) beat_magnitude = 0.0;
    if (beat_magnitude > 1.0) beat_magnitude = 1.0;
    
    // Set the beat magnitude (will decay over time)
    state->beat_magnitude = beat_magnitude;
    state->beat_decay = BEAT_DECAY_RATE;
}

void update_bouncing_circle(void *vis_ptr, double dt) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    if (!vis || vis->width <= 0 || vis->height <= 0 || dt <= 0) return;
    
    BouncingCircleState *state = &vis->bouncing_circle_state;
    
    // Decay beat magnitude over time
    if (state->beat_decay > 0.0) {
        state->beat_decay -= dt;
        if (state->beat_decay < 0.0) {
            state->beat_decay = 0.0;
            state->beat_magnitude = 0.0;
        } else {
            // Smooth decay: keep magnitude while decay timer is active
            // Once decay reaches 0, magnitude goes to 0
        }
    } else {
        state->beat_magnitude = 0.0;
    }
    
    // Calculate gravity based on beat
    // Higher beat = LOWER gravity (inverted relationship)
    // At beat_magnitude = 1.0, gravity becomes negative (upward lift)
    // At beat_magnitude = 0.0, gravity is full BASE_GRAVITY (downward pull)
    state->current_gravity = BASE_GRAVITY - (state->beat_magnitude * BASE_GRAVITY * 2.0);
    
    // Calculate growth rate based on beat
    // Higher beat = FASTER growth (minimum of 2.0)
    state->current_growth_rate = BASE_GROWTH_RATE + (state->beat_magnitude * 4.0);
    
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    double container_radius = fmin(vis->width, vis->height) * 0.4;
    
    // Apply gravity
    state->vel_y += state->current_gravity * dt;
    
    // Update position
    state->pos_x += state->vel_x * dt;
    state->pos_y += state->vel_y * dt;
    
    // Distance from center
    double dx = state->pos_x - center_x;
    double dy = state->pos_y - center_y;
    double dist_from_center = sqrt(dx*dx + dy*dy);
    
    // Container collision with bounce
    double collision_radius = container_radius - state->radius;
    if (dist_from_center > collision_radius) {
        // Normal vector from center to ball
        double nx = dx / dist_from_center;
        double ny = dy / dist_from_center;
        
        // Move ball to collision boundary
        state->pos_x = center_x + nx * collision_radius;
        state->pos_y = center_y + ny * collision_radius;
        
        // Reflect velocity
        double vel_dot_normal = state->vel_x * nx + state->vel_y * ny;
        state->vel_x = (state->vel_x - 2 * vel_dot_normal * nx) * DAMPING;
        state->vel_y = (state->vel_y - 2 * vel_dot_normal * ny) * DAMPING;
        
        // Increment bounce counter
        state->bounce_counter += 1.0;
        state->total_bounces++;
        
        // Grow the ball using beat-responsive growth rate
        state->radius += state->current_growth_rate;
        
        // Subtle velocity boost as ball gets bigger to prevent exponential growth
        // Just enough to keep it bouncing instead of sitting on the bottom
        double size_factor = 1.0 + (state->radius / (container_radius * 0.95)) * 0.08;
        state->vel_x *= size_factor;
        state->vel_y *= size_factor;
        
        // Reset if ball reaches container size
        if (state->radius >= container_radius * 0.95) {
            state->radius = container_radius * INITIAL_BALL_RADIUS_FACTOR;
            state->bounce_counter = 0;
            state->hue_offset = fmod(state->hue_offset + 0.3, 1.0);
            
            // Complete clean reset - put ball back in center with fresh velocity
            state->pos_x = center_x;
            state->pos_y = center_y;
            state->vel_x = 300.0;
            state->vel_y = 200.0;
        }
    }
    
    // Add trail point using circular buffer
    int current_index = state->trail_count % MAX_TRAIL_POINTS;
    TrailPoint *point = &state->trail[current_index];
    point->x = state->pos_x;
    point->y = state->pos_y;
    
    // Calculate hue based on total trail count (creates smooth color progression)
    double hue = fmod((double)state->trail_count * 0.01 + state->hue_offset, 1.0);
    double sat = 0.9;
    double val = 0.95;
    
    // HSV to RGB conversion
    double c = val * sat;
    double h_prime = fmod(hue * 6.0, 6.0);
    double x = c * (1.0 - fabs(fmod(h_prime, 2.0) - 1.0));
    
    double r, g, b;
    if (h_prime < 1.0) { r = c; g = x; b = 0; }
    else if (h_prime < 2.0) { r = x; g = c; b = 0; }
    else if (h_prime < 3.0) { r = 0; g = c; b = x; }
    else if (h_prime < 4.0) { r = 0; g = x; b = c; }
    else if (h_prime < 5.0) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    
    point->r = r;
    point->g = g;
    point->b = b;
    point->a = 1.0; // Full opacity when added
    
    state->trail_count++;
}

void draw_bouncing_circle(void *vis_ptr, cairo_t *cr) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    if (!vis || vis->width <= 0 || vis->height <= 0) return;
    
    BouncingCircleState *state = &vis->bouncing_circle_state;
    
    // Set background
    cairo_set_source_rgb(cr, 0.05, 0.05, 0.08);
    cairo_paint(cr);
    
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    double container_radius = fmin(vis->width, vis->height) * 0.4;
    
    // Draw main container circle
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.25);
    cairo_set_line_width(cr, 3.0);
    cairo_arc(cr, center_x, center_y, container_radius, 0, 2 * M_PI);
    cairo_stroke(cr);
    
    // Draw rainbow trail with age-based fading
    int actual_trail_length = state->trail_count < MAX_TRAIL_POINTS ? state->trail_count : MAX_TRAIL_POINTS;
    
    for (int i = 0; i < actual_trail_length - 1; i++) {
        // Calculate which points to draw (going backwards through the buffer)
        int idx1 = (state->trail_count - actual_trail_length + i) % MAX_TRAIL_POINTS;
        int idx2 = (state->trail_count - actual_trail_length + i + 1) % MAX_TRAIL_POINTS;
        
        TrailPoint *p1 = &state->trail[idx1];
        TrailPoint *p2 = &state->trail[idx2];
        
        // Calculate age-based alpha (older points fade out)
        double age_factor = (double)i / actual_trail_length;
        double alpha1 = p1->a * (1.0 - age_factor * 0.3); // Slight fade based on age
        double alpha2 = p2->a * (1.0 - age_factor * 0.3);
        
        // Gradient from p1 to p2
        cairo_pattern_t *gradient = cairo_pattern_create_linear(p1->x, p1->y, p2->x, p2->y);
        cairo_pattern_add_color_stop_rgba(gradient, 0.0, p1->r, p1->g, p1->b, alpha1);
        cairo_pattern_add_color_stop_rgba(gradient, 1.0, p2->r, p2->g, p2->b, alpha2);
        
        cairo_set_source(cr, gradient);
        cairo_set_line_width(cr, TRAIL_LINE_WIDTH + (1.0 - age_factor) * 4.0);
        cairo_move_to(cr, p1->x, p1->y);
        cairo_line_to(cr, p2->x, p2->y);
        cairo_stroke(cr);
        
        cairo_pattern_destroy(gradient);
    }
    
    // Draw the ball
    double hue = fmod(state->bounce_counter / 10.0 + state->hue_offset, 1.0);
    double c = 0.9;
    double h_prime = fmod(hue * 6.0, 6.0);
    double x = c * (1.0 - fabs(fmod(h_prime, 2.0) - 1.0));
    
    double r, g, b;
    if (h_prime < 1.0) { r = c; g = x; b = 0; }
    else if (h_prime < 2.0) { r = x; g = c; b = 0; }
    else if (h_prime < 3.0) { r = 0; g = c; b = x; }
    else if (h_prime < 4.0) { r = 0; g = x; b = c; }
    else if (h_prime < 5.0) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    
    // Ball glow (intensified by beat)
    double glow_intensity = 0.3 + (state->beat_magnitude * 0.3);
    cairo_set_source_rgba(cr, r, g, b, glow_intensity);
    cairo_set_line_width(cr, 6.0);
    cairo_arc(cr, state->pos_x, state->pos_y, state->radius + 4, 0, 2 * M_PI);
    cairo_stroke(cr);
    
    // Ball body
    cairo_set_source_rgb(cr, r, g, b);
    cairo_arc(cr, state->pos_x, state->pos_y, state->radius, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Ball highlight
    cairo_set_source_rgba(cr, 1, 1, 1, 0.5);
    cairo_arc(cr, state->pos_x - state->radius * 0.3, state->pos_y - state->radius * 0.3, 
              state->radius * 0.4, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Draw info text
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_font_size(cr, 16);
    
    char info_text[128];
    snprintf(info_text, sizeof(info_text), "Bounces: %d | Size: %.1f/%.1f | Gravity: %.0f | Growth: %.2f", 
             state->total_bounces, state->radius, container_radius * 0.95, 
             state->current_gravity, state->current_growth_rate);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, info_text, &extents);
    
    cairo_move_to(cr, vis->width * 0.5 - extents.width * 0.5, vis->height - 30);
    cairo_show_text(cr, info_text);
    
    // Draw beat indicator
    if (state->beat_magnitude > 0.0) {
        cairo_set_source_rgba(cr, 1.0, 0.5, 0.0, state->beat_magnitude * 0.7);
        cairo_set_font_size(cr, 14);
        const char *beat_text = "♪ BEAT ♪";
        cairo_text_extents(cr, beat_text, &extents);
        cairo_move_to(cr, vis->width * 0.5 - extents.width * 0.5, 30);
        cairo_show_text(cr, beat_text);
    }
}
