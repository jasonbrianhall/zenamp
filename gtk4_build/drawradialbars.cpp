#include "visualization.h"
#include <math.h>

static RadialBarsSystem radial_bars_sys;
static gboolean radial_bars_initialized = FALSE;

// Local HSV to RGB conversion for this visualization
static void radial_hsv_to_rgb(double h, double s, double v, double *r, double *g, double *b) {
    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h * 6.0, 2.0) - 1.0));
    double m = v - c;
    
    if (h < 1.0/6.0) {
        *r = c; *g = x; *b = 0.0;
    } else if (h < 2.0/6.0) {
        *r = x; *g = c; *b = 0.0;
    } else if (h < 3.0/6.0) {
        *r = 0.0; *g = c; *b = x;
    } else if (h < 4.0/6.0) {
        *r = 0.0; *g = x; *b = c;
    } else if (h < 5.0/6.0) {
        *r = x; *g = 0.0; *b = c;
    } else {
        *r = c; *g = 0.0; *b = x;
    }
    
    *r += m;
    *g += m;
    *b += m;
}

static void init_radial_bars_bouncing_system(Visualizer *vis) {
    if (radial_bars_initialized) return;
    
    radial_bars_sys.center_x = vis->width / 2.0;
    radial_bars_sys.center_y = vis->height / 2.0;
    radial_bars_sys.inner_radius = 40.0;
    radial_bars_sys.outer_radius = 150.0;
    radial_bars_sys.ball_count = 0;
    radial_bars_sys.spawn_timer = 0.0;
    
    radial_bars_initialized = TRUE;
}

static void update_radial_bars_bouncing(Visualizer *vis, double dt) {
    if (!radial_bars_initialized) init_radial_bars_bouncing_system(vis);
    radial_bars_sys.spawn_timer += dt;
}

static void draw_radial_bars_bouncing(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    if (!radial_bars_initialized) init_radial_bars_bouncing_system(vis);
    
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    
    // Scale radii based on screen size
    double screen_size = fmin(vis->width, vis->height);
    double inner_radius = screen_size * 0.1;
    double outer_radius = screen_size * 0.4;
    
    // Draw background
    cairo_set_source_rgba(cr, vis->bg_r, vis->bg_g, vis->bg_b, 1.0);
    cairo_paint(cr);
    
    // Calculate rotation based on music
    double total_volume = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        total_volume += vis->frequency_bands[i];
    }
    //double rotation = vis->rotation * 0.1 + total_volume * 0.5;
    double rotation = vis->rotation;
    
    // Draw bars radiating from center
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        double angle = (2.0 * M_PI * i) / VIS_FREQUENCY_BARS + rotation;
        double height = vis->frequency_bands[i] * (outer_radius - inner_radius);
        
        double x1 = center_x + cos(angle) * inner_radius;
        double y1 = center_y + sin(angle) * inner_radius;
        double x2 = center_x + cos(angle) * (inner_radius + height);
        double y2 = center_y + sin(angle) * (inner_radius + height);
        
        // Rainbow colors using local HSV to RGB
        double hue = (double)i / VIS_FREQUENCY_BARS;
        double r, g, b;
        radial_hsv_to_rgb(hue, 1.0, 1.0, &r, &g, &b);
        
        cairo_set_source_rgba(cr, r, g, b, 0.9);
        cairo_set_line_width(cr, 4.0);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }
    
    // Draw center circle
    cairo_set_source_rgba(cr, vis->accent_r, vis->accent_g, vis->accent_b, 0.3);
    cairo_arc(cr, center_x, center_y, inner_radius, 0, 2 * M_PI);
    cairo_stroke(cr);
    
    // Draw center volume indicator circle
    double vol_radius = vis->volume_level * inner_radius * 0.8;
    if (vol_radius > 2.0) {
        cairo_set_source_rgba(cr, vis->accent_r, vis->accent_g, vis->accent_b, 0.7);
        cairo_arc(cr, center_x, center_y, vol_radius, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

static void cleanup_radial_bars_bouncing(void) {
    radial_bars_initialized = FALSE;
}
