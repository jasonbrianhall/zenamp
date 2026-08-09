#include "rainbow.h"
#include "visualization.h"
#include <stdlib.h>
#include <string.h>

// Initialize the rainbow system
void init_rainbow_system(Visualizer *vis) {
    RainbowSystem *rainbow = &vis->rainbow_system;
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
    rainbow->random_spawn_locations = TRUE;  // Enable random spawning
    rainbow->spawn_waves_randomly = TRUE;    // Random wave locations
    rainbow->last_left_click = FALSE;
    rainbow->last_right_click = FALSE;
    
    rainbow->vortex.x = 0;
    rainbow->vortex.y = 0;
    rainbow->vortex.magnitude = 0;
    rainbow->vortex.frequency = 2.0;
    rainbow->vortex.active = FALSE;
    rainbow->vortex.effect_time = 0.0;
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
void update_rainbow_system(Visualizer *vis, double dt) {
    RainbowSystem *rainbow = &vis->rainbow_system;
    double audio_level = vis->volume_level;
    int mouse_x = vis->mouse_x;
    int mouse_y = vis->mouse_y;
    gboolean mouse_left = vis->mouse_left_pressed;
    gboolean mouse_right = vis->mouse_right_pressed;
    
    // Update screen dimensions for random spawning
    rainbow->screen_width = vis->width;
    rainbow->screen_height = vis->height;
    
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
    
    // Left mouse button: spawn particles from click location (on press, not continuously)
    if (rainbow->mouse_interactive && mouse_left && !rainbow->last_left_click) {
        // Spawn wave at random location
        if (rainbow->spawn_waves_randomly && rainbow->screen_width > 0) {
            spawn_rainbow_wave_random_location(rainbow,
                                              rainbow->screen_width,
                                              rainbow->screen_height,
                                              rainbow->global_hue_offset);
        } else {
            spawn_rainbow_wave(rainbow, mouse_x, mouse_y, rainbow->global_hue_offset);
        }
        
        // Spawn particles from click location in circular pattern
        for (int i = 0; i < 20; i++) {
            double angle = i * 2.0 * M_PI / 20.0;
            double speed = 200.0;
            double hue = fmod(rainbow->global_hue_offset + i * 0.05, 1.0);
            
            spawn_rainbow_particle(rainbow, mouse_x, mouse_y,
                                  cos(angle) * speed,
                                  sin(angle) * speed,
                                  hue, rand() % 3);
        }
    }
    
    // Right mouse button: draw actual rainbow with gold particles (on press)
    if (rainbow->mouse_interactive && mouse_right && !rainbow->last_right_click) {
        rainbow->vortex.x = mouse_x;
        rainbow->vortex.y = mouse_y;
        rainbow->vortex.effect_time = 3.0;  // 3 second rainbow duration
        rainbow->vortex.active = TRUE;
        
        // Spawn gold particles at the end of the rainbow (pot of gold!)
        double gold_end_x = mouse_x + 150.0 * cos(M_PI / 4.0);  // Rainbow end at 45 degrees
        double gold_end_y = mouse_y + 150.0 * sin(M_PI / 4.0);
        
        for (int i = 0; i < 30; i++) {
            double angle = (rand() % 360) * M_PI / 180.0;
            double speed = 100.0 + (rand() % 100) * 0.5;
            
            spawn_rainbow_particle(rainbow, gold_end_x, gold_end_y,
                                  cos(angle) * speed,
                                  sin(angle) * speed,
                                  0.14,  // Gold hue (yellow-orange)
                                  rand() % 3);
        }
    }
    
    // Continuous mouse-based particle spawning from pointer while left button is held
    if (rainbow->mouse_interactive && mouse_left) {
        for (int i = 0; i < 12; i++) {
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
    
    // Track click states for next frame
    rainbow->last_left_click = mouse_left;
    rainbow->last_right_click = mouse_right;
    
    // Visual circle effect decay
    if (rainbow->vortex.active) {
        rainbow->vortex.effect_time -= dt;
        if (rainbow->vortex.effect_time <= 0) {
            rainbow->vortex.active = FALSE;
        }
    }
}

// Draw all particles
void draw_rainbow_particle(Visualizer *vis_ptr, cairo_t *cr) {
    RainbowSystem *rainbow = &vis_ptr->rainbow_system;
    
    for (int i = 0; i < MAX_RAINBOW_PARTICLES; i++) {
        RainbowParticle *p = &rainbow->particles[i];
        if (!p->active) continue;
        
        cairo_save(cr);
        cairo_translate(cr, p->x, p->y);
        cairo_rotate(cr, p->rotation);
        
        double r, g, b;
        hsv_to_rgb_rainbow(p->hue, 1.0, 1.0, &r, &g, &b);
        
        double alpha = p->life;
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
                for (int j = 0; j < 5; j++) {
                    double angle = j * 2 * M_PI / 5.0;
                    double x = cos(angle) * p->size;
                    double y = sin(angle) * p->size;
                    if (j == 0) {
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
}

// Draw the rainbow system
void draw_rainbow_system(Visualizer *vis, cairo_t *cr) {
    RainbowSystem *rainbow = &vis->rainbow_system;
    
    int width = vis->width;
    int height = vis->height;
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
    draw_rainbow_particle(vis, cr);
    
    // Draw rainbow effect if active (actual rainbow arc)
    if (rainbow->vortex.active) {
        double alpha = rainbow->vortex.effect_time / 3.0;  // Fade out over time
        cairo_set_line_width(cr, 15.0);
        
        // Draw rainbow as concentric arcs with spectrum colors
        double start_radius = 60.0;
        int num_bands = 7;  // ROYGBIV
        
        for (int i = 0; i < num_bands; i++) {
            double hue = (double)i / (double)num_bands;  // Spectrum from red to violet
            double r, g, b;
            hsv_to_rgb_rainbow(hue, 1.0, 1.0, &r, &g, &b);
            
            double radius = start_radius + i * 12.0;
            cairo_set_source_rgba(cr, r, g, b, alpha * 0.8);
            cairo_arc(cr, rainbow->vortex.x, rainbow->vortex.y, radius, M_PI, 0);  // Upper semicircle
            cairo_stroke(cr);
        }
    }
    
    // Draw instruction text (for interactive mode)
    if (rainbow->mouse_interactive) {
        cairo_set_font_size(cr, 14);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
        cairo_move_to(cr, 10, height - 10);
        cairo_show_text(cr, "Click to spawn rainbow particles");
    }
}

// Cleanup
void cleanup_rainbow_system(RainbowSystem *rainbow) {
    memset(rainbow, 0, sizeof(RainbowSystem));
}
