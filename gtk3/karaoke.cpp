#include "visualization.h"
#include "cdg.h"
#include "karafun.h"
#include <cairo.h>
#include <string.h>

static void apply_smooth_filter(unsigned char *data, int width, int height, int stride) {
    unsigned char *temp = (unsigned char*)malloc(height * stride);
    
    memcpy(temp, data, height * stride);
    for (int y = 0; y < height; y++) {
        for (int x = 1; x < width - 1; x++) {
            int offset = y * stride + x * 4;
            for (int c = 0; c < 3; c++) {
                int left = temp[offset - 4 + c];
                int center = temp[offset + c];
                int right = temp[offset + 4 + c];
                data[offset + c] = (left + center * 2 + right) / 4;
            }
        }
    }
    
    memcpy(temp, data, height * stride);
    for (int y = 1; y < height - 1; y++) {
        for (int x = 0; x < width; x++) {
            int offset = y * stride + x * 4;
            for (int c = 0; c < 3; c++) {
                int top = temp[offset - stride + c];
                int center = temp[offset + c];
                int bottom = temp[offset + stride + c];
                data[offset + c] = (top + center * 2 + bottom) / 4;
            }
        }
    }
    
    free(temp);
}

void draw_karaoke_visualization(Visualizer *vis, cairo_t *cr, double center_x, double center_y) {
    // Rotating energy beams
    int num_beams = 12;
    for (int i = 0; i < num_beams; i++) {
        double angle = (vis->time_offset * 0.4 + i * 2.0 * M_PI / num_beams);
        double length = 250.0 + vis->volume_level * 150.0;
        double width = 40.0;
        
        double x1 = center_x + cos(angle) * 40.0;
        double y1 = center_y + sin(angle) * 40.0;
        double x2 = center_x + cos(angle) * length;
        double y2 = center_y + sin(angle) * length;
        
        double hue = fmod(i * 30.0 + vis->time_offset * 25.0, 360.0);
        double r, g, b;
        hsv_to_rgb(hue, 0.85, 0.9, &r, &g, &b);
        
        cairo_pattern_t *grad = cairo_pattern_create_linear(x1, y1, x2, y2);
        cairo_pattern_add_color_stop_rgba(grad, 0, r, g, b, 0.6);
        cairo_pattern_add_color_stop_rgba(grad, 0.7, r, g, b, 0.3);
        cairo_pattern_add_color_stop_rgba(grad, 1, r, g, b, 0.0);
        
        cairo_set_line_width(cr, width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_source(cr, grad);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
        
        cairo_pattern_destroy(grad);
    }
    
    // Pulsing concentric circles
    for (int ring = 4; ring >= 0; ring--) {
        double radius = 60.0 + ring * 70.0 + vis->volume_level * 50.0;
        double pulse = sin(vis->time_offset * 0.6 + ring * 0.8) * 0.4 + 0.6;
        
        double hue = fmod(vis->time_offset * 18.0 + ring * 40.0, 360.0);
        double r, g, b;
        hsv_to_rgb(hue, 0.9, pulse, &r, &g, &b);
        
        cairo_set_source_rgba(cr, r, g, b, 0.25 * pulse);
        cairo_set_line_width(cr, 4.0);
        cairo_arc(cr, center_x, center_y, radius, 0, 2 * M_PI);
        cairo_stroke(cr);
    }
    
    // Audio-reactive spectrum burst
    double spectrum_radius = 100.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        double angle = (double)i / VIS_FREQUENCY_BARS * 2.0 * M_PI;
        double intensity = vis->frequency_bands[i];
        double length = spectrum_radius + intensity * 180.0;
        
        double x1 = center_x + cos(angle) * spectrum_radius;
        double y1 = center_y + sin(angle) * spectrum_radius;
        double x2 = center_x + cos(angle) * length;
        double y2 = center_y + sin(angle) * length;
        
        double hue = (double)i / VIS_FREQUENCY_BARS * 300.0;
        double r, g, b;
        hsv_to_rgb(hue, 1.0, 1.0, &r, &g, &b);
        
        cairo_pattern_t *grad = cairo_pattern_create_linear(x1, y1, x2, y2);
        cairo_pattern_add_color_stop_rgba(grad, 0, r, g, b, 0.5 * intensity);
        cairo_pattern_add_color_stop_rgba(grad, 1, r, g, b, 0.0);
        
        cairo_set_source(cr, grad);
        cairo_set_line_width(cr, 4.0);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
        
        cairo_pattern_destroy(grad);
    }
    
    // Corner accents
    double corner_size = 100.0;
    double corner_intensity = vis->volume_level * 0.7;
    double corner_hue = fmod(vis->time_offset * 30.0, 360.0);
    double r, g, b;
    hsv_to_rgb(corner_hue, 0.8, 0.9, &r, &g, &b);
    
    cairo_pattern_t *corner_grad = cairo_pattern_create_radial(0, 0, 0, 0, 0, corner_size);
    cairo_pattern_add_color_stop_rgba(corner_grad, 0, r, g, b, 0.4 * corner_intensity);
    cairo_pattern_add_color_stop_rgba(corner_grad, 1, r, g, b, 0.0);
    cairo_set_source(cr, corner_grad);
    cairo_rectangle(cr, 0, 0, corner_size, corner_size);
    cairo_fill(cr);
    cairo_pattern_destroy(corner_grad);
    
    corner_grad = cairo_pattern_create_radial(vis->width, 0, 0, vis->width, 0, corner_size);
    cairo_pattern_add_color_stop_rgba(corner_grad, 0, r, g, b, 0.4 * corner_intensity);
    cairo_pattern_add_color_stop_rgba(corner_grad, 1, r, g, b, 0.0);
    cairo_set_source(cr, corner_grad);
    cairo_rectangle(cr, vis->width - corner_size, 0, corner_size, corner_size);
    cairo_fill(cr);
    cairo_pattern_destroy(corner_grad);
    
    corner_grad = cairo_pattern_create_radial(0, vis->height, 0, 0, vis->height, corner_size);
    cairo_pattern_add_color_stop_rgba(corner_grad, 0, r, g, b, 0.4 * corner_intensity);
    cairo_pattern_add_color_stop_rgba(corner_grad, 1, r, g, b, 0.0);
    cairo_set_source(cr, corner_grad);
    cairo_rectangle(cr, 0, vis->height - corner_size, corner_size, corner_size);
    cairo_fill(cr);
    cairo_pattern_destroy(corner_grad);
    
    corner_grad = cairo_pattern_create_radial(vis->width, vis->height, 0, vis->width, vis->height, corner_size);
    cairo_pattern_add_color_stop_rgba(corner_grad, 0, r, g, b, 0.4 * corner_intensity);
    cairo_pattern_add_color_stop_rgba(corner_grad, 1, r, g, b, 0.0);
    cairo_set_source(cr, corner_grad);
    cairo_rectangle(cr, vis->width - corner_size, vis->height - corner_size, corner_size, corner_size);
    cairo_fill(cr);
    cairo_pattern_destroy(corner_grad);
}


void draw_karaoke_boring(Visualizer *vis, cairo_t *cr) {
    // Check if Karafun is active first
    KarafunState *kfn = karafun_get_state();
    if (kfn && kfn->active) {
        draw_karafun_lyrics(vis, cr);
        return;
    }
    
    // Check if we actually have valid CDG data loaded
    if (!vis->cdg_display || !vis->cdg_display->packets || vis->cdg_display->packet_count == 0) {
        // Draw "no CDG loaded" message
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
        cairo_paint(cr);
        
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 20);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, "No Karaoke File Loaded", &extents);
        cairo_move_to(cr, (vis->width - extents.width) / 2, vis->height / 2);
        cairo_show_text(cr, "No Karaoke File Loaded");
        return;
    }
    
    CDGDisplay *cdg = vis->cdg_display;
    
    // Create or update the Cairo surface for CDG graphics (2x size for better quality)
    int render_width = CDG_WIDTH * 2;
    int render_height = CDG_HEIGHT * 2;
    
    if (!vis->cdg_surface || 
        cairo_image_surface_get_width(vis->cdg_surface) != render_width ||
        cairo_image_surface_get_height(vis->cdg_surface) != render_height) {
        
        if (vis->cdg_surface) {
            cairo_surface_destroy(vis->cdg_surface);
        }
        vis->cdg_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, render_width, render_height);
        vis->cdg_last_packet = -1;
    }
    
    // Always update surface when packets have changed
    if (vis->cdg_last_packet != cdg->current_packet) {
        unsigned char *data = cairo_image_surface_get_data(vis->cdg_surface);
        int stride = cairo_image_surface_get_stride(vis->cdg_surface);
        
        // Convert CDG indexed color buffer to RGB surface with 2x upscaling
        for (int y = 0; y < CDG_HEIGHT; y++) {
            for (int x = 0; x < CDG_WIDTH; x++) {
                uint8_t color_index = cdg->screen[y][x];
                uint32_t rgb = cdg->palette[color_index];
                
                // Extract RGB components
                uint8_t r = (rgb >> 16) & 0xFF;
                uint8_t g = (rgb >> 8) & 0xFF;
                uint8_t b = rgb & 0xFF;
                
                // Write to 2x2 block for upscaling
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        int dest_y = y * 2 + dy;
                        int dest_x = x * 2 + dx;
                        unsigned char *row = data + dest_y * stride;
                        int pixel_offset = dest_x * 4;
                        
                        row[pixel_offset + 0] = b;   // Blue
                        row[pixel_offset + 1] = g;   // Green
                        row[pixel_offset + 2] = r;   // Red
                        row[pixel_offset + 3] = 255; // Alpha
                    }
                }
            }
        }
        
        // Apply smoothing filter for anti-aliasing
        apply_smooth_filter(data, render_width, render_height, stride);
        
        cairo_surface_mark_dirty(vis->cdg_surface);
        vis->cdg_last_packet = cdg->current_packet;
    }
    
    // Draw black background
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    
    // Calculate scaling to fit widget
    double scale_x = vis->width / (double)render_width;
    double scale_y = vis->height / (double)render_height;
    double scale = (scale_x < scale_y) ? scale_x : scale_y;
    
    // Center the display
    double offset_x = (vis->width - render_width * scale) / 2.0;
    double offset_y = (vis->height - render_height * scale) / 2.0;
    
    // Draw the CDG surface with high-quality filtering
    cairo_save(cr);
    cairo_translate(cr, offset_x, offset_y);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, vis->cdg_surface, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BEST);
    cairo_paint(cr);
    cairo_restore(cr);
}

void draw_karaoke_exciting(Visualizer *vis, cairo_t *cr) {
    // Check if Karafun is active first
    KarafunState *kfn = karafun_get_state();
    if (kfn && kfn->active) {
        draw_karafun_lyrics(vis, cr);
        return;
    }
    
    if (!vis->cdg_display || !vis->cdg_display->packets || vis->cdg_display->packet_count == 0) {
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
        cairo_paint(cr);
        
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 20);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, "No Karaoke File Loaded", &extents);
        cairo_move_to(cr, (vis->width - extents.width) / 2, vis->height / 2);
        cairo_show_text(cr, "No Karaoke File Loaded");
        return;
    }
    
    CDGDisplay *cdg = vis->cdg_display;
    
    int render_width = CDG_WIDTH * 2;
    int render_height = CDG_HEIGHT * 2;
    
    if (!vis->cdg_surface || 
        cairo_image_surface_get_width(vis->cdg_surface) != render_width ||
        cairo_image_surface_get_height(vis->cdg_surface) != render_height) {
        
        if (vis->cdg_surface) {
            cairo_surface_destroy(vis->cdg_surface);
        }
        vis->cdg_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, render_width, render_height);
        vis->cdg_last_packet = -1;
    }
    
    if (vis->cdg_last_packet != cdg->current_packet) {
        unsigned char *data = cairo_image_surface_get_data(vis->cdg_surface);
        int stride = cairo_image_surface_get_stride(vis->cdg_surface);
        
        for (int y = 0; y < CDG_HEIGHT; y++) {
            for (int x = 0; x < CDG_WIDTH; x++) {
                uint8_t color_index = cdg->screen[y][x];
                uint32_t rgb = cdg->palette[color_index];
                
                uint8_t r = (rgb >> 16) & 0xFF;
                uint8_t g = (rgb >> 8) & 0xFF;
                uint8_t b = rgb & 0xFF;
                
                // Calculate luminance to determine alpha
                double luminance = (0.299 * r + 0.587 * g + 0.114 * b);
                uint8_t alpha = (uint8_t)luminance;
                
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        int dest_y = y * 2 + dy;
                        int dest_x = x * 2 + dx;
                        unsigned char *row = data + dest_y * stride;
                        int pixel_offset = dest_x * 4;
                        
                        row[pixel_offset + 0] = b;
                        row[pixel_offset + 1] = g;
                        row[pixel_offset + 2] = r;
                        row[pixel_offset + 3] = alpha;
                    }
                }
            }
        }
        
        apply_smooth_filter(data, render_width, render_height, stride);
        
        cairo_surface_mark_dirty(vis->cdg_surface);
        vis->cdg_last_packet = cdg->current_packet;
    }
    
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    
    // Draw visualization FIRST (background layer)
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    draw_karaoke_visualization(vis, cr, center_x, center_y);
    
    // Calculate scaling for CDG
    double scale_x = vis->width / (double)render_width;
    double scale_y = vis->height / (double)render_height;
    double scale = (scale_x < scale_y) ? scale_x : scale_y;
    
    double offset_x = (vis->width - render_width * scale) / 2.0;
    double offset_y = (vis->height - render_height * scale) / 2.0;
    
    // Draw CDG with alpha transparency ON TOP of visualization
    cairo_save(cr);
    cairo_translate(cr, offset_x, offset_y);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, vis->cdg_surface, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BEST);
    cairo_paint(cr);
    cairo_restore(cr);
}
