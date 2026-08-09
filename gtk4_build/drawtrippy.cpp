#include "visualization.h"
#include <math.h>

// Static variables for animation state
static double time_offset = 0;
static double dolphin_jumps[VIS_FREQUENCY_BARS] = {0};
static int dolphin_colors[VIS_FREQUENCY_BARS] = {0};
static double bird_x[5] = {0, -100, -200, -300, -400};
static double bird_y[5] = {0};
static int bird_colors[5] = {0, 1, 2, 3, 4};

void update_trippy(Visualizer *vis, double dt) {
    const double min_dt = 1.0 / 120.0;
    double speed_factor = dt / 0.033;  // Calculate speed relative to 30 FPS baseline
    
    if (dt < min_dt) {
        dt = min_dt;
        speed_factor = dt / 0.033;  // Recalculate speed_factor
    }
    
    // Update time offset for animations
    time_offset += 0.05 * speed_factor;
    
    // Update bird positions
    for (int b = 0; b < 5; b++) {
        bird_x[b] += (3 + b * 0.5) * speed_factor;
        if (bird_x[b] > vis->width + 50) {
            bird_x[b] = -50;
            bird_y[b] = 50 + (rand() % (vis->height - 150));
            bird_colors[b] = rand() % 6;
        }
        if (bird_y[b] == 0) {
            bird_y[b] = 50 + (rand() % (vis->height - 150));
        }
    }
    
    // Update dolphin jumps
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        if (dolphin_jumps[i] > 0) {
            dolphin_jumps[i] -= 0.02 * speed_factor;
            if (dolphin_jumps[i] < 0) {
                dolphin_jumps[i] = 0;
            }
        }
        
        // Trigger new dolphin jumps on strong beats
        if (vis->frequency_bands[i] > 0.6 && dolphin_jumps[i] <= 0) {
            if ((rand() % 100) < 15) { // 15% chance to jump
                dolphin_jumps[i] = 1.0;
                dolphin_colors[i] = rand() % 6;
            }
        }
    }
}

void draw_trippy(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double bar_width = (double)vis->width / VIS_FREQUENCY_BARS;
    
    // Psychedelic background waves
    for (int layer = 0; layer < 3; layer++) {
        cairo_save(cr);
        for (int x = 0; x < vis->width; x += 2) {
            double wave = sin(x * 0.02 + time_offset + layer) * 30;
            double y_pos = vis->height * 0.5 + wave + layer * 40;
            
            double hue_shift = (x / (double)vis->width + time_offset * 0.1 + layer * 0.3);
            hue_shift = fmod(hue_shift, 1.0);
            
            double r = 0.5 + 0.5 * sin(hue_shift * 6.28);
            double g = 0.5 + 0.5 * sin(hue_shift * 6.28 + 2.09);
            double b = 0.5 + 0.5 * sin(hue_shift * 6.28 + 4.18);
            
            cairo_set_source_rgba(cr, r, g, b, 0.15);
            cairo_rectangle(cr, x, y_pos, 2, 20);
            cairo_fill(cr);
        }
        cairo_restore(cr);
    }
    
    // Draw frequency bars with trippy effects
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        double base_height = vis->frequency_bands[i] * vis->height * 0.9;
        
        // Add wave distortion to each bar
        double wave_x = sin(time_offset * 2 + i * 0.3) * 10;
        double wave_scale = 1.0 + sin(time_offset * 3 + i * 0.5) * 0.2;
        double height = base_height * wave_scale;
        
        double x = i * bar_width + wave_x;
        double y = vis->height - height;
        
        // Cycling rainbow colors
        double hue = fmod((double)i / VIS_FREQUENCY_BARS + time_offset * 0.1, 1.0);
        double r = 0.5 + 0.5 * sin(hue * 6.28);
        double g = 0.5 + 0.5 * sin(hue * 6.28 + 2.09);
        double b = 0.5 + 0.5 * sin(hue * 6.28 + 4.18);
        
        // Create pulsing gradient
        cairo_pattern_t *gradient = cairo_pattern_create_linear(0, vis->height, 0, y);
        cairo_pattern_add_color_stop_rgba(gradient, 0, r, g, b, 0.2);
        cairo_pattern_add_color_stop_rgba(gradient, 0.5, r * 1.3, g * 1.3, b * 1.3, 0.7);
        cairo_pattern_add_color_stop_rgba(gradient, 1, r * 1.5, g * 1.5, b * 1.5, 1.0);
        
        cairo_set_source(cr, gradient);
        cairo_rectangle(cr, x + 1, y, bar_width - 2, height);
        cairo_fill(cr);
        cairo_pattern_destroy(gradient);
        
        // Draw glowing outline
        cairo_set_source_rgba(cr, r * 2, g * 2, b * 2, 0.5);
        cairo_set_line_width(cr, 2);
        cairo_rectangle(cr, x + 1, y, bar_width - 2, height);
        cairo_stroke(cr);
        
        // Add random sparkles at high frequencies
        if (vis->frequency_bands[i] > 0.7 && (i * 17 + (int)(time_offset * 50)) % 7 == 0) {
            double sparkle_y = y + (rand() % (int)height);
            cairo_set_source_rgba(cr, 1, 1, 1, 0.8);
            cairo_arc(cr, x + bar_width/2, sparkle_y, 2, 0, 6.28);
            cairo_fill(cr);
        }
        
        // Draw dolphins jumping from bars
        if (dolphin_jumps[i] > 0) {
            double jump_progress = 1.0 - dolphin_jumps[i];
            double arc_x = x + bar_width/2;
            double arc_y = vis->height - (jump_progress * vis->height * 0.6 * sin(jump_progress * 3.14));
            
            // Dolphin colors
            double dr, dg, db;
            switch(dolphin_colors[i]) {
                case 0: dr=1.0; dg=0.3; db=0.8; break; // Pink
                case 1: dr=0.3; dg=0.8; db=1.0; break; // Cyan
                case 2: dr=1.0; dg=0.8; db=0.2; break; // Yellow
                case 3: dr=0.5; dg=1.0; db=0.3; break; // Green
                case 4: dr=1.0; dg=0.5; db=0.2; break; // Orange
                default: dr=0.6; dg=0.3; db=1.0; break; // Purple
            }
            
            // Draw dolphin body (curved arc shape)
            cairo_save(cr);
            cairo_translate(cr, arc_x, arc_y);
            cairo_rotate(cr, jump_progress * 3.14 - 1.57);
            cairo_scale(cr, 1.0, 0.6);
            
            // Body
            cairo_set_source_rgba(cr, dr, dg, db, 0.9);
            cairo_arc(cr, 0, 0, 15, 0, 6.28);
            cairo_fill(cr);
            
            // Tail
            cairo_move_to(cr, -10, 0);
            cairo_line_to(cr, -20, -8);
            cairo_line_to(cr, -20, 8);
            cairo_close_path(cr);
            cairo_fill(cr);
            
            // Dorsal fin
            cairo_move_to(cr, 5, -8);
            cairo_line_to(cr, 8, -15);
            cairo_line_to(cr, 11, -8);
            cairo_close_path(cr);
            cairo_fill(cr);
            
            // Eye
            cairo_set_source_rgba(cr, 1, 1, 1, 0.9);
            cairo_arc(cr, 8, 2, 2, 0, 6.28);
            cairo_fill(cr);
            
            cairo_restore(cr);
            
            // Add splash effect at launch
            if (dolphin_jumps[i] > 0.8) {
                cairo_set_source_rgba(cr, dr * 0.5, dg * 0.5, db * 0.5, 0.4);
                for (int s = 0; s < 5; s++) {
                    double splash_x = x + bar_width/2 + (rand() % 20 - 10);
                    double splash_y = vis->height - (rand() % 30);
                    cairo_arc(cr, splash_x, splash_y, 3, 0, 6.28);
                    cairo_fill(cr);
                }
            }
        }
    }
    
    // Draw flying birds
    for (int b = 0; b < 5; b++) {
        double bx = bird_x[b];
        double by = bird_y[b] + sin(time_offset * 2 + b) * 10;
        
        // Bird colors
        double br, bg, bb;
        switch(bird_colors[b]) {
            case 0: br=1.0; bg=0.2; bb=0.2; break; // Red
            case 1: br=0.2; bg=0.6; bb=1.0; break; // Blue
            case 2: br=1.0; bg=1.0; bb=0.2; break; // Yellow
            case 3: br=0.3; bg=1.0; bb=0.3; break; // Green
            case 4: br=1.0; bg=0.5; bb=1.0; break; // Magenta
            default: br=1.0; bg=0.7; bb=0.2; break; // Orange
        }
        
        cairo_save(cr);
        cairo_translate(cr, bx, by);
        
        // Animated wing flap
        double wing_angle = sin(time_offset * 5 + b) * 0.5;
        
        // Body
        cairo_set_source_rgba(cr, br, bg, bb, 0.8);
        cairo_arc(cr, 0, 0, 4, 0, 6.28);
        cairo_fill(cr);
        
        // Left wing
        cairo_move_to(cr, 0, 0);
        cairo_curve_to(cr, -10, -5 + wing_angle * 10, -15, -3 + wing_angle * 15, -18, 0 + wing_angle * 10);
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
        
        // Right wing  
        cairo_move_to(cr, 0, 0);
        cairo_curve_to(cr, -10, 5 - wing_angle * 10, -15, 3 - wing_angle * 15, -18, 0 - wing_angle * 10);
        cairo_stroke(cr);
        
        cairo_restore(cr);
    }
    
    // Add radial blur effect overlay
    cairo_pattern_t *vignette = cairo_pattern_create_radial(
        vis->width/2, vis->height/2, vis->height * 0.3,
        vis->width/2, vis->height/2, vis->height * 0.8
    );
    cairo_pattern_add_color_stop_rgba(vignette, 0, 0, 0, 0, 0);
    cairo_pattern_add_color_stop_rgba(vignette, 1, 0, 0, 0, 0.3);
    cairo_set_source(cr, vignette);
    cairo_paint(cr);
    cairo_pattern_destroy(vignette);
}
