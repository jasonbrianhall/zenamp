#include "visualization.h"

void init_clock_system(Visualizer *vis) {
    vis->swirl_particle_count = 0;
    vis->swirl_spawn_timer = 0.0;
    vis->swirl_beat_threshold = 0.15;
    vis->clock_dot_size = 8.0;
    vis->clock_digit_spacing = 60.0;
    vis->clock_colon_blink_timer = 0.0;
    vis->clock_beat_pulse = 0.0;
    vis->clock_show_seconds = TRUE;
    
    // Initialize all particles as inactive
    for (int i = 0; i < MAX_SWIRL_PARTICLES; i++) {
        vis->swirl_particles[i].active = FALSE;
    }
}

gboolean clock_detect_beat(Visualizer *vis) {
    // Simple beat detection based on volume spike
    double current_volume = vis->volume_level;
    static double last_volume = 0.0;
    static double beat_cooldown = 0.0;
    
    beat_cooldown -= 0.033; // Decrease cooldown
    if (beat_cooldown < 0) beat_cooldown = 0;
    
    gboolean beat = (current_volume > vis->swirl_beat_threshold && 
                     current_volume > last_volume * 1.5 && 
                     beat_cooldown <= 0);
    
    if (beat) {
        beat_cooldown = 0.2; // 200ms cooldown between beats
    }
    
    last_volume = current_volume;
    return beat;
}

void spawn_swirl_particle(Visualizer *vis, double intensity, int frequency_band) {
    // Find inactive particle slot
    int slot = -1;
    for (int i = 0; i < MAX_SWIRL_PARTICLES; i++) {
        if (!vis->swirl_particles[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        // Replace oldest particle
        slot = 0;
    }
    
    SwirlParticle *particle = &vis->swirl_particles[slot];
    
    // Start near the clock center with some randomness
    double spawn_radius = 50.0 + ((double)rand() / RAND_MAX) * 30.0;
    double spawn_angle = ((double)rand() / RAND_MAX) * 2.0 * M_PI;
    
    particle->x = vis->clock_center_x + cos(spawn_angle) * spawn_radius;
    particle->y = vis->clock_center_y + sin(spawn_angle) * spawn_radius;
    particle->angle = spawn_angle;
    particle->radius = spawn_radius;
    
    // Velocity based on frequency band and intensity
    particle->angular_velocity = (1.0 + intensity * 2.0) * (frequency_band % 2 == 0 ? 1.0 : -1.0);
    particle->radial_velocity = intensity * 50.0 * (((double)rand() / RAND_MAX) > 0.5 ? 1.0 : -1.0);
    
    // Visual properties
    particle->hue = (double)frequency_band / VIS_FREQUENCY_BARS * 360.0;
    particle->size = 3.0 + intensity * 5.0;
    particle->intensity = intensity;
    particle->life = 1.0;
    particle->frequency_band = frequency_band;
    particle->active = TRUE;
    
    if (slot >= vis->swirl_particle_count) {
        vis->swirl_particle_count = slot + 1;
    }
}

void update_clock_swirls(Visualizer *vis, double dt) {
    vis->swirl_spawn_timer += dt;
    vis->clock_colon_blink_timer += dt;
    
    // Update beat pulse effect
    if (vis->clock_beat_pulse > 0) {
        vis->clock_beat_pulse -= dt * 3.0; // Fast decay
        if (vis->clock_beat_pulse < 0) vis->clock_beat_pulse = 0;
    }
    
    // Detect beats and spawn particles
    if (clock_detect_beat(vis)) {
        vis->clock_beat_pulse = 1.0;
        
        // Spawn multiple particles on beat
        for (int i = 0; i < 3; i++) {
            int band = rand() % VIS_FREQUENCY_BARS;
            spawn_swirl_particle(vis, vis->frequency_bands[band], band);
        }
    }
    
    // Also spawn particles based on frequency activity
    for (int band = 0; band < VIS_FREQUENCY_BARS; band += 4) {
        if (vis->frequency_bands[band] > vis->swirl_beat_threshold && 
            vis->swirl_spawn_timer > 0.1) {
            spawn_swirl_particle(vis, vis->frequency_bands[band], band);
            vis->swirl_spawn_timer = 0.0;
        }
    }
    
    // Update existing particles
    for (int i = 0; i < vis->swirl_particle_count; i++) {
        SwirlParticle *particle = &vis->swirl_particles[i];
        if (!particle->active) continue;
        
        // Update position
        particle->angle += particle->angular_velocity * dt;
        particle->radius += particle->radial_velocity * dt;
        
        // Update world coordinates
        particle->x = vis->clock_center_x + cos(particle->angle) * particle->radius;
        particle->y = vis->clock_center_y + sin(particle->angle) * particle->radius;
        
        // Apply some randomness to movement
        particle->angular_velocity *= 0.99;
        particle->radial_velocity *= 0.98;
        
        // Decay life
        particle->life -= dt * 0.5; // 2 second lifespan
        
        // Remove particles that are too old or too far
        if (particle->life <= 0 || particle->radius > 300.0 || particle->radius < 5.0) {
            particle->active = FALSE;
        }
    }
    
    // Compact particle array
    int write_pos = 0;
    for (int read_pos = 0; read_pos < vis->swirl_particle_count; read_pos++) {
        if (vis->swirl_particles[read_pos].active) {
            if (write_pos != read_pos) {
                vis->swirl_particles[write_pos] = vis->swirl_particles[read_pos];
            }
            write_pos++;
        }
    }
    vis->swirl_particle_count = write_pos;
}

void draw_digit_matrix(cairo_t *cr, int digit, double x, double y, double dot_size, 
                      double r, double g, double b, double intensity) {
    if (digit < 0 || digit > 9) return;
    
    for (int row = 0; row < DIGIT_HEIGHT; row++) {
        for (int col = 0; col < DIGIT_WIDTH; col++) {
            if (digit_patterns[digit][row][col]) {
                double dot_x = x + col * (dot_size + 2);
                double dot_y = y + row * (dot_size + 2);
                
                // Add some glow effect based on audio intensity
                double glow_size = dot_size * (1.0 + intensity * 0.5);
                
                // Glow
                cairo_set_source_rgba(cr, r, g, b, intensity * 0.3);
                cairo_arc(cr, dot_x, dot_y, glow_size, 0, 2 * M_PI);
                cairo_fill(cr);
                
                // Main dot
                cairo_set_source_rgba(cr, r, g, b, 0.9);
                cairo_arc(cr, dot_x, dot_y, dot_size * 0.7, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }
    }
}

void draw_clock_swirls(Visualizer *vis, cairo_t *cr) {
    for (int i = 0; i < vis->swirl_particle_count; i++) {
        SwirlParticle *particle = &vis->swirl_particles[i];
        if (!particle->active) continue;
        
        double r, g, b;
        hsv_to_rgb(particle->hue, 0.8, 0.9, &r, &g, &b);
        
        double alpha = particle->life * 0.8;
        double size = particle->size * particle->life;
        
        // Draw particle with glow
        cairo_set_source_rgba(cr, r, g, b, alpha * 0.3);
        cairo_arc(cr, particle->x, particle->y, size * 2, 0, 2 * M_PI);
        cairo_fill(cr);
        
        cairo_set_source_rgba(cr, r, g, b, alpha);
        cairo_arc(cr, particle->x, particle->y, size, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Add bright center
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha * 0.8);
        cairo_arc(cr, particle->x, particle->y, size * 0.3, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

void draw_clock_visualization(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    // Update center position
    vis->clock_center_x = vis->width / 2.0;
    vis->clock_center_y = vis->height / 2.0;
    
    // Get current time
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    
    // Calculate audio intensity for glow effects
    double audio_intensity = vis->volume_level + vis->clock_beat_pulse * 0.5;
    if (audio_intensity > 1.0) audio_intensity = 1.0;
    
    // Calculate positions for time display
    double total_width = vis->clock_show_seconds ? 
        (8 * (DIGIT_WIDTH * (vis->clock_dot_size + 2))) + (2 * 20) : // HH:MM:SS
        (5 * (DIGIT_WIDTH * (vis->clock_dot_size + 2))) + (1 * 20);  // HH:MM
    
    double start_x = vis->clock_center_x - total_width / 2.0;
    double start_y = vis->clock_center_y - (DIGIT_HEIGHT * (vis->clock_dot_size + 2)) / 2.0;
    
    double current_x = start_x;
    
    // Color scheme - change based on audio
    double base_hue = fmod(vis->time_offset * 10.0, 360.0);
    double r, g, b;
    hsv_to_rgb(base_hue, 0.7, 0.8 + audio_intensity * 0.2, &r, &g, &b);
    
    // Draw hours
    int hours = local_time->tm_hour;
    draw_digit_matrix(cr, hours / 10, current_x, start_y, vis->clock_dot_size, 
                     r, g, b, audio_intensity);
    current_x += DIGIT_WIDTH * (vis->clock_dot_size + 2) + 5;
    
    draw_digit_matrix(cr, hours % 10, current_x, start_y, vis->clock_dot_size, 
                     r, g, b, audio_intensity);
    current_x += DIGIT_WIDTH * (vis->clock_dot_size + 2) + 15;
    
    // Draw colon (blinking based on audio or seconds)
    gboolean show_colon = (fmod(vis->clock_colon_blink_timer, 1.0) < 0.5) || 
                         (audio_intensity > 0.3);
    if (show_colon) {
        double colon_x = current_x;
        double colon_y = start_y + (vis->clock_dot_size + 2);
        
        cairo_set_source_rgba(cr, r, g, b, 0.9);
        cairo_arc(cr, colon_x, colon_y, vis->clock_dot_size * 0.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        colon_y += (vis->clock_dot_size + 2) * 2;
        cairo_arc(cr, colon_x, colon_y, vis->clock_dot_size * 0.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    current_x += 20;
    
    // Draw minutes
    int minutes = local_time->tm_min;
    draw_digit_matrix(cr, minutes / 10, current_x, start_y, vis->clock_dot_size, 
                     r, g, b, audio_intensity);
    current_x += DIGIT_WIDTH * (vis->clock_dot_size + 2) + 5;
    
    draw_digit_matrix(cr, minutes % 10, current_x, start_y, vis->clock_dot_size, 
                     r, g, b, audio_intensity);
    current_x += DIGIT_WIDTH * (vis->clock_dot_size + 2) + 15;
    
    // Draw seconds if enabled
    if (vis->clock_show_seconds) {
        // Second colon
        if (show_colon) {
            double colon_x = current_x;
            double colon_y = start_y + (vis->clock_dot_size + 2);
            
            cairo_set_source_rgba(cr, r, g, b, 0.9);
            cairo_arc(cr, colon_x, colon_y, vis->clock_dot_size * 0.5, 0, 2 * M_PI);
            cairo_fill(cr);
            
            colon_y += (vis->clock_dot_size + 2) * 2;
            cairo_arc(cr, colon_x, colon_y, vis->clock_dot_size * 0.5, 0, 2 * M_PI);
            cairo_fill(cr);
        }
        current_x += 20;
        
        int seconds = local_time->tm_sec;
        draw_digit_matrix(cr, seconds / 10, current_x, start_y, vis->clock_dot_size, 
                         r, g, b, audio_intensity);
        current_x += DIGIT_WIDTH * (vis->clock_dot_size + 2) + 5;
        
        draw_digit_matrix(cr, seconds % 10, current_x, start_y, vis->clock_dot_size, 
                         r, g, b, audio_intensity);
    }
    
    // Draw the swirling particles
    draw_clock_swirls(vis, cr);
    
    // Draw beat pulse ring around clock
    if (vis->clock_beat_pulse > 0) {
        double ring_radius = 150 + vis->clock_beat_pulse * 50;
        cairo_set_source_rgba(cr, r, g, b, vis->clock_beat_pulse * 0.5);
        cairo_set_line_width(cr, 3.0);
        cairo_arc(cr, vis->clock_center_x, vis->clock_center_y, ring_radius, 0, 2 * M_PI);
        cairo_stroke(cr);
    }
}

// Analog Clock

void init_analog_clock_system(Visualizer *vis) {
    vis->clock_particle_count = 0;
    vis->clock_particle_spawn_timer = 0.0;
    vis->clock_beat_pulse_outer = 0.0;
    vis->clock_beat_pulse_inner = 0.0;
    vis->clock_hand_glow_intensity = 0.0;
    vis->clock_face_glow = 0.0;
    vis->clock_tick_volume_index = 0;
    vis->clock_show_numbers = TRUE;
    vis->clock_particles_enabled = TRUE;
    
    // Initialize volume history for beat detection
    for (int i = 0; i < 10; i++) {
        vis->clock_tick_volume_history[i] = 0.0;
    }
    
    // Initialize all particles as inactive
    for (int i = 0; i < MAX_CLOCK_PARTICLES; i++) {
        vis->clock_particles[i].active = FALSE;
    }
}

gboolean analog_clock_detect_beat(Visualizer *vis) {
    // Update volume history
    vis->clock_tick_volume_history[vis->clock_tick_volume_index] = vis->volume_level;
    vis->clock_tick_volume_index = (vis->clock_tick_volume_index + 1) % 10;
    
    // Calculate average volume
    double avg_volume = 0.0;
    for (int i = 0; i < 10; i++) {
        avg_volume += vis->clock_tick_volume_history[i];
    }
    avg_volume /= 10.0;
    
    // Beat if current volume is significantly above average
    return (vis->volume_level > avg_volume * 1.4 && vis->volume_level > 0.1);
}

void spawn_clock_particle(Visualizer *vis, double angle, double intensity, int type) {
    // Find inactive particle slot
    int slot = -1;
    for (int i = 0; i < MAX_CLOCK_PARTICLES; i++) {
        if (!vis->clock_particles[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        // Replace oldest particle
        slot = rand() % MAX_CLOCK_PARTICLES;
    }
    
    ClockParticle *particle = &vis->clock_particles[slot];
    
    // Position based on type
    double spawn_radius;
    if (type == 1) { // Hour mark
        spawn_radius = vis->analog_clock_radius * 0.85;
    } else if (type == 2) { // Minute mark  
        spawn_radius = vis->analog_clock_radius * 0.95;
    } else { // Normal particle
        spawn_radius = vis->analog_clock_radius * (0.3 + ((double)rand() / RAND_MAX) * 0.5);
    }
    
    particle->angle = angle;
    particle->radius = spawn_radius;
    particle->x = vis->analog_clock_center_x + cos(angle) * spawn_radius;
    particle->y = vis->analog_clock_center_y + sin(angle) * spawn_radius;
    
    // Movement properties
    particle->angular_velocity = (((double)rand() / RAND_MAX) - 0.5) * intensity * 2.0;
    particle->radial_velocity = (((double)rand() / RAND_MAX) - 0.5) * intensity * 30.0;
    
    // Visual properties
    if (type == 1) { // Hour marks - gold/yellow
        particle->hue = 45.0 + ((double)rand() / RAND_MAX) * 30.0;
        particle->size = 4.0 + intensity * 3.0;
    } else if (type == 2) { // Minute marks - silver/white
        particle->hue = 0.0;
        particle->size = 2.0 + intensity * 2.0;
    } else { // Normal particles - spectrum
        particle->hue = ((double)rand() / RAND_MAX) * 360.0;
        particle->size = 2.0 + intensity * 4.0;
    }
    
    particle->intensity = intensity;
    particle->life = type == 0 ? 1.0 : 2.0; // Mark particles live longer
    particle->pulse_phase = ((double)rand() / RAND_MAX) * 2.0 * M_PI;
    particle->type = type;
    particle->active = TRUE;
    
    if (slot >= vis->clock_particle_count) {
        vis->clock_particle_count = slot + 1;
    }
}

void update_analog_clock(Visualizer *vis, double dt) {
    vis->clock_particle_spawn_timer += dt;
    
    // Update clock dimensions
    vis->analog_clock_center_x = vis->width / 2.0;
    vis->analog_clock_center_y = vis->height / 2.0;
    vis->analog_clock_radius = fmin(vis->width, vis->height) * 0.35;
    
    // Update pulse effects
    if (vis->clock_beat_pulse_outer > 0) {
        vis->clock_beat_pulse_outer -= dt * 2.0;
        if (vis->clock_beat_pulse_outer < 0) vis->clock_beat_pulse_outer = 0;
    }
    
    if (vis->clock_beat_pulse_inner > 0) {
        vis->clock_beat_pulse_inner -= dt * 3.0;
        if (vis->clock_beat_pulse_inner < 0) vis->clock_beat_pulse_inner = 0;
    }
    
    // Update hand glow based on audio
    vis->clock_hand_glow_intensity = vis->volume_level * 0.8;
    vis->clock_face_glow = vis->volume_level * 0.3;
    
    // Detect beats and create effects
    if (analog_clock_detect_beat(vis)) {
        vis->clock_beat_pulse_outer = 1.0;
        vis->clock_beat_pulse_inner = 1.0;
        
        // Spawn particles around clock face on beat
        if (vis->clock_particles_enabled) {
            for (int i = 0; i < 5; i++) {
                double angle = ((double)rand() / RAND_MAX) * 2.0 * M_PI;
                spawn_clock_particle(vis, angle, vis->volume_level, 0);
            }
        }
    }
    
    // Spawn hour mark particles based on frequency bands
    if (vis->clock_particles_enabled && vis->clock_particle_spawn_timer > 0.5) {
        for (int i = 0; i < CLOCK_HOUR_MARKS; i++) {
            int band = (i * VIS_FREQUENCY_BARS) / CLOCK_HOUR_MARKS;
            if (vis->frequency_bands[band] > 0.2) {
                double angle = (double)i / CLOCK_HOUR_MARKS * 2.0 * M_PI - M_PI/2;
                spawn_clock_particle(vis, angle, vis->frequency_bands[band], 1);
            }
        }
        vis->clock_particle_spawn_timer = 0.0;
    }
    
    // Update existing particles
    for (int i = 0; i < vis->clock_particle_count; i++) {
        ClockParticle *particle = &vis->clock_particles[i];
        if (!particle->active) continue;
        
        // Update movement
        particle->angle += particle->angular_velocity * dt;
        particle->radius += particle->radial_velocity * dt;
        particle->pulse_phase += dt * 3.0;
        
        // Update position
        particle->x = vis->analog_clock_center_x + cos(particle->angle) * particle->radius;
        particle->y = vis->analog_clock_center_y + sin(particle->angle) * particle->radius;
        
        // Apply drag
        particle->angular_velocity *= 0.98;
        particle->radial_velocity *= 0.95;
        
        // Decay life
        particle->life -= dt * (particle->type == 0 ? 0.8 : 0.3);
        
        // Remove dead or out-of-bounds particles
        if (particle->life <= 0 || particle->radius > vis->analog_clock_radius * 1.5 || 
            particle->radius < 5.0) {
            particle->active = FALSE;
        }
    }
    
    // Compact particle array
    int write_pos = 0;
    for (int read_pos = 0; read_pos < vis->clock_particle_count; read_pos++) {
        if (vis->clock_particles[read_pos].active) {
            if (write_pos != read_pos) {
                vis->clock_particles[write_pos] = vis->clock_particles[read_pos];
            }
            write_pos++;
        }
    }
    vis->clock_particle_count = write_pos;
}

void draw_clock_face(Visualizer *vis, cairo_t *cr) {
    double center_x = vis->analog_clock_center_x;
    double center_y = vis->analog_clock_center_y;
    double radius = vis->analog_clock_radius;
    
    // Outer ring with audio glow
    double glow_radius = radius + vis->clock_face_glow * 20.0;
    cairo_pattern_t *outer_gradient = cairo_pattern_create_radial(
        center_x, center_y, radius * 0.8,
        center_x, center_y, glow_radius);
    
    cairo_pattern_add_color_stop_rgba(outer_gradient, 0, 0.2, 0.2, 0.3, 0.8);
    cairo_pattern_add_color_stop_rgba(outer_gradient, 0.7, 0.1, 0.1, 0.2, 0.6);
    cairo_pattern_add_color_stop_rgba(outer_gradient, 1, 0.0, 0.0, 0.1, 0.0);
    
    cairo_set_source(cr, outer_gradient);
    cairo_arc(cr, center_x, center_y, glow_radius, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(outer_gradient);
    
    // Main clock face
    cairo_pattern_t *face_gradient = cairo_pattern_create_radial(
        center_x - radius * 0.3, center_y - radius * 0.3, 0,
        center_x, center_y, radius);
    
    cairo_pattern_add_color_stop_rgba(face_gradient, 0, 0.9, 0.9, 0.95, 0.9);
    cairo_pattern_add_color_stop_rgba(face_gradient, 1, 0.6, 0.6, 0.7, 0.95);
    
    cairo_set_source(cr, face_gradient);
    cairo_arc(cr, center_x, center_y, radius, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(face_gradient);
    
    // Beat pulse rings
    if (vis->clock_beat_pulse_outer > 0) {
        double pulse_radius = radius + vis->clock_beat_pulse_outer * 40.0;
        cairo_set_source_rgba(cr, 1.0, 0.8, 0.2, vis->clock_beat_pulse_outer * 0.6);
        cairo_set_line_width(cr, 3.0);
        cairo_arc(cr, center_x, center_y, pulse_radius, 0, 2 * M_PI);
        cairo_stroke(cr);
    }
    
    if (vis->clock_beat_pulse_inner > 0) {
        double pulse_radius = radius * (0.7 + vis->clock_beat_pulse_inner * 0.2);
        cairo_set_source_rgba(cr, 0.2, 0.8, 1.0, vis->clock_beat_pulse_inner * 0.8);
        cairo_set_line_width(cr, 2.0);
        cairo_arc(cr, center_x, center_y, pulse_radius, 0, 2 * M_PI);
        cairo_stroke(cr);
    }
}

void draw_clock_hour_marks(Visualizer *vis, cairo_t *cr) {
    double center_x = vis->analog_clock_center_x;
    double center_y = vis->analog_clock_center_y;
    double radius = vis->analog_clock_radius;
    
    // Hour marks
    for (int i = 0; i < CLOCK_HOUR_MARKS; i++) {
        double angle = (double)i / CLOCK_HOUR_MARKS * 2.0 * M_PI - M_PI/2;
        
        // Get audio intensity for this hour position
        int band = (i * VIS_FREQUENCY_BARS) / CLOCK_HOUR_MARKS;
        double intensity = vis->frequency_bands[band];
        
        double inner_radius = radius * 0.85;
        double outer_radius = radius * 0.95;
        
        double x1 = center_x + cos(angle) * inner_radius;
        double y1 = center_y + sin(angle) * inner_radius;
        double x2 = center_x + cos(angle) * outer_radius;
        double y2 = center_y + sin(angle) * outer_radius;
        
        // Color and thickness based on audio
        double r = 0.2 + intensity * 0.6;
        double g = 0.2 + intensity * 0.4;
        double b = 0.2 + intensity * 0.2;
        double line_width = 2.0 + intensity * 3.0;
        
        cairo_set_source_rgba(cr, r, g, b, 0.9);
        cairo_set_line_width(cr, line_width);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
        
        // Draw numbers if enabled
        if (vis->clock_show_numbers) {
            int hour = (i == 0) ? 12 : i;
            char hour_str[3];
            snprintf(hour_str, sizeof(hour_str), "%d", hour);
            
            double text_radius = radius * 0.75;
            double text_x = center_x + cos(angle) * text_radius;
            double text_y = center_y + sin(angle) * text_radius;
            
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 16 + intensity * 8);
            
            cairo_text_extents_t extents;
            cairo_text_extents(cr, hour_str, &extents);
            
            text_x -= extents.width / 2;
            text_y += extents.height / 2;
            
            cairo_set_source_rgba(cr, 0.1 + intensity * 0.4, 0.1 + intensity * 0.3, 0.1 + intensity * 0.2, 0.9);
            cairo_move_to(cr, text_x, text_y);
            cairo_show_text(cr, hour_str);
        }
    }
    
    // Minute marks (smaller)
    for (int i = 0; i < CLOCK_MINUTE_MARKS; i++) {
        if (i % 5 != 0) { // Skip hour positions
            double angle = (double)i / CLOCK_MINUTE_MARKS * 2.0 * M_PI - M_PI/2;
            
            double inner_radius = radius * 0.90;
            double outer_radius = radius * 0.95;
            
            double x1 = center_x + cos(angle) * inner_radius;
            double y1 = center_y + sin(angle) * inner_radius;
            double x2 = center_x + cos(angle) * outer_radius;
            double y2 = center_y + sin(angle) * outer_radius;
            
            cairo_set_source_rgba(cr, 0.4, 0.4, 0.4, 0.6);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, x1, y1);
            cairo_line_to(cr, x2, y2);
            cairo_stroke(cr);
        }
    }
}

void draw_clock_hands(Visualizer *vis, cairo_t *cr) {
    double center_x = vis->analog_clock_center_x;
    double center_y = vis->analog_clock_center_y;
    double radius = vis->analog_clock_radius;
    
    // Get current time
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    
    // Calculate angles (12 o'clock = -π/2)
    double second_angle = ((double)local_time->tm_sec / 60.0) * 2.0 * M_PI - M_PI/2;
    double minute_angle = ((double)local_time->tm_min / 60.0 + (double)local_time->tm_sec / 3600.0) * 2.0 * M_PI - M_PI/2;
    double hour_angle = ((double)(local_time->tm_hour % 12) / 12.0 + (double)local_time->tm_min / 720.0) * 2.0 * M_PI - M_PI/2;
    
    // Hour hand
    double hour_length = radius * 0.5;
    double hour_x = center_x + cos(hour_angle) * hour_length;
    double hour_y = center_y + sin(hour_angle) * hour_length;
    
    cairo_set_source_rgba(cr, 0.2 + vis->clock_hand_glow_intensity * 0.6, 
                         0.2 + vis->clock_hand_glow_intensity * 0.4, 
                         0.2 + vis->clock_hand_glow_intensity * 0.2, 0.9);
    cairo_set_line_width(cr, 6.0 + vis->clock_hand_glow_intensity * 4.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, center_x, center_y);
    cairo_line_to(cr, hour_x, hour_y);
    cairo_stroke(cr);
    
    // Minute hand
    double minute_length = radius * 0.7;
    double minute_x = center_x + cos(minute_angle) * minute_length;
    double minute_y = center_y + sin(minute_angle) * minute_length;
    
    cairo_set_source_rgba(cr, 0.1 + vis->clock_hand_glow_intensity * 0.7, 
                         0.1 + vis->clock_hand_glow_intensity * 0.5, 
                         0.1 + vis->clock_hand_glow_intensity * 0.3, 0.9);
    cairo_set_line_width(cr, 4.0 + vis->clock_hand_glow_intensity * 3.0);
    cairo_move_to(cr, center_x, center_y);
    cairo_line_to(cr, minute_x, minute_y);
    cairo_stroke(cr);
    
    // Second hand (thinner, red, audio-reactive)
    double second_length = radius * 0.8;
    double second_x = center_x + cos(second_angle) * second_length;
    double second_y = center_y + sin(second_angle) * second_length;
    
    cairo_set_source_rgba(cr, 0.8 + vis->clock_hand_glow_intensity * 0.2, 
                         0.2 * (1.0 - vis->clock_hand_glow_intensity), 
                         0.2 * (1.0 - vis->clock_hand_glow_intensity), 0.9);
    cairo_set_line_width(cr, 2.0 + vis->clock_hand_glow_intensity * 2.0);
    cairo_move_to(cr, center_x, center_y);
    cairo_line_to(cr, second_x, second_y);
    cairo_stroke(cr);
    
    // Center dot
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 0.9);
    cairo_arc(cr, center_x, center_y, 6.0 + vis->clock_hand_glow_intensity * 3.0, 0, 2 * M_PI);
    cairo_fill(cr);
}

void draw_clock_particles(Visualizer *vis, cairo_t *cr) {
    for (int i = 0; i < vis->clock_particle_count; i++) {
        ClockParticle *particle = &vis->clock_particles[i];
        if (!particle->active) continue;
        
        double r, g, b;
        double pulse = 0.8 + 0.2 * sin(particle->pulse_phase);
        
        if (particle->type == 2) { // Minute marks - white/silver
            r = g = b = 0.8 + 0.2 * particle->intensity;
        } else {
            hsv_to_rgb(particle->hue, 0.8, 0.9, &r, &g, &b);
        }
        
        double alpha = particle->life * 0.8 * pulse;
        double size = particle->size * particle->life * pulse;
        
        // Glow effect
        cairo_set_source_rgba(cr, r, g, b, alpha * 0.3);
        cairo_arc(cr, particle->x, particle->y, size * 2.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Main particle
        cairo_set_source_rgba(cr, r, g, b, alpha);
        cairo_arc(cr, particle->x, particle->y, size, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Bright center for hour marks
        if (particle->type == 1) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 0.8, alpha * 0.8);
            cairo_arc(cr, particle->x, particle->y, size * 0.4, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
}

void draw_analog_clock(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    // Draw clock components in order
    draw_clock_face(vis, cr);
    draw_clock_hour_marks(vis, cr);
    
    if (vis->clock_particles_enabled) {
        draw_clock_particles(vis, cr);
    }
    
    draw_clock_hands(vis, cr);
}
