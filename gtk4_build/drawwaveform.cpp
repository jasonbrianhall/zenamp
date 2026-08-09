#include "visualization.h"

void draw_waveform(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    cairo_set_source_rgba(cr, vis->fg_r, vis->fg_g, vis->fg_b, 0.8);
    cairo_set_line_width(cr, 2.0);
    
    cairo_move_to(cr, 0, vis->height / 2.0);
    
    for (int i = 0; i < VIS_SAMPLES; i++) {
        double x = (double)i * vis->width / (VIS_SAMPLES - 1);
        double y = vis->height / 2.0 + vis->audio_samples[i] * vis->height / 2.5;
        
        // Clamp y to screen bounds
        if (y < 0) y = 0;
        if (y > vis->height) y = vis->height;
        
        if (i == 0) {
            cairo_move_to(cr, x, y);
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    
    cairo_stroke(cr);
    
    // Add a secondary waveform with phase shift for visual interest
    cairo_set_source_rgba(cr, vis->accent_r, vis->accent_g, vis->accent_b, 0.4);
    cairo_set_line_width(cr, 1.0);
    
    cairo_move_to(cr, 0, vis->height / 2.0);
    
    for (int i = 0; i < VIS_SAMPLES; i++) {
        double x = (double)i * vis->width / (VIS_SAMPLES - 1);
        double phase_shifted = i < VIS_SAMPLES - 10 ? vis->audio_samples[i + 10] : 0.0;
        double y = vis->height / 2.0 + phase_shifted * vis->height / 3.0;
        
        // Clamp y to screen bounds
        if (y < 0) y = 0;
        if (y > vis->height) y = vis->height;
        
        if (i == 0) {
            cairo_move_to(cr, x, y);
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    
    cairo_stroke(cr);
}
