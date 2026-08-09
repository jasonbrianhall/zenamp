#include "visualization.h"

void draw_volume_meter(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    // Draw VU meter style visualization
    double meter_width = vis->width * 0.8;
    double meter_height = vis->height * 0.6;
    double meter_x = (vis->width - meter_width) / 2;
    double meter_y = (vis->height - meter_height) / 2;
    
    // Background
    cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.8);
    cairo_rectangle(cr, meter_x, meter_y, meter_width, meter_height);
    cairo_fill(cr);
    
    // Draw level bars
    int num_bars = 20;
    double bar_width = meter_width / num_bars;
    double level = vis->volume_level;
    
    for (int i = 0; i < num_bars; i++) {
        double bar_level = (double)i / num_bars;
        double x = meter_x + i * bar_width;
        
        if (level > bar_level) {
            // Color coding: green -> yellow -> red
            double r, g, b;
            if (bar_level < 0.7) {
                r = 0.0; g = 1.0; b = 0.0; // Green
            } else if (bar_level < 0.9) {
                r = 1.0; g = 1.0; b = 0.0; // Yellow
            } else {
                r = 1.0; g = 0.0; b = 0.0; // Red
            }
            
            cairo_set_source_rgba(cr, r, g, b, 0.9);
        } else {
            cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 0.5);
        }
        
        cairo_rectangle(cr, x + 1, meter_y + 5, bar_width - 2, meter_height - 10);
        cairo_fill(cr);
    }
    
    // Draw peak indicator
    if (level > 0.01) {
        double peak_x = meter_x + level * meter_width;
        cairo_set_source_rgba(cr, vis->accent_r, vis->accent_g, vis->accent_b, 1.0);
        cairo_rectangle(cr, peak_x - 2, meter_y, 4, meter_height);
        cairo_fill(cr);
    }
    
    // Draw frequency bars below
    double freq_y = meter_y + meter_height + 20;
    double freq_height = vis->height - freq_y - 10;
    if (freq_height > 0) {
        double freq_bar_width = meter_width / VIS_FREQUENCY_BARS;
        
        for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
            double height = vis->frequency_bands[i] * freq_height;
            double x = meter_x + i * freq_bar_width;
            double y = freq_y + freq_height - height;
            
            double hue = (double)i / VIS_FREQUENCY_BARS;
            cairo_set_source_rgba(cr, 
                                 vis->fg_r + hue * (vis->accent_r - vis->fg_r),
                                 vis->fg_g + hue * (vis->accent_g - vis->fg_g),
                                 vis->fg_b + hue * (vis->accent_b - vis->fg_b),
                                 0.7);
            
            cairo_rectangle(cr, x + 1, y, freq_bar_width - 2, height);
            cairo_fill(cr);
        }
    }
}
