#include "visualization.h"
#include <math.h>

/**
 * INTERACTIVE FRACTAL BLOOM - THREE-BUTTON VERSION
 * 
 * Three unique interactive effects:
 * 
 * LEFT CLICK (Burst):
 *   - All shards expand to 1.9x instantly
 *   - Smooth decay back to normal
 *   - Effect: Explosive, energetic
 * 
 * MIDDLE CLICK (Spin):
 *   - Rapid 360° rotation spin
 *   - Shards maintain current size during spin
 *   - Effect: Dizzying, dynamic rotation
 * 
 * RIGHT CLICK (Shrink):
 *   - All shards shrink to 0.6x
 *   - Smooth expansion back to normal
 *   - Effect: Implosion, opposite of burst
 * 
 * MOUSE MOVEMENT (Pull Distortion):
 *   - Shards stretch toward cursor
 *   - Works with all three click effects
 *   - Effect: Organic, fluid deformation
 * 
 * AUDIO REACTIVITY:
 *   - Shards still extend with frequency
 *   - Bloom rotates with amplitude
 *   - All effects layer on top of audio
 */

// Track state across frames
static double burst_scale = 1.0;      // For left click burst
static double shrink_scale = 1.0;     // For right click shrink
static double spin_velocity = 0.0;    // For middle click spin
static double spin_friction = 0.98;   // Spin deceleration

#define SPIN_SPEED 0.3                // Initial spin velocity on middle click

void draw_waveform_fractal_bloom(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;

    int num_shards = VIS_FREQUENCY_BARS;
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    double max_radius = fmin(vis->width, vis->height) * 0.45;

    // Audio-based rotation
    double avg_amp = 0.0;
    for (int i = 0; i < num_shards; i++) {
        avg_amp += vis->frequency_bands[i];
    }
    avg_amp /= num_shards;
    vis->rotation += avg_amp * 0.03;

    // Mouse pull calculation
    double mouse_pull_strength = 0.0;
    double mouse_pull_angle = 0.0;
    
    if (vis->mouse_over) {
        double dx = vis->mouse_x - center_x;
        double dy = vis->mouse_y - center_y;
        
        double dist = sqrt(dx*dx + dy*dy);
        mouse_pull_strength = 1.0 - fmin(1.0, dist / max_radius);
        mouse_pull_angle = atan2(dy, dx);
    }

    // LEFT CLICK - MASSIVE BURST
    if (vis->mouse_left_pressed) {
        burst_scale = 2.3;  // Even bigger! (230%)
        vis->mouse_left_pressed = FALSE;
    }
    
    if (burst_scale > 1.0) {
        burst_scale = (burst_scale - 1.0) * 0.88 + 1.0;  // Faster decay
    }

    // RIGHT CLICK - EXTREME SHRINK
    if (vis->mouse_right_pressed) {
        shrink_scale = 0.4;  // Shrink more! (40%)
        spin_velocity = 7.0;
        vis->mouse_right_pressed = FALSE;
    }
    
    if (shrink_scale < 1.0) {
        shrink_scale = (shrink_scale - 1.0) * 0.88 + 1.0;  // Faster expansion
    }

    // MIDDLE CLICK - WILD SPIN
    if (vis->mouse_middle_pressed) {
        spin_velocity = 0.5;  // Double the spin speed!
        vis->mouse_middle_pressed = FALSE;
    }
    
    if (spin_velocity > 0.001) {
        vis->rotation += spin_velocity;
        spin_velocity *= 0.96;  // Slower deceleration (spins longer)
    }

    double combined_scale = fmax(burst_scale, shrink_scale);

    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, vis->rotation);
    cairo_translate(cr, -center_x, -center_y);

    for (int i = 0; i < num_shards; i++) {
        double angle = ((double)i / num_shards) * 2.0 * M_PI;
        double amplitude = vis->frequency_bands[i];
        double radius = amplitude * max_radius;

        if (vis->mouse_over && mouse_pull_strength > 0.01) {
            double angle_diff = angle - mouse_pull_angle;
            
            while (angle_diff > M_PI) angle_diff -= 2.0*M_PI;
            while (angle_diff < -M_PI) angle_diff += 2.0*M_PI;
            
            double pull_factor = cos(angle_diff);
            pull_factor = pull_factor * pull_factor;
            
            // Stronger pull in ultra version
            double pull_amount = mouse_pull_strength * pull_factor * 0.7;  // 70% max
            radius *= (1.0 + pull_amount);
            
            // More aggressive angle shift
            double angle_shift = sin(angle_diff) * mouse_pull_strength * 0.25;
            angle += angle_shift;
        }

        radius *= combined_scale;

        double tip_x = center_x + radius * cos(angle);
        double tip_y = center_y + radius * sin(angle);

        double base_angle_offset = M_PI / num_shards;
        double base_x1 = center_x + (radius * 0.3) * cos(angle - base_angle_offset);
        double base_y1 = center_y + (radius * 0.3) * sin(angle - base_angle_offset);
        double base_x2 = center_x + (radius * 0.3) * cos(angle + base_angle_offset);
        double base_y2 = center_y + (radius * 0.3) * sin(angle + base_angle_offset);

        double hue = (double)i / num_shards;
        double sat = fmin(1.0, amplitude * 2.0);
        double val = 1.0;

        double r, g, b;
        int h_i = (int)(hue * 6);
        double f = hue * 6 - h_i;
        double p = val * (1 - sat);
        double q = val * (1 - f * sat);
        double t = val * (1 - (1 - f) * sat);
        
        switch (h_i % 6) {
            case 0: r = val; g = t; b = p; break;
            case 1: r = q; g = val; b = p; break;
            case 2: r = p; g = val; b = t; break;
            case 3: r = p; g = q; b = val; break;
            case 4: r = t; g = p; b = val; break;
            case 5: r = val; g = p; b = q; break;
        }

        cairo_set_source_rgba(cr, r, g, b, 0.8);
        cairo_move_to(cr, center_x, center_y);
        cairo_line_to(cr, base_x1, base_y1);
        cairo_line_to(cr, tip_x, tip_y);
        cairo_line_to(cr, base_x2, base_y2);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

