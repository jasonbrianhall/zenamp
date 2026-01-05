#include "rainbow.h"
#include <stdlib.h>
#include <string.h>

// Initialize the rainbow system
void init_rainbow_system(RainbowSystem *rainbow) {
    memset(rainbow, 0, sizeof(RainbowSystem));
    rainbow->particle_count = 0;
    rainbow->wave_count = 0;
    rainbow->global_hue_offset = 0.0;
    rainbow->time_elapsed = 0.0;
    rainbow->spawn_timer = 0.0;
    rainbow->background_glow = 0.3;
    rainbow->audio_reactive = TRUE;
    rainbow->mouse_interactive = TRUE;
    rainbow->interaction_intensity = 1.0;
    rainbow->particle_shape_mode = 0; // All shapes
    rainbow->random_spawn_locations = TRUE;  // NEW: Enable random spawning!
    rainbow->spawn_waves_randomly = TRUE;    // NEW: Random wave locations!
    
    rainbow->vortex.x = 0;
    rainbow->vortex.y = 0;
    rainbow->vortex.magnitude = 0;
    rainbow->vortex.frequency = 2.0;
    rainbow->vortex.active = FALSE;
}

// Convert HSV to RGB
void hsv_to_rgb_rainbow(double h, double s, double v, double *r, double *g, double *b) {
    // Normalize h to 0-1 range
    h = fmod(h, 1.0);
    if (h < 0) h += 1.0;
    
    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h * 6.0, 2.0) - 1.0));
    double m = v - c;
    
    double r1, g1, b1;
    
    if (h < 1.0/6.0) {
        r1 = c; g1 = x; b1 = 0;
    } else if (h < 2.0/6.0) {
        r1 = x; g1 = c; b1 = 0;
    } else if (h < 3.0/6.0) {
        r1 = 0; g1 = c; b1 = x;
    } else if (h < 4.0/6.0) {
        r1 = 0; g1 = x; b1 = c;
    } else if (h < 5.0/6.0) {
        r1 = x; g1 = 0; b1 = c;
    } else {
        r1 = c; g1 = 0; b1 = x;
    }
    
    *r = r1 + m;
    *g = g1 + m;
    *b = b1 + m;
}

// Spawn a particle
void spawn_rainbow_particle(RainbowSystem *rainbow, double x, double y, 
                           double vx, double vy, double hue, int shape) {
    if (rainbow->particle_count >= MAX_RAINBOW_PARTICLES) return;
    
    RainbowParticle *p = &rainbow->particles[rainbow->particle_count];
    p->x = x;
    p->y = y;
    p->vx = vx + (rand() % 100 - 50) * 0.01; // Add randomness
    p->vy = vy + (rand() % 100 - 50) * 0.01;
    p->life = 1.0;
    p->hue = hue;
    p->size = 8.0 + (rand() % 50) * 0.3;  // Much larger particles (8.0 to 23.0)
    p->rotation = (rand() % 360) * M_PI / 180.0;
    p->trail_length = 5.0 + (rand() % 20) * 0.5;
    p->shape = shape % 3;
    p->active = TRUE;
    
    rainbow->particle_count++;
}

// Spawn a particle from a random screen location
void spawn_rainbow_particle_random_location(RainbowSystem *rainbow, double width, double height,
                                           double hue, int shape, double speed) {
    if (rainbow->particle_count >= MAX_RAINBOW_PARTICLES) return;
    
    // Pick random spawn location type
    int spawn_type = rand() % 4;
    double spawn_x, spawn_y, target_x, target_y;
    
    switch (spawn_type) {
        case 0: // Top edge
            spawn_x = (rand() % (int)width);
            spawn_y = -10;
            target_x = width / 2.0;
            target_y = height / 2.0;
            break;
        case 1: // Bottom edge
            spawn_x = (rand() % (int)width);
            spawn_y = height + 10;
            target_x = width / 2.0;
            target_y = height / 2.0;
            break;
        case 2: // Left edge
            spawn_x = -10;
            spawn_y = (rand() % (int)height);
            target_x = width / 2.0;
            target_y = height / 2.0;
            break;
        case 3: // Right edge
            spawn_x = width + 10;
            spawn_y = (rand() % (int)height);
            target_x = width / 2.0;
            target_y = height / 2.0;
            break;
    }
    
    // Calculate direction to center
    double dx = target_x - spawn_x;
    double dy = target_y - spawn_y;
    double dist = sqrt(dx*dx + dy*dy);
    
    double vx = (dist > 0) ? (dx / dist) * speed : 0;
    double vy = (dist > 0) ? (dy / dist) * speed : 0;
    
    spawn_rainbow_particle(rainbow, spawn_x, spawn_y, vx, vy, hue, shape);
}

// Spawn a wave at a random location
void spawn_rainbow_wave_random_location(RainbowSystem *rainbow, double width, double height,
                                       double hue) {
    if (rainbow->wave_count >= MAX_RAINBOW_WAVES) return;
    
    // Pick random location on screen
    double x = (rand() % (int)width);
    double y = (rand() % (int)height);
    
    spawn_rainbow_wave(rainbow, x, y, hue);
}

// Spawn a wave
void spawn_rainbow_wave(RainbowSystem *rainbow, double x, double y, double hue) {
    if (rainbow->wave_count >= MAX_RAINBOW_WAVES) return;
    
    RainbowWave *w = &rainbow->waves[rainbow->wave_count];
    w->x = x;
    w->y = y;
    w->radius = 0.0;
    w->max_radius = 200.0;
    w->life = 1.0;
    w->hue_start = hue;
    w->thickness = 3.0;
    w->intensity = 1.0;
    w->active = TRUE;
    
    rainbow->wave_count++;
}

// Update the rainbow system
void update_rainbow_system(RainbowSystem *rainbow, double dt, double audio_level,
                          double mouse_x, double mouse_y, gboolean mouse_active) {
    rainbow->time_elapsed += dt;
    rainbow->global_hue_offset = fmod(rainbow->time_elapsed * 0.1, 1.0);
    rainbow->last_audio_level = audio_level;
    
    // Update particles
    int active_count = 0;
    for (int i = 0; i < MAX_RAINBOW_PARTICLES; i++) {
        RainbowParticle *p = &rainbow->particles[i];
        if (!p->active) continue;
        
        // Apply gravity
        p->vy += 50.0 * dt;
        
        // Update position
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        
        // Rotation
        p->rotation += 2.0 * dt;
        
        // Decay life (slower decay for longer lifetime)
        p->life -= dt * 0.2;  // Particles last ~5 seconds instead of 2
        
        if (p->life <= 0) {
            p->active = FALSE;
        } else {
            active_count++;
        }
    }
    rainbow->particle_count = active_count;
    
    // Update waves
    int wave_count = 0;
    for (int i = 0; i < MAX_RAINBOW_WAVES; i++) {
        RainbowWave *w = &rainbow->waves[i];
        if (!w->active) continue;
        
        w->radius += 200.0 * dt;
        w->life -= dt * 0.8;
        w->thickness -= dt * 10.0;
        
        if (w->life <= 0 || w->radius >= w->max_radius) {
            w->active = FALSE;
        } else {
            wave_count++;
        }
    }
    rainbow->wave_count = wave_count;
    
    // Audio-reactive particle spawning
    if (rainbow->audio_reactive && audio_level > 0.1) {
        rainbow->spawn_timer += dt;
        double spawn_rate = 0.008 / (0.1 + audio_level);  // Spawn 2.5x more frequently
        
        while (rainbow->spawn_timer > spawn_rate) {
            double hue = fmod(rainbow->global_hue_offset + (rand() % 100) * 0.01, 1.0);
            double speed = 50.0 + audio_level * 300.0;
            
            if (rainbow->random_spawn_locations && rainbow->screen_width > 0) {
                // Spawn from random screen edges, moving toward center
                spawn_rainbow_particle_random_location(rainbow,
                                                      rainbow->screen_width,
                                                      rainbow->screen_height,
                                                      hue,
                                                      rand() % 3,
                                                      speed);
            } else {
                // Original behavior: spawn from center
                double angle = (rand() % 360) * M_PI / 180.0;
                spawn_rainbow_particle(rainbow, 
                                      rainbow->vortex.base_x, 
                                      rainbow->vortex.base_y,
                                      cos(angle) * speed,
                                      sin(angle) * speed * 0.5,
                                      hue,
                                      rand() % 3);
            }
            
            rainbow->spawn_timer -= spawn_rate;
        }
    }
    
    // Mouse interaction
    if (rainbow->mouse_interactive && mouse_active) {
        for (int i = 0; i < 12; i++) {  // Increased from 5 to 12 particles
            double angle = i * 2.0 * M_PI / 12.0;
            double speed = 150.0;
            double hue = fmod(rainbow->global_hue_offset + i * 0.2, 1.0);
            
            spawn_rainbow_particle(rainbow,
                                  mouse_x, mouse_y,
                                  cos(angle) * speed,
                                  sin(angle) * speed,
                                  hue,
                                  rand() % 3);
        }
    }
    
    // Vortex decay
    if (rainbow->vortex.active) {
        rainbow->vortex.magnitude *= 0.95;
        if (rainbow->vortex.magnitude < 0.01) {
            rainbow->vortex.active = FALSE;
        }
    }
    
    // Update vortex influence on particles
    if (rainbow->vortex.active && rainbow->vortex.magnitude > 0.1) {
        for (int i = 0; i < MAX_RAINBOW_PARTICLES; i++) {
            RainbowParticle *p = &rainbow->particles[i];
            if (!p->active) continue;
            
            double dx = p->x - rainbow->vortex.x;
            double dy = p->y - rainbow->vortex.y;
            double dist = sqrt(dx*dx + dy*dy);
            
            if (dist < 300.0) {
                // Circular motion around vortex
                double force = rainbow->vortex.magnitude * (1.0 - dist / 300.0);
                double angle = atan2(dy, dx);
                p->vx += cos(angle + M_PI/2) * force * 100 * dt;
                p->vy += sin(angle + M_PI/2) * force * 100 * dt;
            }
        }
    }
}

// Draw a single particle
void draw_rainbow_particle(cairo_t *cr, RainbowParticle *p, double alpha) {
    cairo_save(cr);
    cairo_translate(cr, p->x, p->y);
    cairo_rotate(cr, p->rotation);
    
    double r, g, b;
    hsv_to_rgb_rainbow(p->hue, 1.0, 1.0, &r, &g, &b);
    
    alpha *= p->life;
    cairo_set_source_rgba(cr, r, g, b, alpha);
    
    switch (p->shape) {
        case 0: { // Circle
            cairo_arc(cr, 0, 0, p->size, 0, 2*M_PI);
            cairo_fill(cr);
            break;
        }
        case 1: { // Square
            double half = p->size;
            cairo_rectangle(cr, -half, -half, half*2, half*2);
            cairo_fill(cr);
            break;
        }
        case 2: { // Star
            for (int i = 0; i < 5; i++) {
                double angle = i * 2 * M_PI / 5.0;
                double x = cos(angle) * p->size;
                double y = sin(angle) * p->size;
                if (i == 0) {
                    cairo_move_to(cr, x, y);
                } else {
                    cairo_line_to(cr, x, y);
                }
            }
            cairo_close_path(cr);
            cairo_fill(cr);
            break;
        }
    }
    
    cairo_restore(cr);
}

// Draw the rainbow system
void draw_rainbow_system(cairo_t *cr, RainbowSystem *rainbow, 
                        int width, int height) {
    // Store screen dimensions for random spawning
    rainbow->screen_width = width;
    rainbow->screen_height = height;
    
    // Set vortex to center
    rainbow->vortex.base_x = width / 2.0;
    rainbow->vortex.base_y = height / 2.0;
    
    // Draw background gradient
    cairo_pattern_t *pattern = cairo_pattern_create_radial(
        width/2, height/2, 0,
        width/2, height/2, sqrt(width*width + height*height) / 2);
    
    double bg_hue = rainbow->global_hue_offset;
    double r1, g1, b1, r2, g2, b2;
    hsv_to_rgb_rainbow(bg_hue, 0.4, 0.15, &r1, &g1, &b1);
    hsv_to_rgb_rainbow(fmod(bg_hue + 0.5, 1.0), 0.4, 0.1, &r2, &g2, &b2);
    
    cairo_pattern_add_color_stop_rgba(pattern, 0.0, r1, g1, b1, 1.0);
    cairo_pattern_add_color_stop_rgba(pattern, 1.0, r2, g2, b2, 1.0);
    cairo_set_source(cr, pattern);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    cairo_pattern_destroy(pattern);
    
    // Draw waves
    for (int i = 0; i < MAX_RAINBOW_WAVES; i++) {
        RainbowWave *w = &rainbow->waves[i];
        if (!w->active) continue;
        
        double wave_r, wave_g, wave_b;
        hsv_to_rgb_rainbow(fmod(w->hue_start + rainbow->global_hue_offset, 1.0), 
                  1.0, 1.0, &wave_r, &wave_g, &wave_b);
        
        cairo_set_source_rgba(cr, wave_r, wave_g, wave_b, w->life * 0.8);
        cairo_set_line_width(cr, w->thickness);
        cairo_arc(cr, w->x, w->y, w->radius, 0, 2*M_PI);
        cairo_stroke(cr);
    }
    
    // Draw particles
    for (int i = 0; i < MAX_RAINBOW_PARTICLES; i++) {
        RainbowParticle *p = &rainbow->particles[i];
        if (!p->active) continue;
        draw_rainbow_particle(cr, p, 1.0);
    }
    
    // Draw vortex effect if active
    if (rainbow->vortex.active && rainbow->vortex.magnitude > 0.1) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.3 * rainbow->vortex.magnitude);
        cairo_arc(cr, rainbow->vortex.x, rainbow->vortex.y, 50, 0, 2*M_PI);
        cairo_fill(cr);
    }
    
    // Draw instruction text (for interactive mode)
    if (rainbow->mouse_interactive) {
        cairo_set_font_size(cr, 14);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
        cairo_move_to(cr, 10, height - 10);
        cairo_show_text(cr, "Click to spawn rainbow particles");
    }
}

// Handle mouse click
void rainbow_on_mouse_click(RainbowSystem *rainbow, double x, double y) {
    if (!rainbow->mouse_interactive) return;
    
    // Spawn wave at click location
    spawn_rainbow_wave(rainbow, x, y, rainbow->global_hue_offset);
    
    // Spawn particles around click (increased from 8 to 20)
    for (int i = 0; i < 20; i++) {
        double angle = i * 2.0 * M_PI / 20.0;
        double speed = 200.0;
        double hue = fmod(rainbow->global_hue_offset + i * 0.05, 1.0);
        
        spawn_rainbow_particle(rainbow, x, y,
                              cos(angle) * speed,
                              sin(angle) * speed,
                              hue, rand() % 3);
    }
}

// Handle scroll to create vortex
void rainbow_on_scroll(RainbowSystem *rainbow, double x, double y, int direction) {
    rainbow->vortex.x = x;
    rainbow->vortex.y = y;
    rainbow->vortex.magnitude = (direction > 0) ? 1.0 : -1.0;
    rainbow->vortex.active = TRUE;
}

// Cleanup
void cleanup_rainbow_system(RainbowSystem *rainbow) {
    memset(rainbow, 0, sizeof(RainbowSystem));
}
