#include <cairo.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cometbuster.h"
#include "visualization.h"

#ifdef ExternalSound
#include "audio_wad.h"
#endif 
// ============================================================================
// STATIC MEMORY VERSION - NO MALLOC/FREE
// ============================================================================

// ============================================================================
// INITIALIZATION & CLEANUP
// ============================================================================

void init_comet_buster_system(void *vis) {
    Visualizer *visualizer = (Visualizer *)vis;
    
    comet_buster_reset_game(&visualizer->comet_buster);    
}

void comet_buster_cleanup(CometBusterGame *game) {
    if (!game) return;
    
    memset(game, 0, sizeof(CometBusterGame));
}

void comet_buster_reset_game(CometBusterGame *game) {
    if (!game) return;
    
    // Clear arrays explicitly instead of memset to avoid compiler warnings
    game->comet_count = 0;
    game->bullet_count = 0;
    game->particle_count = 0;
    game->floating_text_count = 0;
    game->high_score_count = 0;
    game->enemy_ship_count = 0;
    game->enemy_bullet_count = 0;
    
    // Initialize boss as inactive
    game->boss_active = false;
    game->boss.active = false;
    game->boss_spawn_timer = 0;
    game->last_boss_wave = 0;  // Track which wave had the boss
    
    // Initialize non-zero values - use defaults, will be set by visualizer if needed
    game->ship_x = 400.0;
    game->ship_y = 300.0;
    game->ship_vx = 0;
    game->ship_vy = 0;
    game->ship_angle = 0;
    game->ship_speed = 0;
    game->ship_lives = 3;
    game->invulnerability_time = 0;
    
    // Shield system
    game->shield_health = 3;           // Start with 3 shield points
    game->max_shield_health = 3;       // Maximum 3 shield points
    game->shield_regen_timer = 0;      // No regeneration timer at start
    game->shield_regen_delay = 3.0;    // 3 seconds before shield starts regenerating
    game->shield_regen_rate = 0.5;     // 0.5 shield points per second
    game->shield_impact_angle = 0;     // Impact angle (for visual)
    game->shield_impact_timer = 0;     // Impact timer
    
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
    game->enemy_ship_spawn_timer = 5.0;  // Start spawning enemy ships after 5 seconds
    game->enemy_ship_spawn_rate = 8.0;   // Spawn a new enemy ship every 8 seconds on average
    
    // Mouse state
    game->mouse_left_pressed = false;
    game->mouse_fire_cooldown = 0;
    game->mouse_right_pressed = false;
    game->mouse_middle_pressed = false;
    game->omni_fire_cooldown = 0;
    
    // Keyboard state
    game->keyboard.key_a_pressed = false;
    game->keyboard.key_d_pressed = false;
    game->keyboard.key_w_pressed = false;
    game->keyboard.key_s_pressed = false;
    game->keyboard.key_z_pressed = false;
    game->keyboard.key_x_pressed = false;
    game->keyboard.key_space_pressed = false;
    game->keyboard.key_ctrl_pressed = false;
    
    // Advanced thrusters (fuel system)
    game->energy_amount = 100.0;
    game->max_energy = 100.0;
    game->energy_burn_rate = 25.0;        // Burn 25 energy per second at full boost (much slower)
    game->energy_recharge_rate = 10.0;     // Recharge 10 energy per second when idle (much slower)
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
    
    // Mega comets most likely, then large, then medium, then small
    if (rnd < 40) {
        comet->size = COMET_MEGA;
        comet->radius = 50;
    } else if (rnd < 70) {
        comet->size = COMET_LARGE;
        comet->radius = 30;
    } else if (rnd < 90) {
        comet->size = COMET_MEDIUM;
        comet->radius = 20;
    } else {
        comet->size = COMET_SMALL;
        comet->radius = 10;
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
// ENEMY SHIP SPAWNING
// ============================================================================

void comet_buster_spawn_enemy_ship(CometBusterGame *game, int screen_width, int screen_height) {
    if (!game || game->enemy_ship_count >= MAX_ENEMY_SHIPS) {
        return;
    }
    
    int slot = game->enemy_ship_count;
    EnemyShip *ship = &game->enemy_ships[slot];
    
    memset(ship, 0, sizeof(EnemyShip));
    
    // Random edge to spawn from (now includes diagonals)
    int edge = rand() % 8;
    double speed = 80.0 + (rand() % 40);  // 80-120 pixels per second
    double diagonal_speed = speed / sqrt(2);  // Normalize diagonal speed
    
    // Randomly decide ship type:
    // 10% chance of aggressive red ship (attacks player)
    // 80% chance of patrol blue ship (shoots comets)
    // 15% chance of hunter green ship (shoots comets fast, chases if close)
    int type_roll = rand() % 100;
    if (type_roll < 10) {
        ship->ship_type = 1;  // Red (aggressive)
    } else if (type_roll < 90) {
        ship->ship_type = 0;  // Blue (patrol)
    } else {
        ship->ship_type = 2;  // Green (hunter)
    }
    
    switch (edge) {
        case 0:  // From left to right
            ship->x = -20;
            ship->y = 50 + (rand() % (screen_height - 100));
            ship->vx = speed;
            ship->vy = 0;
            ship->angle = 0;  // Facing right
            ship->base_vx = speed;
            ship->base_vy = 0;
            break;
        case 1:  // From right to left
            ship->x = screen_width + 20;
            ship->y = 50 + (rand() % (screen_height - 100));
            ship->vx = -speed;
            ship->vy = 0;
            ship->angle = M_PI;  // Facing left
            ship->base_vx = -speed;
            ship->base_vy = 0;
            break;
        case 2:  // From top to bottom
            ship->x = 50 + (rand() % (screen_width - 100));
            ship->y = -20;
            ship->vx = 0;
            ship->vy = speed;
            ship->angle = M_PI / 2;  // Facing down
            ship->base_vx = 0;
            ship->base_vy = speed;
            break;
        case 3:  // From bottom to top
            ship->x = 50 + (rand() % (screen_width - 100));
            ship->y = screen_height + 20;
            ship->vx = 0;
            ship->vy = -speed;
            ship->angle = 3 * M_PI / 2;  // Facing up
            ship->base_vx = 0;
            ship->base_vy = -speed;
            break;
        case 4:  // From top-left to bottom-right (diagonal)
            ship->x = -20;
            ship->y = -20;
            ship->vx = diagonal_speed;
            ship->vy = diagonal_speed;
            ship->angle = atan2(diagonal_speed, diagonal_speed);  // 45 degrees
            ship->base_vx = diagonal_speed;
            ship->base_vy = diagonal_speed;
            break;
        case 5:  // From top-right to bottom-left (diagonal)
            ship->x = screen_width + 20;
            ship->y = -20;
            ship->vx = -diagonal_speed;
            ship->vy = diagonal_speed;
            ship->angle = atan2(diagonal_speed, -diagonal_speed);  // 135 degrees
            ship->base_vx = -diagonal_speed;
            ship->base_vy = diagonal_speed;
            break;
        case 6:  // From bottom-left to top-right (diagonal)
            ship->x = -20;
            ship->y = screen_height + 20;
            ship->vx = diagonal_speed;
            ship->vy = -diagonal_speed;
            ship->angle = atan2(-diagonal_speed, diagonal_speed);  // 315 degrees
            ship->base_vx = diagonal_speed;
            ship->base_vy = -diagonal_speed;
            break;
        case 7:  // From bottom-right to top-left (diagonal)
            ship->x = screen_width + 20;
            ship->y = screen_height + 20;
            ship->vx = -diagonal_speed;
            ship->vy = -diagonal_speed;
            ship->angle = atan2(-diagonal_speed, -diagonal_speed);  // 225 degrees
            ship->base_vx = -diagonal_speed;
            ship->base_vy = -diagonal_speed;
            break;
    }
    
    ship->health = 1;
    ship->shoot_cooldown = 1.0 + (rand() % 20) / 10.0;  // Shoot after 1-3 seconds
    ship->path_time = 0.0;  // Start at beginning of sine wave
    ship->active = true;
    
    // Shield system for enemy ships (varies by type)
    if (ship->ship_type == 1) {
        // Red ships (aggressive): 2 shield points
        ship->max_shield_health = 2;
        ship->shield_health = 2;
    } else if (ship->ship_type == 2) {
        // Green ships (hunter): 3 shield points
        ship->max_shield_health = 3;
        ship->shield_health = 3;
    } else {
        // Blue ships (patrol): 3 shield points
        ship->max_shield_health = 3;
        ship->shield_health = 3;
    }
    
    ship->shield_impact_timer = 0;
    ship->shield_impact_angle = 0;
    
    game->enemy_ship_count++;
}

void comet_buster_spawn_enemy_bullet(CometBusterGame *game, double x, double y, double vx, double vy) {
    if (!game || game->enemy_bullet_count >= MAX_ENEMY_BULLETS) {
        return;
    }
    
    int slot = game->enemy_bullet_count;
    Bullet *bullet = &game->enemy_bullets[slot];
    
    bullet->x = x;
    bullet->y = y;
    bullet->vx = vx;
    bullet->vy = vy;
    bullet->angle = atan2(vy, vx);
    bullet->lifetime = 10.0;  // Bullets live for 10 seconds
    bullet->max_lifetime = 10.0;
    bullet->active = true;
    
    game->enemy_bullet_count++;
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
    
    // Reset boss.active flag so the boss can be spawned again in a future boss wave
    game->boss.active = false;
    
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
    // AND if boss is not active (boss must be defeated before next wave)
    if (game->comet_count == 0 && game->wave_complete_timer == 0 && !game->boss_active) {
        // All comets destroyed and no boss active - start countdown to next wave
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
    // Fire in all 32 directions (Last Starfighter style)
    if (!game) return;
    
    // Check if we have enough fuel (costs 30 fuel per omnidirectional burst)
    if (game->energy_amount < 30) {
        return;  // Not enough fuel
    }
    
    double bullet_speed = 400.0;
    int directions = 32;  // 32 directions in a circle
    
    for (int i = 0; i < directions; i++) {
        if (game->bullet_count >= MAX_BULLETS) break;
        
        int slot = game->bullet_count;
        Bullet *bullet = &game->bullets[slot];
        
        memset(bullet, 0, sizeof(Bullet));
        
        bullet->x = game->ship_x;
        bullet->y = game->ship_y;
        
        // Calculate angle for this direction (32 evenly spaced directions)
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
    game->energy_amount -= 30.0;
    if (game->energy_amount < 0) game->energy_amount = 0;
    
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

void comet_buster_update_ship(CometBusterGame *game, double dt, int mouse_x, int mouse_y, int width, int height, bool mouse_active) {
    if (game->game_over || !game) return;
    
    if (game->invulnerability_time > 0) {
        game->invulnerability_time -= dt;
    }
    
    // Check if any keyboard movement keys are pressed
    bool keyboard_active = game->keyboard.key_a_pressed || 
                          game->keyboard.key_d_pressed || 
                          game->keyboard.key_w_pressed || 
                          game->keyboard.key_s_pressed;
    
    // Use keyboard if any movement keys pressed, otherwise use mouse
    if (keyboard_active) {
        // KEYBOARD-BASED ARCADE CONTROLS
        // Rotation: A=left, D=right
        double rotation_speed = 6.0;  // Radians per second
        mouse_active=false;
        if (game->keyboard.key_a_pressed) {
            game->ship_angle -= rotation_speed * dt;
        }
        if (game->keyboard.key_d_pressed) {
            game->ship_angle += rotation_speed * dt;
        }
        
        // Normalize angle to [0, 2π)
        while (game->ship_angle < 0) game->ship_angle += 2.0 * M_PI;
        while (game->ship_angle >= 2.0 * M_PI) game->ship_angle -= 2.0 * M_PI;
        
        // Thrust: W=forward, S=backward
        double thrust_accel = 500.0;
        
        if (game->keyboard.key_w_pressed) {
            double thrust_vx = cos(game->ship_angle) * thrust_accel;
            double thrust_vy = sin(game->ship_angle) * thrust_accel;
            
            game->ship_vx += thrust_vx * dt;
            game->ship_vy += thrust_vy * dt;
        }
        
        if (game->keyboard.key_s_pressed) {
            double thrust_vx = cos(game->ship_angle) * thrust_accel;
            double thrust_vy = sin(game->ship_angle) * thrust_accel;
            
            game->ship_vx -= thrust_vx * dt;
            game->ship_vy -= thrust_vy * dt;
        }
    } else if (mouse_active) {
        // MOUSE-BASED CONTROLS (Original system - use mouse to aim)
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
        
        // Normal mouse-based movement: based on mouse distance
        double dx_move = mouse_x - game->ship_x;
        double dy_move = mouse_y - game->ship_y;
        double mouse_dist = sqrt(dx_move*dx_move + dy_move*dy_move);
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
        
        if (mouse_dist > 0.1) {
            game->ship_vx += (dx_move / mouse_dist) * accel_magnitude * dt;
            game->ship_vy += (dy_move / mouse_dist) * accel_magnitude * dt;
        }
    }
    
    // BOOST: X or SPACE key (works with both control schemes)
    // Boost adds significant forward acceleration
    // Requires at least 2.0 energy to prevent boosting during recharge
    if ((game->keyboard.key_x_pressed || game->keyboard.key_space_pressed) && game->energy_amount >= 2.0) {
        game->is_boosting = true;
        
        // Apply boost forward acceleration in facing direction
        double boost_accel = 800.0;  // Boost acceleration
        double boost_vx = cos(game->ship_angle) * boost_accel;
        double boost_vy = sin(game->ship_angle) * boost_accel;
        
        game->ship_vx += boost_vx * dt;
        game->ship_vy += boost_vy * dt;
    } else if (game->mouse_right_pressed && game->energy_amount >= 2.0) {
        // Right-click boost (mouse) - accelerate in facing direction
        // Requires at least 2.0 energy to prevent boosting during recharge
        game->is_boosting = true;
        
        double boost_accel = 800.0;  // Same as keyboard boost
        double boost_vx = cos(game->ship_angle) * boost_accel;
        double boost_vy = sin(game->ship_angle) * boost_accel;
        
        game->ship_vx += boost_vx * dt;
        game->ship_vy += boost_vy * dt;
    } else {
        game->is_boosting = false;
    }
    
    // Apply max velocity cap
    double max_speed = 400.0;
    double current_speed = sqrt(game->ship_vx * game->ship_vx + game->ship_vy * game->ship_vy);
    if (current_speed > max_speed) {
        game->ship_vx = (game->ship_vx / current_speed) * max_speed;
        game->ship_vy = (game->ship_vy / current_speed) * max_speed;
    }
    
    // Apply friction/drag
    double friction = 0.95;
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

void comet_buster_update_bullets(CometBusterGame *game, double dt, int width, int height, void *vis) {
    if (!game) return;
    
    for (int i = 0; i < game->bullet_count; i++) {
        Bullet *b = &game->bullets[i];
        
        // Remove inactive bullets from the list
        if (!b->active) {
            // Swap with last bullet
            if (i != game->bullet_count - 1) {
                game->bullets[i] = game->bullets[game->bullet_count - 1];
            }
            game->bullet_count--;
            i--;
            continue;
        }
        
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
                comet_buster_destroy_comet(game, j, width, height, vis);
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

void comet_buster_update_enemy_ships(CometBusterGame *game, double dt, int width, int height, void *vis) {
    if (!game) return;
    
    Visualizer *visualizer = (Visualizer *)vis;
    
    for (int i = 0; i < game->enemy_ship_count; i++) {
        EnemyShip *ship = &game->enemy_ships[i];
        
        if (!ship->active) continue;
        
        // Update shield impact timer
        if (ship->shield_impact_timer > 0) {
            ship->shield_impact_timer -= dt;
        }
        
        if (ship->ship_type == 1) {
            // AGGRESSIVE RED SHIP: Chase player
            double dx = game->ship_x - ship->x;
            double dy = game->ship_y - ship->y;
            double dist_to_player = sqrt(dx*dx + dy*dy);
            
            if (dist_to_player > 0.1) {
                // Move toward player at constant speed
                double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                if (base_speed < 1.0) base_speed = 100.0;  // Default speed if not set
                
                ship->vx = (dx / dist_to_player) * base_speed;
                ship->vy = (dy / dist_to_player) * base_speed;
                
                // Update angle to face player
                ship->angle = atan2(dy, dx);
            }
        } else if (ship->ship_type == 2) {
            // HUNTER GREEN SHIP: Follow sine wave, but chase player if close
            double dx = game->ship_x - ship->x;
            double dy = game->ship_y - ship->y;
            double dist_to_player = sqrt(dx*dx + dy*dy);
            
            double chase_range = 300.0;  // Switch to chasing if player within 300px
            
            if (dist_to_player < chase_range && dist_to_player > 0.1) {
                // Chase player
                double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                if (base_speed < 1.0) base_speed = 90.0;
                
                ship->vx = (dx / dist_to_player) * base_speed;
                ship->vy = (dy / dist_to_player) * base_speed;
                ship->angle = atan2(dy, dx);
            } else {
                // Follow sine wave pattern
                ship->path_time += dt;
                
                double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                
                if (base_speed > 0.1) {
                    double dir_x = ship->base_vx / base_speed;
                    double dir_y = ship->base_vy / base_speed;
                    
                    double perp_x = -dir_y;
                    double perp_y = dir_x;
                    
                    double wave_amplitude = 50.0;
                    double wave_frequency = 1.5;
                    double sine_offset = sin(ship->path_time * wave_frequency * M_PI) * wave_amplitude;
                    
                    ship->vx = dir_x * base_speed + perp_x * sine_offset;
                    ship->vy = dir_y * base_speed + perp_y * sine_offset;
                    
                    // Update angle to face direction of movement
                    ship->angle = atan2(ship->vy, ship->vx);
                }
            }
        } else {
            // PATROL BLUE SHIP: Follow sine wave pattern, avoid comets
            ship->path_time += dt;
            
            double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
            
            if (base_speed > 0.1) {
                // Normalize base direction
                double dir_x = ship->base_vx / base_speed;
                double dir_y = ship->base_vy / base_speed;
                
                // Perpendicular direction (for sine wave oscillation)
                double perp_x = -dir_y;
                double perp_y = dir_x;
                
                // Sine wave motion (classic Asteroids-style pattern)
                double wave_amplitude = 50.0;  // Pixels left/right to oscillate
                double wave_frequency = 1.5;   // Oscillations per second
                double sine_offset = sin(ship->path_time * wave_frequency * M_PI) * wave_amplitude;
                
                // Calculate velocity: base direction + sine wave perpendicular motion
                ship->vx = dir_x * base_speed + perp_x * sine_offset;
                ship->vy = dir_y * base_speed + perp_y * sine_offset;
            }
        }
        
        // Emergency collision avoidance (only when VERY close)
        double avoid_x = 0.0;
        double avoid_y = 0.0;
        double max_avoidance = 0.0;
        
        for (int j = 0; j < game->comet_count; j++) {
            Comet *comet = &game->comets[j];
            if (!comet->active) continue;
            
            double dx = ship->x - comet->x;
            double dy = ship->y - comet->y;
            double dist = sqrt(dx*dx + dy*dy);
            
            double collision_radius = 50.0;  // Only emergency dodge when very close
            
            if (dist < collision_radius && dist > 0.1) {
                double strength = (1.0 - (dist / collision_radius)) * 0.3;
                double norm_x = dx / dist;
                double norm_y = dy / dist;
                
                avoid_x += norm_x * strength;
                avoid_y += norm_y * strength;
                max_avoidance = (strength > max_avoidance) ? strength : max_avoidance;
            }
        }
        
        // Apply emergency avoidance only if collision imminent
        if (max_avoidance > 0.1) {
            double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
            if (base_speed < 1.0) base_speed = 100.0;
            
            double blend_factor = 0.2;
            ship->vx = ship->vx * (1.0 - blend_factor) + avoid_x * base_speed * blend_factor;
            ship->vy = ship->vy * (1.0 - blend_factor) + avoid_y * base_speed * blend_factor;
            
            // Normalize to maintain base speed
            double new_speed = sqrt(ship->vx*ship->vx + ship->vy*ship->vy);
            if (new_speed > 0.1) {
                ship->vx = (ship->vx / new_speed) * base_speed;
                ship->vy = (ship->vy / new_speed) * base_speed;
            }
        }
        
        // Update position
        ship->x += ship->vx * dt;
        ship->y += ship->vy * dt;
        
        // Remove if goes off screen
        if (ship->x < -50 || ship->x > width + 50 ||
            ship->y < -50 || ship->y > height + 50) {
            ship->active = false;
            
            if (i != game->enemy_ship_count - 1) {
                game->enemy_ships[i] = game->enemy_ships[game->enemy_ship_count - 1];
            }
            game->enemy_ship_count--;
            i--;
            continue;
        }
        
        // Update shooting
        if (ship->ship_type == 1) {
            // RED SHIPS: Shoot at player
            ship->shoot_cooldown -= dt;
            if (ship->shoot_cooldown <= 0) {
                double dx = game->ship_x - ship->x;
                double dy = game->ship_y - ship->y;
                double dist = sqrt(dx*dx + dy*dy);
                
                if (dist > 0.01) {
                    double bullet_speed = 150.0;
                    double vx = (dx / dist) * bullet_speed;
                    double vy = (dy / dist) * bullet_speed;
                    
                    comet_buster_spawn_enemy_bullet(game, ship->x, ship->y, vx, vy);
                    
                    // Play alien fire sound
#ifdef ExternalSound
                    if (visualizer && visualizer->audio.sfx_alien_fire) {
                        audio_play_sound(&visualizer->audio, visualizer->audio.sfx_alien_fire);
                    }
#endif
                    
                    // Aggressive ships shoot more frequently
                    ship->shoot_cooldown = 0.3 + (rand() % 50) / 100.0;  // 0.3-0.8 sec (faster)
                }
            }
        } else if (ship->ship_type == 2) {
            // GREEN SHIPS: Shoot at nearest comet VERY fast, OR at player if close
            double chase_range = 300.0;  // Range to start shooting at player
            double dx_player = game->ship_x - ship->x;
            double dy_player = game->ship_y - ship->y;
            double dist_to_player = sqrt(dx_player*dx_player + dy_player*dy_player);
            
            // Check if player is within range
            if (dist_to_player < chase_range) {
                // Shoot at player
                ship->shoot_cooldown -= dt;
                if (ship->shoot_cooldown <= 0) {
                    if (dist_to_player > 0.01) {
                        double bullet_speed = 150.0;
                        double vx = (dx_player / dist_to_player) * bullet_speed;
                        double vy = (dy_player / dist_to_player) * bullet_speed;
                        
                        comet_buster_spawn_enemy_bullet(game, ship->x, ship->y, vx, vy);
                        
                        // Play alien fire sound
#ifdef ExternalSound
                        if (visualizer && visualizer->audio.sfx_alien_fire) {
                            audio_play_sound(&visualizer->audio, visualizer->audio.sfx_alien_fire);
                        }
#endif
                        
                        // Green ships shoot VERY fast at player too
                        ship->shoot_cooldown = 0.15 + (rand() % 25) / 100.0;  // 0.15-0.4 sec (very fast!)
                    }
                }
            } else if (game->comet_count > 0) {
                // Player not in range, shoot at nearest comet instead
                ship->shoot_cooldown -= dt;
                if (ship->shoot_cooldown <= 0) {
                    // Find nearest comet
                    int nearest_comet_idx = -1;
                    double nearest_dist = 1e9;
                    
                    for (int j = 0; j < game->comet_count; j++) {
                        Comet *comet = &game->comets[j];
                        if (!comet->active) continue;
                        
                        double dx = comet->x - ship->x;
                        double dy = comet->y - ship->y;
                        double dist = sqrt(dx*dx + dy*dy);
                        
                        if (dist < nearest_dist) {
                            nearest_dist = dist;
                            nearest_comet_idx = j;
                        }
                    }
                    
                    // Shoot at nearest comet if in range
                    if (nearest_comet_idx >= 0 && nearest_dist < 600.0) {
                        Comet *target = &game->comets[nearest_comet_idx];
                        double dx = target->x - ship->x;
                        double dy = target->y - ship->y;
                        double dist = sqrt(dx*dx + dy*dy);
                        
                        if (dist > 0.01) {
                            double bullet_speed = 150.0;
                            double vx = (dx / dist) * bullet_speed;
                            double vy = (dy / dist) * bullet_speed;
                            
                            comet_buster_spawn_enemy_bullet(game, ship->x, ship->y, vx, vy);
                            
                            // Play alien fire sound
#ifdef ExternalSound
                            if (visualizer && visualizer->audio.sfx_alien_fire) {
                                audio_play_sound(&visualizer->audio, visualizer->audio.sfx_alien_fire);
                            }
#endif
                            
                            // Green ships shoot VERY fast
                            ship->shoot_cooldown = 0.15 + (rand() % 25) / 100.0;  // 0.15-0.4 sec (very fast!)
                        }
                    } else {
                        // Reload even if no target in range
                        ship->shoot_cooldown = 0.3;
                    }
                }
            }
        } else {
            // BLUE SHIPS: Shoot at nearest comet
            if (game->comet_count > 0) {
                ship->shoot_cooldown -= dt;
                if (ship->shoot_cooldown <= 0) {
                    // Find nearest comet
                    int nearest_comet_idx = -1;
                    double nearest_dist = 1e9;
                    
                    for (int j = 0; j < game->comet_count; j++) {
                        Comet *comet = &game->comets[j];
                        if (!comet->active) continue;
                        
                        double dx = comet->x - ship->x;
                        double dy = comet->y - ship->y;
                        double dist = sqrt(dx*dx + dy*dy);
                        
                        if (dist < nearest_dist) {
                            nearest_dist = dist;
                            nearest_comet_idx = j;
                        }
                    }
                    
                    // Shoot at nearest comet if in range
                    if (nearest_comet_idx >= 0 && nearest_dist < 500.0) {
                        Comet *target = &game->comets[nearest_comet_idx];
                        double dx = target->x - ship->x;
                        double dy = target->y - ship->y;
                        double dist = sqrt(dx*dx + dy*dy);
                        
                        if (dist > 0.01) {
                            double bullet_speed = 150.0;
                            double vx = (dx / dist) * bullet_speed;
                            double vy = (dy / dist) * bullet_speed;
                            
                            comet_buster_spawn_enemy_bullet(game, ship->x, ship->y, vx, vy);
                            
                            // Play alien fire sound
#ifdef ExternalSound
                            if (visualizer && visualizer->audio.sfx_alien_fire) {
                                audio_play_sound(&visualizer->audio, visualizer->audio.sfx_alien_fire);
                            }
#endif
                            
                            // Blue ships shoot less frequently
                            ship->shoot_cooldown = 0.8 + (rand() % 100) / 100.0;  // 0.8-1.8 sec
                        }
                    } else {
                        // Reload even if no target in range
                        ship->shoot_cooldown = 0.5;
                    }
                }
            }
        }
    }
    
    // Spawn new enemy ships randomly
    if (!game->game_over) {
        game->enemy_ship_spawn_timer -= dt;
        if (game->enemy_ship_spawn_timer <= 0) {
            if (game->enemy_ship_count < MAX_ENEMY_SHIPS) {
                comet_buster_spawn_enemy_ship(game, width, height);
            }
            game->enemy_ship_spawn_timer = game->enemy_ship_spawn_rate + (rand() % 300) / 100.0;
        }
    }
}

void comet_buster_update_enemy_bullets(CometBusterGame *game, double dt, int width, int height, void *vis) {
    if (!game) return;
    
    for (int i = 0; i < game->enemy_bullet_count; i++) {
        Bullet *b = &game->enemy_bullets[i];
        
        // Update lifetime
        b->lifetime -= dt;
        if (b->lifetime <= 0) {
            b->active = false;
            
            // Swap with last bullet
            if (i != game->enemy_bullet_count - 1) {
                game->enemy_bullets[i] = game->enemy_bullets[game->enemy_bullet_count - 1];
            }
            game->enemy_bullet_count--;
            i--;
            continue;
        }
        
        // Update position
        b->x += b->vx * dt;
        b->y += b->vy * dt;
        
        // Check collision with comets
        for (int j = 0; j < game->comet_count; j++) {
            Comet *c = &game->comets[j];
            if (!c->active) continue;
            
            if (comet_buster_check_bullet_comet(b, c)) {
                comet_buster_destroy_comet(game, j, width, height, vis);
                b->active = false;
                break;
            }
        }
        
        // Skip further checks if bullet was destroyed
        if (!b->active) {
            // Swap with last
            if (i != game->enemy_bullet_count - 1) {
                game->enemy_bullets[i] = game->enemy_bullets[game->enemy_bullet_count - 1];
            }
            game->enemy_bullet_count--;
            i--;
            continue;
        }
        
        // Remove if goes off screen
        if (b->x < -50 || b->x > width + 50 ||
            b->y < -50 || b->y > height + 50) {
            b->active = false;
            
            // Swap with last
            if (i != game->enemy_bullet_count - 1) {
                game->enemy_bullets[i] = game->enemy_bullets[game->enemy_bullet_count - 1];
            }
            game->enemy_bullet_count--;
            i--;
            continue;
        }
    }
}

void comet_buster_update_shooting(CometBusterGame *game, double dt, void *vis) {
    if (!game || game->game_over) return;
    
    // Update fire cooldown
    if (game->mouse_fire_cooldown > 0) {
        game->mouse_fire_cooldown -= dt;
    }
    
    // Update omnidirectional fire cooldown
    if (game->omni_fire_cooldown > 0) {
        game->omni_fire_cooldown -= dt;
    }
        
    // If left mouse button is held down, fire continuously (costs fuel)
    if (game->mouse_left_pressed) {
        if (game->mouse_fire_cooldown <= 0) {
            // Normal fire costs 0.25 fuel per bullet
            if (game->energy_amount >= 0.25) {
                comet_buster_spawn_bullet(game);
                game->energy_amount -= 0.25;  // Consume 0.25 fuel
                game->mouse_fire_cooldown = 0.05;  // ~20 bullets per second
                
                // Play fire sound
#ifdef ExternalSound
                Visualizer *visualizer = (Visualizer *)vis;
                if (visualizer && visualizer->audio.sfx_fire) {
                    audio_play_sound(&visualizer->audio, visualizer->audio.sfx_fire);
                }
#endif
            }
        }
    }
    
    // CTRL key fire (keyboard-based)
    if (game->keyboard.key_ctrl_pressed) {
        if (game->mouse_fire_cooldown <= 0) {
            // Normal fire costs 0.25 fuel per bullet
            if (game->energy_amount >= 0.25) {
                comet_buster_spawn_bullet(game);
                game->energy_amount -= 0.25;  // Consume 0.25 fuel
                game->mouse_fire_cooldown = 0.05;  // ~20 bullets per second
                
                // Play fire sound
#ifdef ExternalSound
                Visualizer *visualizer = (Visualizer *)vis;
                if (visualizer && visualizer->audio.sfx_fire) {
                    audio_play_sound(&visualizer->audio, visualizer->audio.sfx_fire);
                }
#endif
            }
        }
    }
    
    // Z key omnidirectional fire (32 directions)
    if (game->keyboard.key_z_pressed) {
        if (game->omni_fire_cooldown <= 0) {
            if (game->energy_amount >= 30) {
                comet_buster_spawn_omnidirectional_fire(game);  // This function handles energy drain
                game->omni_fire_cooldown = 0.3;  // Slower than normal fire
                
                // Play fire sound
#ifdef ExternalSound
                Visualizer *visualizer = (Visualizer *)vis;
                if (visualizer && visualizer->audio.sfx_fire) {
                    audio_play_sound(&visualizer->audio, visualizer->audio.sfx_fire);
                }
#endif
            }
        }
    }
    
    // If middle mouse button is pressed, fire omnidirectionally (Last Starfighter style)
    if (game->mouse_middle_pressed) {
        if (game->omni_fire_cooldown <= 0) {
            if (game->energy_amount >= 30) {
                comet_buster_spawn_omnidirectional_fire(game);  // This function handles energy drain
                game->omni_fire_cooldown = 0.3;  // Slower than normal fire
                
                // Play fire sound
#ifdef ExternalSound
                Visualizer *visualizer = (Visualizer *)vis;
                if (visualizer && visualizer->audio.sfx_fire) {
                    audio_play_sound(&visualizer->audio, visualizer->audio.sfx_fire);
                }
#endif
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
    if (game->is_boosting && game->energy_amount > 0) {
        // Burning fuel while boosting
        game->energy_amount -= game->energy_burn_rate * dt;
        if (game->energy_amount <= 0) {
            game->energy_amount = 0;
            game->is_boosting = false;
        }
    } else if (!game->mouse_left_pressed && !game->keyboard.key_ctrl_pressed) {
        // Recharging fuel when not boosting AND not firing (either mouse or keyboard)
        if (game->energy_amount < game->max_energy) {
            game->energy_amount += game->energy_recharge_rate * dt;
            if (game->energy_amount > game->max_energy) {
                game->energy_amount = game->max_energy;
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
    game->mouse_right_pressed = visualizer->mouse_right_pressed;
    game->mouse_middle_pressed = visualizer->mouse_middle_pressed;
    
#ifdef ExternalSound
        // Copy arcade-style keyboard input state from visualizer
    game->keyboard.key_a_pressed = visualizer->key_a_pressed;
    game->keyboard.key_d_pressed = visualizer->key_d_pressed;
    game->keyboard.key_w_pressed = visualizer->key_w_pressed;
    game->keyboard.key_s_pressed = visualizer->key_s_pressed;
    game->keyboard.key_z_pressed = visualizer->key_z_pressed;
    game->keyboard.key_x_pressed = visualizer->key_x_pressed;
    game->keyboard.key_space_pressed = visualizer->key_space_pressed;
    game->keyboard.key_ctrl_pressed = visualizer->key_ctrl_pressed;
    
    // If ANY keyboard movement key is pressed, disable mouse control immediately
    bool keyboard_active = visualizer->key_a_pressed || visualizer->key_d_pressed || 
                          visualizer->key_w_pressed || visualizer->key_s_pressed;
    bool mouse_active = visualizer->mouse_just_moved && !keyboard_active;
    
    // Update game state
    comet_buster_update_ship(game, dt, mouse_x, mouse_y, width, height, mouse_active);
#else
    comet_buster_update_ship(game, dt, mouse_x, mouse_y, width, height, true);
#endif

    comet_buster_update_comets(game, dt, width, height);
    comet_buster_update_shooting(game, dt, visualizer);  // Uses mouse_left_pressed state
    comet_buster_update_bullets(game, dt, width, height, visualizer);
    comet_buster_update_particles(game, dt);
    comet_buster_update_floating_text(game, dt);  // Update floating text popups
    comet_buster_update_fuel(game, dt);  // Update fuel system
    
    // Update shield regeneration
    if (game->shield_health < game->max_shield_health) {
        game->shield_regen_timer += dt;
        if (game->shield_regen_timer >= game->shield_regen_delay) {
            // Shield regeneration active
            double regen_amount = game->shield_regen_rate * dt;
            game->shield_health += regen_amount;
            if (game->shield_health > game->max_shield_health) {
                game->shield_health = game->max_shield_health;
            }
        }
    }
    
    // Update shield impact timer
    if (game->shield_impact_timer > 0) {
        game->shield_impact_timer -= dt;
    }
    
    comet_buster_update_enemy_ships(game, dt, width, height, visualizer);  // Update enemy ships
    comet_buster_update_enemy_bullets(game, dt, width, height, visualizer);  // Update enemy bullets
    
    // Update boss if active
    if (game->boss_active) {
        comet_buster_update_boss(game, dt, width, height);
    }
    
    // Spawn boss on waves 5, 10, 15, 20, etc. (every 5 waves starting at wave 5)
    // But only if the boss hasn't already been defeated this wave (game->wave_complete_timer == 0)
    if ((game->current_wave % 5 == 0) && !game->boss_active && game->comet_count == 0 && !game->boss.active && game->wave_complete_timer == 0) {
        fprintf(stdout, "[UPDATE] Conditions met to spawn boss: Wave=%d, BossActive=%d, CometCount=%d\n",
                game->current_wave, game->boss_active, game->comet_count);
        comet_buster_spawn_boss(game, width, height);
    }
    
    // Update fuel system
    comet_buster_update_fuel(game, dt);
    
    // Handle wave completion and progression (only if not already counting down)
    // BUT: Don't progress if boss is active (boss must be defeated first)
    if (game->wave_complete_timer <= 0 && !game->boss_active) {
        comet_buster_update_wave_progression(game);
    }
    
    // Handle wave complete timer (delay before next wave spawns)
    // If boss was just killed, spawn next wave after timer regardless of remaining comets
    if (game->wave_complete_timer > 0 && !game->boss_active) {
        game->wave_complete_timer -= dt;
        if (game->wave_complete_timer <= 0) {
            // Timer expired - spawn the next wave
            game->current_wave++;  // Increment wave
            comet_buster_spawn_wave(game, width, height);
            game->wave_complete_timer = 0;
        }
    }
    
    // Check ship-comet collisions
    for (int i = 0; i < game->comet_count; i++) {
        if (comet_buster_check_ship_comet(game, &game->comets[i])) {
            // Destroy the comet when ship hits it
            comet_buster_destroy_comet(game, i, width, height, visualizer);
            // Damage the ship
            comet_buster_on_ship_hit(game, visualizer);
            break;  // Exit loop since we just modified comet_count
        }
    }
    
    // Check bullet-enemy ship collisions
    for (int i = 0; i < game->enemy_ship_count; i++) {
        for (int j = 0; j < game->bullet_count; j++) {
            if (comet_buster_check_bullet_enemy_ship(&game->bullets[j], &game->enemy_ships[i])) {
                EnemyShip *enemy = &game->enemy_ships[i];
                
                // Check if this is a blue (patrol) ship that hasn't been provoked yet
                bool was_provoked = comet_buster_hit_enemy_ship_provoke(game, i);
                
                if (!was_provoked) {
                    // Not a blue ship, or already provoked - normal damage system
                    // Check if enemy has shield
                    if (enemy->shield_health > 0) {
                        // Shield absorbs the bullet
                        enemy->shield_health--;
                        enemy->shield_impact_angle = atan2(enemy->y - game->bullets[j].y, 
                                                            enemy->x - game->bullets[j].x);
                        enemy->shield_impact_timer = 0.2;
                        
                        // Play alien hit sound
#ifdef ExternalSound
                        if (visualizer && visualizer->audio.sfx_hit) {
                            audio_play_sound(&visualizer->audio, visualizer->audio.sfx_hit);
                        }
#endif
                    } else {
                        // No shield, destroy the ship
                        comet_buster_destroy_enemy_ship(game, i, width, height, visualizer);
                    }
                }
                // If it was provoked, the bullet just triggers the provocation but doesn't destroy it
                
                game->bullets[j].active = false;  // Bullet is consumed either way
                break;
            }
        }
    }
    
    // Check enemy bullet-ship collisions
    for (int i = 0; i < game->enemy_bullet_count; i++) {
        if (comet_buster_check_enemy_bullet_ship(game, &game->enemy_bullets[i])) {
            comet_buster_on_ship_hit(game, visualizer);
        }
    }
    
    // Check enemy ship-player ship collisions (both take damage)
    for (int i = 0; i < game->enemy_ship_count; i++) {
        EnemyShip *enemy_ship = &game->enemy_ships[i];
        if (!enemy_ship->active) continue;
        
        double dx = game->ship_x - enemy_ship->x;
        double dy = game->ship_y - enemy_ship->y;
        double dist = sqrt(dx*dx + dy*dy);
        double collision_dist = 15.0 + 15.0;  // Both ships have ~15px radius
        
        if (dist < collision_dist) {
            // Enemy ship is destroyed
            comet_buster_destroy_enemy_ship(game, i, width, height, visualizer);
            // Player ship takes damage
            comet_buster_on_ship_hit(game, visualizer);
            break;  // Exit loop since we just modified enemy_ship_count
        }
    }
    
    // Check enemy ship-comet collisions (ships take damage from comets)
    for (int i = 0; i < game->enemy_ship_count; i++) {
        for (int j = 0; j < game->comet_count; j++) {
            EnemyShip *ship = &game->enemy_ships[i];
            Comet *comet = &game->comets[j];
            if (!ship->active || !comet->active) continue;
            
            double dx = ship->x - comet->x;
            double dy = ship->y - comet->y;
            double dist = sqrt(dx*dx + dy*dy);
            double collision_dist = 30.0 + comet->radius;  // Ship radius ~ 30px
            
            if (dist < collision_dist) {
                // Check if ship has shield
                if (ship->shield_health > 0) {
                    // Shield absorbs the comet impact
                    ship->shield_health--;
                    ship->shield_impact_angle = atan2(ship->y - comet->y, 
                                                       ship->x - comet->x);
                    ship->shield_impact_timer = 0.2;
                } else {
                    // No shield, ship is destroyed
                    comet_buster_destroy_enemy_ship(game, i, width, height, visualizer);
                }
                
                // Comet is destroyed either way
                comet_buster_destroy_comet(game, j, width, height, visualizer);
                break;
            }
        }
    }
    
    // Check player bullets hitting boss
    if (game->boss_active) {
        for (int j = 0; j < game->bullet_count; j++) {
            if (comet_buster_check_bullet_boss(&game->bullets[j], &game->boss)) {
                game->bullets[j].active = false;  // Consume bullet
                
                // Shield reduces damage but doesn't block it
                bool shield_active = (game->boss.shield_active && game->boss.shield_health > 0);
                
                if (shield_active) {
                    // Shield takes damage and reduces boss damage
                    game->boss.shield_health--;
                    game->boss.shield_impact_timer = 0.2;
                    game->boss.shield_impact_angle = atan2(game->boss.y - game->bullets[j].y,
                                                           game->boss.x - game->bullets[j].x);
                    
                    // Boss still takes reduced damage (50% damage gets through shield)
                    game->boss.health--;  // Still damage the boss
                    game->boss.damage_flash_timer = 0.1;
                    game->consecutive_hits++;
                    
#ifdef ExternalSound
                    if (visualizer && visualizer->audio.sfx_hit) {
                        audio_play_sound(&visualizer->audio, visualizer->audio.sfx_hit);
                    }
#endif
                } else {
                    // No shield - full damage
                    game->boss.health -= 2;  // Double damage when no shield
                    game->boss.damage_flash_timer = 0.1;
                    game->consecutive_hits++;
                    
#ifdef ExternalSound
                    if (visualizer && visualizer->audio.sfx_hit) {
                        audio_play_sound(&visualizer->audio, visualizer->audio.sfx_hit);
                    }
#endif
                }
                
                if (game->boss.health <= 0) {
                    comet_buster_destroy_boss(game, width, height, visualizer);
                }
                break;
            }
        }
        
        // Check boss-player ship collisions
        double dx = game->ship_x - game->boss.x;
        double dy = game->ship_y - game->boss.y;
        double dist = sqrt(dx*dx + dy*dy);
        double collision_dist = 20.0 + 35.0;  // Player ~20px, Boss ~35px
        
        if (dist < collision_dist) {
            comet_buster_on_ship_hit(game, visualizer);
        }
    }
    
    // Update timers
    game->muzzle_flash_timer -= dt;
    game->difficulty_timer -= dt;
    if (game->game_over) {
        game->game_over_timer -= dt;
        
        // Handle right-click restart
        if (game->mouse_right_pressed) {
            comet_buster_reset_game(game);
        }
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

bool comet_buster_check_bullet_enemy_ship(Bullet *b, EnemyShip *e) {
    if (!b->active || !e->active) return false;
    
    double dx = b->x - e->x;
    double dy = b->y - e->y;
    double dist = sqrt(dx*dx + dy*dy);
    
    return dist < 15.0;  // Enemy ship collision radius is 15 pixels
}

bool comet_buster_check_enemy_bullet_ship(CometBusterGame *game, Bullet *b) {
    if (!b->active) return false;
    
    double dx = game->ship_x - b->x;
    double dy = game->ship_y - b->y;
    double dist = sqrt(dx*dx + dy*dy);
    
    return dist < 15.0;  // Player ship collision radius
}

void comet_buster_destroy_comet(CometBusterGame *game, int comet_index, int width, int height, void *vis) {
    (void)width;
    (void)height;
    if (comet_index < 0 || comet_index >= game->comet_count) return;
    
    Comet *c = &game->comets[comet_index];
    if (!c->active) return;
    
    // Create explosion
    int particle_count = 15;
    if (c->size == COMET_MEGA) particle_count = 30;
    else if (c->size == COMET_LARGE) particle_count = 20;
    else if (c->size == COMET_SMALL) particle_count = 8;
    
    comet_buster_spawn_explosion(game, c->x, c->y, c->frequency_band, particle_count);
    
    // Play explosion sound
    if (vis) {
        Visualizer *visualizer = (Visualizer *)vis;
#ifdef ExternalSound
        audio_play_sound(&visualizer->audio, visualizer->audio.sfx_explosion);
#endif
    }
    
    // Award points
    int points = 0;
    switch (c->size) {
        case COMET_SMALL: points = 50; break;
        case COMET_MEDIUM: points = 100; break;
        case COMET_LARGE: points = 200; break;
        case COMET_MEGA: points = 500; break;
        case COMET_SPECIAL: points = 500; break;
    }
    
    int score_add = (int)(points * game->score_multiplier);
    game->score += score_add;
    game->comets_destroyed++;
    game->consecutive_hits++;
    
    // Check for extra life bonus - every 100000 points
    int current_milestone = game->score / 100000;
    if (current_milestone > game->last_life_milestone) {
        game->ship_lives++;
        game->last_life_milestone = current_milestone;
        
        // Spawn floating text popup
        char text[32];
        snprintf(text, sizeof(text), "* +1 LIFE *");
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
    } else if (c->size == COMET_MEGA) {
        // Mega comets break into 3 large comets
        for (int i = 0; i < 3; i++) {
            if (game->comet_count >= MAX_COMETS) break;
            
            int slot = game->comet_count;
            Comet *child = &game->comets[slot];
            memset(child, 0, sizeof(Comet));
            
            // Spawn at parent location
            child->x = c->x + (rand() % 30 - 15);  // Slightly larger offset
            child->y = c->y + (rand() % 30 - 15);
            
            // Scatter in random direction
            double angle = (rand() % 360) * (M_PI / 180.0);
            double speed = 80.0 + (rand() % 80);
            child->vx = cos(angle) * speed;
            child->vy = sin(angle) * speed;
            
            // Set as large asteroid
            child->size = COMET_LARGE;
            child->radius = 30;
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

void comet_buster_destroy_enemy_ship(CometBusterGame *game, int ship_index, int width, int height, void *vis) {
    (void)width;
    (void)height;
    if (ship_index < 0 || ship_index >= game->enemy_ship_count) return;
    
    EnemyShip *ship = &game->enemy_ships[ship_index];
    if (!ship->active) return;
    
    // Create explosion
    comet_buster_spawn_explosion(game, ship->x, ship->y, 1, 12);  // Mid-frequency explosion
    
    // Play explosion sound
    if (vis) {
        Visualizer *visualizer = (Visualizer *)vis;
#ifdef ExternalSound
        audio_play_sound(&visualizer->audio, visualizer->audio.sfx_explosion);
#endif
    }
    
    // Award points
    int points = 300;  // Enemy ships worth 300 points
    int score_add = (int)(points * game->score_multiplier);
    game->score += score_add;
    game->consecutive_hits++;
    
    // Floating text
    char text[32];
    snprintf(text, sizeof(text), "+%d", score_add);
    comet_buster_spawn_floating_text(game, ship->x, ship->y, text, 0.0, 1.0, 0.0);  // Green
    
    // Increase multiplier
    if (game->consecutive_hits % 5 == 0) {
        game->score_multiplier += 0.1;
        if (game->score_multiplier > 5.0) game->score_multiplier = 5.0;
    }
    
    // Swap with last and remove
    if (ship_index != game->enemy_ship_count - 1) {
        game->enemy_ships[ship_index] = game->enemy_ships[game->enemy_ship_count - 1];
    }
    game->enemy_ship_count--;
}

// ============================================================================
// BOSS (DEATH STAR) FUNCTIONS
// ============================================================================

void comet_buster_spawn_boss(CometBusterGame *game, int screen_width, int screen_height) {
    if (!game) return;
    
    fprintf(stdout, "[SPAWN BOSS] Attempting to spawn boss at Wave %d\n", game->current_wave);
    
    BossShip *boss = &game->boss;
    memset(boss, 0, sizeof(BossShip));
    
    // Spawn boss off-screen at the top so it scrolls in
    boss->x = screen_width / 2.0;
    boss->y = -80.0;  // Start above the screen
    boss->vx = 40.0 + (rand() % 40);  // Slow horizontal movement
    boss->vy = 100.0;  // Scroll down at 100 pixels per second
    boss->angle = 0;
    
    // Boss health - reduced from 100 to 60
    boss->health = 60;
    boss->max_health = 60;
    
    // Shield system - reduced from 20 to 10, shield reduces damage but doesn't block it
    boss->shield_health = 10;
    boss->max_shield_health = 10;
    boss->shield_active = true;
    
    // Shooting
    boss->shoot_cooldown = 0;
    
    // Phases
    boss->phase = 0;  // Start in normal phase
    boss->phase_timer = 0;
    boss->phase_duration = 5.0;  // 5 seconds per phase
    
    // Visual
    boss->rotation = 0;
    boss->rotation_speed = 45.0;  // degrees per second
    boss->damage_flash_timer = 0;
    
    boss->active = true;
    game->boss_active = true;
    
    // Spawn some normal comets alongside the boss
    comet_buster_spawn_random_comets(game, 3, screen_width, screen_height);
    
    fprintf(stdout, "[SPAWN BOSS] Boss spawned! Position: (%.1f, %.1f), Active: %d, Health: %d\n", 
            boss->x, boss->y, boss->active, boss->health);
}

void comet_buster_update_boss(CometBusterGame *game, double dt, int width, int height) {
    if (!game || !game->boss_active) return;
    
    BossShip *boss = &game->boss;
    if (!boss->active) {
        game->boss_active = false;
        return;
    }
    
    // Update phase timer
    boss->phase_timer += dt;
    if (boss->phase_timer >= boss->phase_duration) {
        boss->phase_timer = 0;
        boss->phase = (boss->phase + 1) % 3;  // Cycle through phases: 0, 1, 2, 0, 1, 2...
        
        // Change shield state based on phase
        if (boss->phase == 1) {
            // Shield phase - shield reduces damage but doesn't block it
            boss->shield_active = true;
            boss->shield_health = boss->max_shield_health;
        } else {
            // Normal and enraged phases still have active shield, but it doesn't block
            boss->shield_active = false;
        }
    }
    
    // Movement - bounces off sides, stops scrolling down after entering screen
    boss->x += boss->vx * dt;
    
    // Only move down if boss is above the visible area
    if (boss->y < 100.0) {
        boss->y += boss->vy * dt;
    } else {
        // Once boss is in play area, stop vertical movement
        boss->vy = 0;
    }
    
    if (boss->x < 60 || boss->x > width - 60) {
        boss->vx = -boss->vx;  // Bounce
    }
    
    // Wrap position vertically
    if (boss->y > height + 100) {
        boss->active = false;
        game->boss_active = false;
        return;
    }
    
    // Update rotation
    boss->rotation += boss->rotation_speed * dt;
    
    // Update damage flash
    if (boss->damage_flash_timer > 0) {
        boss->damage_flash_timer -= dt;
    }
    
    // Firing pattern based on phase - all rates reduced
    boss->shoot_cooldown -= dt;
    
    if (boss->phase == 0) {
        // Normal phase - reduced from 0.5 to 0.8 seconds
        if (boss->shoot_cooldown <= 0) {
            comet_buster_boss_fire(game);
            boss->shoot_cooldown = 0.8;
        }
    } else if (boss->phase == 1) {
        // Shield phase - less firing, reduced from 0.7 to 1.0 seconds
        if (boss->shield_health < boss->max_shield_health) {
            boss->shield_health++;  // Regen 1 HP per frame
            if (boss->shield_health > boss->max_shield_health) {
                boss->shield_health = boss->max_shield_health;
            }
        }
        if (boss->shoot_cooldown <= 0) {
            comet_buster_boss_fire(game);
            boss->shoot_cooldown = 1.0;
        }
    } else if (boss->phase == 2) {
        // Enraged phase - still faster but reduced from 0.3 to 0.5 seconds
        if (boss->shoot_cooldown <= 0) {
            comet_buster_boss_fire(game);
            // Fire extra bullets in spread pattern
            comet_buster_boss_fire(game);
            boss->shoot_cooldown = 0.5;
        }
    }
}

void comet_buster_boss_fire(CometBusterGame *game) {
    if (!game || !game->boss_active) return;
    
    BossShip *boss = &game->boss;
    double bullet_speed = 180.0;
    
    // Calculate angle toward the player ship
    double dx = game->ship_x - boss->x;
    double dy = game->ship_y - boss->y;
    double angle_to_ship = atan2(dy, dx);
    
    // Fire fewer bullets in different patterns
    int num_bullets;
    double angle_spread;
    
    if (boss->phase == 2) {
        // Enraged phase - 3 bullets (reduced from 5)
        num_bullets = 3;
        angle_spread = 45.0 * M_PI / 180.0;  // Reduced spread from 60 to 45 degrees
    } else {
        // Normal and shield phases - 2 bullets (reduced from 3)
        num_bullets = 2;
        angle_spread = 30.0 * M_PI / 180.0;  // 30 degree spread
    }
    
    double start_angle = angle_to_ship - angle_spread / 2.0;
    
    for (int i = 0; i < num_bullets; i++) {
        double angle = start_angle + (angle_spread / (num_bullets - 1)) * i;
        double vx = cos(angle) * bullet_speed;
        double vy = sin(angle) * bullet_speed;
        
        comet_buster_spawn_enemy_bullet(game, boss->x, boss->y, vx, vy);
    }
}

bool comet_buster_check_bullet_boss(Bullet *b, BossShip *boss) {
    if (!b || !b->active || !boss || !boss->active) return false;
    
    double dx = boss->x - b->x;
    double dy = boss->y - b->y;
    double dist = sqrt(dx*dx + dy*dy);
    double collision_dist = 35.0;  // Boss collision radius
    
    return (dist < collision_dist);
}

void comet_buster_destroy_boss(CometBusterGame *game, int width, int height, void *vis) {
    (void)width;
    (void)height;
    
    if (!game || !game->boss_active) return;
    
    BossShip *boss = &game->boss;
    
    // Create large explosion
    comet_buster_spawn_explosion(game, boss->x, boss->y, 1, 60);  // HUGE explosion
    
    // Play explosion sound
    if (vis) {
        Visualizer *visualizer = (Visualizer *)vis;
#ifdef ExternalSound
        audio_play_sound(&visualizer->audio, visualizer->audio.sfx_explosion);
#endif
    }
    
    // Award MASSIVE points
    int points = 5000;  // Boss worth 5000 points!
    int score_add = (int)(points * game->score_multiplier);
    game->score += score_add;
    game->consecutive_hits += 10;  // Big hit bonus
    
    // Floating text - BIG text
    char text[64];
    snprintf(text, sizeof(text), "BOSS DESTROYED! +%d", score_add);
    comet_buster_spawn_floating_text(game, boss->x, boss->y, text, 1.0, 1.0, 0.0);  // Yellow
    
    // Increase multiplier significantly
    game->score_multiplier += 1.0;
    if (game->score_multiplier > 5.0) game->score_multiplier = 5.0;
    
    boss->active = false;
    game->boss_active = false;
    
    // Reset boss.active flag for future boss waves
    game->boss.active = false;
    
    // Set wave complete timer for 2 second countdown before next wave
    // Boss is dead, so wave will progress after countdown regardless of remaining comets
    game->wave_complete_timer = 2.0;
}


void comet_buster_on_ship_hit(CometBusterGame *game, Visualizer *visualizer) {
    if (game->invulnerability_time > 0) return;
    
    // Play hit sound
#ifdef ExternalSound
    if (visualizer) {
        audio_play_sound(&visualizer->audio, visualizer->audio.sfx_hit);
    }
#endif
    
    // Priority 1: Try to use 80% energy to absorb the hit
    if (game->energy_amount >= 80.0) {
        game->energy_amount -= 80.0;
        
        // Floating text for energy absorption
        comet_buster_spawn_floating_text(game, game->ship_x, game->ship_y - 30, 
                                         "ENERGY USED", 1.0, 1.0, 0.0);  // Yellow
        
        // Minor invulnerability for energy hit
        game->invulnerability_time = 0.5;
        return;
    }
    
    // If energy is less than 80%, it drains to zero (but still allows shield check)
    if (game->energy_amount > 0) {
        game->energy_amount = 0;
        
        // Floating text for energy drain
        comet_buster_spawn_floating_text(game, game->ship_x, game->ship_y - 30, 
                                         "ENERGY DRAINED", 1.0, 0.5, 0.0);  // Orange
        
        // Don't return here - continue to shield check!
    }
    
    // Priority 2: Use shield if available (either after energy drain or if energy was already 0)
    if (game->shield_health > 0) {
        game->shield_health--;
        game->shield_regen_timer = 0;  // Reset regen timer
        
        // Track impact angle for visual effect (angle from ship to source of hit)
        // We don't have exact hit source, so just use a random direction
        game->shield_impact_angle = (rand() % 360) * (M_PI / 180.0);
        game->shield_impact_timer = 0.2;  // Flash for 0.2 seconds
        
        // Floating text for shield hit
        comet_buster_spawn_floating_text(game, game->ship_x, game->ship_y - 30, 
                                         "SHIELD HIT", 0.0, 1.0, 1.0);  // Cyan
        
        // Minor invulnerability for shield hit
        game->invulnerability_time = 0.5;
        return;
    }
    
    // Priority 3: If energy is zero and shield is down, take actual damage (lose life)
    game->ship_lives--;
    game->consecutive_hits = 0;
    game->score_multiplier = 1.0;
    game->shield_regen_timer = 0;  // Reset shield regen after taking life damage
    
    // Reset shield to full when taking life damage
    game->shield_health = game->max_shield_health;
    game->shield_impact_timer = 0;
    
    if (game->ship_lives <= 0) {
        game->game_over = true;
        game->game_over_timer = 3.0;
        
        // Play game over sound effect
        #ifdef ExternalSound
        if (visualizer && visualizer->audio.sfx_game_over) {
            audio_play_sound(&visualizer->audio, visualizer->audio.sfx_game_over);
        }
        #endif
        
        // Don't add high score here - let the GUI dialog handle player name entry
        // The high score will be added when player submits their name in the dialog
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
    // Initialize high scores array
    for (int i = 0; i < MAX_HIGH_SCORES; i++) {
        game->high_scores[i].score = 0;
        game->high_scores[i].wave = 0;
        game->high_scores[i].timestamp = 0;
        game->high_scores[i].player_name[0] = '\0';
    }
    // Note: Actual loading is done by comet_main.cpp (high_scores_load function)
    // This function is kept here for API compatibility
}

void comet_buster_save_high_scores(CometBusterGame *game) {
    if (!game) return;
    // Note: Actual saving is done by comet_main.cpp (high_scores_save function)
    // This function is kept here for API compatibility
}

void comet_buster_add_high_score(CometBusterGame *game, int score, int wave, const char *name) {
    return;
    if (!game || !name) return;
    // Find insertion position to maintain sorted order (highest score first)
    int insert_pos = game->high_score_count;
    for (int i = 0; i < game->high_score_count; i++) {
        if (score > game->high_scores[i].score) {
            insert_pos = i;
            break;
        }
    }
    
    // Don't add if score is below the 10th place and we're already full
    if (insert_pos >= MAX_HIGH_SCORES) {
        return;
    }
    
    // Shift scores down to make room, removing the lowest score if list is full
    if (game->high_score_count >= MAX_HIGH_SCORES) {
        // List is full, shift entries down from the insert position
        for (int i = MAX_HIGH_SCORES - 1; i > insert_pos; i--) {
            game->high_scores[i] = game->high_scores[i - 1];
        }
    } else {
        // List has room, shift entries down and increment count
        for (int i = game->high_score_count; i > insert_pos; i--) {
            game->high_scores[i] = game->high_scores[i - 1];
        }
        game->high_score_count++;
    }
    
    // Insert the new high score at the correct position
    HighScore *hs = &game->high_scores[insert_pos];
    hs->score = score;
    hs->wave = wave;
    hs->timestamp = time(NULL);
    strncpy(hs->player_name, name, sizeof(hs->player_name) - 1);
    hs->player_name[sizeof(hs->player_name) - 1] = '\0';
    
    fprintf(stdout, "[HIGH SCORE] Added new high score: %d points at position %d\n", score, insert_pos + 1);
}

bool comet_buster_is_high_score(CometBusterGame *game, int score) {
    if (!game) return false;
    
    // If we have fewer than 10 scores, any score is a high score
    if (game->high_score_count < MAX_HIGH_SCORES) return true;
    
    // If we have 10 scores, check if the score beats the lowest (10th) score
    if (game->high_score_count >= MAX_HIGH_SCORES) {
        return score > game->high_scores[MAX_HIGH_SCORES - 1].score;
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
    draw_comet_buster_enemy_ships(game, cr, width, height);
    draw_comet_buster_boss(&game->boss, cr, width, height);  // Draw boss if active
    draw_comet_buster_enemy_bullets(game, cr, width, height);
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
        
        if (c->size == COMET_MEGA) {
            // Mega asteroid: giant 12+ pointed shape that appears on boss waves (wave % 5 == 0)
            // Thicker lines for emphasis
            cairo_set_line_width(cr, 3.5);
            
            if (shape_variant == 0) {
                // Complex mega variant 1
                double points[][2] = {
                    {radius, 0},
                    {radius * 0.8, radius * 0.55},
                    {radius * 0.6, radius * 0.9},
                    {radius * 0.2, radius * 0.95},
                    {-radius * 0.4, radius * 0.85},
                    {-radius * 0.75, radius * 0.65},
                    {-radius * 0.95, radius * 0.2},
                    {-radius * 0.9, -radius * 0.35},
                    {-radius * 0.6, -radius * 0.8},
                    {-radius * 0.1, -radius * 0.95},
                    {radius * 0.5, -radius * 0.85},
                    {radius * 0.85, -radius * 0.5}
                };
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 12; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            } else if (shape_variant == 1) {
                // Complex mega variant 2
                double points[][2] = {
                    {radius * 0.95, radius * 0.15},
                    {radius * 0.7, radius * 0.75},
                    {radius * 0.3, radius * 0.95},
                    {-radius * 0.2, radius * 0.9},
                    {-radius * 0.65, radius * 0.75},
                    {-radius * 0.9, radius * 0.3},
                    {-radius * 0.95, -radius * 0.2},
                    {-radius * 0.75, -radius * 0.7},
                    {-radius * 0.35, -radius * 0.92},
                    {radius * 0.15, -radius * 0.95},
                    {radius * 0.65, -radius * 0.75},
                    {radius * 0.9, -radius * 0.35}
                };
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 12; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            } else {
                // Complex mega variant 3
                double points[][2] = {
                    {radius, -radius * 0.1},
                    {radius * 0.8, radius * 0.6},
                    {radius * 0.5, radius * 0.88},
                    {radius * 0.1, radius * 0.96},
                    {-radius * 0.35, radius * 0.88},
                    {-radius * 0.7, radius * 0.7},
                    {-radius * 0.95, radius * 0.15},
                    {-radius * 0.88, -radius * 0.4},
                    {-radius * 0.55, -radius * 0.85},
                    {-radius * 0.05, -radius * 0.96},
                    {radius * 0.6, -radius * 0.8},
                    {radius * 0.9, -radius * 0.4}
                };
                cairo_move_to(cr, points[0][0], points[0][1]);
                for (int j = 1; j < 12; j++) {
                    cairo_line_to(cr, points[j][0], points[j][1]);
                }
            }
        } else if (c->size == COMET_LARGE) {
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

void draw_comet_buster_enemy_ships(CometBusterGame *game, cairo_t *cr, int width, int height) {
    if (!game) return;
    (void)width;    // Suppress unused parameter warning
    (void)height;   // Suppress unused parameter warning
    
    for (int i = 0; i < game->enemy_ship_count; i++) {
        EnemyShip *ship = &game->enemy_ships[i];
        
        if (!ship->active) continue;
        
        // Draw enemy ship as a triangle (pointing in direction angle)
        cairo_save(cr);
        cairo_translate(cr, ship->x, ship->y);
        cairo_rotate(cr, ship->angle);
        
        // Choose color based on ship type
        if (ship->ship_type == 1) {
            // Aggressive red ship
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);  // Bright red
        } else if (ship->ship_type == 2) {
            // Hunter green ship
            cairo_set_source_rgb(cr, 0.2, 1.0, 0.2);  // Bright green
        } else {
            // Patrol blue ship
            cairo_set_source_rgb(cr, 0.2, 0.6, 1.0);  // Blue
        }
        
        cairo_set_line_width(cr, 1.5);
        
        // Draw ship body as triangle
        double ship_size = 12;
        cairo_move_to(cr, ship_size, 0);              // Front point
        cairo_line_to(cr, -ship_size, -ship_size/1.5);  // Back left
        cairo_line_to(cr, -ship_size, ship_size/1.5);   // Back right
        cairo_close_path(cr);
        cairo_fill_preserve(cr);
        cairo_stroke(cr);
        
        // Draw health indicator (single bar at top of ship)
        cairo_set_source_rgb(cr, 0.2, 1.0, 0.2);  // Green
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, ship_size - 5, -ship_size - 3);
        cairo_line_to(cr, ship_size - 5, -ship_size);
        cairo_stroke(cr);
        
        cairo_restore(cr);
        
        // Draw shield circle around enemy ship if it has shields
        if (ship->shield_health > 0) {
            cairo_save(cr);
            cairo_translate(cr, ship->x, ship->y);
            
            // Shield color based on ship type
            if (ship->ship_type == 1) {
                // Red ship shield: orange/red
                cairo_set_source_rgba(cr, 1.0, 0.5, 0.0, 0.5);
            } else if (ship->ship_type == 2) {
                // Green ship shield: bright green
                cairo_set_source_rgba(cr, 0.5, 1.0, 0.5, 0.5);
            } else {
                // Blue ship: no shield (shouldn't reach here)
                cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.5);
            }
            
            cairo_set_line_width(cr, 2.0);
            cairo_arc(cr, 0, 0, 22, 0, 2 * M_PI);
            cairo_stroke(cr);
            
            // Draw shield impact flash
            if (ship->shield_impact_timer > 0) {
                double impact_x = 22 * cos(ship->shield_impact_angle);
                double impact_y = 22 * sin(ship->shield_impact_angle);
                double flash_alpha = ship->shield_impact_timer / 0.2;
                
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, flash_alpha * 0.8);
                cairo_arc(cr, impact_x, impact_y, 4, 0, 2 * M_PI);
                cairo_fill(cr);
                
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, flash_alpha * 0.4);
                cairo_set_line_width(cr, 1.0);
                double ring_radius = 6 + (1.0 - flash_alpha) * 10;
                cairo_arc(cr, impact_x, impact_y, ring_radius, 0, 2 * M_PI);
                cairo_stroke(cr);
            }
            
            cairo_restore(cr);
        }
    }
}

void draw_comet_buster_enemy_bullets(CometBusterGame *game, cairo_t *cr, int width, int height) {
    if (!game) return;
    (void)width;    // Suppress unused parameter warning
    (void)height;   // Suppress unused parameter warning
    
    for (int i = 0; i < game->enemy_bullet_count; i++) {
        Bullet *b = &game->enemy_bullets[i];
        
        // Draw enemy bullets as small cyan circles (different from player's yellow)
        cairo_set_source_rgb(cr, 0.0, 1.0, 1.0);  // Cyan - very distinct from yellow
        cairo_arc(cr, b->x, b->y, 2.5, 0, 2.0 * M_PI);
        cairo_fill(cr);
        
        // Draw trail
        double trail_length = 4.0;
        double norm_len = sqrt(b->vx*b->vx + b->vy*b->vy);
        if (norm_len > 0.1) {
            double trail_x = b->x - (b->vx / norm_len) * trail_length;
            double trail_y = b->y - (b->vy / norm_len) * trail_length;
            cairo_move_to(cr, trail_x, trail_y);
            cairo_line_to(cr, b->x, b->y);
            cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 0.3);  // Cyan trail
            cairo_set_line_width(cr, 0.5);
            cairo_stroke(cr);
        }
    }
}

void draw_comet_buster_boss(BossShip *boss, cairo_t *cr, int width, int height) {
    (void)width;
    (void)height;
    
    if (!boss) {
        fprintf(stderr, "[DRAW BOSS] ERROR: boss pointer is NULL\n");
        return;
    }
    
    if (!boss->active) {
        // Boss not active - this is normal when no boss spawned
        return;
    }
    
    fprintf(stdout, "[DRAW BOSS] Drawing boss at (%.1f, %.1f) - Health: %d, Active: %d\n", 
            boss->x, boss->y, boss->health, boss->active);
    
    // Draw the boss (death star) as a large circle with rotating patterns
    cairo_save(cr);
    cairo_translate(cr, boss->x, boss->y);
    cairo_rotate(cr, boss->rotation * M_PI / 180.0);  // Convert degrees to radians
    
    // Main body - large circle
    double body_radius = 35.0;
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.4);  // Dark gray
    cairo_arc(cr, 0, 0, body_radius, 0, 2.0 * M_PI);
    cairo_fill(cr);
    
    // Highlight if taking damage
    if (boss->damage_flash_timer > 0) {
        cairo_set_source_rgba(cr, 1.0, 0.5, 0.5, 0.7);  // Red flash
        cairo_arc(cr, 0, 0, body_radius, 0, 2.0 * M_PI);
        cairo_fill(cr);
    }
    
    // Outer ring - metallic look
    cairo_set_source_rgba(cr, 0.6, 0.6, 0.7, 0.8);
    cairo_set_line_width(cr, 2.5);
    cairo_arc(cr, 0, 0, body_radius, 0, 2.0 * M_PI);
    cairo_stroke(cr);
    
    // Draw rotating pattern on the boss
    cairo_set_line_width(cr, 1.5);
    for (int i = 0; i < 8; i++) {
        double angle = (i * 2.0 * M_PI / 8.0);
        double x1 = cos(angle) * 20.0;
        double y1 = sin(angle) * 20.0;
        double x2 = cos(angle) * 30.0;
        double y2 = sin(angle) * 30.0;
        
        cairo_set_source_rgb(cr, 0.8, 0.8, 0.9);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }
    
    // Core/center glow (pulsing)
    double core_radius = 8.0;
    cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);  // Red core
    cairo_arc(cr, 0, 0, core_radius, 0, 2.0 * M_PI);
    cairo_fill(cr);
    
    // Inner glow
    cairo_set_source_rgba(cr, 1.0, 0.3, 0.3, 0.6);
    cairo_arc(cr, 0, 0, core_radius + 3.0, 0, 2.0 * M_PI);
    cairo_stroke(cr);
    
    cairo_restore(cr);
    
    // Draw health bar above boss (Red)
    double bar_width = 80.0;
    double bar_height = 6.0;
    double bar_x = boss->x - bar_width / 2.0;
    double bar_y = boss->y - 50.0;
    
    // Background (gray)
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_rectangle(cr, bar_x, bar_y, bar_width, bar_height);
    cairo_fill(cr);
    
    // Health fill
    double health_ratio = (double)boss->health / boss->max_health;
    cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);  // Red
    cairo_rectangle(cr, bar_x, bar_y, bar_width * health_ratio, bar_height);
    cairo_fill(cr);
    
    // Border
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, bar_x, bar_y, bar_width, bar_height);
    cairo_stroke(cr);
    
    // Draw shield if active
    if (boss->shield_active && boss->shield_health > 0) {
        // Shield circle (larger than body)
        double shield_radius = 50.0;
        double shield_ratio = (double)boss->shield_health / boss->max_shield_health;
        
        cairo_save(cr);
        cairo_translate(cr, boss->x, boss->y);
        
        // Outer shield glow (pulsing)
        cairo_set_source_rgba(cr, 0.0, 0.8, 1.0, 0.3 + 0.1 * sin(boss->shield_impact_timer * 10.0));
        cairo_arc(cr, 0, 0, shield_radius, 0, 2.0 * M_PI);
        cairo_fill(cr);
        
        // Shield outline
        cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 0.8);
        cairo_set_line_width(cr, 2.0);
        cairo_arc(cr, 0, 0, shield_radius, 0, 2.0 * M_PI);
        cairo_stroke(cr);
        
        // Shield segments
        int num_segments = 12;
        for (int i = 0; i < num_segments; i++) {
            if (i < (num_segments * shield_ratio)) {
                double angle = (i * 2.0 * M_PI / num_segments);
                double x1 = cos(angle) * (shield_radius - 3.0);
                double y1 = sin(angle) * (shield_radius - 3.0);
                double x2 = cos(angle) * (shield_radius + 3.0);
                double y2 = sin(angle) * (shield_radius + 3.0);
                
                cairo_set_source_rgb(cr, 0.0, 1.0, 1.0);
                cairo_set_line_width(cr, 1.5);
                cairo_move_to(cr, x1, y1);
                cairo_line_to(cr, x2, y2);
                cairo_stroke(cr);
            }
        }
        
        cairo_restore(cr);
    }
    
    // Draw phase indicator (top left of boss)
    double phase_x = boss->x - 25;
    double phase_y = boss->y - 25;
    
    const char *phase_text;
    double phase_r, phase_g, phase_b;
    
    if (boss->phase == 0) {
        phase_text = "NORMAL";
        phase_r = 1.0; phase_g = 1.0; phase_b = 0.5;  // Yellow
    } else if (boss->phase == 1) {
        phase_text = "SHIELDED";
        phase_r = 0.0; phase_g = 1.0; phase_b = 1.0;  // Cyan
    } else {
        phase_text = "ENRAGED!";
        phase_r = 1.0; phase_g = 0.2; phase_b = 0.2;  // Red
    }
    
    cairo_set_source_rgb(cr, phase_r, phase_g, phase_b);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10);
    cairo_move_to(cr, phase_x, phase_y);
    cairo_show_text(cr, phase_text);
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
    
    // Draw shield circle around ship
    if (game->shield_health > 0) {
        cairo_save(cr);
        cairo_translate(cr, game->ship_x, game->ship_y);
        
        // Shield circle - brighter when more healthy
        double shield_alpha = (double)game->shield_health / game->max_shield_health;
        
        // Shield color: cyan when healthy, orange when low
        if (game->shield_health >= 2) {
            cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, shield_alpha * 0.6);  // Bright cyan
        } else if (game->shield_health >= 1) {
            cairo_set_source_rgba(cr, 1.0, 0.8, 0.0, shield_alpha * 0.6);  // Orange/yellow
        } else {
            cairo_set_source_rgba(cr, 1.0, 0.3, 0.3, shield_alpha * 0.6);  // Red warning
        }
        
        cairo_set_line_width(cr, 2.5);
        cairo_arc(cr, 0, 0, 28, 0, 2 * M_PI);
        cairo_stroke(cr);
        
        // Draw shield segments/pips to show health
        cairo_set_line_width(cr, 1.5);
        double segment_angle = (2 * M_PI) / game->max_shield_health;
        
        for (int i = 0; i < game->shield_health; i++) {
            double angle = (i * segment_angle) - (M_PI / 2);  // Start at top
            double x1 = 24 * cos(angle);
            double y1 = 24 * sin(angle);
            double x2 = 32 * cos(angle);
            double y2 = 32 * sin(angle);
            
            cairo_move_to(cr, x1, y1);
            cairo_line_to(cr, x2, y2);
            cairo_stroke(cr);
        }
        
        // Draw impact flash at hit point
        if (game->shield_impact_timer > 0) {
            double impact_x = 28 * cos(game->shield_impact_angle);
            double impact_y = 28 * sin(game->shield_impact_angle);
            double flash_alpha = game->shield_impact_timer / 0.2;  // Fade out over time
            
            // Bright white flash at impact point
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, flash_alpha * 0.8);
            cairo_arc(cr, impact_x, impact_y, 5, 0, 2 * M_PI);
            cairo_fill(cr);
            
            // Expanding rings at impact
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, flash_alpha * 0.4);
            cairo_set_line_width(cr, 1.0);
            double ring_radius = 8 + (1.0 - flash_alpha) * 12;
            cairo_arc(cr, impact_x, impact_y, ring_radius, 0, 2 * M_PI);
            cairo_stroke(cr);
        }
        
        cairo_restore(cr);
    }
}

void draw_comet_buster_hud(CometBusterGame *game, cairo_t *cr, int width, int height) {
    if (!game) return;
    (void)height;   // Suppress unused parameter warning
    
    cairo_set_font_size(cr, 18);
    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    
    char text[256];
    
    // Score (with multiplier indicator)
    sprintf(text, "SCORE: %d (x%.1f)", game->score, game->score_multiplier);
    cairo_move_to(cr, 20, 30);
    cairo_show_text(cr, text);
    
    // Lives
    sprintf(text, "LIVES: %d", game->ship_lives);
    cairo_move_to(cr, 20, 55);
    cairo_show_text(cr, text);
    
    // Shield status
    sprintf(text, "SHIELD: %d/%d", game->shield_health, game->max_shield_health);
    if (game->shield_health <= 0) {
        cairo_set_source_rgb(cr, 1.0, 0.3, 0.3);  // Red when no shield
    } else if (game->shield_health == 1) {
        cairo_set_source_rgb(cr, 1.0, 0.8, 0.0);  // Orange when low
    } else {
        cairo_set_source_rgb(cr, 0.0, 1.0, 1.0);  // Cyan when healthy
    }
    cairo_move_to(cr, 20, 105);
    cairo_show_text(cr, text);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);  // Reset to white
    
    // Wave
    sprintf(text, "WAVE: %d", game->current_wave);
    cairo_move_to(cr, width - 180, 30);
    cairo_show_text(cr, text);
    
    // Asteroids remaining
    sprintf(text, "ASTEROIDS: %d", game->comet_count);
    cairo_move_to(cr, width - 280, 55);
    cairo_show_text(cr, text);
    
    // Wave progress info (only show if wave is incomplete)
    if (game->wave_complete_timer > 0) {
        sprintf(text, "NEXT WAVE in %.1fs", game->wave_complete_timer);
        cairo_set_font_size(cr, 18);
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        cairo_move_to(cr, width / 2 - 160, height / 2 - 50);
        cairo_show_text(cr, text);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_set_font_size(cr, 18);
    } else if (game->comet_count > 0) {
        int expected_count = comet_buster_get_wave_comet_count(game->current_wave);
        sprintf(text, "DESTROYED: %d/%d", expected_count - game->comet_count, expected_count);
        cairo_set_font_size(cr, 12);
        cairo_move_to(cr, width - 280, 75);
        cairo_show_text(cr, text);
        cairo_set_font_size(cr, 18);
    }
    
    // Render floating text popups
    cairo_set_font_size(cr, 24);
    for (int i = 0; i < game->floating_text_count; i++) {
        FloatingText *ft = &game->floating_texts[i];
        if (ft->active) {
            // Calculate fade (alpha) based on remaining lifetime
            double alpha = ft->lifetime / ft->max_lifetime;
            
            // Set color with fade
            cairo_set_source_rgba(cr, ft->color[0], ft->color[1], ft->color[2], alpha);
            
            // Draw text centered at position
            cairo_move_to(cr, ft->x - 30, ft->y);
            cairo_show_text(cr, ft->text);
        }
    }
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);  // Reset to white
    
    // Fuel bar (bottom left)
    cairo_set_font_size(cr, 14);
    sprintf(text, "ENERGY: %.0f%%", game->energy_amount);
    
    // Color based on fuel level
    if (game->energy_amount < 20) {
        cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);  // Red - critical
    } else if (game->energy_amount < 50) {
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
    double fuel_percent = game->energy_amount / game->max_energy;
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
// PROVOKE BLUE SHIPS - Convert patrol to aggressive when hit
// ============================================================================

bool comet_buster_hit_enemy_ship_provoke(CometBusterGame *game, int ship_index) {
    if (!game || ship_index < 0 || ship_index >= game->enemy_ship_count) {
        return false;
    }
    
    EnemyShip *ship = &game->enemy_ships[ship_index];
    
    // If this is a patrol (blue) ship, convert it to aggressive
    if (ship->ship_type == 0) {
        ship->ship_type = 1;  // Change to aggressive ship (red)
        ship->max_shield_health = 3;  // Boost its shield
        if (ship->shield_health < 3) {
            ship->shield_health = 3;  // Restore full shield
        }
        ship->shoot_cooldown = 0.0;  // Immediately start shooting
        
        // Spawn visual indicator that ship became angry
        char anger_text[64];
        sprintf(anger_text, "PROVOKED!");
        comet_buster_spawn_floating_text(game, ship->x, ship->y, anger_text, 1.0, 0.2, 0.2);
        
        return true;  // Successfully provoked
    }
    
    return false;  // Already aggressive or different type
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
