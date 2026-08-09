#include "visualization.h"

// Mouse tracking structure (add to visualization.h struct)
typedef struct {
    double x;
    double y;
    gboolean in_window;
    gboolean left_button_pressed;
    gboolean right_button_pressed;
    gboolean middle_button_pressed;
    guint64 last_click_time;
} MouseState;

void init_ripple_system(Visualizer *vis) {
    vis->ripple_count = 0;
    vis->ripple_spawn_timer = 0.0;
    vis->last_ripple_volume = 0.0;
    vis->ripple_beat_threshold = 0.1;
    
    // Initialize all ripples as inactive
    for (int i = 0; i < MAX_RIPPLES; i++) {
        vis->ripples[i].active = FALSE;
    }
}

// Mouse event handlers are managed by layout.cpp - ripples are spawned in update_ripples() instead


// Create ripples at mouse position (spawn ripple on click)
void spawn_ripple_at_mouse(Visualizer *vis, double mouse_x, double mouse_y, 
                          double intensity, int frequency_band) {
    // Find an inactive ripple slot
    for (int i = 0; i < MAX_RIPPLES; i++) {
        if (!vis->ripples[i].active) {
            Ripple *ripple = &vis->ripples[i];
            
            ripple->center_x = mouse_x;
            ripple->center_y = mouse_y;
            ripple->radius = 5.0;
            
            // Scale max radius based on screen size and intensity
            double screen_diagonal = sqrt(vis->width * vis->width + vis->height * vis->height);
            ripple->max_radius = screen_diagonal * 0.6 * (0.3 + intensity);
            
            // Slower speed for better visibility
            ripple->speed = 30.0 + intensity * 100.0 + frequency_band * 3.0;
            
            ripple->intensity = intensity;
            ripple->life = 1.0;
            
            // FORCE BRIGHT COLOR VARIETY
            double bright_hues[] = {
                0.33,   // Bright green
                0.75,   // Bright purple/violet
                0.67,   // Bright blue
                0.50,   // Bright cyan
                0.83,   // Bright magenta
                0.17,   // Bright yellow-green
                0.92,   // Bright pink
                0.58,   // Bright blue-cyan
                0.08,   // Bright orange
                0.42    // Bright teal
            };
            
            int color_index;
            if (frequency_band < 10) {
                color_index = frequency_band % 10;
            } else {
                color_index = ((int)(vis->time_offset * 5) + i) % 10;
            }
            
            ripple->hue = bright_hues[color_index];
            ripple->thickness = 4.0 + intensity * 12.0;
            ripple->frequency_band = frequency_band;
            ripple->active = TRUE;
            
            break;
        }
    }
}

void spawn_ripple(Visualizer *vis, double x, double y, double intensity, int frequency_band) {
    // Find an inactive ripple slot
    for (int i = 0; i < MAX_RIPPLES; i++) {
        if (!vis->ripples[i].active) {
            Ripple *ripple = &vis->ripples[i];
            
            ripple->center_x = x;
            ripple->center_y = y;
            ripple->radius = 5.0;
            
            // Scale max radius based on screen size and intensity
            double screen_diagonal = sqrt(vis->width * vis->width + vis->height * vis->height);
            ripple->max_radius = screen_diagonal * 0.6 * (0.3 + intensity); // Bigger ripples
            
            // Slower speed for better visibility
            ripple->speed = 30.0 + intensity * 100.0 + frequency_band * 3.0;
            
            ripple->intensity = intensity;
            ripple->life = 1.0;
            
            // FORCE BRIGHT COLOR VARIETY - use predefined bright colors
            // Define specific bright hues for green, purple, blue, cyan, magenta, yellow
            double bright_hues[] = {
                0.33,   // Bright green
                0.75,   // Bright purple/violet
                0.67,   // Bright blue
                0.50,   // Bright cyan
                0.83,   // Bright magenta
                0.17,   // Bright yellow-green
                0.92,   // Bright pink
                0.58,   // Bright blue-cyan
                0.08,   // Bright orange
                0.42    // Bright teal
            };
            
            // Use different selection methods for maximum variety
            int color_index;
            if (frequency_band < 10) {
                // Use frequency band for low frequencies
                color_index = frequency_band % 10;
            } else {
                // Use time-based cycling for higher frequencies
                color_index = ((int)(vis->time_offset * 5) + i) % 10;
            }
            
            ripple->hue = bright_hues[color_index];
            
            // MUCH thicker rings for visibility
            ripple->thickness = 4.0 + intensity * 12.0;
            
            ripple->frequency_band = frequency_band;
            ripple->active = TRUE;
            
            break;
        }
    }
}

void update_ripples(Visualizer *vis, double dt) {
    // Spawn new ripples based on audio
    vis->ripple_spawn_timer += dt;
    
    // Spawn ripples on mouse clicks
    if (vis->mouse_left_pressed) {
        spawn_ripple_at_mouse(vis, vis->mouse_x, vis->mouse_y, 0.5, 0);
        vis->mouse_left_pressed = FALSE;
    }
    
    if (vis->mouse_middle_pressed) {
        spawn_ripple_at_mouse(vis, vis->mouse_x, vis->mouse_y, 0.5, 0);
        vis->mouse_middle_pressed = FALSE;
    }
    
    if (vis->mouse_right_pressed) {
        spawn_ripple_at_mouse(vis, vis->mouse_x, vis->mouse_y, 0.5, 0);
        vis->mouse_right_pressed = FALSE;
    }
    
    // Check for volume spikes (beats)
    double volume_diff = vis->volume_level - vis->last_ripple_volume;
    vis->last_ripple_volume = vis->volume_level;
    
    // Spawn ripples on volume spikes or timer
    gboolean should_spawn = FALSE;
    int spawn_frequency_band = 0;
    double spawn_intensity = vis->volume_level;
    
    // Method 1: Volume spike detection (KEEP THIS)
    if (volume_diff > vis->ripple_beat_threshold && vis->volume_level > 0.1) {
        should_spawn = TRUE;
        spawn_intensity = volume_diff * 2.0;
        
        // Find the strongest frequency band
        double max_band = 0.0;
        for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
            if (vis->frequency_bands[i] > max_band) {
                max_band = vis->frequency_bands[i];
                spawn_frequency_band = i;
            }
        }
    }
    
    // Method 2: Timer-based spawning - REMOVE THIS ENTIRE SECTION
    /*
    if (vis->ripple_spawn_timer > 0.3 && vis->volume_level > 0.05) {
        should_spawn = TRUE;
        vis->ripple_spawn_timer = 0.0;
        spawn_intensity = vis->volume_level;
        spawn_frequency_band = (int)(vis->time_offset * 3) % VIS_FREQUENCY_BARS;
    }
    */
    
    if (should_spawn) {
        // Spawn ripple at center, or offset based on stereo/frequency data
        double center_x = vis->width / 2.0;
        double center_y = vis->height / 2.0;
        
        // Add some variation to spawn position based on frequency
        double offset_x = sin(vis->time_offset + spawn_frequency_band * 0.5) * vis->width * 0.1;
        double offset_y = cos(vis->time_offset + spawn_frequency_band * 0.3) * vis->height * 0.1;
        
        spawn_ripple(vis, center_x + offset_x, center_y + offset_y, 
                    spawn_intensity, spawn_frequency_band);
        
        vis->ripple_spawn_timer = 0.0;
    }
    
    // Update existing ripples
    for (int i = 0; i < MAX_RIPPLES; i++) {
        if (vis->ripples[i].active) {
            Ripple *ripple = &vis->ripples[i];
            
            // Expand radius
            ripple->radius += ripple->speed * dt;
            
            // Calculate life based on radius
            if (ripple->radius >= ripple->max_radius) {
                ripple->life -= dt * 2.0; // Fade out when max radius reached
            } else {
                // Life decreases slowly during expansion
                ripple->life -= dt * 0.5;
            }
            
            // Remove dead ripples
            if (ripple->life <= 0.0) {
                ripple->active = FALSE;
            }
        }
    }
}


void draw_ripples(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    // Draw all active ripples
    for (int i = 0; i < MAX_RIPPLES; i++) {
        if (vis->ripples[i].active) {
            Ripple *ripple = &vis->ripples[i];
            
            // DEBUG: Force specific colors to test
            double r, g, b;
            
            // Test with hardcoded bright colors first
            if (i % 5 == 0) {
                r = 0.0; g = 1.0; b = 0.0; // Bright green
            } else if (i % 5 == 1) {
                r = 0.5; g = 0.0; b = 1.0; // Bright purple
            } else if (i % 5 == 2) {
                r = 0.0; g = 0.0; b = 1.0; // Bright blue
            } else if (i % 5 == 3) {
                r = 0.0; g = 1.0; b = 1.0; // Bright cyan
            } else {
                r = 1.0; g = 0.0; b = 1.0; // Bright magenta
            }
            
            // Strong alpha for bright visibility
            double alpha = ripple->life * 0.9;
            if (alpha > 0.1) {
                
                // Draw main ripple ring - BRIGHT and SATURATED
                cairo_set_source_rgba(cr, r, g, b, alpha);
                cairo_set_line_width(cr, ripple->thickness);
                cairo_arc(cr, ripple->center_x, ripple->center_y, ripple->radius, 0, 2 * M_PI);
                cairo_stroke(cr);
                
                // Draw inner glow effect - even brighter
                cairo_set_source_rgba(cr, 
                                     fmin(1.0, r * 1.3), 
                                     fmin(1.0, g * 1.3), 
                                     fmin(1.0, b * 1.3), 
                                     alpha * 0.7);
                cairo_set_line_width(cr, ripple->thickness * 0.5);
                cairo_arc(cr, ripple->center_x, ripple->center_y, 
                         ripple->radius - ripple->thickness * 0.5, 0, 2 * M_PI);
                cairo_stroke(cr);
                
                // Skip complementary color for now - just test basic colors
                
                // Add center dot when ripple starts (for debugging)
                if (ripple->radius < 20.0) {
                    cairo_set_source_rgba(cr, r, g, b, alpha);
                    cairo_arc(cr, ripple->center_x, ripple->center_y, 3.0, 0, 2 * M_PI);
                    cairo_fill(cr);
                }
            }
        }
    }
}
