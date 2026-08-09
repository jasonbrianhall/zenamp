#include "visualization.h"
#include <math.h>

// Static variables for animation state
static double time_offset = 0;
static double tunnel_depth = 0;
static double energy_streams[20][4] = {0}; // angle, distance, speed, color
static double light_streaks[50][5] = {0}; // x, y, z, speed, brightness
static double vortex_rotation = 0;
static double wormhole_pulse = 0;

void update_stargate(Visualizer *vis, double dt) {
    const double min_dt = 1.0 / 120.0;
    double speed_factor = dt / 0.033;
    
    if (dt < min_dt) {
        dt = min_dt;
        speed_factor = dt / 0.033;
    }
    
    time_offset += 0.1 * speed_factor;
    tunnel_depth += 5.0 * speed_factor;
    vortex_rotation += 0.03 * speed_factor;
    wormhole_pulse = sin(time_offset * 2) * 0.5 + 0.5;
    
    // Calculate average audio intensity
    double avg_intensity = 0;
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        avg_intensity += vis->frequency_bands[i];
    }
    avg_intensity /= VIS_FREQUENCY_BARS;
    
    // Update energy streams spiraling around tunnel
    for (int i = 0; i < 20; i++) {
        if (energy_streams[i][1] <= 0) {
            // Initialize new stream
            energy_streams[i][0] = (rand() % 360) * M_PI / 180.0; // angle
            energy_streams[i][1] = 1000.0; // distance
            energy_streams[i][2] = 5.0 + (rand() % 10); // speed
            energy_streams[i][3] = rand() % 3; // color type
        }
        
        // Move stream toward viewer
        energy_streams[i][1] -= energy_streams[i][2] * speed_factor * (1.0 + avg_intensity);
        energy_streams[i][0] += 0.02 * speed_factor; // spiral rotation
    }
    
    // Update light streaks flying past
    for (int i = 0; i < 50; i++) {
        if (light_streaks[i][2] <= -100) {
            // Create new streak
            light_streaks[i][0] = (rand() % 200 - 100) / 100.0; // x position (-1 to 1)
            light_streaks[i][1] = (rand() % 200 - 100) / 100.0; // y position (-1 to 1)
            light_streaks[i][2] = 800.0; // z depth
            light_streaks[i][3] = 10.0 + (rand() % 20); // speed
            light_streaks[i][4] = 0.5 + (rand() % 50) / 100.0; // brightness
        }
        
        // Move toward viewer
        light_streaks[i][2] -= light_streaks[i][3] * speed_factor * (1.0 + avg_intensity * 0.5);
    }
}

void draw_stargate(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    double max_radius = fmin(vis->width, vis->height) * 0.6;
    
    // Calculate average audio intensity for effects
    double avg_intensity = 0;
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        avg_intensity += vis->frequency_bands[i];
    }
    avg_intensity /= VIS_FREQUENCY_BARS;
    
    // Deep space background
    cairo_set_source_rgb(cr, 0.01, 0.01, 0.05);
    cairo_paint(cr);
    
    // Draw tunnel rings receding into distance
    for (int ring = 50; ring >= 0; ring--) {
        double z = ring * 20.0 - fmod(tunnel_depth, 20.0);
        if (z < 0) continue;
        
        double scale = 100.0 / (z + 100.0);
        double ring_radius = max_radius * scale;
        
        if (ring_radius < 5) continue;
        
        // Color shifts based on depth and audio
        double depth_color = 1.0 - (z / 1000.0);
        double r = 0.2 + depth_color * 0.3 + avg_intensity * 0.3;
        double g = 0.3 + depth_color * 0.5 + wormhole_pulse * 0.3;
        double b = 0.6 + depth_color * 0.4;
        double alpha = fmin(1.0, scale * 2.0);
        
        // Draw ring
        cairo_set_source_rgba(cr, r, g, b, alpha * 0.6);
        cairo_set_line_width(cr, fmax(2, 8 * scale));
        cairo_arc(cr, center_x, center_y, ring_radius, 0, 2 * M_PI);
        cairo_stroke(cr);
        
        // Add glow
        cairo_set_source_rgba(cr, r * 1.5, g * 1.5, b * 1.5, alpha * 0.3);
        cairo_set_line_width(cr, fmax(3, 12 * scale));
        cairo_arc(cr, center_x, center_y, ring_radius, 0, 2 * M_PI);
        cairo_stroke(cr);
    }
    
    // Draw energy streams spiraling through the tunnel
    for (int i = 0; i < 20; i++) {
        double angle = energy_streams[i][0] + vortex_rotation;
        double z = energy_streams[i][1];
        int color_type = (int)energy_streams[i][3];
        
        if (z <= 0 || z > 1000) continue;
        
        double scale = 100.0 / (z + 100.0);
        double radius = max_radius * scale * 0.7;
        
        double stream_x = center_x + cos(angle) * radius;
        double stream_y = center_y + sin(angle) * radius;
        
        // Different colors for energy streams
        double sr, sg, sb;
        switch(color_type) {
            case 0: sr=0.3; sg=0.6; sb=1.0; break; // Blue
            case 1: sr=0.2; sg=1.0; sb=0.8; break; // Cyan
            default: sr=0.5; sg=0.4; sb=1.0; break; // Purple
        }
        
        double alpha = fmin(1.0, scale * 3.0);
        double size = fmax(2, 12 * scale);
        
        // Draw energy ball
        cairo_pattern_t *glow = cairo_pattern_create_radial(
            stream_x, stream_y, 0,
            stream_x, stream_y, size
        );
        cairo_pattern_add_color_stop_rgba(glow, 0, sr, sg, sb, alpha);
        cairo_pattern_add_color_stop_rgba(glow, 0.5, sr * 0.8, sg * 0.8, sb * 0.8, alpha * 0.5);
        cairo_pattern_add_color_stop_rgba(glow, 1, sr * 0.3, sg * 0.3, sb * 0.3, 0);
        
        cairo_set_source(cr, glow);
        cairo_arc(cr, stream_x, stream_y, size, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(glow);
    }
    
    // Draw light streaks flying past
    for (int i = 0; i < 50; i++) {
        double x_norm = light_streaks[i][0];
        double y_norm = light_streaks[i][1];
        double z = light_streaks[i][2];
        double brightness = light_streaks[i][4];
        
        if (z <= -100 || z > 800) continue;
        
        double scale = 100.0 / (z + 100.0);
        double screen_x = center_x + x_norm * max_radius * scale * 1.2;
        double screen_y = center_y + y_norm * max_radius * scale * 1.2;
        
        // Calculate streak length based on speed
        double prev_z = z + 20;
        double prev_scale = 100.0 / (prev_z + 100.0);
        double prev_x = center_x + x_norm * max_radius * prev_scale * 1.2;
        double prev_y = center_y + y_norm * max_radius * prev_scale * 1.2;
        
        double alpha = fmin(1.0, scale * 2.0) * brightness;
        
        cairo_set_source_rgba(cr, 0.8, 0.9, 1.0, alpha);
        cairo_set_line_width(cr, fmax(1, 3 * scale));
        cairo_move_to(cr, prev_x, prev_y);
        cairo_line_to(cr, screen_x, screen_y);
        cairo_stroke(cr);
        
        // Bright point at the front
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha * 0.8);
        cairo_arc(cr, screen_x, screen_y, fmax(1, 2 * scale), 0, 2 * M_PI);
        cairo_fill(cr);
    }
    
    // Draw frequency bars as energy waves on the tunnel walls
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        double angle = (i / (double)VIS_FREQUENCY_BARS) * 2 * M_PI + vortex_rotation;
        double intensity = vis->frequency_bands[i];
        
        // Create wave effect on tunnel wall
        for (int depth = 0; depth < 5; depth++) {
            double z = depth * 150.0;
            double scale = 100.0 / (z + 100.0);
            double radius = max_radius * scale * (0.95 + intensity * 0.15);
            
            double x = center_x + cos(angle) * radius;
            double y = center_y + sin(angle) * radius;
            
            // Color based on frequency
            double hue = (double)i / VIS_FREQUENCY_BARS;
            double r = 0.3 + 0.7 * sin(hue * 2 * M_PI);
            double g = 0.4 + 0.6 * sin(hue * 2 * M_PI + 2.09);
            double b = 0.6 + 0.4 * sin(hue * 2 * M_PI + 4.18);
            double alpha = scale * intensity;
            
            cairo_set_source_rgba(cr, r, g, b, alpha);
            cairo_arc(cr, x, y, fmax(2, 8 * scale * intensity), 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
    
    // Add vortex swirl effect
    for (int swirl = 0; swirl < 3; swirl++) {
        double swirl_offset = swirl * 2.0 * M_PI / 3.0;
        
        for (int segment = 0; segment < 30; segment++) {
            double z = segment * 30.0 - fmod(tunnel_depth * 0.5, 30.0);
            if (z < 0 || z > 900) continue;
            
            double scale = 100.0 / (z + 100.0);
            double angle = vortex_rotation * 3 + swirl_offset + segment * 0.3;
            double radius = max_radius * scale * 0.4;
            
            double x = center_x + cos(angle) * radius;
            double y = center_y + sin(angle) * radius;
            
            double swirl_brightness = 0.5 + wormhole_pulse * 0.5;
            double alpha = scale * swirl_brightness * 0.6;
            
            cairo_set_source_rgba(cr, 0.4, 0.7, 1.0, alpha);
            cairo_arc(cr, x, y, fmax(1, 6 * scale), 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
    
    // Add center glow (the destination getting closer)
    cairo_pattern_t *center_glow = cairo_pattern_create_radial(
        center_x, center_y, 0,
        center_x, center_y, max_radius * 0.3
    );
    double glow_intensity = 0.3 + avg_intensity * 0.4 + wormhole_pulse * 0.3;
    cairo_pattern_add_color_stop_rgba(center_glow, 0, 0.6, 0.8, 1.0, glow_intensity);
    cairo_pattern_add_color_stop_rgba(center_glow, 0.5, 0.3, 0.5, 0.9, glow_intensity * 0.4);
    cairo_pattern_add_color_stop_rgba(center_glow, 1, 0.1, 0.2, 0.4, 0);
    
    cairo_set_source(cr, center_glow);
    cairo_arc(cr, center_x, center_y, max_radius * 0.3, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(center_glow);
}
