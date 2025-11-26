#include "visualization.h"
#include <math.h>

/**
 * INTERACTIVE KALEIDOSCOPE - THREE BUTTON VERSION
 * 
 * Three unique interactive effects:
 * 
 * LEFT CLICK (Explode):
 *   - All shapes scale up dramatically
 *   - Increased spawn rate (more shapes)
 *   - Vibrant color boost
 *   - Effect: Explosive burst of color
 * 
 * MIDDLE CLICK (Reverse/Invert):
 *   - Mirror count changes (creates different symmetry)
 *   - Rotation direction reverses
 *   - Colors invert/shift
 *   - Effect: Kaleidoscopic twist
 * 
 * RIGHT CLICK (Freeze/Slow):
 *   - All shapes slow down dramatically
 *   - Rotation slows to crawl
 *   - Colors mellow and dim
 *   - Effect: Hypnotic slow motion
 * 
 * All effects persist and fade smoothly!
 */

// Effect state tracking
static double explode_intensity = 0.0;      // Left click
static double invert_intensity = 0.0;       // Middle click
static double freeze_intensity = 0.0;       // Right click
static double last_mouse_angle = 0.0;       // Track previous angle for delta rotation
static double manual_rotation_speed = 0.0;  // Current rotation speed from mouse control
static double mouse_idle_time = 0.0;        // How long mouse hasn't moved

#define NORMAL_SPAWN_RATE 0.15
#define EXPLODE_SPAWN_RATE 0.05
#define MOUSE_IDLE_THRESHOLD 2.0             // Seconds before starting to fade to auto-rotate
#define FADE_TO_AUTO_SPEED 3.0               // Seconds to fade from manual to auto

void kaleidoscope_hsv_to_rgb(double h, double s, double v, double *r, double *g, double *b) {
    if (s == 0.0) {
        *r = *g = *b = v;
        return;
    }
    
    h = h * 6.0;
    int i = (int)h;
    double f = h - i;
    double p = v * (1.0 - s);
    double q = v * (1.0 - s * f);
    double t = v * (1.0 - s * (1.0 - f));
    
    switch(i % 6) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        case 5: *r = v; *g = p; *b = q; break;
    }
}

void init_kaleidoscope_system(Visualizer *vis) {
    vis->kaleidoscope_shape_count = 0;
    vis->kaleidoscope_rotation = 0.0;
    vis->kaleidoscope_rotation_speed = 1.5;
    vis->kaleidoscope_zoom = 1.0;
    vis->kaleidoscope_zoom_target = 1.0;
    vis->kaleidoscope_spawn_timer = 0.0;
    vis->kaleidoscope_mirror_offset = 0.0;
    vis->kaleidoscope_mirror_count = 8;
    vis->kaleidoscope_color_shift = 0.0;
    vis->kaleidoscope_auto_shapes = TRUE;
    
    // Initialize effect states
    explode_intensity = 0.0;
    invert_intensity = 0.0;
    freeze_intensity = 0.0;
    
    for (int i = 0; i < MAX_KALEIDOSCOPE_SHAPES; i++) {
        vis->kaleidoscope_shapes[i].active = FALSE;
    }
    
    for (int i = 0; i < 12; i++) {
        int slot = i;
        KaleidoscopeShape *shape = &vis->kaleidoscope_shapes[slot];
        
        shape->x = 0.3 + (rand() / (double)RAND_MAX) * 0.4;
        shape->y = 0.3 + (rand() / (double)RAND_MAX) * 0.4;
        shape->vx = ((rand() / (double)RAND_MAX) - 0.5) * 0.08;
        shape->vy = ((rand() / (double)RAND_MAX) - 0.5) * 0.08;
        shape->rotation = (rand() / (double)RAND_MAX) * 2.0 * M_PI;
        shape->rotation_speed = ((rand() / (double)RAND_MAX) - 0.5) * 6.0;
        shape->scale = 0.2;
        shape->scale_speed = 2.5;
        shape->hue = rand() / (double)RAND_MAX;
        shape->saturation = 1.0;
        shape->base_brightness = 1.0;
        shape->brightness = 1.0;
        shape->shape_type = rand() % 7;
        shape->life = 3.0;
        shape->pulse_phase = (rand() / (double)RAND_MAX) * 2.0 * M_PI;
        shape->frequency_band = 0;
        shape->active = TRUE;
    }
    
    vis->kaleidoscope_shape_count = 12;
}

void spawn_kaleidoscope_shape(Visualizer *vis, double intensity, int frequency_band) {
    int slot = -1;
    for (int i = 0; i < MAX_KALEIDOSCOPE_SHAPES; i++) {
        if (!vis->kaleidoscope_shapes[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        slot = rand() % MAX_KALEIDOSCOPE_SHAPES;
    }
    
    KaleidoscopeShape *shape = &vis->kaleidoscope_shapes[slot];
    
    shape->x = 0.3 + (rand() / (double)RAND_MAX) * 0.4;
    shape->y = 0.3 + (rand() / (double)RAND_MAX) * 0.4;
    shape->vx = ((rand() / (double)RAND_MAX) - 0.5) * 0.08;
    shape->vy = ((rand() / (double)RAND_MAX) - 0.5) * 0.08;
    shape->rotation = (rand() / (double)RAND_MAX) * 2.0 * M_PI;
    shape->rotation_speed = ((rand() / (double)RAND_MAX) - 0.5) * 6.0;
    shape->scale = 0.15 + intensity * 0.3;
    shape->scale_speed = 2.5 + intensity * 2.5;
    shape->hue = rand() / (double)RAND_MAX;
    shape->saturation = 1.0;
    shape->base_brightness = 1.0;
    shape->brightness = 1.0;
    shape->shape_type = rand() % 7;
    shape->life = 3.0;
    shape->pulse_phase = (rand() / (double)RAND_MAX) * 2.0 * M_PI;
    shape->frequency_band = frequency_band;
    shape->active = TRUE;
    
    vis->kaleidoscope_shape_count++;
}

void update_kaleidoscope(Visualizer *vis, double dt) {
    // ========== LEFT CLICK - EXPLODE ==========
    if (vis->mouse_left_pressed) {
        explode_intensity = 1.0;
        // Spawn extra shapes on explode
        for (int i = 0; i < 3; i++) {
            spawn_kaleidoscope_shape(vis, 0.8, 0);
        }
        vis->mouse_left_pressed = FALSE;
    }
    
    if (explode_intensity > 0.0) {
        explode_intensity *= 0.96;  // Longer persistence (~1.2 seconds)
    }

    // ========== MIDDLE CLICK - INVERT/REVERSE ==========
    if (vis->mouse_middle_pressed) {
        invert_intensity = 1.0;
        vis->kaleidoscope_rotation_speed *= -1.0;  // Reverse direction
        vis->mouse_middle_pressed = FALSE;
    }
    
    if (invert_intensity > 0.0) {
        invert_intensity *= 0.95;  // Slower decay (~1.3 seconds)
    }

    // ========== RIGHT CLICK - FREEZE/SLOW ==========
    if (vis->mouse_right_pressed) {
        freeze_intensity = 1.0;
        vis->mouse_right_pressed = FALSE;
    }
    
    if (freeze_intensity > 0.0) {
        freeze_intensity *= 0.97;  // Longest persistence (~1.7 seconds)
    }

    // ========== CIRCULAR MOUSE-CONTROLLED ROTATION ==========
    // Mouse position relative to center determines rotation direction and speed
    
    double rotation_factor = 1.0;
    if (freeze_intensity > 0.1) {
        rotation_factor = 0.1 + (1.0 - freeze_intensity) * 0.9;  // Slow to normal
    }
    
    if (vis->mouse_over) {
        // Calculate mouse position relative to center
        double cx = vis->width / 2.0;
        double cy = vis->height / 2.0;
        double mouse_dx = vis->mouse_x - cx;
        double mouse_dy = vis->mouse_y - cy;
        
        // Calculate distance from center (normalized 0 to 1)
        double distance_to_center = sqrt(mouse_dx * mouse_dx + mouse_dy * mouse_dy);
        double max_distance = sqrt(cx * cx + cy * cy);  // Diagonal distance
        double normalized_distance = fmin(distance_to_center / max_distance, 1.0);
        
        // Calculate angle of mouse around center
        double mouse_angle = atan2(mouse_dy, mouse_dx);
        
        // Calculate angle delta (change in angle)
        double angle_delta = mouse_angle - last_mouse_angle;
        
        // Handle angle wrapping (-π to π)
        if (angle_delta > M_PI) {
            angle_delta -= 2.0 * M_PI;
        } else if (angle_delta < -M_PI) {
            angle_delta += 2.0 * M_PI;
        }
        
        // Always calculate rotation speed based on angle change and distance
        // This preserves direction even when mouse stops
        manual_rotation_speed = angle_delta / dt * normalized_distance;  // Speed in rad/s
        
        // Detect if mouse is moving (angle delta is significant)
        if (fabs(angle_delta) > 0.001) {  // Threshold for detecting movement
            mouse_idle_time = 0.0;  // Reset idle timer when mouse moves
        } else {
            // Mouse not moving, accumulate idle time
            mouse_idle_time += dt;
            
            // When idle, gradually increase velocity toward auto-rotation
            double auto_rotation_speed = 2.5;  // Normal auto-rotate speed
            
            // Start increasing velocity after idle threshold
            if (mouse_idle_time > MOUSE_IDLE_THRESHOLD) {
                // Gradually increase from current speed to auto speed
                double fade_time = mouse_idle_time - MOUSE_IDLE_THRESHOLD;
                double fade_factor = fmin(fade_time / FADE_TO_AUTO_SPEED, 1.0);
                
                // If we're moving slower than auto speed, accelerate
                if (fabs(manual_rotation_speed) < auto_rotation_speed) {
                    // Smoothly increase velocity
                    manual_rotation_speed += auto_rotation_speed * fade_factor;
                }
            }
        }
        
        // Apply rotation
        vis->kaleidoscope_rotation += manual_rotation_speed * dt * rotation_factor;
        
        // Store current angle for next frame
        last_mouse_angle = mouse_angle;
        
    } else {
        // Auto-rotate when mouse not over
        vis->kaleidoscope_rotation += 2.5 * dt * rotation_factor;
        // Reset tracking when mouse leaves
        last_mouse_angle = atan2(vis->height / 2.0 - vis->mouse_y, vis->width / 2.0 - vis->mouse_x);
        mouse_idle_time = 0.0;
    }
    
    if (vis->kaleidoscope_rotation > 2.0 * M_PI) {
        vis->kaleidoscope_rotation -= 2.0 * M_PI;
    } else if (vis->kaleidoscope_rotation < 0) {
        vis->kaleidoscope_rotation += 2.0 * M_PI;
    }
    
    // ========== ZOOM WITH EXPLODE EFFECT ==========
    double base_zoom = 0.8 + vis->volume_level * 0.5;
    double explode_zoom = 1.0 + explode_intensity * 0.4;  // Zoom out during explode
    vis->kaleidoscope_zoom = base_zoom * explode_zoom;
    
    // ========== SPAWN SHAPES WITH EFFECT MODULATION ==========
    double spawn_rate = NORMAL_SPAWN_RATE;
    if (explode_intensity > 0.1) {
        spawn_rate = EXPLODE_SPAWN_RATE;  // Faster spawning during explode
    }
    
    vis->kaleidoscope_spawn_timer += dt;
    if (vis->kaleidoscope_spawn_timer > spawn_rate) {
        double spawn_intensity = 0.6 + explode_intensity * 0.4;  // More intense during explode
        
        // Find the frequency band with highest energy
        int dominant_band = 0;
        double max_energy = 0.0;
        for (int b = 0; b < VIS_FREQUENCY_BARS; b++) {
            if (vis->frequency_bands[b] > max_energy) {
                max_energy = vis->frequency_bands[b];
                dominant_band = b;
            }
        }
        
        spawn_kaleidoscope_shape(vis, spawn_intensity, dominant_band);
        vis->kaleidoscope_spawn_timer = 0.0;
    }

    // ========== UPDATE SHAPES ==========
    int active = 0;
    for (int i = 0; i < MAX_KALEIDOSCOPE_SHAPES; i++) {
        KaleidoscopeShape *s = &vis->kaleidoscope_shapes[i];
        
        if (!s->active) continue;
        active++;
        
        // Position update with freeze effect
        double velocity_factor = 1.0;
        if (freeze_intensity > 0.1) {
            velocity_factor = 0.05 + (1.0 - freeze_intensity) * 0.95;  // Slow movement
        }
        
        s->x += s->vx * dt * velocity_factor;
        s->y += s->vy * dt * velocity_factor;
        
        if (s->x < 0.0) s->x = 1.0;
        if (s->x > 1.0) s->x = 0.0;
        if (s->y < 0.0) s->y = 1.0;
        if (s->y > 1.0) s->y = 0.0;
        
        // Rotation with freeze and explode effects
        double rotation_speed_factor = 1.0;
        if (freeze_intensity > 0.1) {
            rotation_speed_factor = 0.05 + (1.0 - freeze_intensity) * 0.95;
        }
        if (explode_intensity > 0.1) {
            rotation_speed_factor *= (1.0 + explode_intensity * 0.5);  // Faster rotation
        }
        
        s->rotation += s->rotation_speed * dt * rotation_speed_factor;
        
        // Pulse with effects
        double pulse_speed_factor = 1.0;
        if (freeze_intensity > 0.1) {
            pulse_speed_factor = 0.1 + (1.0 - freeze_intensity) * 0.9;
        }
        
        s->pulse_phase += s->scale_speed * dt * pulse_speed_factor;
        
        double pulse = sin(s->pulse_phase) * 0.5 + 1.0;
        double explode_scale = 1.0 + explode_intensity * 0.8;  // Bigger during explode
        s->scale = 0.2 * pulse * explode_scale;
        
        // Brightness with color effects
        s->brightness = 0.8 + sin(vis->time_offset * 8.0 + i) * 0.2;
        
        // Color shift during invert
        if (invert_intensity > 0.1) {
            s->hue += invert_intensity * 0.01;  // Shift hue during invert
            if (s->hue > 1.0) s->hue -= 1.0;
            s->saturation = 1.0 - invert_intensity * 0.3;  // Less saturated
        }
        
        // Dim during freeze
        if (freeze_intensity > 0.1) {
            s->brightness *= (0.5 + freeze_intensity * 0.5);  // Dimmer
        }
        
        s->life -= dt * 0.25;
        if (s->life <= 0.0) {
            s->active = FALSE;
            active--;
        }
    }
    
    vis->kaleidoscope_shape_count = active;
}

void draw_kaleidoscope_shape(cairo_t *cr, KaleidoscopeShape *shape, double scale_factor) {
    double r, g, b;
    kaleidoscope_hsv_to_rgb(shape->hue, shape->saturation, shape->brightness, &r, &g, &b);
    
    cairo_set_source_rgba(cr, r, g, b, shape->life / 3.0);
    cairo_rotate(cr, shape->rotation);
    cairo_scale(cr, shape->scale * scale_factor, shape->scale * scale_factor);
    
    switch (shape->shape_type) {
        case 0:
            cairo_arc(cr, 0, 0, 1.5, 0, 2 * M_PI);
            cairo_fill(cr);
            break;
            
        case 1:
            cairo_move_to(cr, 0, -2);
            cairo_line_to(cr, 1.7, 1);
            cairo_line_to(cr, -1.7, 1);
            cairo_close_path(cr);
            cairo_fill(cr);
            break;
            
        case 2:
            cairo_rectangle(cr, -1.5, -1.5, 3.0, 3.0);
            cairo_fill(cr);
            break;
            
        case 3:
            cairo_move_to(cr, 0, -2);
            for (int i = 1; i < 10; i++) {
                double angle = i * M_PI / 5.0;
                double rad = (i % 2 == 0) ? 2.0 : 0.8;
                cairo_line_to(cr, sin(angle) * rad, -cos(angle) * rad);
            }
            cairo_close_path(cr);
            cairo_fill(cr);
            break;
            
        case 4:
            cairo_move_to(cr, 1.5, 0);
            for (int i = 1; i < 6; i++) {
                double angle = i * M_PI / 3.0;
                cairo_line_to(cr, cos(angle) * 1.5, sin(angle) * 1.5);
            }
            cairo_close_path(cr);
            cairo_fill(cr);
            break;
            
        case 5:
            cairo_move_to(cr, 0, -2);
            cairo_line_to(cr, 1.4, 0);
            cairo_line_to(cr, 0, 2);
            cairo_line_to(cr, -1.4, 0);
            cairo_close_path(cr);
            cairo_fill(cr);
            break;
            
        case 6:
            cairo_rectangle(cr, -0.4, -2, 0.8, 4);
            cairo_fill(cr);
            cairo_rectangle(cr, -2, -0.4, 4, 0.8);
            cairo_fill(cr);
            break;
    }
}

void draw_kaleidoscope(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double cx = vis->width / 2.0;
    double cy = vis->height / 2.0;
    double rad = fmin(cx, cy) * 0.9;
    
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    
    cairo_save(cr);
    cairo_arc(cr, cx, cy, rad, 0, 2 * M_PI);
    cairo_clip(cr);
    
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, vis->kaleidoscope_rotation);
    cairo_scale(cr, vis->kaleidoscope_zoom, vis->kaleidoscope_zoom);
    
    // ========== MIRROR COUNT WITH INVERT EFFECT ==========
    int mirrors = 12;
    if (invert_intensity > 0.1) {
        // During invert, change mirror count (creates kaleidoscope twist)
        mirrors = 6 + (int)(invert_intensity * 6.0);  // 6 to 12 mirrors
    }
    
    double angle = 2.0 * M_PI / mirrors;
    
    for (int m = 0; m < mirrors; m++) {
        cairo_save(cr);
        cairo_rotate(cr, m * angle);
        
        cairo_move_to(cr, 0, 0);
        cairo_line_to(cr, rad * 2, 0);
        cairo_line_to(cr, rad * 2 * cos(angle), rad * 2 * sin(angle));
        cairo_close_path(cr);
        cairo_clip(cr);
        
        for (int i = 0; i < MAX_KALEIDOSCOPE_SHAPES; i++) {
            KaleidoscopeShape *s = &vis->kaleidoscope_shapes[i];
            if (!s->active) continue;
            
            cairo_save(cr);
            cairo_translate(cr, (s->x - 0.5) * rad * 1.5, (s->y - 0.5) * rad * 1.5);
            draw_kaleidoscope_shape(cr, s, 50.0);
            cairo_restore(cr);
        }
        
        if (m % 2 == 1) {
            cairo_save(cr);
            cairo_scale(cr, 1.0, -1.0);
            
            for (int i = 0; i < MAX_KALEIDOSCOPE_SHAPES; i++) {
                KaleidoscopeShape *s = &vis->kaleidoscope_shapes[i];
                if (!s->active) continue;
                
                cairo_save(cr);
                cairo_translate(cr, (s->x - 0.5) * rad * 1.5, (s->y - 0.5) * rad * 1.5);
                draw_kaleidoscope_shape(cr, s, 50.0);
                cairo_restore(cr);
            }
            cairo_restore(cr);
        }
        
        cairo_restore(cr);
    }
    
    cairo_restore(cr);
    
    // ========== AUDIO-REACTIVE OUTLINE ==========
    int num_points = VIS_FREQUENCY_BARS * 4;
    
    // Main waveform ring with variable line width
    double outline_intensity = 0.85 + explode_intensity * 0.15;  // Brighter during explode
    
    for (int i = 0; i < num_points; i++) {
        double outline_angle = (double)i / num_points * 2.0 * M_PI + vis->kaleidoscope_rotation * 0.5;
        double next_angle = (double)(i + 1) / num_points * 2.0 * M_PI + vis->kaleidoscope_rotation * 0.5;
        
        // Map to frequency bands
        int band = ((i % num_points) * VIS_FREQUENCY_BARS) / num_points;
        if (band >= VIS_FREQUENCY_BARS) band = VIS_FREQUENCY_BARS - 1;
        
        double band_fraction = ((double)(i % num_points) * VIS_FREQUENCY_BARS) / num_points - band;
        int next_band = (band + 1) % VIS_FREQUENCY_BARS;
        
        double bulge = vis->frequency_bands[band] * (1.0 - band_fraction) + 
                       vis->frequency_bands[next_band] * band_fraction;
        double next_bulge = vis->frequency_bands[next_band];
        
        // Add ripple effect
        double ripple = sin(vis->time_offset * 2.0 + outline_angle * 3.0) * 0.08;
        double next_ripple = sin(vis->time_offset * 2.0 + next_angle * 3.0) * 0.08;
        
        double radius = rad + bulge * 40.0 + ripple * 8.0;
        double next_radius = rad + next_bulge * 40.0 + next_ripple * 8.0;
        
        double x = cx + cos(outline_angle) * radius;
        double y = cy + sin(outline_angle) * radius;
        double next_x = cx + cos(next_angle) * next_radius;
        double next_y = cy + sin(next_angle) * next_radius;
        
        double line_width = 1.5 + bulge * 4.0;
        double intensity = 0.7 + bulge * 0.3;
        
        cairo_set_line_width(cr, line_width);
        cairo_set_source_rgba(cr, 
            vis->accent_r * intensity, 
            vis->accent_g * intensity, 
            vis->accent_b * intensity, 
            outline_intensity + bulge * 0.15);
        
        cairo_move_to(cr, x, y);
        cairo_line_to(cr, next_x, next_y);
        cairo_stroke(cr);
    }
    
    // Inner glow layer with effect modulation
    double glow_intensity = 0.4;
    if (explode_intensity > 0.1) {
        glow_intensity = 0.4 + explode_intensity * 0.3;  // Brighter glow during explode
    }
    if (freeze_intensity > 0.1) {
        glow_intensity = 0.4 - freeze_intensity * 0.2;  // Dimmer glow during freeze
    }
    
    cairo_new_path(cr);
    for (int i = 0; i <= num_points; i++) {
        double outline_angle = (double)i / num_points * 2.0 * M_PI + vis->kaleidoscope_rotation * 0.5;
        
        int band = ((i % num_points) * VIS_FREQUENCY_BARS) / num_points;
        if (band >= VIS_FREQUENCY_BARS) band = VIS_FREQUENCY_BARS - 1;
        
        double band_fraction = ((double)(i % num_points) * VIS_FREQUENCY_BARS) / num_points - band;
        int next_band = (band + 1) % VIS_FREQUENCY_BARS;
        
        double bulge = vis->frequency_bands[band] * (1.0 - band_fraction) + 
                       vis->frequency_bands[next_band] * band_fraction;
        double ripple = sin(vis->time_offset * 2.0 + outline_angle * 3.0) * 0.08;
        
        double radius = rad + bulge * 40.0 + ripple * 8.0;
        double x = cx + cos(outline_angle) * radius;
        double y = cy + sin(outline_angle) * radius;
        
        if (i == 0) {
            cairo_move_to(cr, x, y);
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    cairo_close_path(cr);
    cairo_set_line_width(cr, 3.0);
    cairo_set_source_rgba(cr, vis->fg_r, vis->fg_g, vis->fg_b, glow_intensity);
    cairo_stroke(cr);
    
    // Secondary phase-shifted waveform for depth
    for (int i = 0; i < num_points; i++) {
        double outline_angle = (double)i / num_points * 2.0 * M_PI + vis->kaleidoscope_rotation * 0.5;
        double next_angle = (double)(i + 1) / num_points * 2.0 * M_PI + vis->kaleidoscope_rotation * 0.5;
        
        int band = ((i % num_points) * VIS_FREQUENCY_BARS) / num_points;
        if (band >= VIS_FREQUENCY_BARS) band = VIS_FREQUENCY_BARS - 1;
        
        double band_fraction = ((double)(i % num_points) * VIS_FREQUENCY_BARS) / num_points - band;
        int next_band = (band + 1) % VIS_FREQUENCY_BARS;
        
        // Phase shift for secondary wave
        int shifted_band = (band + VIS_FREQUENCY_BARS / 8) % VIS_FREQUENCY_BARS;
        int shifted_next = (next_band + VIS_FREQUENCY_BARS / 8) % VIS_FREQUENCY_BARS;
        
        double bulge = vis->frequency_bands[shifted_band] * (1.0 - band_fraction) + 
                       vis->frequency_bands[shifted_next] * band_fraction;
        
        double ripple = sin(vis->time_offset * 2.0 + outline_angle * 3.0 + M_PI / 4.0) * 0.06;
        double next_ripple = sin(vis->time_offset * 2.0 + next_angle * 3.0 + M_PI / 4.0) * 0.06;
        
        double radius = rad * 0.95 + bulge * 30.0 + ripple * 6.0;
        double next_radius = rad * 0.95 + bulge * 30.0 + next_ripple * 6.0;
        
        double x = cx + cos(outline_angle) * radius;
        double y = cy + sin(outline_angle) * radius;
        double next_x = cx + cos(next_angle) * next_radius;
        double next_y = cy + sin(next_angle) * next_radius;
        
        double line_width = 1.0 + bulge * 2.5;
        
        cairo_set_line_width(cr, line_width);
        cairo_set_source_rgba(cr, 
            vis->fg_r * 0.7, 
            vis->fg_g * 0.7, 
            vis->fg_b * 0.7, 
            0.3 + bulge * 0.2);
        
        cairo_move_to(cr, x, y);
        cairo_line_to(cr, next_x, next_y);
        cairo_stroke(cr);
    }
    
    // ========== STATUS INDICATOR ==========
    if (explode_intensity > 0.1 || invert_intensity > 0.1 || freeze_intensity > 0.1) {
        cairo_set_font_size(cr, 14.0);
        cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.7);
        
        if (explode_intensity > 0.1) {
            cairo_move_to(cr, 10, 25);
            cairo_show_text(cr, "EXPLODE");
        } else if (invert_intensity > 0.1) {
            cairo_move_to(cr, 10, 25);
            cairo_show_text(cr, "INVERT");
        } else if (freeze_intensity > 0.1) {
            cairo_move_to(cr, 10, 25);
            cairo_show_text(cr, "FREEZE");
        }
    }
}
