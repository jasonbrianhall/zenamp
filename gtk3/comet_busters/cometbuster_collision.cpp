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
