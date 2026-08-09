#include "visualization.h"

void init_eye_of_sauron(Visualizer *vis) {
    vis->eye_of_sauron.pupil_size = 0.3;
    vis->eye_of_sauron.iris_rotation = 0.0;
    vis->eye_of_sauron.blink_timer = 0.0;
    vis->eye_of_sauron.blink_duration = 0.0;
    vis->eye_of_sauron.is_blinking = FALSE;
    vis->eye_of_sauron.look_x = 0.0;
    vis->eye_of_sauron.look_y = 0.0;
    vis->eye_of_sauron.look_target_x = 0.0;
    vis->eye_of_sauron.look_target_y = 0.0;
    vis->eye_of_sauron.intensity_pulse = 0.0;
    vis->eye_of_sauron.fire_ring_rotation = 0.0;
    vis->eye_of_sauron.fire_intensity = 1.0;
    vis->eye_of_sauron.fire_particle_count = 0;
}

void spawn_fire_particle(Visualizer *vis, int type) {
    EyeOfSauronState *eye = &vis->eye_of_sauron;
    
    if (eye->fire_particle_count >= 60) return;
    
    int idx = eye->fire_particle_count++;
    
    eye->fire_particles[idx].angle = ((double)rand() / RAND_MAX) * 2.0 * M_PI;
    eye->fire_particles[idx].radius = 1.0 + ((double)rand() / RAND_MAX) * 0.3;
    eye->fire_particles[idx].height = 0.0;
    eye->fire_particles[idx].life = 1.0;
    eye->fire_particles[idx].speed = 0.3 + ((double)rand() / RAND_MAX) * 0.5;
    eye->fire_particles[idx].type = type;
    
    if (type == 0) {  // Flame
        eye->fire_particles[idx].size = 8.0 + ((double)rand() / RAND_MAX) * 12.0;
    } else if (type == 1) {  // Ember
        eye->fire_particles[idx].size = 3.0 + ((double)rand() / RAND_MAX) * 5.0;
    } else {  // Spark
        eye->fire_particles[idx].size = 2.0 + ((double)rand() / RAND_MAX) * 3.0;
        eye->fire_particles[idx].speed *= 1.5;
    }
}

void update_eye_of_sauron(Visualizer *vis, double dt) {
    EyeOfSauronState *eye = &vis->eye_of_sauron;
    
    // Pupil dilation based on volume
    double target_pupil = 0.2 + vis->volume_level * 0.4;
    eye->pupil_size += (target_pupil - eye->pupil_size) * dt * 5.0;
    
    // Iris rotation
    eye->iris_rotation += dt * (0.5 + vis->volume_level * 2.0);
    
    // Fire ring rotation
    eye->fire_ring_rotation += dt * 0.3;
    
    // Fire intensity based on audio
    eye->fire_intensity = 0.7 + vis->volume_level * 0.3;
    
    // Blinking
    eye->blink_timer += dt;
    if (eye->blink_timer > 3.0 + ((double)rand() / RAND_MAX) * 4.0) {
        eye->is_blinking = TRUE;
        eye->blink_duration = 0.0;
        eye->blink_timer = 0.0;
    }
    
    if (eye->is_blinking) {
        eye->blink_duration += dt;
        if (eye->blink_duration > 0.2) {
            eye->is_blinking = FALSE;
        }
    }
    
    // Eye looking based on frequency bands
    double look_force_x = 0.0;
    double look_force_y = 0.0;
    
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        double angle = (double)i / VIS_FREQUENCY_BARS * 2.0 * M_PI;
        look_force_x += cos(angle) * vis->frequency_bands[i];
        look_force_y += sin(angle) * vis->frequency_bands[i];
    }
    
    eye->look_target_x = look_force_x * 30.0;
    eye->look_target_y = look_force_y * 30.0;
    eye->look_x += (eye->look_target_x - eye->look_x) * dt * 3.0;
    eye->look_y += (eye->look_target_y - eye->look_y) * dt * 3.0;
    
    // Intensity pulse
    if (vis->volume_level > 0.3) {
        eye->intensity_pulse = 1.0;
    }
    if (eye->intensity_pulse > 0) {
        eye->intensity_pulse -= dt * 2.0;
        if (eye->intensity_pulse < 0) eye->intensity_pulse = 0;
    }
    
    // Update fire particles
    for (int i = 0; i < eye->fire_particle_count; i++) {
        eye->fire_particles[i].height += eye->fire_particles[i].speed * dt;
        eye->fire_particles[i].angle += dt * 0.5 * (eye->fire_particles[i].type == 2 ? 2.0 : 1.0);
        eye->fire_particles[i].life -= dt * (0.4 + eye->fire_particles[i].speed * 0.3);
        
        // Add some wobble to flames
        if (eye->fire_particles[i].type == 0) {
            eye->fire_particles[i].radius += sin(eye->fire_particles[i].height * 10.0) * dt * 0.1;
        }
    }
    
    // Remove dead particles
    for (int i = 0; i < eye->fire_particle_count; i++) {
        if (eye->fire_particles[i].life <= 0 || eye->fire_particles[i].height > 2.5) {
            eye->fire_particles[i] = eye->fire_particles[eye->fire_particle_count - 1];
            eye->fire_particle_count--;
            i--;
        }
    }
    
    // Spawn new fire particles based on audio
    int spawn_count = (int)(vis->volume_level * 10.0) + 2;
    for (int i = 0; i < spawn_count && eye->fire_particle_count < 60; i++) {
        int type = rand() % 100;
        if (type < 60) {
            spawn_fire_particle(vis, 0);  // Flame
        } else if (type < 85) {
            spawn_fire_particle(vis, 1);  // Ember
        } else {
            spawn_fire_particle(vis, 2);  // Spark
        }
    }
}

void draw_fire_ring(Visualizer *vis, cairo_t *cr, double center_x, double center_y, double base_radius) {
    EyeOfSauronState *eye = &vis->eye_of_sauron;
    
    // Draw fire particles
    for (int i = 0; i < eye->fire_particle_count; i++) {
        double angle = eye->fire_particles[i].angle + eye->fire_ring_rotation;
        double radius = base_radius * eye->fire_particles[i].radius;
        double height = eye->fire_particles[i].height;
        double life = eye->fire_particles[i].life;
        
        double px = center_x + cos(angle) * radius;
        double py = center_y - height * 80.0 + sin(angle) * (radius * 0.3);
        
        double size = eye->fire_particles[i].size * life;
        
        // Color based on type and life
        double r, g, b, a;
        
        if (eye->fire_particles[i].type == 0) {  // Flame
            if (life > 0.7) {
                // Bright yellow-white at base
                r = 1.0;
                g = 0.9 + life * 0.1;
                b = 0.7 * life;
            } else if (life > 0.4) {
                // Orange
                r = 1.0;
                g = 0.5 + life * 0.3;
                b = 0.0;
            } else {
                // Red fading
                r = 0.9 + life * 0.1;
                g = life * 0.3;
                b = 0.0;
            }
            a = life * 0.7;
        } else if (eye->fire_particles[i].type == 1) {  // Ember
            r = 1.0;
            g = 0.3 + life * 0.2;
            b = 0.0;
            a = life * 0.9;
        } else {  // Spark
            r = 1.0;
            g = 0.9;
            b = 0.6;
            a = life * 0.95;
        }
        
        // Glow effect
        cairo_set_source_rgba(cr, r, g, b, a * 0.3);
        cairo_arc(cr, px, py, size * 2.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Main particle
        cairo_set_source_rgba(cr, r, g, b, a);
        cairo_arc(cr, px, py, size, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Bright center for flames
        if (eye->fire_particles[i].type == 0 && life > 0.5) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 0.9, a * 0.8);
            cairo_arc(cr, px, py, size * 0.4, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
    
    // Draw base ring of fire (the platform)
    int segments = 64;
    for (int i = 0; i < segments; i++) {
        double angle1 = (double)i / segments * 2.0 * M_PI + eye->fire_ring_rotation;
        double angle2 = (double)(i + 1) / segments * 2.0 * M_PI + eye->fire_ring_rotation;
        
        double x1 = center_x + cos(angle1) * base_radius * 0.9;
        double y1 = center_y + sin(angle1) * (base_radius * 0.3);
        double x2 = center_x + cos(angle2) * base_radius * 0.9;
        double y2 = center_y + sin(angle2) * (base_radius * 0.3);
        
        // Pulsing intensity
        double pulse = 0.8 + 0.2 * sin(angle1 * 3.0 + vis->time_offset * 2.0);
        double intensity = eye->fire_intensity * pulse;
        
        cairo_set_source_rgba(cr, 1.0, 0.4 * intensity, 0.0, 0.8);
        cairo_set_line_width(cr, 8.0 + intensity * 6.0);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }
}

void draw_eye_of_sauron(Visualizer *vis, cairo_t *cr) {
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    double eye_width = fmin(vis->width, vis->height) * 0.4;
    double eye_height = eye_width * 0.6;
    
    EyeOfSauronState *eye = &vis->eye_of_sauron;
    
    // Draw fire ring first (behind eye)
    draw_fire_ring(vis, cr, center_x, center_y, eye_width * 1.3);
    
    // Outer glow
    double glow_intensity = 0.3 + eye->intensity_pulse * 0.4 + vis->volume_level * 0.3;
    cairo_pattern_t *glow = cairo_pattern_create_radial(
        center_x + eye->look_x, center_y + eye->look_y, eye_height * 0.3,
        center_x + eye->look_x, center_y + eye->look_y, eye_height * 1.5);
    
    cairo_pattern_add_color_stop_rgba(glow, 0, 1.0, 0.4, 0.0, glow_intensity * 0.8);
    cairo_pattern_add_color_stop_rgba(glow, 0.5, 1.0, 0.2, 0.0, glow_intensity * 0.4);
    cairo_pattern_add_color_stop_rgba(glow, 1, 0.3, 0.0, 0.0, 0);
    
    cairo_set_source(cr, glow);
    cairo_arc(cr, center_x + eye->look_x, center_y + eye->look_y, 
              eye_height * 1.5, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(glow);
    
    // Eye sclera
    if (!eye->is_blinking) {
        cairo_save(cr);
        cairo_translate(cr, center_x + eye->look_x, center_y + eye->look_y);
        cairo_scale(cr, 1.0, 0.6);
        
        cairo_pattern_t *sclera = cairo_pattern_create_radial(0, 0, 0, 0, 0, eye_width);
        cairo_pattern_add_color_stop_rgba(sclera, 0, 0.9, 0.8, 0.6, 0.95);
        cairo_pattern_add_color_stop_rgba(sclera, 1, 0.6, 0.5, 0.3, 0.9);
        cairo_set_source(cr, sclera);
        cairo_arc(cr, 0, 0, eye_width, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(sclera);
        
        cairo_restore(cr);
        
        // Iris with flame patterns
        cairo_save(cr);
        cairo_translate(cr, center_x + eye->look_x, center_y + eye->look_y);
        
        double iris_radius = eye_height * 0.8;
        
        // Rotating flame rays
        for (int i = 0; i < 16; i++) {
            double angle = eye->iris_rotation + (i / 16.0) * 2.0 * M_PI;
            double x1 = cos(angle) * iris_radius * 0.2;
            double y1 = sin(angle) * iris_radius * 0.2;
            double x2 = cos(angle) * iris_radius * 1.1;
            double y2 = sin(angle) * iris_radius * 1.1;
            
            cairo_pattern_t *ray = cairo_pattern_create_linear(x1, y1, x2, y2);
            cairo_pattern_add_color_stop_rgba(ray, 0, 1.0, 0.6, 0.1, 0.9);
            cairo_pattern_add_color_stop_rgba(ray, 0.5, 1.0, 0.3, 0.0, 0.6);
            cairo_pattern_add_color_stop_rgba(ray, 1, 0.5, 0.1, 0.0, 0.2);
            
            cairo_set_source(cr, ray);
            cairo_set_line_width(cr, iris_radius * 0.12);
            cairo_move_to(cr, x1, y1);
            cairo_line_to(cr, x2, y2);
            cairo_stroke(cr);
            cairo_pattern_destroy(ray);
        }
        
        // Iris base
        cairo_pattern_t *iris = cairo_pattern_create_radial(0, 0, 0, 0, 0, iris_radius);
        cairo_pattern_add_color_stop_rgba(iris, 0, 1.0, 0.5, 0.0, 0.95);
        cairo_pattern_add_color_stop_rgba(iris, 0.6, 1.0, 0.3, 0.0, 0.85);
        cairo_pattern_add_color_stop_rgba(iris, 1, 0.4, 0.1, 0.0, 0.9);
        cairo_set_source(cr, iris);
        cairo_arc(cr, 0, 0, iris_radius, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(iris);
        
        // Pupil
        double pupil_radius = eye_height * eye->pupil_size;
        
        cairo_pattern_t *pupil = cairo_pattern_create_radial(0, 0, 0, 0, 0, pupil_radius * 1.5);
        cairo_pattern_add_color_stop_rgba(pupil, 0, 0.0, 0.0, 0.0, 1.0);
        cairo_pattern_add_color_stop_rgba(pupil, 0.7, 0.0, 0.0, 0.0, 1.0);
        cairo_pattern_add_color_stop_rgba(pupil, 1, 0.4, 0.0, 0.0, 0.0);
        cairo_set_source(cr, pupil);
        cairo_arc(cr, 0, 0, pupil_radius * 1.5, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(pupil);
        
        // Reflection
        cairo_set_source_rgba(cr, 1.0, 0.4, 0.0, 0.4);
        cairo_arc(cr, -pupil_radius * 0.3, -pupil_radius * 0.3, pupil_radius * 0.2, 0, 2 * M_PI);
        cairo_fill(cr);
        
        cairo_restore(cr);
    } else {
        // Blinking
        double blink_progress = eye->blink_duration / 0.2;
        if (blink_progress > 0.5) blink_progress = 1.0 - blink_progress;
        blink_progress *= 2.0;
        
        cairo_save(cr);
        cairo_translate(cr, center_x + eye->look_x, center_y + eye->look_y);
        cairo_scale(cr, 1.0, 1.0 - blink_progress * 0.95);
        
        cairo_set_source_rgba(cr, 0.3, 0.2, 0.1, 0.9);
        cairo_arc(cr, 0, 0, eye_width, 0, 2 * M_PI);
        cairo_fill(cr);
        
        cairo_restore(cr);
    }
    
    // Eye outline
    cairo_save(cr);
    cairo_translate(cr, center_x + eye->look_x, center_y + eye->look_y);
    cairo_scale(cr, 1.0, 0.6);
    
    cairo_set_source_rgba(cr, 0.2, 0.1, 0.0, 0.9);
    cairo_set_line_width(cr, 5.0);
    cairo_arc(cr, 0, 0, eye_width, 0, 2 * M_PI);
    cairo_stroke(cr);
    
    cairo_restore(cr);
}
