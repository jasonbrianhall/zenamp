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

void comet_buster_spawn_wave(CometBusterGame *game, int screen_width, int screen_height) {
    if (!game) return;
    
    // Reset boss flags
    game->boss.active = false;
    game->spawn_queen.active = false;
    
    // Check if this is a boss wave
    if (game->current_wave > 0 && game->current_wave % 10 == 0) {
        // Spawn Queen appears on waves 10, 20, 30, etc.
        comet_buster_spawn_spawn_queen(game, screen_width, screen_height);
        // Don't spawn normal comets - spawn queen controls the difficulty
    } else if (game->current_wave % 10 == 5) {
        // Regular boss on waves 5, 15, 25, etc.
        comet_buster_spawn_boss(game, screen_width, screen_height);
        // Spawn some normal comets alongside the boss
        comet_buster_spawn_random_comets(game, 3, screen_width, screen_height);
    } else {
        // Normal waves - just comets
        int wave_count = comet_buster_get_wave_comet_count(game->current_wave);
        
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

void comet_buster_spawn_enemy_ship_internal(CometBusterGame *game, int screen_width, int screen_height, 
                                            int ship_type, int edge, double speed, int formation_id, int formation_size) {
    if (!game || game->enemy_ship_count >= MAX_ENEMY_SHIPS) {
        return;
    }
    
    int slot = game->enemy_ship_count;
    EnemyShip *ship = &game->enemy_ships[slot];
    
    memset(ship, 0, sizeof(EnemyShip));
    
    double diagonal_speed = speed / sqrt(2);  // Normalize diagonal speed
    
    ship->ship_type = ship_type;
    
    // Formation fields for sentinels
    if (ship_type == 3) {
        ship->formation_id = formation_id;
        ship->formation_size = formation_size;
        ship->has_partner = (formation_size > 1);
        ship->formation_cohesion = 0.7;
    } else {
        ship->formation_id = -1;
        ship->formation_size = 1;
        ship->has_partner = false;
        ship->formation_cohesion = 0.0;
    }
    
    // Set position based on edge
    switch (edge) {
        case 0:  // From left to right
            ship->x = -20;
            ship->y = 50 + (rand() % (screen_height - 100));  // Avoid top/bottom edges
            ship->vx = speed;
            ship->vy = 0;
            ship->angle = 0;  // Facing right
            ship->base_vx = speed;
            ship->base_vy = 0;
            break;
        case 1:  // From right to left
            ship->x = screen_width + 20;
            ship->y = 50 + (rand() % (screen_height - 100));  // Avoid top/bottom edges
            ship->vx = -speed;
            ship->vy = 0;
            ship->angle = M_PI;  // Facing left
            ship->base_vx = -speed;
            ship->base_vy = 0;
            break;
        case 2:  // From top to bottom
            ship->x = 50 + (rand() % (screen_width - 100));  // Avoid left/right edges
            ship->y = -20;
            ship->vx = 0;
            ship->vy = speed;
            ship->angle = M_PI / 2;  // Facing down
            ship->base_vx = 0;
            ship->base_vy = speed;
            break;
        case 3:  // From bottom to top
            ship->x = 50 + (rand() % (screen_width - 100));  // Avoid left/right edges
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
    
    // Add slight offset for sentinel formation ships (so they don't spawn on top of each other)
    if (ship_type == 3) {
        double offset_angle = (formation_size > 1) ? (2.0 * M_PI * formation_id / formation_size) : 0;
        double offset_dist = 30.0;  // Pixels apart
        ship->x += cos(offset_angle) * offset_dist;
        ship->y += sin(offset_angle) * offset_dist;
    }
    
    ship->health = 1;
    ship->shoot_cooldown = 1.0 + (rand() % 20) / 10.0;  // Shoot after 1-3 seconds
    ship->path_time = 0.0;  // Start at beginning of sine wave
    ship->active = true;
    
    // Initialize patrol behavior for blue, green, and purple ships
    // Red aggressive ships (type 1) don't use patrol behaviors
    ship->patrol_behavior_timer = 0.0;
    ship->patrol_behavior_duration = 2.0 + (rand() % 20) / 10.0;  // 2-4 seconds before behavior change
    ship->patrol_behavior_type = 0;  // Start with straight movement (0=straight, 1=circle, 2=evasive turns)
    ship->patrol_circle_radius = 80.0 + (rand() % 60);  // 80-140px radius circles
    ship->patrol_circle_angle = 0.0;
    
    // Shield system for enemy ships (varies by type)
    if (ship->ship_type == 1) {
        // Red ships (aggressive): 2 shield points
        ship->max_shield_health = 2;
        ship->shield_health = 2;
    } else if (ship->ship_type == 2) {
        // Green ships (hunter): 3 shield points
        ship->max_shield_health = 3;
        ship->shield_health = 3;
    } else if (ship->ship_type == 3) {
        // Purple ships (sentinel): 4 shield points (more durable)
        ship->max_shield_health = 4;
        ship->shield_health = 4;
    } else {
        // Blue ships (patrol): 3 shield points
        ship->max_shield_health = 3;
        ship->shield_health = 3;
    }
    
    ship->shield_impact_timer = 0;
    ship->shield_impact_angle = 0;
    
    game->enemy_ship_count++;
}

void comet_buster_spawn_enemy_ship(CometBusterGame *game, int screen_width, int screen_height) {
    if (!game) {
        return;
    }
    
    // Random edge to spawn from (now includes diagonals)
    int edge = rand() % 8;
    double speed = 80.0 + (rand() % 40);  // 80-120 pixels per second
    
    // Randomly decide ship type:
    // 10% chance of aggressive red ship (attacks player)
    // 75% chance of patrol blue ship (shoots comets)
    // 10% chance of hunter green ship (shoots comets fast, chases if close)
    // 5% chance of sentinel purple ship (defensive formation) - reduced to 0% if red ship active
    
    // Check if any red ships are currently active
    bool red_ship_active = false;
    for (int i = 0; i < game->enemy_ship_count; i++) {
        if (game->enemy_ships[i].active && game->enemy_ships[i].ship_type == 1) {
            red_ship_active = true;
            break;
        }
    }
    
    int type_roll = rand() % 100;
    if (type_roll < 10) {
        // Red (aggressive) - single ship
        comet_buster_spawn_enemy_ship_internal(game, screen_width, screen_height, 1, edge, speed, -1, 1);
    } else if (type_roll < 85) {
        // Blue (patrol) - single ship
        comet_buster_spawn_enemy_ship_internal(game, screen_width, screen_height, 0, edge, speed, -1, 1);

    } else if (type_roll < 95) {
        // Green (hunter) - single ship
        comet_buster_spawn_enemy_ship_internal(game, screen_width, screen_height, 2, edge, speed, -1, 1);
    } else if (!red_ship_active && game->enemy_ship_count + 2 < MAX_ENEMY_SHIPS) {
        // Purple (sentinel) - spawn as PAIR (2-3 ships) - only if no red ships active
        // and if there's room for at least 2 more ships
        int formation_id = game->current_wave * 100 + (int)(game->enemy_ship_spawn_timer * 10);
        int formation_size = (rand() % 2) + 2;  // 2 or 3 sentinels
        
        // Spawn all sentinels in the formation at the same edge
        for (int i = 0; i < formation_size; i++) {
            comet_buster_spawn_enemy_ship_internal(game, screen_width, screen_height, 3, edge, speed, formation_id, formation_size);
        }
    } else {
        // Fallback to blue ship if conditions not met for sentinel
        comet_buster_spawn_enemy_ship_internal(game, screen_width, screen_height, 0, edge, speed, -1, 1);
    }
}

void comet_buster_spawn_enemy_bullet(CometBusterGame *game, double x, double y, double vx, double vy) {
    comet_buster_spawn_enemy_bullet_from_ship(game, x, y, vx, vy, -1);
}

void comet_buster_spawn_enemy_bullet_from_ship(CometBusterGame *game, double x, double y, 
                                               double vx, double vy, int owner_ship_id) {
    if (!game || game->enemy_bullet_count >= MAX_ENEMY_BULLETS) {
        return;
    }
    
    int slot = game->enemy_bullet_count;
    Bullet *bullet = &game->enemy_bullets[slot];
    
    // Initialize ALL fields to prevent garbage data
    memset(bullet, 0, sizeof(Bullet));
    
    bullet->x = x;
    bullet->y = y;
    bullet->vx = vx;
    bullet->vy = vy;
    bullet->angle = atan2(vy, vx);
    bullet->lifetime = 10.0;
    bullet->max_lifetime = 10.0;
    bullet->active = true;
    bullet->owner_ship_id = owner_ship_id;  // EXPLICITLY set the owner
    
    game->enemy_bullet_count++;
}


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
    
    // Boss health - tripled for epic final battle
    boss->health = 180;
    boss->max_health = 180;
    
    // Shield system - also increased proportionally
    boss->shield_health = 30;
    boss->max_shield_health = 30;
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
