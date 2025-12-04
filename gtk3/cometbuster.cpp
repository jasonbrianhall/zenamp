#include <cairo.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cometbuster.h"
#include "visualization.h"

// ============================================================================
// STATIC MEMORY VERSION - NO MALLOC/FREE
// ============================================================================

// ============================================================================
// INITIALIZATION & CLEANUP
// ============================================================================

void init_comet_buster_system(void *vis) {
    Visualizer *visualizer = (Visualizer *)vis;
    
    comet_buster_reset_game(&visualizer->comet_buster);
    
    printf("Comet Buster system initialized (STATIC MEMORY)\n");
    printf("Memory usage: %zu bytes per game\n", sizeof(CometBusterGame));
    printf("Max objects: %d comets, %d bullets, %d particles\n",
           MAX_COMETS, MAX_BULLETS, MAX_PARTICLES);
}

void comet_buster_cleanup(CometBusterGame *game) {
    if (!game) return;
    
    memset(game, 0, sizeof(CometBusterGame));
    
    printf("Comet Buster cleaned up (all static memory zeroed)\n");
}

void comet_buster_reset_game(CometBusterGame *game) {
    if (!game) return;
    
    memset(game, 0, sizeof(CometBusterGame));
    
    // Initialize non-zero values - use defaults, will be set by visualizer if needed
    game->ship_x = 400.0;
    game->ship_y = 300.0;
    game->ship_vx = 0;
    game->ship_vy = 0;
    game->ship_angle = 0;
    game->ship_speed = 0;
    game->ship_lives = 3;
    game->invulnerability_time = 0;
    
    // Game state
    game->score = 0;
    game->comets_destroyed = 0;
    game->score_multiplier = 1.0;
    game->consecutive_hits = 0;
    game->current_wave = 1;
    game->wave_comets = 0;
    game->last_life_milestone = 0;  // Track score milestones for extra lives
    game->game_over = false;
    game->game_won = false;
    
    // Object counts
    game->comet_count = 0;
    game->bullet_count = 0;
    game->particle_count = 0;
    game->floating_text_count = 0;
    game->high_score_count = 0;
    
    // Timing
    game->spawn_timer = 1.0;
    game->base_spawn_rate = 1.0;
    game->beat_fire_cooldown = 0;
    game->last_beat_time = -1;
    game->difficulty_timer = 0;
    
    // Mouse state
    game->mouse_left_pressed = false;
    game->mouse_fire_cooldown = 0;
    game->mouse_right_pressed = false;
    game->mouse_middle_pressed = false;
    game->omni_fire_cooldown = 0;
    
    // Advanced thrusters (fuel system)
    game->fuel_amount = 100.0;
    game->max_fuel = 100.0;
    game->fuel_burn_rate = 60.0;        // Burn 60 fuel per second at full boost
    game->fuel_recharge_rate = 25.0;    // Recharge 25 fuel per second when idle
    game->boost_multiplier = 2.5;       // 2.5x speed boost
    game->is_boosting = false;
    game->boost_thrust_timer = 0.0;
    
    // Load high scores from file (if they exist)
    comet_buster_load_high_scores(game);
    
    // Spawn initial comets (with default screen size, will be updated on first update)
    comet_buster_spawn_wave(game, 800, 600);
}

// ============================================================================
// INPUT HANDLING - SIMPLIFIED (Visualizer handles mouse state)
// ============================================================================
// The visualizer already tracks mouse_left_pressed automatically
// We just read it in update_comet_buster() - no separate handlers needed!

// ============================================================================
// SPAWNING FUNCTIONS (No malloc, just find next available slot)
// ============================================================================

void comet_buster_spawn_comet(CometBusterGame *game, int frequency_band, int screen_width, int screen_height) {
    if (!game) return;
    
    if (game->comet_count >= MAX_COMETS) {
        return;
    }
    
    int slot = game->comet_count;
    Comet *comet = &game->comets[slot];
    
    memset(comet, 0, sizeof(Comet));
    
    // Random position on screen edge
    int edge = rand() % 4;
    
    switch (edge) {
        case 0:  // Top
            comet->x = rand() % screen_width;
            comet->y = -30;
            break;
        case 1:  // Right
            comet->x = screen_width + 30;
            comet->y = rand() % screen_height;
            break;
        case 2:  // Bottom
            comet->x = rand() % screen_width;
            comet->y = screen_height + 30;
            break;
        case 3:  // Left
            comet->x = -30;
            comet->y = rand() % screen_height;
            break;
    }
    
    // Random velocity toward center-ish
    double target_x = screen_width / 2 + (rand() % 200 - 100);
    double target_y = screen_height / 2 + (rand() % 200 - 100);
    double dx = target_x - comet->x;
    double dy = target_y - comet->y;
    double len = sqrt(dx*dx + dy*dy);
    
    double speed = 50.0 + (rand() % 50);
    if (len > 0) {
        comet->vx = (dx / len) * speed;
        comet->vy = (dy / len) * speed;
    }
    
    // Set size based on wave
    int rnd = rand() % 100;
    if (game->current_wave == 1) {
        comet->size = COMET_LARGE;
        comet->radius = 30;
    } else if (game->current_wave == 2) {
        if (rnd < 70) comet->size = COMET_LARGE;
        else comet->size = COMET_MEDIUM;
        comet->radius = (comet->size == COMET_LARGE) ? 30 : 20;
    } else {
        if (rnd < 50) comet->size = COMET_LARGE;
        else if (rnd < 80) comet->size = COMET_MEDIUM;
        else comet->size = COMET_SMALL;
        comet->radius = (comet->size == COMET_LARGE) ? 30 : 
                        (comet->size == COMET_MEDIUM) ? 20 : 10;
    }
    
    // Set properties
    comet->frequency_band = frequency_band;
    comet->rotation = 0;
    comet->rotation_speed = 50 + (rand() % 200);
    comet->active = true;
    comet->health = 1;
    
    // For vector asteroids, set a base rotation angle and shape variant
    comet->base_angle = (rand() % 360) * (M_PI / 180.0);
    
    // Store a shape variant based on current comet count (deterministic but varies)
    // This ensures same-sized asteroids don't all have the same shape
    // Use modulo on the integer calculation, then convert to double
    int speed_variant = ((int)comet->rotation_speed + game->comet_count * 17) % 360;
    comet->rotation_speed = speed_variant + (comet->rotation_speed - (int)comet->rotation_speed);
    
    // Set color based on frequency
    comet_buster_get_frequency_color(frequency_band, 
                                     &comet->color[0], 
                                     &comet->color[1], 
                                     &comet->color[2]);
    
    game->comet_count++;
}

void comet_buster_spawn_random_comets(CometBusterGame *game, int count, int screen_width, int screen_height) {
    for (int i = 0; i < count; i++) {
        int band = rand() % 3;
        comet_buster_spawn_comet(game, band, screen_width, screen_height);
    }
}

// ============================================================================
// WAVE SYSTEM
// ============================================================================

int comet_buster_get_wave_comet_count(int wave) {
    // Returns the number of comets to spawn for a given wave
    // Wave difficulty increases progressively
    if (wave <= 0) wave = 1;
    
    // Base count increases by 2 per wave: Wave 1=3, Wave 2=5, Wave 3=7, etc.
    // With exponential scaling for later waves
    if (wave == 1) return 3;
    else if (wave == 2) return 5;
    else if (wave == 3) return 7;
    else if (wave == 4) return 9;
    else if (wave == 5) return 11;
    else {
        // For waves 6+, use exponential growth with a cap
        int count = 11 + (wave - 5) * 3;
        return (count > 25) ? 25 : count;  // Cap at 25 to prevent too many
    }
}

double comet_buster_get_wave_speed_multiplier(int wave) {
    // Returns speed multiplier for comets in given wave
    // Speeds increase as waves progress
    if (wave <= 1) return 1.0;
    else if (wave == 2) return 1.1;
    else if (wave == 3) return 1.2;
    else if (wave == 4) return 1.35;
    else if (wave == 5) return 1.5;
    else {
        // For waves 6+, continue scaling (but cap at 2.5x)
        double multiplier = 1.5 + (wave - 5) * 0.1;
        return (multiplier > 2.5) ? 2.5 : multiplier;
    }
}

void comet_buster_spawn_wave(CometBusterGame *game, int screen_width, int screen_height) {
    if (!game) return;
    
    // Get number of comets for this wave
    int wave_count = comet_buster_get_wave_comet_count(game->current_wave);
    
    // Spawn comets for the wave
    for (int i = 0; i < wave_count; i++) {
        int band = rand() % 3;
        comet_buster_spawn_comet(game, band, screen_width, screen_height);
        
        // Apply speed multiplier based on wave
        if (game->comet_count > 0) {
            Comet *last_comet = &game->comets[game->comet_count - 1];
            double speed_mult = comet_buster_get_wave_speed_multiplier(game->current_wave);
            last_comet->vx *= speed_mult;
            last_comet->vy *= speed_mult;
        }
    }
    
    game->wave_comets = 0;  // Reset wave comet counter
}

void comet_buster_update_wave_progression(CometBusterGame *game) {
    if (!game || game->game_over) return;
    
    // Check if all comets are destroyed to trigger next wave
    // Only trigger if we're not already in countdown (wave_complete_timer == 0)
    if (game->comet_count == 0 && game->wave_complete_timer == 0) {
        // All comets destroyed - start countdown to next wave
        printf("WAVE %d COMPLETE! Spawning Wave %d in 2.0 seconds...\n", 
               game->current_wave, game->current_wave + 1);
        game->wave_complete_timer = 2.0;  // 2 second delay before next wave
    }
}

void comet_buster_spawn_bullet(CometBusterGame *game) {
    if (!game) return;
    
    if (game->bullet_count >= MAX_BULLETS) {
        return;
    }
    
    int slot = game->bullet_count;
    Bullet *bullet = &game->bullets[slot];
    
    memset(bullet, 0, sizeof(Bullet));
    
    bullet->x = game->ship_x;
    bullet->y = game->ship_y;
    
    double bullet_speed = 400.0;
    bullet->vx = cos(game->ship_angle) * bullet_speed;
    bullet->vy = sin(game->ship_angle) * bullet_speed;
    
    bullet->angle = game->ship_angle;
    bullet->lifetime = 1.5;  // Bullets disappear quickly
    bullet->max_lifetime = 1.5;
    bullet->active = true;
    
    game->bullet_count++;
    
    // Muzzle flash
    game->muzzle_flash_timer = 0.1;
}

void comet_buster_spawn_omnidirectional_fire(CometBusterGame *game) {
    // Fire in all 8 directions (Last Starfighter style)
    if (!game) return;
    
    // Check if we have enough fuel (costs 30 fuel per omnidirectional burst)
    if (game->fuel_amount < 30) {
        return;  // Not enough fuel
    }
    
    double bullet_speed = 400.0;
    int directions = 8;  // 8 directions in a circle
    
    for (int i = 0; i < directions; i++) {
        if (game->bullet_count >= MAX_BULLETS) break;
        
        int slot = game->bullet_count;
        Bullet *bullet = &game->bullets[slot];
        
        memset(bullet, 0, sizeof(Bullet));
        
        bullet->x = game->ship_x;
        bullet->y = game->ship_y;
        
        // Calculate angle for this direction (0, 45, 90, 135, 180, 225, 270, 315 degrees)
        double angle = (i * 360.0 / directions) * (M_PI / 180.0);
        
        bullet->vx = cos(angle) * bullet_speed;
        bullet->vy = sin(angle) * bullet_speed;
        
        bullet->angle = angle;
        bullet->lifetime = 1.5;
        bullet->max_lifetime = 1.5;
        bullet->active = true;
        
        game->bullet_count++;
    }
    
    // Consume fuel for omnidirectional fire
    game->fuel_amount -= 30.0;
    if (game->fuel_amount < 0) game->fuel_amount = 0;
    
    // Muzzle flash
    game->muzzle_flash_timer = 0.15;
}

void comet_buster_spawn_explosion(CometBusterGame *game, double x, double y,
                                   int frequency_band, int particle_count) {
    for (int i = 0; i < particle_count; i++) {
        if (game->particle_count >= MAX_PARTICLES) {
            break;
        }
        
        int slot = game->particle_count;
        Particle *p = &game->particles[slot];
        
        memset(p, 0, sizeof(Particle));
        
        double angle = (2.0 * M_PI * i) / particle_count + 
                       ((rand() % 100) / 100.0) * 0.3;
        double speed = 100.0 + (rand() % 100);
        
        p->x = x;
        p->y = y;
        p->vx = cos(angle) * speed;
        p->vy = sin(angle) * speed;
        p->lifetime = 0.3 + (rand() % 20) / 100.0;
        p->max_lifetime = p->lifetime;
        p->size = 2.0 + (rand() % 4);
        p->active = true;
        
        comet_buster_get_frequency_color(frequency_band,
                                        &p->color[0],
                                        &p->color[1],
                                        &p->color[2]);
        
        game->particle_count++;
    }
}

void comet_buster_spawn_floating_text(CometBusterGame *game, double x, double y, const char *text, double r, double g, double b) {
    if (!game || game->floating_text_count >= MAX_FLOATING_TEXT) {
        return;
    }
    
    int slot = game->floating_text_count;
    FloatingText *ft = &game->floating_texts[slot];
    
    memset(ft, 0, sizeof(FloatingText));
    
    ft->x = x;
    ft->y = y;
    ft->lifetime = 2.0;  // Display for 2 seconds
    ft->max_lifetime = 2.0;
    ft->color[0] = r;
    ft->color[1] = g;
    ft->color[2] = b;
    ft->active = true;
    
    strncpy(ft->text, text, sizeof(ft->text) - 1);
    ft->text[sizeof(ft->text) - 1] = '\0';
    
    game->floating_text_count++;
}

void comet_buster_update_ship(CometBusterGame *game, double dt, int mouse_x, int mouse_y, int width, int height) {
    if (game->game_over || !game) return;
    
    if (game->invulnerability_time > 0) {
        game->invulnerability_time -= dt;
    }
    
    // Rotate ship to face mouse
    double dx = mouse_x - game->ship_x;
    double dy = mouse_y - game->ship_y;
    double target_angle = atan2(dy, dx);
    
    double angle_diff = target_angle - game->ship_angle;
    while (angle_diff > M_PI) angle_diff -= 2.0 * M_PI;
    while (angle_diff < -M_PI) angle_diff += 2.0 * M_PI;
    
    double rotation_speed = 5.0;
    if (fabs(angle_diff) > rotation_speed * dt) {
        if (angle_diff > 0) {
            game->ship_angle += rotation_speed * dt;
        } else {
            game->ship_angle -= rotation_speed * dt;
        }
    } else {
        game->ship_angle = target_angle;
    }
    
    // Movement based on mouse distance
    double mouse_dist = sqrt(dx*dx + dy*dy);
    double max_dist = 400.0;
    
    double acceleration = 1.0;
    if (mouse_dist < 50.0) {
        acceleration = 0.1;
    } else if (mouse_dist > max_dist) {
        acceleration = 2.0;
    } else {
        acceleration = 1.0 + (mouse_dist / max_dist) * 1.5;
    }
    
    double accel_magnitude = acceleration * 200.0;
    
    // Apply boost multiplier if right-click is held and fuel available
    double speed_multiplier = 1.0;
    if (game->mouse_right_pressed && game->fuel_amount > 0) {
        game->is_boosting = true;
        speed_multiplier = game->boost_multiplier;
        game->boost_thrust_timer = 0.1;  // Visual effect duration
    } else {
        game->is_boosting = false;
    }
    
    accel_magnitude *= speed_multiplier;
    
    if (mouse_dist > 0.1) {
        game->ship_vx += (dx / mouse_dist) * accel_magnitude * dt;
        game->ship_vy += (dy / mouse_dist) * accel_magnitude * dt;
    }
    
    // Friction (slightly more with boost for balance)
    double friction = game->is_boosting ? 0.92 : 0.95;
    game->ship_vx *= friction;
    game->ship_vy *= friction;
    
    // Update position
    game->ship_x += game->ship_vx * dt;
    game->ship_y += game->ship_vy * dt;
    
    // Wrap
    comet_buster_wrap_position(&game->ship_x, &game->ship_y, width, height);
}

void comet_buster_update_comets(CometBusterGame *game, double dt, int width, int height) {
    if (!game) return;
    
    for (int i = 0; i < game->comet_count; i++) {
        Comet *c = &game->comets[i];
        
        // Update position
        c->x += c->vx * dt;
        c->y += c->vy * dt;
        
        // Update rotation
        c->rotation += c->rotation_speed * dt;
        while (c->rotation > 360) c->rotation -= 360;
        
        // Wrap
        comet_buster_wrap_position(&c->x, &c->y, width, height);
    }
    
    // Check comet-comet collisions
    for (int i = 0; i < game->comet_count; i++) {
        for (int j = i + 1; j < game->comet_count; j++) {
            Comet *c1 = &game->comets[i];
            Comet *c2 = &game->comets[j];
            
            if (!c1->active || !c2->active) continue;
            
            // Check collision distance
            double dx = c2->x - c1->x;
            double dy = c2->y - c1->y;
            double dist = sqrt(dx*dx + dy*dy);
            double min_dist = c1->radius + c2->radius;
            
            if (dist < min_dist) {
                // Collision detected - perform elastic collision physics
                comet_buster_handle_comet_collision(c1, c2, dx, dy, dist, min_dist);
            }
        }
    }
}

void comet_buster_update_bullets(CometBusterGame *game, double dt, int width, int height) {
    if (!game) return;
    
    for (int i = 0; i < game->bullet_count; i++) {
        Bullet *b = &game->bullets[i];
        
        // Update lifetime
        b->lifetime -= dt;
        if (b->lifetime <= 0) {
            b->active = false;
            
            // Swap with last bullet
            if (i != game->bullet_count - 1) {
                game->bullets[i] = game->bullets[game->bullet_count - 1];
            }
            game->bullet_count--;
            i--;
            continue;
        }
        
        // Update position
        b->x += b->vx * dt;
        b->y += b->vy * dt;
        
        // Wrap
        comet_buster_wrap_position(&b->x, &b->y, width, height);
        
        // Check collision with comets
        for (int j = 0; j < game->comet_count; j++) {
            Comet *c = &game->comets[j];
            
            if (comet_buster_check_bullet_comet(b, c)) {
                b->active = false;
                comet_buster_destroy_comet(game, j, width, height);
                break;
            }
        }
    }
}

void comet_buster_update_particles(CometBusterGame *game, double dt) {
    if (!game) return;
    
    for (int i = 0; i < game->particle_count; i++) {
        Particle *p = &game->particles[i];
        
        // Update lifetime
        p->lifetime -= dt;
        if (p->lifetime <= 0) {
            p->active = false;
            
            // Swap with last
            if (i != game->particle_count - 1) {
                game->particles[i] = game->particles[game->particle_count - 1];
            }
            game->particle_count--;
            i--;
            continue;
        }
        
        // Update position with gravity
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vy += 100.0 * dt;
    }
}

void comet_buster_update_floating_text(CometBusterGame *game, double dt) {
    if (!game) return;
    
    for (int i = 0; i < game->floating_text_count; i++) {
        FloatingText *ft = &game->floating_texts[i];
        
        // Update lifetime
        ft->lifetime -= dt;
        if (ft->lifetime <= 0) {
            ft->active = false;
            
            // Swap with last
            if (i != game->floating_text_count - 1) {
                game->floating_texts[i] = game->floating_texts[game->floating_text_count - 1];
            }
            game->floating_text_count--;
            i--;
            continue;
        }
        
        // Float upward
        ft->y -= 20.0 * dt;
    }
}

void comet_buster_update_shooting(CometBusterGame *game, double dt) {
    if (!game || game->game_over) return;
    
    // Update fire cooldown
    if (game->mouse_fire_cooldown > 0) {
        game->mouse_fire_cooldown -= dt;
    }
    
    // Update omnidirectional fire cooldown
    if (game->omni_fire_cooldown > 0) {
        game->omni_fire_cooldown -= dt;
    }
    
    // DEBUG: Print state
    static int debug_frame = 0;
    if (debug_frame++ % 60 == 0) {  // Print every 60 frames (~1 sec at 60 FPS)
        printf("Comet Buster: mouse_left_pressed=%d, fire_cooldown=%.3f, bullets=%d, fuel=%.0f\n",
               game->mouse_left_pressed, game->mouse_fire_cooldown, game->bullet_count, game->fuel_amount);
    }
    
    // If left mouse button is held down, fire continuously (costs fuel)
    if (game->mouse_left_pressed) {
        if (game->mouse_fire_cooldown <= 0) {
            // Normal fire costs 0.25 fuel per bullet
            if (game->fuel_amount >= 0.25) {
                printf("  >> FIRING BULLET (cooldown: %.3f, fuel: %.1f)\n", game->mouse_fire_cooldown, game->fuel_amount);
                comet_buster_spawn_bullet(game);
                game->fuel_amount -= 0.25;  // Consume 0.25 fuel
                game->mouse_fire_cooldown = 0.05;  // ~20 bullets per second
            } else {
                // Can't fire without fuel
                if (debug_frame % 60 == 0) {  // Only print occasionally to avoid spam
                    printf("  >> NOT ENOUGH FUEL FOR NORMAL FIRE (need 0.25, have %.1f)\n", game->fuel_amount);
                }
            }
        }
    }
    
    // If middle mouse button is pressed, fire omnidirectionally (Last Starfighter style)
    if (game->mouse_middle_pressed) {
        if (game->omni_fire_cooldown <= 0) {
            if (game->fuel_amount >= 30) {
                printf("  >> OMNIDIRECTIONAL FIRE! (fuel: %.0f)\n", game->fuel_amount);
                comet_buster_spawn_omnidirectional_fire(game);
                game->omni_fire_cooldown = 0.3;  // Slower than normal fire
            } else {
                printf("  >> NOT ENOUGH FUEL FOR OMNIDIRECTIONAL FIRE (need 30, have %.0f)\n", game->fuel_amount);
            }
        }
    }
}

void comet_buster_update_fuel(CometBusterGame *game, double dt) {
    if (!game) return;
    
    // Update boost timer for visual effects
    if (game->boost_thrust_timer > 0) {
        game->boost_thrust_timer -= dt;
    }
    
    // Handle fuel burn and recharge
    if (game->is_boosting && game->fuel_amount > 0) {
        // Burning fuel while boosting
        game->fuel_amount -= game->fuel_burn_rate * dt;
        if (game->fuel_amount < 0) {
            game->fuel_amount = 0;
            game->is_boosting = false;
        }
    } else if (!game->mouse_left_pressed) {
        // Recharging fuel when not boosting AND not firing
        if (game->fuel_amount < game->max_fuel) {
            game->fuel_amount += game->fuel_recharge_rate * dt;
            if (game->fuel_amount > game->max_fuel) {
                game->fuel_amount = game->max_fuel;
            }
        }
    }
}

void update_comet_buster(void *vis, double dt) {
    Visualizer *visualizer = (Visualizer *)vis;
    if (!visualizer) return;
    
    CometBusterGame *game = &visualizer->comet_buster;
    
    int mouse_x = visualizer->mouse_x;
    int mouse_y = visualizer->mouse_y;
    int width = visualizer->width;
    int height = visualizer->height;
    
    // Initialize ship position on first run (resolution-aware)
    static bool first_run = true;
    if (first_run && width > 0 && height > 0) {
        game->ship_x = width / 2.0;
        game->ship_y = height / 2.0;
        first_run = false;
    }
    
    // ✓ CRITICAL FIX: Use visualizer's mouse_left_pressed directly!
    // The visualizer already tracks mouse button state
    game->mouse_left_pressed = visualizer->mouse_left_pressed;
    game->mouse_right_pressed = visualizer->mouse_right_pressed;  // Add right-click tracking
    game->mouse_middle_pressed = visualizer->mouse_middle_pressed;  // Middle-click for omni-fire
    
    // Update game state
    comet_buster_update_ship(game, dt, mouse_x, mouse_y, width, height);
    comet_buster_update_comets(game, dt, width, height);
    comet_buster_update_shooting(game, dt);  // Uses mouse_left_pressed state
    comet_buster_update_bullets(game, dt, width, height);
    comet_buster_update_particles(game, dt);
    comet_buster_update_floating_text(game, dt);  // Update floating text popups
    
    // Update fuel system
    comet_buster_update_fuel(game, dt);
    
    // Handle wave completion and progression (only if not already counting down)
    if (game->wave_complete_timer <= 0) {
        comet_buster_update_wave_progression(game);
    }
    
    // Handle wave complete timer (delay before next wave spawns)
    if (game->wave_complete_timer > 0) {
        game->wave_complete_timer -= dt;
        if (game->wave_complete_timer <= 0) {
            // Timer expired - spawn the next wave
            if (game->comet_count == 0) {
                game->current_wave++;  // Increment AFTER timer expires
                comet_buster_spawn_wave(game, width, height);
                printf("Spawning Wave %d\n", game->current_wave);
            }
            game->wave_complete_timer = 0;
        }
    }
    
    // Check ship-comet collisions
    for (int i = 0; i < game->comet_count; i++) {
        if (comet_buster_check_ship_comet(game, &game->comets[i])) {
            comet_buster_on_ship_hit(game, visualizer);
        }
    }
    
    // Update timers
    game->muzzle_flash_timer -= dt;
    game->difficulty_timer -= dt;
    if (game->game_over) {
        game->game_over_timer -= dt;
    }
}

// ============================================================================
// COMET COLLISION PHYSICS
// ============================================================================

void comet_buster_handle_comet_collision(Comet *c1, Comet *c2, double dx, double dy, 
                                         double dist, double min_dist) {
    if (dist < 0.01) dist = 0.01;  // Avoid division by zero
    
    // Normalize collision vector
    double nx = dx / dist;  // Normal x
    double ny = dy / dist;  // Normal y
    
    // Relative velocity
    double dvx = c2->vx - c1->vx;
    double dvy = c2->vy - c1->vy;
    
    // Relative velocity in collision normal direction
    double dvn = dvx * nx + dvy * ny;
    
    // Don't collide if velocities are moving apart
    if (dvn >= 0) return;
    
    // Get masses (proportional to size - larger asteroids are "heavier")
    double m1 = c1->radius * c1->radius;  // Mass proportional to area
    double m2 = c2->radius * c2->radius;
    
    // Collision impulse (simplified elastic collision)
    // For equal elasticity, exchange velocity components
    double impulse = 2.0 * dvn / (m1 + m2);
    
    // Apply impulse to both comets
    c1->vx += impulse * m2 * nx;
    c1->vy += impulse * m2 * ny;
    
    c2->vx -= impulse * m1 * nx;
    c2->vy -= impulse * m1 * ny;
    
    // Separate comets to prevent overlap (overlap resolution)
    double overlap = min_dist - dist;
    double separate = (overlap / 2.0) + 0.01;  // Small buffer to prevent re-collision
    
    double ratio1 = m2 / (m1 + m2);  // How much c1 moves
    double ratio2 = m1 / (m1 + m2);  // How much c2 moves
    
    c1->x -= separate * ratio1 * nx;
    c1->y -= separate * ratio1 * ny;
    
    c2->x += separate * ratio2 * nx;
    c2->y += separate * ratio2 * ny;
}

// ============================================================================
// COLLISION & DESTRUCTION
// ============================================================================

bool comet_buster_check_bullet_comet(Bullet *b, Comet *c) {
    if (!b->active || !c->active) return false;
    
    double dx = b->x - c->x;
    double dy = b->y - c->y;
    double dist = sqrt(dx*dx + dy*dy);
    
    return dist < (c->radius + 2.0);
}

bool comet_buster_check_ship_comet(CometBusterGame *game, Comet *c) {
    if (!c->active) return false;
    
    double dx = game->ship_x - c->x;
    double dy = game->ship_y - c->y;
    double dist = sqrt(dx*dx + dy*dy);
    
    return dist < (c->radius + 15.0);
}

void comet_buster_destroy_comet(CometBusterGame *game, int comet_index, int width, int height) {
    (void)width;
    (void)height;
    if (comet_index < 0 || comet_index >= game->comet_count) return;
    
    Comet *c = &game->comets[comet_index];
    if (!c->active) return;
    
    // Create explosion
    int particle_count = 15;
    if (c->size == COMET_LARGE) particle_count = 20;
    else if (c->size == COMET_SMALL) particle_count = 8;
    
    comet_buster_spawn_explosion(game, c->x, c->y, c->frequency_band, particle_count);
    
    // Award points
    int points = 0;
    switch (c->size) {
        case COMET_SMALL: points = 50; break;
        case COMET_MEDIUM: points = 100; break;
        case COMET_LARGE: points = 200; break;
        case COMET_SPECIAL: points = 500; break;
    }
    
    int score_add = (int)(points * game->score_multiplier);
    game->score += score_add;
    game->comets_destroyed++;
    game->consecutive_hits++;
    
    // Check for extra life bonus every 5000 points
    int current_milestone = game->score / 5000;
    if (current_milestone > game->last_life_milestone) {
        game->ship_lives++;
        game->last_life_milestone = current_milestone;
        printf("★ EXTRA LIFE! Lives now: %d (Score: %d)\n", game->ship_lives, game->score);
        
        // Spawn floating text popup
        char text[32];
        snprintf(text, sizeof(text), "★ +1 LIFE ★");
        comet_buster_spawn_floating_text(game, game->ship_x, game->ship_y - 30, text, 1.0, 1.0, 0.0);  // Yellow
    }
    
    // Increase multiplier
    if (game->consecutive_hits % 5 == 0) {
        game->score_multiplier += 0.1;
        if (game->score_multiplier > 5.0) game->score_multiplier = 5.0;
    }
    
    // Spawn child comets (at parent location, not at screen edge)
    if (c->size == COMET_LARGE) {
        for (int i = 0; i < 2; i++) {
            if (game->comet_count >= MAX_COMETS) break;
            
            int slot = game->comet_count;
            Comet *child = &game->comets[slot];
            memset(child, 0, sizeof(Comet));
            
            // Spawn at parent location
            child->x = c->x + (rand() % 20 - 10);  // Small offset
            child->y = c->y + (rand() % 20 - 10);
            
            // Scatter in random direction
            double angle = (rand() % 360) * (M_PI / 180.0);
            double speed = 100.0 + (rand() % 100);
            child->vx = cos(angle) * speed;
            child->vy = sin(angle) * speed;
            
            // Set as medium asteroid
            child->size = COMET_MEDIUM;
            child->radius = 20;
            child->frequency_band = c->frequency_band;
            child->rotation = 0;
            child->rotation_speed = 50 + (rand() % 200);
            child->active = true;
            child->health = 1;
            child->base_angle = (rand() % 360) * (M_PI / 180.0);
            
            // Color
            comet_buster_get_frequency_color(c->frequency_band, 
                                             &child->color[0], 
                                             &child->color[1], 
                                             &child->color[2]);
            
            game->comet_count++;
        }
    } else if (c->size == COMET_MEDIUM) {
        for (int i = 0; i < 2; i++) {
            if (game->comet_count >= MAX_COMETS) break;
            
            int slot = game->comet_count;
            Comet *child = &game->comets[slot];
            memset(child, 0, sizeof(Comet));
            
            // Spawn at parent location
            child->x = c->x + (rand() % 20 - 10);  // Small offset
            child->y = c->y + (rand() % 20 - 10);
            
            // Scatter in random direction
            double angle = (rand() % 360) * (M_PI / 180.0);
            double speed = 150.0 + (rand() % 100);
            child->vx = cos(angle) * speed;
            child->vy = sin(angle) * speed;
            
            // Set as small asteroid
            child->size = COMET_SMALL;
            child->radius = 10;
            child->frequency_band = c->frequency_band;
            child->rotation = 0;
            child->rotation_speed = 50 + (rand() % 200);
            child->active = true;
            child->health = 1;
            child->base_angle = (rand() % 360) * (M_PI / 180.0);
            
            // Color
            comet_buster_get_frequency_color(c->frequency_band, 
                                             &child->color[0], 
                                             &child->color[1], 
                                             &child->color[2]);
            
            game->comet_count++;
        }
    }
    
    // Swap with last and remove
    if (comet_index != game->comet_count - 1) {
        game->comets[comet_index] = game->comets[game->comet_count - 1];
    }
    game->comet_count--;
}

void comet_buster_on_ship_hit(CometBusterGame *game, Visualizer *visualizer) {
    if (game->invulnerability_time > 0) return;
    
    game->ship_lives--;
    game->consecutive_hits = 0;
    game->score_multiplier = 1.0;
    
    if (game->ship_lives <= 0) {
        game->game_over = true;
        game->game_over_timer = 3.0;
        
        if (comet_buster_is_high_score(game, game->score)) {
            comet_buster_add_high_score(game, game->score, game->current_wave, "Player");
            comet_buster_save_high_scores(game);
        }
    } else {
        // Move ship to center (like classic Asteroids) - resolution aware
        if (visualizer && visualizer->width > 0 && visualizer->height > 0) {
            game->ship_x = visualizer->width / 2.0;
            game->ship_y = visualizer->height / 2.0;
        } else {
            game->ship_x = 400.0;  // Fallback
            game->ship_y = 300.0;
        }
        game->ship_vx = 0;
        game->ship_vy = 0;
        game->ship_speed = 0;
        
        // Invulnerability period while finding safe spot
        game->invulnerability_time = 3.0;
    }
}

// ============================================================================
// HIGH SCORE MANAGEMENT (All STATIC)
// ============================================================================

void comet_buster_load_high_scores(CometBusterGame *game) {
    if (!game) return;
    
    game->high_score_count = 0;
    memset(game->high_scores, 0, sizeof(game->high_scores));
}

void comet_buster_save_high_scores(CometBusterGame *game) {
    if (!game) return;
}

void comet_buster_add_high_score(CometBusterGame *game, int score, int wave, const char *name) {
    if (!game || game->high_score_count >= MAX_HIGH_SCORES) return;
    
    int slot = game->high_score_count;
    HighScore *hs = &game->high_scores[slot];
    
    hs->score = score;
    hs->wave = wave;
    hs->timestamp = time(NULL);
    strncpy(hs->player_name, name, sizeof(hs->player_name) - 1);
    
    game->high_score_count++;
}

bool comet_buster_is_high_score(CometBusterGame *game, int score) {
    if (!game) return false;
    
    if (game->high_score_count < MAX_HIGH_SCORES) return true;
    
    for (int i = 0; i < game->high_score_count; i++) {
        if (score > game->high_scores[i].score) return true;
    }
    
    return false;
}

// ============================================================================
// RENDERING - VECTOR-BASED ASTEROIDS
// ============================================================================

void draw_comet_buster(void *vis, cairo_t *cr) {
    Visualizer *visualizer = (Visualizer *)vis;
    if (!visualizer || !cr) return;
    
    CometBusterGame *game = &visualizer->comet_buster;
    int width = visualizer->width;
    int height = visualizer->height;
    
    // Background
    cairo_set_source_rgb(cr, 0.04, 0.06, 0.15);
    cairo_paint(cr);
    
    // Grid
    cairo_set_source_rgb(cr, 0.1, 0.15, 0.35);
    cairo_set_line_width(cr, 0.5);
    for (int i = 0; i <= width; i += 50) {
        cairo_move_to(cr, i, 0);
        cairo_line_to(cr, i, height);
    }
    for (int i = 0; i <= height; i += 50) {
        cairo_move_to(cr, 0, i);
        cairo_line_to(cr, width, i);
    }
    cairo_stroke(cr);
    
    // Draw game elements
    draw_comet_buster_comets(game, cr, width, height);
    draw_comet_buster_bullets(game, cr, width, height);
    draw_comet_buster_particles(game, cr, width, height);
    draw_comet_buster_ship(game, cr, width, height);
    
    // Draw HUD
    draw_comet_buster_hud(game, cr, width, height);
    
    // Draw game over
    if (game->game_over) {
        draw_comet_buster_game_over(game, cr, width, height);
    }
}

// ✓ VECTOR-BASED ASTEROIDS (like original Asteroids arcade game)
void draw_comet_buster_comets(CometBusterGame *game, cairo_t *cr, int width, int height) {
    if (!game) return;
    (void)width;    // Suppress unused parameter warning
    (void)height;   // Suppress unused parameter warning
    
    for (int i = 0; i < game->comet_count; i++) {
        Comet *c = &game->comets[i];
        
        cairo_save(cr);
        cairo_translate(cr, c->x, c->y);
        cairo_rotate(cr, c->base_angle + c->rotation * M_PI / 180.0);
        
        // Set color and line width
        cairo_set_source_rgb(cr, c->color[0], c->color[1], c->color[2]);
        cairo_set_line_width(cr, 2.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        
        // Draw vector-based asteroid (polygon with jagged edges)
        double radius = c->radius;
        
        // Use rotation_speed as a shape variant seed (deterministic but varied)
        int shape_variant = (int)c->rotation_speed % 3;
        
        if (c->size == COMET_LARGE) {
            // Large asteroid: multiple shape variants using same point count but different geometry
            if (shape_variant == 0) {
                // Standard 8-pointed
                double points[][2] = {
                    {radius, 0},
                    {radius * 0.7, radius * 0.7},
                    {0, radius},
                    {-radius * 0.6, radius * 0.8},
                    {-radius * 0.9, 0},
                    {-radius * 0.5, -radius * 0.8},
                    {0, -radius * 0.95},
                    {radius * 0.8, -radius * 0.6}
                };
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 8; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            } else if (shape_variant == 1) {
                // More jagged variant
                double points[][2] = {
                    {radius * 0.9, radius * 0.2},
                    {radius * 0.6, radius * 0.8},
                    {radius * 0.1, radius * 0.95},
                    {-radius * 0.7, radius * 0.7},
                    {-radius * 0.95, -0.1},
                    {-radius * 0.6, -radius * 0.8},
                    {radius * 0.2, -radius * 0.9},
                    {radius * 0.85, -radius * 0.3}
                };
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 8; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            } else {
                // Rounder variant
                double points[][2] = {
                    {radius, -radius * 0.2},
                    {radius * 0.75, radius * 0.6},
                    {radius * 0.2, radius * 0.9},
                    {-radius * 0.5, radius * 0.85},
                    {-radius * 0.95, radius * 0.1},
                    {-radius * 0.75, -radius * 0.65},
                    {-radius * 0.1, -radius * 0.95},
                    {radius * 0.7, -radius * 0.75}
                };
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 8; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            }
        } else if (c->size == COMET_MEDIUM) {
            // Medium asteroid: 3 shape variants with 5-6 points
            if (shape_variant == 0) {
                double points[][2] = {
                    {radius, 0},
                    {radius * 0.6, radius * 0.75},
                    {-radius * 0.5, radius * 0.8},
                    {-radius * 0.8, -radius * 0.6},
                    {radius * 0.5, -radius * 0.9}
                };
                
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 5; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            } else if (shape_variant == 1) {
                double points[][2] = {
                    {radius * 0.85, radius * 0.3},
                    {radius * 0.4, radius * 0.85},
                    {-radius * 0.7, radius * 0.6},
                    {-radius * 0.75, -radius * 0.7},
                    {radius * 0.7, -radius * 0.8}
                };
                
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 5; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            } else {
                double points[][2] = {
                    {radius * 0.95, -radius * 0.15},
                    {radius * 0.55, radius * 0.8},
                    {-radius * 0.65, radius * 0.75},
                    {-radius * 0.85, -radius * 0.5},
                    {radius * 0.6, -radius * 0.85},
                    {radius * 0.9, -radius * 0.3}
                };
                
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 6; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            }
        } else {
            // Small asteroid: simple variants with 3-4 points
            if (shape_variant == 0) {
                double points[][2] = {
                    {radius, 0},
                    {-radius * 0.7, radius * 0.7},
                    {-radius * 0.5, -radius * 0.8}
                };
                
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 3; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            } else if (shape_variant == 1) {
                double points[][2] = {
                    {radius * 0.9, radius * 0.2},
                    {-radius * 0.8, radius * 0.6},
                    {-radius * 0.6, -radius * 0.9}
                };
                
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 3; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            } else {
                double points[][2] = {
                    {radius, -radius * 0.3},
                    {-radius * 0.6, radius * 0.8},
                    {-radius * 0.7, -radius * 0.7},
                    {radius * 0.8, -radius * 0.1}
                };
                
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 4; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            }
        }
        
        cairo_close_path(cr);
        cairo_stroke(cr);
        
        cairo_restore(cr);
    }
}

void draw_comet_buster_bullets(CometBusterGame *game, cairo_t *cr, int width, int height) {
    if (!game) return;
    (void)width;    // Suppress unused parameter warning
    (void)height;   // Suppress unused parameter warning
    
    for (int i = 0; i < game->bullet_count; i++) {
        Bullet *b = &game->bullets[i];
        
        // Draw bullet as small diamond
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        cairo_set_line_width(cr, 1.0);
        
        double size = 3.0;
        cairo_move_to(cr, b->x + size, b->y);
        cairo_line_to(cr, b->x, b->y + size);
        cairo_line_to(cr, b->x - size, b->y);
        cairo_line_to(cr, b->x, b->y - size);
        cairo_close_path(cr);
        cairo_fill(cr);
        
        // Draw short trail (classic Asteroids style)
        double trail_length = 5.0;  // Much shorter trail
        double norm_len = sqrt(b->vx*b->vx + b->vy*b->vy);
        if (norm_len > 0.1) {
            double trail_x = b->x - (b->vx / norm_len) * trail_length;
            double trail_y = b->y - (b->vy / norm_len) * trail_length;
            cairo_move_to(cr, trail_x, trail_y);
            cairo_line_to(cr, b->x, b->y);
            cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.3);
            cairo_set_line_width(cr, 0.5);
            cairo_stroke(cr);
        }
    }
}

void draw_comet_buster_particles(CometBusterGame *game, cairo_t *cr, int width, int height) {
    if (!game) return;
    (void)width;    // Suppress unused parameter warning
    (void)height;   // Suppress unused parameter warning
    
    for (int i = 0; i < game->particle_count; i++) {
        Particle *p = &game->particles[i];
        
        double alpha = p->lifetime / p->max_lifetime;
        cairo_set_source_rgba(cr, p->color[0], p->color[1], p->color[2], alpha);
        cairo_arc(cr, p->x, p->y, p->size, 0, 2.0 * M_PI);
        cairo_fill(cr);
    }
}

void draw_comet_buster_ship(CometBusterGame *game, cairo_t *cr, int width, int height) {
    if (!game) return;
    (void)width;    // Suppress unused parameter warning
    (void)height;   // Suppress unused parameter warning
    
    cairo_save(cr);
    cairo_translate(cr, game->ship_x, game->ship_y);
    cairo_rotate(cr, game->ship_angle);
    
    // Ship as vector triangle (like Asteroids)
    double ship_size = 12;
    
    cairo_set_line_width(cr, 2.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    
    if (game->invulnerability_time > 0) {
        double alpha = sin(game->invulnerability_time * 10) * 0.5 + 0.5;
        cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, alpha);
    } else {
        cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
    }
    
    // Main triangle
    cairo_move_to(cr, ship_size, 0);
    cairo_line_to(cr, -ship_size, -ship_size);
    cairo_line_to(cr, -ship_size * 0.3, 0);
    cairo_line_to(cr, -ship_size, ship_size);
    cairo_close_path(cr);
    cairo_stroke(cr);
    
    // Muzzle flash
    if (game->muzzle_flash_timer > 0) {
        double alpha = game->muzzle_flash_timer / 0.1;
        cairo_move_to(cr, ship_size, 0);
        cairo_line_to(cr, ship_size + 20, -5);
        cairo_line_to(cr, ship_size + 20, 5);
        cairo_close_path(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, alpha);
        cairo_fill(cr);
    }
    
    cairo_restore(cr);
}

void draw_comet_buster_hud(CometBusterGame *game, cairo_t *cr, int width, int height) {
    if (!game) return;
    (void)height;   // Suppress unused parameter warning
    
    cairo_set_font_size(cr, 18);
    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    
    char text[256];
    
    // Score
    sprintf(text, "SCORE: %d", game->score);
    cairo_move_to(cr, 20, 30);
    cairo_show_text(cr, text);
    
    // Lives
    sprintf(text, "LIVES: %d", game->ship_lives);
    cairo_move_to(cr, 20, 55);
    cairo_show_text(cr, text);
    
    // Multiplier
    sprintf(text, "MULT: %.1fx", game->score_multiplier);
    cairo_move_to(cr, 20, 80);
    cairo_show_text(cr, text);
    
    // Wave
    sprintf(text, "WAVE: %d", game->current_wave);
    cairo_move_to(cr, width - 180, 30);
    cairo_show_text(cr, text);
    
    // Asteroids remaining
    sprintf(text, "ASTEROIDS: %d", game->comet_count);
    cairo_move_to(cr, width - 280, 55);
    cairo_show_text(cr, text);
    
    // Wave progress info (only show if wave is incomplete)
    if (game->comet_count > 0) {
        int expected_count = comet_buster_get_wave_comet_count(game->current_wave);
        sprintf(text, "DESTROYED: %d/%d", expected_count - game->comet_count, expected_count);
        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, width - 280, 75);
        cairo_show_text(cr, text);
        cairo_set_font_size(cr, 18);
    } else if (game->wave_complete_timer > 0) {
        sprintf(text, "WAVE COMPLETE! NEXT IN %.1fs", game->wave_complete_timer);
        cairo_set_font_size(cr, 16);
        cairo_set_source_rgb(cr, 0.0, 1.0, 0.5);
        cairo_move_to(cr, width / 2 - 120, height / 2 - 50);
        cairo_show_text(cr, text);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_set_font_size(cr, 18);
    }
    
    // Fuel bar (bottom left)
    cairo_set_font_size(cr, 14);
    sprintf(text, "FUEL: %.0f%%", game->fuel_amount);
    
    // Color based on fuel level
    if (game->fuel_amount < 20) {
        cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);  // Red - critical
    } else if (game->fuel_amount < 50) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);  // Yellow - low
    } else {
        cairo_set_source_rgb(cr, 0.2, 1.0, 0.2);  // Green - good
    }
    
    cairo_move_to(cr, 20, height - 40);
    cairo_show_text(cr, text);
    
    // Draw fuel bar (visual indicator)
    double bar_width = 150;
    double bar_height = 12;
    double bar_x = 20;
    double bar_y = height - 25;
    
    // Background (dark)
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_rectangle(cr, bar_x, bar_y, bar_width, bar_height);
    cairo_fill(cr);
    
    // Fuel level (colored)
    double fuel_percent = game->fuel_amount / game->max_fuel;
    if (fuel_percent > 0.5) {
        cairo_set_source_rgb(cr, 0.2, 1.0, 0.2);  // Green
    } else if (fuel_percent > 0.2) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);  // Yellow
    } else {
        cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);  // Red
    }
    cairo_rectangle(cr, bar_x, bar_y, bar_width * fuel_percent, bar_height);
    cairo_fill(cr);
    
    // Border
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, bar_x, bar_y, bar_width, bar_height);
    cairo_stroke(cr);
    
    // Boost indicator
    if (game->is_boosting && game->boost_thrust_timer > 0) {
        cairo_set_font_size(cr, 16);
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.8);
        cairo_move_to(cr, bar_x + bar_width + 20, height - 25);
        cairo_show_text(cr, "⚡ BOOST ⚡");
    }
}

void draw_comet_buster_game_over(CometBusterGame *game, cairo_t *cr, int width, int height) {
    if (!game || !game->game_over) return;
    
    // Overlay
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.6);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    
    // Title
    cairo_set_source_rgb(cr, 1.0, 0.3, 0.3);
    cairo_set_font_size(cr, 48);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_move_to(cr, width / 2 - 150, height / 2 - 80);
    cairo_show_text(cr, "GAME OVER!");
    
    // Score
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_font_size(cr, 24);
    char text[256];
    sprintf(text, "FINAL SCORE: %d", game->score);
    cairo_move_to(cr, width / 2 - 120, height / 2);
    cairo_show_text(cr, text);
    
    // Wave
    sprintf(text, "WAVE REACHED: %d", game->current_wave);
    cairo_move_to(cr, width / 2 - 100, height / 2 + 40);
    cairo_show_text(cr, text);
    
    // Restart prompt - pulsing text
    double pulse = sin(game->game_over_timer * 3) * 0.5 + 0.5;
    cairo_set_source_rgba(cr, 0.0, 1.0, 0.5, pulse);
    cairo_set_font_size(cr, 18);
    cairo_move_to(cr, width / 2 - 100, height / 2 + 100);
    cairo_show_text(cr, "RIGHT CLICK to restart");
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void comet_buster_wrap_position(double *x, double *y, int width, int height) {
    if (*x < -50) *x = width + 50;
    if (*x > width + 50) *x = -50;
    if (*y < -50) *y = height + 50;
    if (*y > height + 50) *y = -50;
}

double comet_buster_distance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx*dx + dy*dy);
}

void comet_buster_get_frequency_color(int frequency_band, double *r, double *g, double *b) {
    switch (frequency_band % 3) {
        case 0:  // Bass - red
            *r = 1.0; *g = 0.3; *b = 0.2;
            break;
        case 1:  // Mid - yellow
            *r = 1.0; *g = 1.0; *b = 0.2;
            break;
        case 2:  // Treble - blue
            *r = 0.2; *g = 0.8; *b = 1.0;
            break;
    }
}

// ============================================================================
// AUDIO INTEGRATION (Stubs)
// ============================================================================

void comet_buster_update_frequency_bands(CometBusterGame *game, void *visualizer) {
    if (!game) return;
    (void)visualizer;   // Suppress unused parameter warning
    game->frequency_bands[0] = 0.5;
    game->frequency_bands[1] = 0.5;
    game->frequency_bands[2] = 0.5;
}

void comet_buster_fire_on_beat(CometBusterGame *game) {
    if (!game) return;
}

bool comet_buster_detect_beat(void *vis) {
    (void)vis;   // Suppress unused parameter warning
    return false;
}

void comet_buster_increase_difficulty(CometBusterGame *game) {
    if (!game) return;
    game->base_spawn_rate *= 0.9;
    if (game->base_spawn_rate < 0.3) game->base_spawn_rate = 0.3;
}
