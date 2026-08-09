#include "visualization.h"

void init_fourier_system(Visualizer *vis) {
    vis->fourier_time = 0.0;
    vis->fourier_rotation_speed = 1.0;
    vis->fourier_zoom = 1.0;
    vis->fourier_display_mode = 0; // Start with circular
    vis->fourier_trail_fade = 0.95;
    vis->show_fourier_math = FALSE; // Start with math overlay off for cleaner look
    
    // Initialize frequency bins
    for (int i = 0; i < FOURIER_POINTS; i++) {
        FourierBin *bin = &vis->fourier_bins[i];
        bin->real = 0.0;
        bin->imag = 0.0;
        bin->magnitude = 0.0;
        bin->phase = 0.0;
        bin->x = 0.0;
        bin->y = 0.0;
        bin->trail_index = 0;
        bin->hue = (double)i / FOURIER_POINTS; // Rainbow across spectrum
        bin->intensity = 0.0;
        
        // Clear trail history
        for (int j = 0; j < FOURIER_HISTORY; j++) {
            bin->trail_x[j] = 0.0;
            bin->trail_y[j] = 0.0;
        }
    }
}

void update_fourier_transform(Visualizer *vis, double dt) {
    vis->fourier_time += dt;
    
    // Update display mode and zoom automatically
    update_fourier_mode(vis, dt);
    
    // Simple DFT calculation (we could optimize with FFT later)
    // This gives us the mathematical foundation that's visually interesting
    
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    double base_radius = fmin(vis->width, vis->height) * 0.3 * vis->fourier_zoom;
    
    // Calculate DFT for visualization
    for (int k = 0; k < FOURIER_POINTS; k++) {
        FourierBin *bin = &vis->fourier_bins[k];
        
        // Reset for this frame
        bin->real = 0.0;
        bin->imag = 0.0;
        
        // Calculate DFT bin k
        for (int n = 0; n < VIS_SAMPLES; n++) {
            if (n >= VIS_SAMPLES) break;
            
            double angle = -2.0 * M_PI * k * n / VIS_SAMPLES;
            double sample = vis->audio_samples[n];
            
            bin->real += sample * cos(angle);
            bin->imag += sample * sin(angle);
        }
        
        // Normalize
        bin->real /= VIS_SAMPLES;
        bin->imag /= VIS_SAMPLES;
        
        // Calculate magnitude and phase
        bin->magnitude = sqrt(bin->real * bin->real + bin->imag * bin->imag);
        bin->phase = atan2(bin->imag, bin->real);
        
        // Apply sensitivity and logarithmic scaling for better visuals
        bin->intensity = log(1.0 + bin->magnitude * vis->sensitivity * 10.0) / log(11.0);
        if (bin->intensity > 1.0) bin->intensity = 1.0;
        
        // Calculate position based on display mode
        switch (vis->fourier_display_mode) {
            case 0: // Circular - traditional complex plane
            {
                double radius = base_radius + bin->magnitude * base_radius * 0.5;
                double angle = (double)k / FOURIER_POINTS * 2.0 * M_PI + vis->fourier_time * vis->fourier_rotation_speed;
                
                bin->x = center_x + cos(angle) * radius;
                bin->y = center_y + sin(angle) * radius;
                break;
            }
            case 1: // Linear - frequency spectrum style
            {
                bin->x = (double)k / FOURIER_POINTS * vis->width;
                bin->y = center_y - bin->magnitude * vis->height * 0.4;
                break;
            }
            case 2: // 3D Perspective - rotating frequency surface
            {
                double angle = (double)k / FOURIER_POINTS * 2.0 * M_PI + vis->fourier_time * vis->fourier_rotation_speed;
                double depth = sin(angle) * 0.5 + 0.5; // 0 to 1
                double scale = 0.5 + depth * 0.5; // Perspective scaling
                
                bin->x = center_x + cos(angle) * base_radius * scale;
                bin->y = center_y + (sin(angle) * base_radius * 0.3 - bin->magnitude * vis->height * 0.3) * scale;
                break;
            }
        }
        
        // Store in trail history
        bin->trail_x[bin->trail_index] = bin->x;
        bin->trail_y[bin->trail_index] = bin->y;
        bin->trail_index = (bin->trail_index + 1) % FOURIER_HISTORY;
    }
}

void draw_fourier_transform(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    
    // Draw background elements
    draw_fourier_background(vis, cr, center_x, center_y);
    
    // Draw trails first (behind the main points)
    draw_fourier_trails(vis, cr);
    
    // Draw main frequency points
    draw_fourier_points(vis, cr);
    
    // Draw mathematical overlay if enabled
    if (vis->show_fourier_math) {
        draw_fourier_math_overlay(vis, cr);
    }
}

void draw_fourier_background(Visualizer *vis, cairo_t *cr, double center_x, double center_y) {
    // Draw coordinate system
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 0.5);
    cairo_set_line_width(cr, 1.0);
    
    if (vis->fourier_display_mode == 0) { // Circular mode
        // Draw unit circles for reference
        for (int i = 1; i <= 3; i++) {
            double radius = fmin(vis->width, vis->height) * 0.1 * i;
            cairo_arc(cr, center_x, center_y, radius, 0, 2 * M_PI);
            cairo_stroke(cr);
        }
        
        // Draw axes
        cairo_move_to(cr, 0, center_y);
        cairo_line_to(cr, vis->width, center_y);
        cairo_move_to(cr, center_x, 0);
        cairo_line_to(cr, center_x, vis->height);
        cairo_stroke(cr);
    }
    
    // Draw frequency grid for linear mode
    if (vis->fourier_display_mode == 1) {
        cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.7);
        for (int i = 1; i < 8; i++) {
            double x = vis->width * i / 8.0;
            cairo_move_to(cr, x, 0);
            cairo_line_to(cr, x, vis->height);
            cairo_stroke(cr);
        }
        for (int i = 1; i < 4; i++) {
            double y = vis->height * i / 4.0;
            cairo_move_to(cr, 0, y);
            cairo_line_to(cr, vis->width, y);
            cairo_stroke(cr);
        }
    }
}

void draw_fourier_trails(Visualizer *vis, cairo_t *cr) {
    cairo_set_line_width(cr, 1.0);
    
    for (int i = 0; i < FOURIER_POINTS; i++) {
        FourierBin *bin = &vis->fourier_bins[i];
        
        if (bin->intensity < 0.01) continue; // Skip quiet frequencies
        
        // Convert hue to RGB
        double r, g, b;
        hsv_to_rgb(bin->hue, 0.8, bin->intensity, &r, &g, &b);
        
        // Draw trail
        for (int j = 1; j < FOURIER_HISTORY; j++) {
            int idx1 = (bin->trail_index + j - 1 + FOURIER_HISTORY) % FOURIER_HISTORY;
            int idx2 = (bin->trail_index + j + FOURIER_HISTORY) % FOURIER_HISTORY;
            
            double fade = (double)j / FOURIER_HISTORY;
            fade = pow(fade, 2.0); // Exponential fade
            
            double alpha = (1.0 - fade) * bin->intensity * 0.6;
            if (alpha < 0.01) continue;
            
            cairo_set_source_rgba(cr, r, g, b, alpha);
            cairo_move_to(cr, bin->trail_x[idx1], bin->trail_y[idx1]);
            cairo_line_to(cr, bin->trail_x[idx2], bin->trail_y[idx2]);
            cairo_stroke(cr);
        }
    }
}

void draw_fourier_points(Visualizer *vis, cairo_t *cr) {
    for (int i = 0; i < FOURIER_POINTS; i++) {
        FourierBin *bin = &vis->fourier_bins[i];
        
        if (bin->intensity < 0.01) continue;
        
        // Convert hue to RGB
        double r, g, b;
        hsv_to_rgb(bin->hue, 1.0, 1.0, &r, &g, &b);
        
        // Draw main point with glow effect
        double point_size = 3.0 + bin->intensity * 8.0;
        
        // Outer glow
        cairo_set_source_rgba(cr, r, g, b, 0.3);
        cairo_arc(cr, bin->x, bin->y, point_size * 1.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Inner bright point
        cairo_set_source_rgba(cr, r, g, b, bin->intensity);
        cairo_arc(cr, bin->x, bin->y, point_size, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Show phase as small line from center
        if (vis->fourier_display_mode == 0 && bin->intensity > 0.3) {
            cairo_set_line_width(cr, 2.0);
            cairo_set_source_rgba(cr, r, g, b, 0.7);
            double phase_x = bin->x + cos(bin->phase) * point_size * 2;
            double phase_y = bin->y + sin(bin->phase) * point_size * 2;
            cairo_move_to(cr, bin->x, bin->y);
            cairo_line_to(cr, phase_x, phase_y);
            cairo_stroke(cr);
        }
    }
}

void draw_fourier_math_overlay(Visualizer *vis, cairo_t *cr) {
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    
    // Show DFT formula
    cairo_move_to(cr, 10, 20);
    cairo_show_text(cr, "DFT: X[k] = Σ x[n] * e^(-j2πkn/N)");
    
    // Show some frequency bin values
    char buffer[256];
    for (int i = 0; i < 8 && i < FOURIER_POINTS; i++) {
        FourierBin *bin = &vis->fourier_bins[i];
        snprintf(buffer, sizeof(buffer), "f[%2d]: %.3f ∠%.1f°", 
                i, bin->magnitude, bin->phase * 180.0 / M_PI);
        cairo_move_to(cr, 10, 40 + i * 15);
        cairo_show_text(cr, buffer);
    }
    
    // Show current mode (no key binding instructions)
    const char* mode_names[] = {"Complex Plane", "Spectrum", "3D Surface"};
    snprintf(buffer, sizeof(buffer), "Mode: %s", 
             mode_names[vis->fourier_display_mode]);
    cairo_move_to(cr, vis->width - 200, vis->height - 20);
    cairo_show_text(cr, buffer);
}

// Automatic mode cycling based on time - no key bindings needed
void update_fourier_mode(Visualizer *vis, double dt) {
    // Auto-cycle through display modes every 15 seconds for variety
    static double mode_timer = 0.0;
    mode_timer += dt;
    
    if (mode_timer > 15.0) {
        vis->fourier_display_mode = (vis->fourier_display_mode + 1) % 3;
        mode_timer = 0.0;
    }
    
    // Auto-adjust zoom based on audio intensity
    double target_zoom = 0.8 + vis->volume_level * 0.4;
    vis->fourier_zoom += (target_zoom - vis->fourier_zoom) * dt * 2.0;
}

/*void hsv_to_rgb(double h, double s, double v, double *r, double *g, double *b) {
    double c = v * s;
    double x = c * (1 - fabs(fmod(h * 6, 2) - 1));
    double m = v - c;
    
    if (h >= 0 && h < 1.0/6.0) {
        *r = c; *g = x; *b = 0;
    } else if (h >= 1.0/6.0 && h < 2.0/6.0) {
        *r = x; *g = c; *b = 0;
    } else if (h >= 2.0/6.0 && h < 3.0/6.0) {
        *r = 0; *g = c; *b = x;
    } else if (h >= 3.0/6.0 && h < 4.0/6.0) {
        *r = 0; *g = x; *b = c;
    } else if (h >= 4.0/6.0 && h < 5.0/6.0) {
        *r = x; *g = 0; *b = c;
    } else {
        *r = c; *g = 0; *b = x;
    }
    
    *r += m;
    *g += m;
    *b += m;
}*/
