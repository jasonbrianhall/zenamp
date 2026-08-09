#include "visualization.h"

void draw_bars(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double bar_width = (double)vis->width / VIS_FREQUENCY_BARS;
    
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        double height = vis->frequency_bands[i] * vis->height * 0.9;
        double x = i * bar_width;
        double y = vis->height - height;
        
        // Color gradient based on frequency band
        double hue = (double)i / VIS_FREQUENCY_BARS;
        double r = vis->fg_r + hue * (vis->accent_r - vis->fg_r);
        double g = vis->fg_g + hue * (vis->accent_g - vis->fg_g);
        double b = vis->fg_b + hue * (vis->accent_b - vis->fg_b);
        
        // Create gradient
        cairo_pattern_t *gradient = cairo_pattern_create_linear(0, vis->height, 0, y);
        cairo_pattern_add_color_stop_rgba(gradient, 0, r, g, b, 0.3);
        cairo_pattern_add_color_stop_rgba(gradient, 1, r, g, b, 1.0);
        
        cairo_set_source(cr, gradient);
        cairo_rectangle(cr, x + 1, y, bar_width - 2, height);
        cairo_fill(cr);
        
        cairo_pattern_destroy(gradient);
        
        // Draw peak
        if (vis->peak_data[i] > 0.01) {
            double peak_y = vis->height - (vis->peak_data[i] * vis->height * 0.9);
            cairo_set_source_rgba(cr, vis->accent_r, vis->accent_g, vis->accent_b, 1.0);
            cairo_rectangle(cr, x + 1, peak_y - 2, bar_width - 2, 2);
            cairo_fill(cr);
        }
    }
}
