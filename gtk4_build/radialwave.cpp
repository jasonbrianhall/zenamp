#include "visualization.h"

void init_radial_wave_system(void *vis_ptr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    
    // Initialize radial wave system
    vis->radial_wave.ring_rotation = 0.0;
    vis->radial_wave.ring_radius = 0.0; // Will be set based on window size
    vis->radial_wave.ripple_phase = 0.0;
    
    // Initialize all segment bulges
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        vis->radial_wave.segment_bulge[i] = 0.0;
    }
    
    // Initialize pulsar core
    vis->radial_wave.core.scale = 1.0;
    vis->radial_wave.core.rotation = 0.0;
    vis->radial_wave.core.glow_intensity = 0.0;
    vis->radial_wave.core.fracture_amount = 0.0;
    vis->radial_wave.core.color_shift = 0.0;
    vis->radial_wave.core.particle_count = 0;
    vis->radial_wave.core.particle_spawn_timer = 0.0;
    
    // Initialize beat detection history
    vis->radial_beat_history_index = 0;
    for (int i = 0; i < 10; i++) {
        vis->radial_beat_volume_history[i] = 0.0;
    }
}

gboolean radial_wave_detect_beat(void *vis_ptr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    
    // Store current volume in history
    vis->radial_beat_volume_history[vis->radial_beat_history_index] = vis->volume_level;
    vis->radial_beat_history_index = (vis->radial_beat_history_index + 1) % 10;
    
    // Calculate average of last 10 frames
    double avg_volume = 0.0;
    for (int i = 0; i < 10; i++) {
        avg_volume += vis->radial_beat_volume_history[i];
    }
    avg_volume /= 10.0;
    
    // Beat detected if current volume significantly exceeds average
    double threshold = avg_volume * 1.5;
    return (vis->volume_level > threshold && vis->volume_level > 0.15);
}

void update_radial_wave(void *vis_ptr, double dt) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    RadialWaveSystem *rw = &vis->radial_wave;
    
    // Update ring rotation (slow continuous rotation)
    rw->ring_rotation += dt * 0.3;
    if (rw->ring_rotation > 2.0 * M_PI) {
        rw->ring_rotation -= 2.0 * M_PI;
    }
    
    // Update ripple phase
    rw->ripple_phase += dt * 2.0;
    if (rw->ripple_phase > 2.0 * M_PI) {
        rw->ripple_phase -= 2.0 * M_PI;
    }
    
    // Update segment bulges based on frequency data
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        double target_bulge = vis->frequency_bands[i] * 0.4;
        rw->segment_bulge[i] = rw->segment_bulge[i] * 0.85 + target_bulge * 0.15;
    }
    
    // Detect beats and update pulsar core
    gboolean beat_detected = radial_wave_detect_beat(vis);
    
    if (beat_detected) {
        update_pulsar_beat(vis);
        spawn_star_particle(vis, vis->volume_level);
    }
    
    // Update pulsar core animation
    PulsarCore *core = &rw->core;
    
    // Scale pulsates continuously with subtle breathing
    double target_scale = 1.0 + sin(rw->ripple_phase) * 0.05;
    core->scale = core->scale * 0.9 + target_scale * 0.1;
    
    // Rotate star slowly
    core->rotation += dt * 0.5;
    if (core->rotation > 2.0 * M_PI) {
        core->rotation -= 2.0 * M_PI;
    }
    
    // Decay glow and fracture
    core->glow_intensity *= 0.95;
    core->fracture_amount *= 0.93;
    
    // Update color shift based on frequency spectrum
    double bass_energy = 0.0, treble_energy = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS / 3; i++) {
        bass_energy += vis->frequency_bands[i];
    }
    for (int i = VIS_FREQUENCY_BARS * 2 / 3; i < VIS_FREQUENCY_BARS; i++) {
        treble_energy += vis->frequency_bands[i];
    }
    
    // Shift from blue (bass heavy) to red (treble heavy)
    double target_shift = (treble_energy - bass_energy) / 2.0;
    core->color_shift = core->color_shift * 0.95 + target_shift * 0.05;
    
    // Update particles
    core->particle_spawn_timer += dt;
    
    for (int i = 0; i < core->particle_count; i++) {
        StarParticle *p = &core->particles[i];
        
        // Update particle position
        p->angle += p->velocity * dt * 0.5;
        p->distance += dt * 30.0;
        
        // Decay life
        p->life -= dt * 0.8;
        
        // Remove dead particles
        if (p->life <= 0.0) {
            core->particles[i] = core->particles[core->particle_count - 1];
            core->particle_count--;
            i--;
        }
    }
}

void update_pulsar_beat(void *vis_ptr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    PulsarCore *core = &vis->radial_wave.core;
    
    // Strong pulse on beat
    core->scale += 0.3;
    if (core->scale > 1.8) core->scale = 1.8;
    
    // Increase glow
    core->glow_intensity = 1.0;
    
    // Check for strong bass (potential drop)
    double bass_energy = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS / 4; i++) {
        bass_energy += vis->frequency_bands[i];
    }
    
    if (bass_energy > 0.6) {
        core->fracture_amount = 1.0;
    }
}

void spawn_star_particle(void *vis_ptr, double intensity) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    PulsarCore *core = &vis->radial_wave.core;
    
    if (core->particle_count >= MAX_STAR_PARTICLES) return;
    
    StarParticle *p = &core->particles[core->particle_count++];
    
    // Random angle
    p->angle = ((double)rand() / RAND_MAX) * 2.0 * M_PI;
    p->distance = 20.0;
    p->velocity = 1.0 + ((double)rand() / RAND_MAX) * 2.0;
    p->life = 1.0 + intensity * 2.0;
    p->size = 2.0 + intensity * 4.0;
    
    // Color based on core's color shift
    double hue = 0.5 + core->color_shift * 0.3;
    if (hue < 0.0) hue += 1.0;
    if (hue > 1.0) hue -= 1.0;
    
    // Convert HSV to RGB
    double h = hue * 360.0;
    double s = 0.8;
    double v = 0.9;
    
    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;
    
    if (h >= 0 && h < 60) {
        p->r = c + m; p->g = x + m; p->b = m;
    } else if (h >= 60 && h < 120) {
        p->r = x + m; p->g = c + m; p->b = m;
    } else if (h >= 120 && h < 180) {
        p->r = m; p->g = c + m; p->b = x + m;
    } else if (h >= 180 && h < 240) {
        p->r = m; p->g = x + m; p->b = c + m;
    } else if (h >= 240 && h < 300) {
        p->r = x + m; p->g = m; p->b = c + m;
    } else {
        p->r = c + m; p->g = m; p->b = x + m;
    }
}

void draw_radial_wave(void *vis_ptr, cairo_t *cr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    
    // Set base ring radius based on screen size
    double max_radius = fmin(center_x, center_y) * 0.85;
    vis->radial_wave.ring_radius = max_radius * 0.7;
    
    // Draw background linear waveform first (behind everything)
    draw_background_waveform(vis, cr);
    
    // Draw radial ring
    draw_radial_ring(vis, cr, center_x, center_y);
    
    // Draw pulsar core
    draw_pulsar_core(vis, cr, center_x, center_y);
}

void draw_background_waveform(void *vis_ptr, cairo_t *cr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    
    if (vis->width <= 0 || vis->height <= 0) return;
    
    // Draw a nearly transparent linear waveform across the entire width
    cairo_set_source_rgba(cr, vis->fg_r, vis->fg_g, vis->fg_b, 0.15);
    cairo_set_line_width(cr, 1.5);
    
    cairo_move_to(cr, 0, vis->height / 2.0);
    
    for (int i = 0; i < VIS_SAMPLES; i++) {
        double x = (double)i * vis->width / (VIS_SAMPLES - 1);
        double y = vis->height / 2.0 + vis->audio_samples[i] * vis->height / 2.5;
        
        // Clamp y to screen bounds
        if (y < 0) y = 0;
        if (y > vis->height) y = vis->height;
        
        if (i == 0) {
            cairo_move_to(cr, x, y);
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    
    cairo_stroke(cr);
    
    // Add a second phase-shifted waveform for subtle depth
    cairo_set_source_rgba(cr, vis->accent_r, vis->accent_g, vis->accent_b, 0.08);
    cairo_set_line_width(cr, 1.0);
    
    cairo_move_to(cr, 0, vis->height / 2.0);
    
    for (int i = 0; i < VIS_SAMPLES; i++) {
        double x = (double)i * vis->width / (VIS_SAMPLES - 1);
        double phase_shifted = i < VIS_SAMPLES - 10 ? vis->audio_samples[i + 10] : 0.0;
        double y = vis->height / 2.0 + phase_shifted * vis->height / 3.0;
        
        // Clamp y to screen bounds
        if (y < 0) y = 0;
        if (y > vis->height) y = vis->height;
        
        if (i == 0) {
            cairo_move_to(cr, x, y);
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    
    cairo_stroke(cr);
}

void draw_radial_ring(void *vis_ptr, cairo_t *cr, double center_x, double center_y) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    RadialWaveSystem *rw = &vis->radial_wave;
    
    double base_radius = rw->ring_radius;
    int num_points = VIS_FREQUENCY_BARS * 4; // More points for smoother curve
    
    // Draw main waveform ring with variable line width
    for (int i = 0; i < num_points; i++) {
        double angle = (double)i / num_points * 2.0 * M_PI + rw->ring_rotation;
        double next_angle = (double)(i + 1) / num_points * 2.0 * M_PI + rw->ring_rotation;
        
        // Find which frequency band this point belongs to
        int band = ((i % num_points) * VIS_FREQUENCY_BARS) / num_points;
        if (band >= VIS_FREQUENCY_BARS) band = VIS_FREQUENCY_BARS - 1;
        
        // Interpolate between adjacent bands for smoothness
        double band_fraction = ((double)(i % num_points) * VIS_FREQUENCY_BARS) / num_points - band;
        int next_band = (band + 1) % VIS_FREQUENCY_BARS;
        
        double bulge = rw->segment_bulge[band] * (1.0 - band_fraction) + 
                       rw->segment_bulge[next_band] * band_fraction;
        
        double next_bulge = rw->segment_bulge[next_band];
        
        // Add ripple effect
        double ripple = sin(rw->ripple_phase + angle * 3.0) * 0.08;
        double next_ripple = sin(rw->ripple_phase + next_angle * 3.0) * 0.08;
        
        double radius = base_radius + bulge * 100.0 + ripple * 15.0;
        double next_radius = base_radius + next_bulge * 100.0 + next_ripple * 15.0;
        
        double x = center_x + cos(angle) * radius;
        double y = center_y + sin(angle) * radius;
        double next_x = center_x + cos(next_angle) * next_radius;
        double next_y = center_y + sin(next_angle) * next_radius;
        
        // Vary line width based on amplitude
        double line_width = 1.5 + bulge * 4.0;
        
        // Vary color intensity based on amplitude
        double intensity = 0.7 + bulge * 0.3;
        
        // Draw segment with gradient
        cairo_set_line_width(cr, line_width);
        cairo_set_source_rgba(cr, 
            vis->accent_r * intensity, 
            vis->accent_g * intensity, 
            vis->accent_b * intensity, 
            0.85 + bulge * 0.15);
        
        cairo_move_to(cr, x, y);
        cairo_line_to(cr, next_x, next_y);
        cairo_stroke(cr);
    }
    
    // Draw inner glow layer with pulsing effect
    cairo_new_path(cr);
    
    for (int i = 0; i <= num_points; i++) {
        double angle = (double)i / num_points * 2.0 * M_PI + rw->ring_rotation;
        
        int band = ((i % num_points) * VIS_FREQUENCY_BARS) / num_points;
        if (band >= VIS_FREQUENCY_BARS) band = VIS_FREQUENCY_BARS - 1;
        
        double band_fraction = ((double)(i % num_points) * VIS_FREQUENCY_BARS) / num_points - band;
        int next_band = (band + 1) % VIS_FREQUENCY_BARS;
        
        double bulge = rw->segment_bulge[band] * (1.0 - band_fraction) + 
                       rw->segment_bulge[next_band] * band_fraction;
        
        double ripple = sin(rw->ripple_phase + angle * 3.0) * 0.08;
        
        double radius = base_radius + bulge * 100.0 + ripple * 15.0;
        
        double x = center_x + cos(angle) * radius;
        double y = center_y + sin(angle) * radius;
        
        if (i == 0) {
            cairo_move_to(cr, x, y);
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    
    cairo_close_path(cr);
    cairo_set_line_width(cr, 3.0);
    cairo_set_source_rgba(cr, vis->fg_r, vis->fg_g, vis->fg_b, 0.4);
    cairo_stroke(cr);
    
    // Draw secondary phase-shifted waveform for depth with animated width
    for (int i = 0; i < num_points; i++) {
        double angle = (double)i / num_points * 2.0 * M_PI + rw->ring_rotation;
        double next_angle = (double)(i + 1) / num_points * 2.0 * M_PI + rw->ring_rotation;
        
        int band = ((i % num_points) * VIS_FREQUENCY_BARS) / num_points;
        if (band >= VIS_FREQUENCY_BARS) band = VIS_FREQUENCY_BARS - 1;
        
        double band_fraction = ((double)(i % num_points) * VIS_FREQUENCY_BARS) / num_points - band;
        int next_band = (band + 1) % VIS_FREQUENCY_BARS;
        
        // Phase shift for secondary wave
        int shifted_band = (band + VIS_FREQUENCY_BARS / 8) % VIS_FREQUENCY_BARS;
        int shifted_next = (next_band + VIS_FREQUENCY_BARS / 8) % VIS_FREQUENCY_BARS;
        
        double bulge = rw->segment_bulge[shifted_band] * (1.0 - band_fraction) + 
                       rw->segment_bulge[shifted_next] * band_fraction;
        
        double ripple = sin(rw->ripple_phase + angle * 3.0 + M_PI / 4.0) * 0.06;
        double next_ripple = sin(rw->ripple_phase + next_angle * 3.0 + M_PI / 4.0) * 0.06;
        
        double radius = base_radius * 0.85 + bulge * 70.0 + ripple * 12.0;
        double next_radius = base_radius * 0.85 + bulge * 70.0 + next_ripple * 12.0;
        
        double x = center_x + cos(angle) * radius;
        double y = center_y + sin(angle) * radius;
        double next_x = center_x + cos(next_angle) * next_radius;
        double next_y = center_y + sin(next_angle) * next_radius;
        
        // Animated line width
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
}

void draw_pulsar_core(void *vis_ptr, cairo_t *cr, double center_x, double center_y) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    PulsarCore *core = &vis->radial_wave.core;
    
    // Calculate star color based on color shift
    double base_hue = 0.55 + core->color_shift * 0.3; // Blue to red shift
    if (base_hue < 0.0) base_hue += 1.0;
    if (base_hue > 1.0) base_hue -= 1.0;
    
    double h = base_hue * 360.0;
    double s = 0.9;
    double v = 0.95;
    
    // Convert HSV to RGB
    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;
    
    double star_r, star_g, star_b;
    if (h >= 0 && h < 60) {
        star_r = c + m; star_g = x + m; star_b = m;
    } else if (h >= 60 && h < 120) {
        star_r = x + m; star_g = c + m; star_b = m;
    } else if (h >= 120 && h < 180) {
        star_r = m; star_g = c + m; star_b = x + m;
    } else if (h >= 180 && h < 240) {
        star_r = m; star_g = x + m; star_b = c + m;
    } else if (h >= 240 && h < 300) {
        star_r = x + m; star_g = m; star_b = c + m;
    } else {
        star_r = c + m; star_g = m; star_b = x + m;
    }
    
    // Draw glow halo
    if (core->glow_intensity > 0.01) {
        double glow_radius = 60.0 * core->scale * (1.0 + core->glow_intensity * 0.5);
        cairo_pattern_t *glow = cairo_pattern_create_radial(
            center_x, center_y, 0,
            center_x, center_y, glow_radius
        );
        cairo_pattern_add_color_stop_rgba(glow, 0, star_r, star_g, star_b, core->glow_intensity * 0.6);
        cairo_pattern_add_color_stop_rgba(glow, 1, star_r, star_g, star_b, 0);
        cairo_set_source(cr, glow);
        cairo_arc(cr, center_x, center_y, glow_radius, 0, 2.0 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(glow);
    }
    
    // Draw star shape
    double star_size = 50.0 * core->scale;
    double inner_radius = star_size * 0.4;
    double outer_radius = star_size;
    
    // Apply fracture effect
    double fracture_offset = core->fracture_amount * 10.0;
    
    cairo_save(cr);
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, core->rotation);
    
    // Draw multiple star layers for depth
    for (int layer = 0; layer < 2; layer++) {
        double layer_scale = 1.0 - layer * 0.3;
        double layer_alpha = 1.0 - layer * 0.4;
        
        cairo_new_path(cr);
        
        for (int i = 0; i <= STAR_POINTS * 2; i++) {
            double angle = (double)i / (STAR_POINTS * 2.0) * 2.0 * M_PI;
            double radius = (i % 2 == 0) ? outer_radius : inner_radius;
            radius *= layer_scale;
            
            // Add fracture displacement
            double fx = ((double)rand() / RAND_MAX - 0.5) * fracture_offset;
            double fy = ((double)rand() / RAND_MAX - 0.5) * fracture_offset;
            
            double x = cos(angle) * radius + fx;
            double y = sin(angle) * radius + fy;
            
            if (i == 0) {
                cairo_move_to(cr, x, y);
            } else {
                cairo_line_to(cr, x, y);
            }
        }
        
        cairo_close_path(cr);
        
        // Fill with gradient
        cairo_pattern_t *star_grad = cairo_pattern_create_radial(0, 0, 0, 0, 0, outer_radius * layer_scale);
        cairo_pattern_add_color_stop_rgba(star_grad, 0, 1.0, 1.0, 1.0, layer_alpha);
        cairo_pattern_add_color_stop_rgba(star_grad, 0.5, star_r, star_g, star_b, layer_alpha * 0.9);
        cairo_pattern_add_color_stop_rgba(star_grad, 1.0, star_r * 0.5, star_g * 0.5, star_b * 0.5, layer_alpha * 0.7);
        cairo_set_source(cr, star_grad);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(star_grad);
        
        // Outline
        cairo_set_source_rgba(cr, star_r * 1.2, star_g * 1.2, star_b * 1.2, layer_alpha);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
    }
    
    cairo_restore(cr);
    
    // Draw particles
    for (int i = 0; i < core->particle_count; i++) {
        StarParticle *p = &core->particles[i];
        
        double px = center_x + cos(p->angle) * p->distance;
        double py = center_y + sin(p->angle) * p->distance;
        
        double alpha = p->life;
        if (alpha > 1.0) alpha = 1.0;
        
        // Particle glow
        cairo_pattern_t *p_glow = cairo_pattern_create_radial(px, py, 0, px, py, p->size * 2);
        cairo_pattern_add_color_stop_rgba(p_glow, 0, p->r, p->g, p->b, alpha * 0.8);
        cairo_pattern_add_color_stop_rgba(p_glow, 1, p->r, p->g, p->b, 0);
        cairo_set_source(cr, p_glow);
        cairo_arc(cr, px, py, p->size * 2, 0, 2.0 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(p_glow);
        
        // Particle core
        cairo_set_source_rgba(cr, p->r * 1.3, p->g * 1.3, p->b * 1.3, alpha);
        cairo_arc(cr, px, py, p->size, 0, 2.0 * M_PI);
        cairo_fill(cr);
    }
}
