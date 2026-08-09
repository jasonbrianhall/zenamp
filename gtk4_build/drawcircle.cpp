#include "visualization.h"

void draw_circle(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    double radius = fmin(center_x, center_y) * 0.8;
    
    // Draw circle background
    cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.5);
    cairo_set_line_width(cr, 2.0);
    cairo_arc(cr, center_x, center_y, radius * 0.3, 0, 2 * M_PI);
    cairo_stroke(cr);
    
    // Draw frequency bars in circle
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        double angle = (double)i / VIS_FREQUENCY_BARS * 2.0 * M_PI + vis->rotation;
        double magnitude = vis->frequency_bands[i];
        
        double inner_radius = radius * 0.3;
        double outer_radius = inner_radius + magnitude * radius * 0.7;
        
        double inner_x = center_x + cos(angle) * inner_radius;
        double inner_y = center_y + sin(angle) * inner_radius;
        double outer_x = center_x + cos(angle) * outer_radius;
        double outer_y = center_y + sin(angle) * outer_radius;
        
        // Color based on magnitude and position
        double intensity = magnitude;
        double hue = (double)i / VIS_FREQUENCY_BARS;
        
        cairo_set_source_rgba(cr, 
                             vis->fg_r + intensity * hue * (vis->accent_r - vis->fg_r),
                             vis->fg_g + intensity * (vis->accent_g - vis->fg_g),
                             vis->fg_b + intensity * (1.0 - hue) * (vis->accent_b - vis->fg_b),
                             0.8);
        
        cairo_set_line_width(cr, 4.0);
        cairo_move_to(cr, inner_x, inner_y);
        cairo_line_to(cr, outer_x, outer_y);
        cairo_stroke(cr);
    }
    
    // Draw center volume indicator
    double vol_radius = vis->volume_level * radius * 0.2;
    if (vol_radius > 2.0) {
        cairo_set_source_rgba(cr, vis->accent_r, vis->accent_g, vis->accent_b, 0.7);
        cairo_arc(cr, center_x, center_y, vol_radius, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}
