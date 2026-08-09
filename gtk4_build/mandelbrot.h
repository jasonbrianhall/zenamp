#ifndef MANDELBROT_H
#define MANDELBROT_H

#include <cairo.h>
#include <math.h>
#include <glib.h>
#include <stdint.h>

#define MANDELBROT_DATA_SIZE (1920 * 1440)  // Max resolution for cached iteration data

typedef struct {
    // Fractal state
    double center_x;
    double center_y;
    double zoom;
    double target_zoom;
    double zoom_speed;
    
    // Rendering
    int max_iterations;
    double iteration_data[MANDELBROT_DATA_SIZE];  // Static buffer for iteration counts
    int data_width;
    int data_height;
    gboolean needs_redraw;
    
    // Beat tracking
    double last_beat_time;
    double beat_zoom_intensity;  // How much to zoom on beat
    double current_zoom_boost;   // Current zoom boost from beat
    
    // Color cycling
    double hue_offset;
    double color_cycle_speed;
    
    // Animation
    double drift_x;
    double drift_y;
    double drift_speed;
    
} MandelbrotState;

#endif
