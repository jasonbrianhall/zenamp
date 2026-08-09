#include "mandelbrot.h"
#include "visualization.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

// Global mutex for thread-safe access
static pthread_mutex_t mandelbrot_mutex = PTHREAD_MUTEX_INITIALIZER;

// Thread worker data
typedef struct {
    MandelbrotState *mb;
    int width;
    int height;
    int start_y;
    int end_y;
    double center_x;
    double center_y;
    double zoom;
    int max_iterations;
} MandelbrotWorkerData;

// Worker thread function
void* mandelbrot_worker_thread(void *arg) {
    MandelbrotWorkerData *data = (MandelbrotWorkerData *)arg;
    MandelbrotState *mb = data->mb;
    
    // Calculate iterations for assigned rows
    for (int y = data->start_y; y < data->end_y; y++) {
        for (int x = 0; x < data->width; x++) {
            double norm_x = (double)x / data->width;
            double norm_y = (double)y / data->height;
            
            int adjusted_iterations = data->max_iterations;
            if (data->width >= 1920) {
                adjusted_iterations = (int)(data->max_iterations * 0.7);
            } else if (data->width >= 1024) {
                adjusted_iterations = (int)(data->max_iterations * 0.9);
            }
            
            int iterations = mandelbrot_calculate_iterations(
                norm_x, norm_y, data->center_x, data->center_y, data->zoom, adjusted_iterations, data->width, data->height
            );
            
            mb->iteration_data[y * data->width + x] = iterations;
        }
    }
    
    free(data);
    return NULL;
}
// Note: this function now takes width/height to properly scale the viewport
int mandelbrot_calculate_iterations(double real, double imag, double center_x, double center_y, 
                                   double zoom, int max_iterations, int width, int height) {
    // Transform pixel coordinates to complex plane
    // real and imag are in [0, 1] range (normalized pixel coords)
    // zoom determines the scale of the view (larger zoom = more zoomed in)
    
    // Base viewport is approximately [-2.5, 1] for real (3.5 units) and [-1.25, 1.25] for imaginary (2.5 units)
    // But we need to maintain aspect ratio based on actual window dimensions
    double aspect_ratio = (double)width / (double)height;
    
    // Height is fixed at 2.5 units, width scales with aspect ratio
    double viewport_height = 2.5 / zoom;
    double viewport_width = viewport_height * aspect_ratio;
    
    double c_real = center_x + (real - 0.5) * viewport_width;
    double c_imag = center_y + (imag - 0.5) * viewport_height;
    
    double z_real = 0.0;
    double z_imag = 0.0;
    
    for (int n = 0; n < max_iterations; n++) {
        // Check if magnitude exceeds 2
        double magnitude_sq = z_real * z_real + z_imag * z_imag;
        if (magnitude_sq > 4.0) {
            return n;
        }
        
        // Iterate: z = z^2 + c
        double z_real_new = z_real * z_real - z_imag * z_imag + c_real;
        double z_imag_new = 2.0 * z_real * z_imag + c_imag;
        
        z_real = z_real_new;
        z_imag = z_imag_new;
    }
    
    return max_iterations;
}

// Convert iteration count to RGB color using HSV color space
void mandelbrot_get_color(int iterations, int max_iterations, double hue_offset, 
                         double *r, double *g, double *b) {
    if (iterations == max_iterations) {
        // Points in the set are black
        *r = 0.0;
        *g = 0.0;
        *b = 0.0;
        return;
    }
    
    // Smooth coloring using the iteration count
    double smooth_iter = iterations + 1.0 - log(log(2.0)) / log(2.0);
    double hue = fmod((smooth_iter / max_iterations + hue_offset) * 6.0, 6.0);
    double saturation = 1.0;
    double value = 0.95;
    
    // Convert HSV to RGB
    int h_int = (int)hue;
    double f = hue - h_int;
    double p = value * (1.0 - saturation);
    double q = value * (1.0 - saturation * f);
    double t = value * (1.0 - saturation * (1.0 - f));
    
    switch (h_int % 6) {
        case 0: *r = value; *g = t;     *b = p;     break;
        case 1: *r = q;     *g = value; *b = p;     break;
        case 2: *r = p;     *g = value; *b = t;     break;
        case 3: *r = p;     *g = q;     *b = value; break;
        case 4: *r = t;     *g = p;     *b = value; break;
        case 5: *r = value; *g = p;     *b = q;     break;
    }
}

// Recalculate the iteration data for the visible region
void mandelbrot_recalculate_region(void *vis_ptr, int width, int height) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    MandelbrotState *mb = &vis->mandelbrot;
    
    if (!mb || width <= 0 || height <= 0) return;
    if (width * height > MANDELBROT_DATA_SIZE) return;  // Safety check
    
    mb->data_width = width;
    mb->data_height = height;
    
    // Determine number of threads (use 4 threads for good balance)
    int num_threads = 4;
    pthread_t threads[num_threads];
    int rows_per_thread = height / num_threads;
    
    // Launch worker threads
    for (int t = 0; t < num_threads; t++) {
        MandelbrotWorkerData *data = (MandelbrotWorkerData *)malloc(sizeof(MandelbrotWorkerData));
        data->mb = mb;
        data->width = width;
        data->height = height;
        data->start_y = t * rows_per_thread;
        data->end_y = (t == num_threads - 1) ? height : (t + 1) * rows_per_thread;
        data->center_x = mb->center_x;
        data->center_y = mb->center_y;
        data->zoom = mb->zoom;
        data->max_iterations = mb->max_iterations;
        
        pthread_create(&threads[t], NULL, mandelbrot_worker_thread, data);
    }
    
    // Wait for all threads to complete
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }
    
    mb->needs_redraw = FALSE;
}

// Initialize the Mandelbrot visualization
void init_mandelbrot_system(void *vis_ptr) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    MandelbrotState *mb = &vis->mandelbrot;
    
    // Zero out the entire structure
    memset(mb, 0, sizeof(MandelbrotState));
    
    // Start centered on an interesting part of the Mandelbrot set
    mb->center_x = -0.75;
    mb->center_y = 0.1;
    mb->zoom = 1.0;
    mb->target_zoom = 1.0;
    mb->zoom_speed = 8.0;  // Much faster animation to target
    
    mb->max_iterations = 256;
    mb->data_width = -1;  // Force recalculation on first draw
    mb->data_height = -1;
    mb->needs_redraw = TRUE;
    
    mb->last_beat_time = 0.0;
    mb->beat_zoom_intensity = 1.5;  // Zoom in 1.5x on each beat
    mb->current_zoom_boost = 1.0;
    
    mb->hue_offset = 0.0;
    mb->color_cycle_speed = 0.1;  // Hue rotates this fast per second
    
    mb->drift_x = 0.0;
    mb->drift_y = 0.0;
    mb->drift_speed = 0.02;  // Subtle panning speed
}

// Detect beat for zoom trigger
gboolean mandelbrot_detect_beat(void *vis_ptr) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    
    // Simple beat detection: check if volume suddenly increased
    if (vis->volume_level > 0.3) {
        // Check if this is a new peak compared to recent history
        if (vis->volume_level > vis->last_peak_level * 1.3) {
            vis->last_peak_level = vis->volume_level;
            return TRUE;
        }
    }
    
    // Decay the peak detection
    vis->last_peak_level *= 0.95;
    
    return FALSE;
}

// Update animation and audio-reactive effects
void update_mandelbrot(void *vis_ptr, double dt) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    MandelbrotState *mb = &vis->mandelbrot;
    
    if (!mb) return;
    
    // Audio-reactive iteration depth - louder = more detail
    int base_iterations = 256;
    int audio_boost = (int)(vis->volume_level * 512);  // Volume controls detail
    mb->max_iterations = base_iterations + audio_boost;
    if (mb->max_iterations > 2048) mb->max_iterations = 2048;
    
    // Detect beats for color bursts and zoom boosts
    if (mandelbrot_detect_beat(vis)) {
        mb->hue_offset += 0.2;  // Color shift on beat
        if (mb->hue_offset > 1.0) mb->hue_offset -= 1.0;
        mb->target_zoom *= 1.5;  // Zoom boost on beat
    }
    
    // Smooth color cycling
    mb->hue_offset += mb->color_cycle_speed * dt * 0.2;
    if (mb->hue_offset > 1.0) mb->hue_offset -= 1.0;
    
    // Smooth zoom animation
    double zoom_diff = mb->target_zoom - mb->zoom;
    mb->zoom += zoom_diff * mb->zoom_speed * dt * 0.5;
    
    // Camera flight through famous fractal regions with continuous zoom
    // Cycle through beautiful spots with increasing zoom
    static double region_timer = 0;
    region_timer += dt;
    
    // Famous regions to explore (x, y, zoom_target)
    struct Region {
        double x, y, zoom;
    } regions[] = {
        {-0.75, 0.1, 1.0},         // Overview
        {-0.7469, 0.1104, 50.0},   // Seahorse Valley zoomed
        {-0.748767, 0.099, 500.0}, // Deep spiral
        {-0.16, 1.036, 100.0},     // Elephant valley
        {-0.7, 0.27015, 75.0},     // Dragon region
        {-0.75, 0.1, 1.0}          // Back to overview
    };
    int num_regions = 6;
    
    // Spend 12 seconds per region
    int current_region = (int)(region_timer / 12.0) % num_regions;
    int next_region = (current_region + 1) % num_regions;
    double blend = fmod(region_timer / 12.0, 1.0);  // 0 to 1 for blending
    
    // Smooth interpolation toward next region
    double target_x = regions[current_region].x + (regions[next_region].x - regions[current_region].x) * blend;
    double target_y = regions[current_region].y + (regions[next_region].y - regions[current_region].y) * blend;
    double target_zoom = regions[current_region].zoom + (regions[next_region].zoom - regions[current_region].zoom) * blend;
    
    // Gradually move toward target region
    mb->center_x += (target_x - mb->center_x) * 0.02;
    mb->center_y += (target_y - mb->center_y) * 0.02;
    
    // Set zoom target for the current region
    mb->target_zoom = target_zoom * (1.0 + vis->volume_level * 0.5);  // Volume also affects zoom
    
    // Add subtle sine wave oscillation for visual interest
    mb->center_x += sin(vis->time_offset * 0.3) * 0.0005;
    mb->center_y += cos(vis->time_offset * 0.2) * 0.0005;
    
    // Cap zoom to avoid numerical issues
    if (mb->zoom > 100000.0) {
        mb->zoom = 1.0;
        mb->target_zoom = 1.0;
    }
    
    vis->time_offset += dt;
}

// Render the Mandelbrot fractal
void draw_mandelbrot(void *vis_ptr, cairo_t *cr) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    MandelbrotState *mb = &vis->mandelbrot;
    
    if (!mb) return;
    
    int width = vis->width;
    int height = vis->height;
    
    // Recalculate only if zoom changed significantly or dimensions changed
    // Don't recalculate every frame - that's too expensive
    static double last_zoom = 1.0;
    static double last_center_x = -0.65;
    static double last_center_y = 0.0;
    static int last_iterations = 256;
    
    // Check if anything meaningful changed - use larger thresholds for stability
    gboolean zoom_changed = last_zoom > 0 && fabs(log(mb->zoom / last_zoom)) > 0.05;  // 5% zoom change
    gboolean pos_changed = fabs(mb->center_x - last_center_x) > 0.005 || 
                          fabs(mb->center_y - last_center_y) > 0.005;  // Stable position threshold
    gboolean iter_changed = mb->max_iterations != last_iterations;
    
    gboolean should_recalc = (zoom_changed || pos_changed || iter_changed || mb->data_width != width || mb->data_height != height);
    
    if (should_recalc) {
        mandelbrot_recalculate_region(vis, width, height);
        last_zoom = mb->zoom;
        last_center_x = mb->center_x;
        last_center_y = mb->center_y;
        last_iterations = mb->max_iterations;
    }
    
    // Clear background
    cairo_set_source_rgb(cr, vis->bg_r, vis->bg_g, vis->bg_b);
    cairo_paint(cr);
    
    // Create image surface for faster rendering
    cairo_surface_t *image_surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    unsigned char *image_data = cairo_image_surface_get_data(image_surface);
    int stride = cairo_image_surface_get_stride(image_surface);
    
    // Fill with Mandelbrot fractal colors
    for (int y = 0; y < height; y++) {
        uint32_t *row = (uint32_t *)(image_data + y * stride);
        
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            if (idx < 0 || idx >= MANDELBROT_DATA_SIZE) continue;  // Bounds check
            
            int iterations = (int)mb->iteration_data[idx];
            
            double r, g, b;
            mandelbrot_get_color(iterations, mb->max_iterations, mb->hue_offset, &r, &g, &b);
            
            // Convert to 8-bit RGB
            uint8_t r8 = (uint8_t)(r * 255);
            uint8_t g8 = (uint8_t)(g * 255);
            uint8_t b8 = (uint8_t)(b * 255);
            
            // Cairo uses ARGB format (little-endian on most systems)
            row[x] = (0xFF << 24) | (r8 << 16) | (g8 << 8) | b8;
        }
    }
    
    cairo_surface_mark_dirty(image_surface);
    
    // Draw the image surface
    cairo_set_source_surface(cr, image_surface, 0, 0);
    cairo_paint(cr);
    
    cairo_surface_destroy(image_surface);
    
    // Draw zoom level indicator at bottom
    char zoom_text[64];
    sprintf(zoom_text, "Zoom: %.1e | Center: (%.4f, %.4f)", mb->zoom, mb->center_x, mb->center_y);
    
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, 10, height - 10);
    cairo_show_text(cr, zoom_text);
}
