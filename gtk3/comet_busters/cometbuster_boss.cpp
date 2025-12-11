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
    
    // RANDOM ASTEROID SPAWNING - Boss occasionally throws asteroids at player!
    // Small chance each frame to spawn an asteroid
    if (game->comet_count < MAX_COMETS && (rand() % 1000) < 15) {  // ~1.5% chance per frame
        // Create an asteroid
        int slot = game->comet_count;
        Comet *asteroid = &game->comets[slot];
        
        memset(asteroid, 0, sizeof(Comet));
        
        // Spawn from random screen corner/edge (away from boss position)
        // This way asteroids don't immediately hit the boss
        int spawn_location = rand() % 4;
        double spawn_x, spawn_y;
        
        switch(spawn_location) {
            case 0:  // Top-left corner
                spawn_x = -50;
                spawn_y = -50;
                break;
            case 1:  // Top-right corner
                spawn_x = width + 50;
                spawn_y = -50;
                break;
            case 2:  // Bottom-left corner
                spawn_x = -50;
                spawn_y = height + 50;
                break;
            case 3:  // Bottom-right corner
                spawn_x = width + 50;
                spawn_y = height + 50;
                break;
        }
        
        asteroid->x = spawn_x;
        asteroid->y = spawn_y;
        
        // Velocity - aimed at player with randomness
        double dx = game->ship_x - asteroid->x;
        double dy = game->ship_y - asteroid->y;
        double dist = sqrt(dx*dx + dy*dy);
        
        if (dist > 0.1) {
            double asteroid_speed = 80.0 + (rand() % 60);  // 80-140 px/s
            // Add randomness so it's not perfectly aimed
            double angle_noise = (rand() % 60 - 30) * (M_PI / 180.0);  // ±30 degrees
            double aimed_angle = atan2(dy, dx) + angle_noise;
            
            asteroid->vx = cos(aimed_angle) * asteroid_speed;
            asteroid->vy = sin(aimed_angle) * asteroid_speed;
        } else {
            asteroid->vx = (rand() % 100 - 50);
            asteroid->vy = (rand() % 100 - 50);
        }
        
        // Sizes - mix of small to large
        int size_roll = rand() % 100;
        if (size_roll < 40) {
            asteroid->size = COMET_LARGE;
            asteroid->radius = 30;
        } else if (size_roll < 75) {
            asteroid->size = COMET_MEDIUM;
            asteroid->radius = 20;
        } else {
            asteroid->size = COMET_SMALL;
            asteroid->radius = 10;
        }
        
        // Set properties
        asteroid->frequency_band = rand() % 3;
        asteroid->rotation = 0;
        asteroid->rotation_speed = 50 + (rand() % 200);
        asteroid->active = true;
        asteroid->health = 1;
        asteroid->base_angle = (rand() % 360) * (M_PI / 180.0);
        
        // Color based on frequency
        comet_buster_get_frequency_color(asteroid->frequency_band,
                                         &asteroid->color[0],
                                         &asteroid->color[1],
                                         &asteroid->color[2]);
        
        game->comet_count++;
        
        fprintf(stdout, "[BOSS] Hurled asteroid from corner! (Total on screen: %d)\n", game->comet_count);
    }
    
    // PURPLE SENTINEL SHIP SUMMONING - Boss calls for backup occasionally!
    // Summon sentinel formations periodically - especially in enraged phase
    double summon_chance = 5.0;  // Base chance per 1000 frames
    
    if (boss->phase == 2) {
        // Enraged phase - MORE aggressive summoning
        summon_chance = 12.0;  // ~1.2% per frame in enraged
    }
    
    if (game->enemy_ship_count < MAX_ENEMY_SHIPS && (rand() % 1000) < summon_chance) {
        // Summon a wave of 15 purple sentinel ships!
        int ships_to_summon = 15;
        int summon_formation_id = game->current_wave * 1000 + (int)(boss->phase_timer * 100);
        int ships_summoned = 0;
        
        fprintf(stdout, "[BOSS] SUMMONING PURPLE SENTINEL FLEET! 15 ships incoming!\n");
        
        for (int i = 0; i < ships_to_summon; i++) {
            if (game->enemy_ship_count >= MAX_ENEMY_SHIPS) {
                fprintf(stdout, "[BOSS] Hit MAX_ENEMY_SHIPS limit, summoning stopped at %d ships\n", ships_summoned);
                break;
            }
            
            // Spread ships across all screen edges
            int edge = (i % 8);  // Cycle through edges 0-7
            double speed = 100.0 + (rand() % 60);  // 100-160 px/s
            
            // Purple sentinels - spawn in coordinated formations
            int formation_id = summon_formation_id + (i / 2);  // 2 ships per formation
            int formation_size = 2;
            
            comet_buster_spawn_enemy_ship_internal(game, width, height,
                                                   3,  // Type 3 = Purple sentinel
                                                   edge, speed,
                                                   formation_id, formation_size);
            
            ships_summoned++;
        }
        
        fprintf(stdout, "[BOSS] Purple sentinel fleet summoned! (%d ships deployed)\n", ships_summoned);
        
        // Visual effect - boss ports flash with energy
        for (int p = 0; p < 40; p++) {
            double angle = 2.0 * M_PI * (rand() % 100) / 100.0;
            double particle_speed = 200.0 + (rand() % 150);
            double vx = cos(angle) * particle_speed;
            double vy = sin(angle) * particle_speed;
            // Particles burst from boss
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

bool comet_buster_check_bullet_boss(Bullet *b, BossShip *boss) {
    if (!b || !b->active || !boss || !boss->active) return false;
    
    double dx = boss->x - b->x;
    double dy = boss->y - b->y;
    double dist = sqrt(dx*dx + dy*dy);
    double collision_dist = 35.0;  // Boss collision radius
    
    return (dist < collision_dist);
}

void comet_buster_spawn_spawn_queen(CometBusterGame *game, int screen_width, int screen_height) {
    if (!game) return;
    
    fprintf(stdout, "[SPAWN QUEEN] Spawning Spawn Queen at Wave %d\n", game->current_wave);
    
    SpawnQueenBoss *queen = &game->spawn_queen;
    memset(queen, 0, sizeof(SpawnQueenBoss));
    
    // Initial position - top center
    queen->x = screen_width / 2.0;
    queen->y = 100.0;
    queen->vx = 0;
    queen->vy = 0;
    
    // Health scales with wave
    int health_base = 80 + (game->current_wave - 10) * 5;
    queen->health = health_base;
    queen->max_health = health_base;
    
    // Shield
    queen->shield_health = 15;
    queen->max_shield_health = 15;
    
    // Spawning mechanics
    queen->spawn_timer = 2.0;  // Start spawning after 2 seconds
    queen->spawn_cooldown = 3.0;
    
    // Attack patterns
    queen->phase = 0;
    queen->phase_timer = 0;
    queen->attack_timer = 0;
    queen->attack_cooldown = 2.0;
    
    // Movement
    queen->movement_timer = 0;
    queen->base_movement_speed = 40.0;
    
    // Visual
    queen->rotation = 0;
    queen->rotation_speed = 30.0;
    queen->damage_flash_timer = 0;
    queen->spawn_particle_timer = 0;
    
    // Flags
    queen->active = true;
    queen->is_spawn_queen = true;
    game->boss_active = true;
    
    fprintf(stdout, "[SPAWN QUEEN] Queen spawned! Health: %d\n", queen->health);
}

void comet_buster_spawn_queen_spawn_ships(CometBusterGame *game, int screen_width, int screen_height) {
    if (!game || game->enemy_ship_count >= MAX_ENEMY_SHIPS) return;
    
    SpawnQueenBoss *queen = &game->spawn_queen;
    
    // Spawn queen now recruits 10 ships at a time with a strategic mix!
    // Mostly Red (aggressive attackers), some Green (miners), some Purple (sentinels)
    // PLUS she throws large asteroids at you!
    
    int ships_spawned = 0;
    int max_ships_to_spawn = 10;  // Spawn 10 ships per recruitment
    
    // Distribute ship types across the 10 ships
    // 60% Red (6 ships) - aggressive attackers
    // 20% Green (2 ships) - hunters
    // 20% Purple (2 ships) - sentinel formations
    
    for (int i = 0; i < max_ships_to_spawn; i++) {
        if (game->enemy_ship_count >= MAX_ENEMY_SHIPS) {
            fprintf(stdout, "[SPAWN QUEEN] Hit MAX_ENEMY_SHIPS limit (%d), stopping spawn\n", MAX_ENEMY_SHIPS);
            break;
        }
        
        int ship_type = 0;
        int formation_id = -1;
        int formation_size = 1;
        
        // Distribute the types: 0-5 = Red, 6-7 = Green, 8-9 = Purple
        if (i < 6) {
            // Red aggressive ships
            ship_type = 1;
        } else if (i < 8) {
            // Green hunter ships
            ship_type = 2;
        } else {
            // Purple sentinel ships - spawn in pairs
            ship_type = 3;
            if (i == 8) {
                // First sentinel - assign formation
                formation_id = game->current_wave * 100 + (int)(game->spawn_queen.spawn_timer * 10);
                formation_size = 2;
            } else if (i == 9) {
                // Second sentinel in the pair - use same formation ID
                formation_id = game->current_wave * 100 + (int)(game->spawn_queen.spawn_timer * 10);
                formation_size = 2;
            }
        }
        
        // Vary spawn edges to spread them out
        int edge = (i % 8);  // Cycle through all 8 edges
        double speed = 90.0 + (rand() % 50);  // Vary speeds
        
        comet_buster_spawn_enemy_ship_internal(game, screen_width, screen_height,
                                               ship_type, edge, speed,
                                               formation_id, formation_size);
        
        ships_spawned++;
        
        // Log the spawn
        const char *type_name = "UNKNOWN";
        if (ship_type == 1) type_name = "Red";
        else if (ship_type == 2) type_name = "Green";
        else if (ship_type == 3) type_name = "Purple";
        
        fprintf(stdout, "[SPAWN QUEEN] Spawned ship %d/10: %s (type %d) on edge %d\n", 
                i + 1, type_name, ship_type, edge);
    }
    
    fprintf(stdout, "[SPAWN QUEEN] Recruitment complete! Spawned %d ships\n", ships_spawned);
    
    // SPAWN LARGE ASTEROIDS as part of the recruitment wave!
    // The spawn queen hurls massive rocks at you along with her ships
    int asteroids_to_spawn = 4 + (rand() % 3);  // 4-6 large asteroids
    
    for (int a = 0; a < asteroids_to_spawn; a++) {
        if (game->comet_count >= MAX_COMETS) {
            fprintf(stdout, "[SPAWN QUEEN] Hit MAX_COMETS limit, can't spawn more asteroids\n");
            break;
        }
        
        // Create a large/mega asteroid
        int slot = game->comet_count;
        Comet *asteroid = &game->comets[slot];
        
        memset(asteroid, 0, sizeof(Comet));
        
        // Spawn from random edge of screen
        int edge = rand() % 4;
        
        switch (edge) {
            case 0:  // Top
                asteroid->x = rand() % screen_width;
                asteroid->y = -50;
                break;
            case 1:  // Right
                asteroid->x = screen_width + 50;
                asteroid->y = rand() % screen_height;
                break;
            case 2:  // Bottom
                asteroid->x = rand() % screen_width;
                asteroid->y = screen_height + 50;
                break;
            case 3:  // Left
                asteroid->x = -50;
                asteroid->y = rand() % screen_height;
                break;
        }
        
        // Aim at player with aggressive velocity
        double dx = game->ship_x - asteroid->x;
        double dy = game->ship_y - asteroid->y;
        double dist = sqrt(dx*dx + dy*dy);
        
        if (dist > 0.1) {
            // Fast moving asteroids - queen is throwing them hard!
            double asteroid_speed = 150.0 + (rand() % 100);  // 150-250 px/s
            asteroid->vx = (dx / dist) * asteroid_speed;
            asteroid->vy = (dy / dist) * asteroid_speed;
        } else {
            asteroid->vx = 0;
            asteroid->vy = 0;
        }
        
        // Make them LARGE - mostly mega asteroids
        int size_roll = rand() % 100;
        if (size_roll < 70) {
            // 70% MEGA asteroids (huge!)
            asteroid->size = COMET_MEGA;
            asteroid->radius = 50;
        } else {
            // 30% LARGE asteroids
            asteroid->size = COMET_LARGE;
            asteroid->radius = 30;
        }
        
        // Set properties
        asteroid->frequency_band = rand() % 3;  // Random audio band
        asteroid->rotation = 0;
        asteroid->rotation_speed = 30 + (rand() % 100);  // Slower rotation for drama
        asteroid->active = true;
        asteroid->health = 1;
        asteroid->base_angle = (rand() % 360) * (M_PI / 180.0);
        
        // Color based on frequency
        comet_buster_get_frequency_color(asteroid->frequency_band,
                                         &asteroid->color[0],
                                         &asteroid->color[1],
                                         &asteroid->color[2]);
        
        game->comet_count++;
        
        fprintf(stdout, "[SPAWN QUEEN] Hurled asteroid %d/%d at player!\n", a + 1, asteroids_to_spawn);
    }
    
    fprintf(stdout, "[SPAWN QUEEN] Recruitment wave complete! 10 ships + %d asteroids incoming!\n", asteroids_to_spawn);
    
    // Visual particle effect from queen's ports - EVEN MORE particles for epic recruitment!
    for (int p = 0; p < 30; p++) {
        double angle = 2.0 * M_PI * (rand() % 100) / 100.0;
        double particle_speed = 150.0 + (rand() % 150);
        double vx = cos(angle) * particle_speed;
        double vy = sin(angle) * particle_speed;
        // Particles would be spawned here from queen's position
    }
}

// ============================================================================
// UPDATING
// ============================================================================

void comet_buster_update_spawn_queen(CometBusterGame *game, double dt, int width, int height) {
    if (!game || !game->spawn_queen.active) return;
    
    SpawnQueenBoss *queen = &game->spawn_queen;
    
    // Update phase based on health percentage
    int old_phase = queen->phase;
    if (queen->health > queen->max_health * 0.75) {
        queen->phase = 0;  // Recruitment
    } else if (queen->health > queen->max_health * 0.4) {
        queen->phase = 1;  // Aggression
    } else {
        queen->phase = 2;  // Desperation
    }
    
    // Log phase changes
    if (queen->phase != old_phase) {
        fprintf(stdout, "[SPAWN QUEEN] Phase changed to %d (health: %.1f%%)\n", 
                queen->phase, 100.0 * queen->health / queen->max_health);
    }
    
    queen->phase_timer += dt;
    
    // --- MOVEMENT ---
    // Slow horizontal sine wave
    queen->movement_timer += dt;
    double sine_offset = sin(queen->movement_timer * queen->base_movement_speed / 100.0) * 150.0;
    queen->x = width / 2.0 + sine_offset;
    
    // Keep in bounds
    if (queen->x < 60) queen->x = 60;
    if (queen->x > width - 60) queen->x = width - 60;
    
    // --- ROTATION ---
    queen->rotation += queen->rotation_speed * dt;
    
    // --- DAMAGE EFFECTS ---
    if (queen->damage_flash_timer > 0) {
        queen->damage_flash_timer -= dt;
    }
    
    queen->spawn_particle_timer -= dt;
    
    // --- SHIP SPAWNING ---
    queen->spawn_timer -= dt;
    if (queen->spawn_timer <= 0) {
        comet_buster_spawn_queen_spawn_ships(game, width, height);
        
        // Spawn cooldown varies by phase
        if (queen->phase == 0) {
            queen->spawn_cooldown = 3.0;
        } else if (queen->phase == 1) {
            queen->spawn_cooldown = 2.5;
        } else {
            queen->spawn_cooldown = 2.0;
        }
        
        queen->spawn_timer = queen->spawn_cooldown;
    }
    
    // --- BULLET ATTACKS ---
    queen->attack_timer -= dt;
    if (queen->attack_timer <= 0) {
        comet_buster_spawn_queen_fire(game);
        
        // Attack cooldown varies by phase
        if (queen->phase == 0) {
            queen->attack_cooldown = 2.0;
        } else if (queen->phase == 1) {
            queen->attack_cooldown = 1.2;
        } else {
            queen->attack_cooldown = 0.8;
        }
        
        queen->attack_timer = queen->attack_cooldown;
    }
}

// ============================================================================
// FIRING
// ============================================================================

void comet_buster_spawn_queen_fire(CometBusterGame *game) {
    if (!game || !game->spawn_queen.active) return;
    
    SpawnQueenBoss *queen = &game->spawn_queen;
    double bullet_speed = 200.0;
    
    double dx = game->ship_x - queen->x;
    double dy = game->ship_y - queen->y;
    double angle_to_ship = atan2(dy, dx);
    
    if (queen->phase == 0) {
        // Phase 0: Minimal fire - 1 bullet toward player
        int num_bullets = 1;
        double spread = 15.0 * M_PI / 180.0;
        
        for (int i = 0; i < num_bullets; i++) {
            double fire_angle = angle_to_ship + (spread * (i - num_bullets/2.0));
            double vx = cos(fire_angle) * bullet_speed;
            double vy = sin(fire_angle) * bullet_speed;
            comet_buster_spawn_enemy_bullet(game, queen->x, queen->y, vx, vy);
        }
        
    } else if (queen->phase == 1) {
        // Phase 1: Burst fire - 3-bullet spread
        int num_bullets = 3;
        double spread = 45.0 * M_PI / 180.0;
        double start_angle = angle_to_ship - spread / 2.0;
        
        for (int i = 0; i < num_bullets; i++) {
            double fire_angle = start_angle + (spread / (num_bullets - 1)) * i;
            double vx = cos(fire_angle) * bullet_speed;
            double vy = sin(fire_angle) * bullet_speed;
            comet_buster_spawn_enemy_bullet(game, queen->x, queen->y, vx, vy);
        }
        
    } else {
        // Phase 2: Ring fire - 16 bullets in all directions
        int num_bullets = 16;
        for (int i = 0; i < num_bullets; i++) {
            double fire_angle = 2.0 * M_PI * i / num_bullets;
            double vx = cos(fire_angle) * (bullet_speed + 50.0);
            double vy = sin(fire_angle) * (bullet_speed + 50.0);
            comet_buster_spawn_enemy_bullet(game, queen->x, queen->y, vx, vy);
        }
    }
}

// ============================================================================
// COLLISION & DAMAGE
// ============================================================================

bool comet_buster_check_bullet_spawn_queen(Bullet *b, SpawnQueenBoss *queen) {
    if (!b || !b->active || !queen || !queen->active) return false;
    
    double dx = queen->x - b->x;
    double dy = queen->y - b->y;
    double dist = sqrt(dx*dx + dy*dy);
    double collision_radius = 50.0;
    
    return (dist < collision_radius);
}

void comet_buster_destroy_spawn_queen(CometBusterGame *game, int width, int height, void *vis) {
    if (!game || !game->spawn_queen.active) return;
    
    SpawnQueenBoss *queen = &game->spawn_queen;
    
    fprintf(stdout, "[SPAWN QUEEN] Destroyed at wave %d!\n", game->current_wave);
    
    // Large explosion (80 particles)
    comet_buster_spawn_explosion(game, queen->x, queen->y, 0, 80);
    
    // Victory text
    comet_buster_spawn_floating_text(game, queen->x, queen->y - 50,
                                     "MOTHERSHIP DOWN", 1.0, 0.2, 1.0);
    
    // Scoring
    int base_score = 1000;
    int wave_bonus = game->current_wave * 100;
    int total_score = base_score + wave_bonus;
    
    // Bonus if multiplier was high
    if (game->score_multiplier >= 4.0) {
        total_score += 500;
        comet_buster_spawn_floating_text(game, queen->x, queen->y,
                                         "MULTIPLIER BONUS!", 0.0, 1.0, 1.0);
    }
    
    game->score += total_score;
    
    // Mark inactive
    queen->active = false;
    game->boss_active = false;
    
    fprintf(stdout, "[SPAWN QUEEN] Score awarded: %d\n", total_score);
}

// ============================================================================
// RENDERING
// ============================================================================

void draw_spawn_queen_boss(SpawnQueenBoss *queen, cairo_t *cr, int width, int height) {
    (void)width;
    (void)height;
    
    if (!queen || !queen->active) return;
    
    cairo_save(cr);
    cairo_translate(cr, queen->x, queen->y);
    cairo_rotate(cr, queen->rotation * M_PI / 180.0);
    
    // Main elliptical body - iridescent magenta
    double major_axis = 70.0;
    double minor_axis = 45.0;
    
    // Body fill
    cairo_set_source_rgb(cr, 0.7, 0.3, 0.8);  // Magenta
    cairo_scale(cr, major_axis, minor_axis);
    cairo_arc(cr, 0, 0, 1.0, 0, 2.0 * M_PI);
    cairo_restore(cr);
    cairo_fill(cr);
    cairo_save(cr);
    
    // Outer metallic ring
    cairo_translate(cr, queen->x, queen->y);
    cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 0.7);
    cairo_set_line_width(cr, 2.5);
    cairo_scale(cr, major_axis, minor_axis);
    cairo_arc(cr, 0, 0, 1.0, 0, 2.0 * M_PI);
    cairo_restore(cr);
    cairo_stroke(cr);
    
    cairo_save(cr);
    cairo_translate(cr, queen->x, queen->y);
    
    // Spawn ports - 6 around equator
    double port_radius = 6.0;
    double port_orbit = 50.0;
    
    // Determine port colors based on phase
    double port_r, port_g, port_b;
    if (queen->phase == 0) {
        // Phase 0: Red ships - ports glow red
        port_r = 1.0; port_g = 0.2; port_b = 0.2;
    } else if (queen->phase == 1) {
        // Phase 1: Mix - ports glow yellow
        port_r = 1.0; port_g = 0.5; port_b = 0.8;
    } else {
        // Phase 2: Sentinel ships - ports glow magenta
        port_r = 0.8; port_g = 0.3; port_b = 1.0;
    }
    
    // Pulsing glow intensity
    double glow_intensity = 0.5 + 0.5 * sin(queen->spawn_particle_timer * 5.0);
    
    for (int i = 0; i < 6; i++) {
        double angle = 2.0 * M_PI * i / 6.0;
        double px = cos(angle) * port_orbit;
        double py = sin(angle) * port_orbit * 0.6;
        
        // Outer glow
        cairo_set_source_rgba(cr, port_r, port_g, port_b, glow_intensity * 0.5);
        cairo_arc(cr, px, py, port_radius + 4.0, 0, 2.0 * M_PI);
        cairo_fill(cr);
        
        // Port center
        cairo_set_source_rgb(cr, port_r, port_g, port_b);
        cairo_arc(cr, px, py, port_radius, 0, 2.0 * M_PI);
        cairo_fill(cr);
    }
    
    cairo_restore(cr);
    
    // Damage flash overlay
    if (queen->damage_flash_timer > 0) {
        cairo_set_source_rgba(cr, 1.0, 0.5, 0.5, 0.4);
        cairo_arc(cr, queen->x, queen->y, major_axis, 0, 2.0 * M_PI);
        cairo_fill(cr);
    }
    
    // Red core (pulsing)
    double core_size = 12.0 + 3.0 * sin(queen->phase_timer * 3.0);
    cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);
    cairo_arc(cr, queen->x, queen->y, core_size, 0, 2.0 * M_PI);
    cairo_fill(cr);
    
    // --- HEALTH BAR ---
    double bar_width = 100.0;
    double bar_height = 8.0;
    double bar_x = queen->x - bar_width / 2.0;
    double bar_y = queen->y - 70.0;
    
    // Background
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_rectangle(cr, bar_x, bar_y, bar_width, bar_height);
    cairo_fill(cr);
    
    // Health fill
    double health_ratio = (double)queen->health / queen->max_health;
    cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);  // Red
    cairo_rectangle(cr, bar_x, bar_y, bar_width * health_ratio, bar_height);
    cairo_fill(cr);
    
    // Border
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, bar_x, bar_y, bar_width, bar_height);
    cairo_stroke(cr);
    
    // --- SHIELD BAR ---
    double shield_y = bar_y + bar_height + 2.0;
    
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_rectangle(cr, bar_x, shield_y, bar_width, bar_height);
    cairo_fill(cr);
    
    double shield_ratio = (double)queen->shield_health / queen->max_shield_health;
    cairo_set_source_rgb(cr, 0.0, 1.0, 1.0);  // Cyan
    cairo_rectangle(cr, bar_x, shield_y, bar_width * shield_ratio, bar_height);
    cairo_fill(cr);
    
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, bar_x, shield_y, bar_width, bar_height);
    cairo_stroke(cr);
    
    // --- PHASE INDICATOR ---
    const char *phase_text;
    double text_r, text_g, text_b;
    
    if (queen->phase == 0) {
        phase_text = "RECRUITING";
        text_r = 1.0; text_g = 0.5; text_b = 0.0;
    } else if (queen->phase == 1) {
        phase_text = "AGGRESSIVE";
        text_r = 1.0; text_g = 1.0; text_b = 0.0;
    } else {
        phase_text = "DESPERATE!";
        text_r = 1.0; text_g = 0.0; text_b = 0.0;
    }
    
    cairo_set_source_rgb(cr, text_r, text_g, text_b);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11);
    cairo_move_to(cr, queen->x - 35, queen->y + 75);
    cairo_show_text(cr, phase_text);
}

void comet_buster_spawn_void_nexus(CometBusterGame *game, int screen_width, int screen_height) {
    if (!game) return;
    
    fprintf(stdout, "[VOID NEXUS] Spawning Void Nexus at Wave %d\n", game->current_wave);
    
    BossShip *boss = &game->boss;
    memset(boss, 0, sizeof(BossShip));
    
    // Spawn at top center, will move into geometric pattern
    boss->x = screen_width / 2.0;
    boss->y = 100.0;
    boss->vx = 0;
    boss->vy = 0;
    boss->angle = 0;
    
    // INCREASED HEALTH: Base 75 HP (was 45), scaling +5 per wave (was +3)
    int health_base = 75 + (game->current_wave - 15) * 5;
    boss->health = health_base;
    boss->max_health = health_base;
    
    // INCREASED SHIELD: 25 points (was 12), max increased proportionally
    boss->shield_health = 25;
    boss->max_shield_health = 25;
    boss->shield_active = true;  // Changed from false to true - shield starts active
    
    // Shooting
    boss->shoot_cooldown = 0;
    
    // Phases - geometric movement patterns
    boss->phase = 0;  // 0=square, 1=hexagon, 2=infinity
    boss->phase_timer = 0;
    boss->phase_duration = 6.0;  // 6 seconds per pattern
    
    // Visual
    boss->rotation = 0;
    boss->rotation_speed = 90.0;  // degrees per second (fast spinning)
    boss->damage_flash_timer = 0;
    
    // Void Nexus specific
    boss->fragment_count = 0;  // Start as main body
    boss->is_fragment = false;
    boss->burst_angle_offset = 0;
    boss->fragment_reunite_timer = 0;
    boss->reunite_speed = 60.0;  // 60 pixels/sec toward center
    boss->last_damage_time = 0;
    
    boss->active = true;
    game->boss_active = true;
    
    fprintf(stdout, "[VOID NEXUS] Boss spawned! Health: %d, Shield: %d, Position: (%.1f, %.1f)\n", 
            boss->health, boss->shield_health, boss->x, boss->y);
}

// ============================================================================
// VOID NEXUS CORE UPDATE LOGIC
// ============================================================================

void comet_buster_update_void_nexus(CometBusterGame *game, double dt, int width, int height) {
    if (!game || !game->boss_active) return;
    
    BossShip *boss = &game->boss;
    if (!boss->active) {
        game->boss_active = false;
        return;
    }
    
    // ========== PHASE MANAGEMENT ==========
    boss->phase_timer += dt;
    if (boss->phase_timer >= boss->phase_duration) {
        boss->phase_timer = 0;
        boss->phase = (boss->phase + 1) % 3;  // Cycle: 0 (square) -> 1 (hexagon) -> 2 (infinity) -> 0
        fprintf(stdout, "[VOID NEXUS] Phase changed to %d\n", boss->phase);
    }
    
    // ========== MOVEMENT - GEOMETRIC PATTERNS ==========
    // Center point for patterns
    double center_x = width / 2.0;
    double center_y = height / 2.0;
    
    double pattern_time = boss->phase_timer;
    double pattern_speed = 1.5;  // Adjust how fast pattern moves
    
    if (boss->fragment_count == 0) {
        // Main body movement
        if (boss->phase == 0) {
            // Square pattern - corners at distance 120 from center
            double corner_dist = 120.0;
            int corner = (int)(pattern_time * pattern_speed) % 4;
            double target_x = center_x;
            double target_y = center_y;
            
            switch(corner) {
                case 0: target_x -= corner_dist; target_y -= corner_dist; break;  // Top-left
                case 1: target_x += corner_dist; target_y -= corner_dist; break;  // Top-right
                case 2: target_x += corner_dist; target_y += corner_dist; break;  // Bottom-right
                case 3: target_x -= corner_dist; target_y += corner_dist; break;  // Bottom-left
            }
            
            // Smoothly move toward target
            double smooth_factor = 0.08;
            boss->x += (target_x - boss->x) * smooth_factor;
            boss->y += (target_y - boss->y) * smooth_factor;
            
        } else if (boss->phase == 1) {
            // Hexagon pattern - 6 points at distance 100
            double hex_dist = 100.0;
            double hex_angle = (pattern_time * pattern_speed * 60.0) * M_PI / 180.0;  // Rotate around
            
            boss->x = center_x + cos(hex_angle) * hex_dist;
            boss->y = center_y + sin(hex_angle) * hex_dist;
            
        } else if (boss->phase == 2) {
            // Infinity/figure-8 pattern
            double inf_scale_x = 100.0;
            double inf_scale_y = 60.0;
            double inf_time = pattern_time * pattern_speed * 2.0;
            
            // Lissajous curve (infinity shape)
            boss->x = center_x + sin(inf_time) * inf_scale_x;
            boss->y = center_y + sin(inf_time * 2.0) * 0.5 * inf_scale_y;
        }
    }
    
    // Keep boss roughly in bounds (with some margin)
    if (boss->x < 60) boss->x = 60;
    if (boss->x > width - 60) boss->x = width - 60;
    if (boss->y < 60) boss->y = 60;
    if (boss->y > height - 60) boss->y = height - 60;
    
    // ========== VISUAL ROTATION ==========
    boss->rotation += boss->rotation_speed * dt;
    
    // ========== DAMAGE FLASH ==========
    if (boss->damage_flash_timer > 0) {
        boss->damage_flash_timer -= dt;
    }
    
    boss->last_damage_time += dt;
    
    // ========== FIRING PATTERN ==========
    boss->shoot_cooldown -= dt;
    
    // 3-way rotating energy bursts every 0.6 seconds
    if (boss->shoot_cooldown <= 0) {
        void_nexus_fire(game);
        boss->shoot_cooldown = 0.6;
        boss->burst_angle_offset += 30;  // Rotate pattern each burst
    }
    
    // ========== SPLITTING MECHANIC ==========
    // When boss health drops below 70%, split into 2 fragments
    if (boss->fragment_count == 0 && boss->health < boss->max_health * 0.7) {
        fprintf(stdout, "[VOID NEXUS] Boss health critical! Splitting into fragments!\n");
        void_nexus_split_into_fragments(game, 2);  // Split into 2
    }
    
    // ========== ENEMY SHIP SPAWNING ==========
    boss->nexus_ship_spawn_timer += dt;
    
    double spawn_interval = 3.0;
    if (boss->fragment_count > 0) {
        spawn_interval = 2.5;
    }
    if (boss->health < boss->max_health * 0.5) {
        spawn_interval = 2.0;
    }
    
    if (boss->nexus_ship_spawn_timer >= spawn_interval) {
        void_nexus_spawn_ship_wave(game, width, height);
        boss->nexus_ship_spawn_timer = 0;
    }
    
    // ========== FRAGMENT MANAGEMENT ==========
    if (boss->fragment_count > 0) {
        // Update all active fragments
        for (int i = 0; i < boss->fragment_count; i++) {
            // Fragments slowly drift apart then regroup
            boss->fragment_reunite_timer += dt;
            
            // Every 4 seconds, fragments try to reunite
            if (boss->fragment_reunite_timer > 4.0) {
                // Move fragments back toward center
                double center_dx = center_x - boss->fragment_positions[i][0];
                double center_dy = center_y - boss->fragment_positions[i][1];
                double dist = sqrt(center_dx*center_dx + center_dy*center_dy);
                
                if (dist > 10.0) {
                    boss->fragment_positions[i][0] += (center_dx / dist) * boss->reunite_speed * dt;
                    boss->fragment_positions[i][1] += (center_dy / dist) * boss->reunite_speed * dt;
                }
            }
            
            // Fragments orbit around center slowly
            double orbit_angle = (boss->phase_timer + i * M_PI * 2.0 / boss->fragment_count) * 0.3;
            double orbit_dist = 80.0;
            double orbit_x = center_x + cos(orbit_angle) * orbit_dist;
            double orbit_y = center_y + sin(orbit_angle) * orbit_dist;
            
            // Lerp toward orbit position (adds natural motion)
            boss->fragment_positions[i][0] += (orbit_x - boss->fragment_positions[i][0]) * 0.02;
            boss->fragment_positions[i][1] += (orbit_y - boss->fragment_positions[i][1]) * 0.02;
            
            // Fragments fire at slower rate
            if (boss->shoot_cooldown <= 0) {
                void_nexus_fragment_fire(game, i);
            }
        }
    }
    
    // ========== CHECK IF BOSS IS DEFEATED ==========
    if (boss->health <= 0) {
        fprintf(stdout, "[VOID NEXUS] Void Nexus destroyed!\n");
        boss->active = false;
        game->boss_active = false;
    }
}

// ============================================================================
// VOID NEXUS FIRING PATTERNS
// ============================================================================

void void_nexus_fire(CometBusterGame *game) {
    if (!game || !game->boss_active) return;
    
    BossShip *boss = &game->boss;
    double bullet_speed = 200.0;
    
    // Calculate angle toward the player ship
    double dx = game->ship_x - boss->x;
    double dy = game->ship_y - boss->y;
    double angle_to_ship = atan2(dy, dx);
    
    // Fire 3 bullets in a rotating spread
    int num_bullets = 3;
    double angle_spread = 60.0 * M_PI / 180.0;  // 60 degree spread
    
    // Offset changes each burst for rotation effect
    double offset = (boss->burst_angle_offset * M_PI / 180.0);
    double start_angle = angle_to_ship - angle_spread / 2.0 + offset;
    
    for (int i = 0; i < num_bullets; i++) {
        double angle = start_angle + (angle_spread / (num_bullets - 1)) * i;
        double vx = cos(angle) * bullet_speed;
        double vy = sin(angle) * bullet_speed;
        
        comet_buster_spawn_enemy_bullet(game, boss->x, boss->y, vx, vy);
    }
    
    fprintf(stdout, "[VOID NEXUS] 3-way burst fired!\n");
}

void void_nexus_fragment_fire(CometBusterGame *game, int fragment_id) {
    if (!game || !game->boss_active || fragment_id < 0 || fragment_id >= 4) return;
    
    BossShip *boss = &game->boss;
    if (boss->fragment_count <= fragment_id) return;
    
    double bullet_speed = 180.0;  // Slightly slower than main body
    
    // Get fragment position
    double frag_x = boss->fragment_positions[fragment_id][0];
    double frag_y = boss->fragment_positions[fragment_id][1];
    
    // Calculate angle toward player
    double dx = game->ship_x - frag_x;
    double dy = game->ship_y - frag_y;
    double angle_to_ship = atan2(dy, dx);
    
    // Fragments fire single shots (not spread)
    double vx = cos(angle_to_ship) * bullet_speed;
    double vy = sin(angle_to_ship) * bullet_speed;
    
    comet_buster_spawn_enemy_bullet(game, frag_x, frag_y, vx, vy);
}

// ============================================================================
// VOID NEXUS SPLITTING MECHANISM
// ============================================================================

void void_nexus_split_into_fragments(CometBusterGame *game, int num_fragments) {
    if (!game || !game->boss_active || num_fragments < 1 || num_fragments > 4) return;
    
    BossShip *boss = &game->boss;
    
    fprintf(stdout, "[VOID NEXUS] Splitting into %d fragments!\n", num_fragments);
    
    boss->fragment_count = num_fragments;
    
    // Distribute health equally among fragments
    int health_per_fragment = boss->health / num_fragments;
    
    double center_x = 400.0;  // Approximate screen center
    double center_y = 300.0;
    
    // Position fragments in a circle around the center
    for (int i = 0; i < num_fragments; i++) {
        double angle = (2.0 * M_PI * i) / num_fragments;
        double dist = 80.0;
        
        boss->fragment_positions[i][0] = center_x + cos(angle) * dist;
        boss->fragment_positions[i][1] = center_y + sin(angle) * dist;
        boss->fragment_health[i] = health_per_fragment;
        
        // Spawn visual effect particles
        for (int p = 0; p < 15; p++) {
            double particle_angle = 2.0 * M_PI * (rand() % 100) / 100.0;
            double particle_speed = 150.0 + (rand() % 100);
            double vx = cos(particle_angle) * particle_speed;
            double vy = sin(particle_angle) * particle_speed;
            
            // Create cyan/blue particles
            comet_buster_spawn_explosion(game, boss->fragment_positions[i][0], 
                                        boss->fragment_positions[i][1], 2, 10);
        }
    }
    
    // Award bonus for focused fire if destroying fragments quickly
    game->score += 500 * num_fragments;
    comet_buster_spawn_floating_text(game, center_x, center_y - 30, "FRAGMENT!", 0.0, 1.0, 1.0);
    
    boss->fragment_reunite_timer = 0;
}

void void_nexus_spawn_ship_wave(CometBusterGame *game, int screen_width, int screen_height) {
    if (!game || game->enemy_ship_count >= MAX_ENEMY_SHIPS) return;
    
    fprintf(stdout, "[VOID NEXUS] Spawning ship wave!\n");
    
    // Void Nexus spawns 8 ships per wave (more aggressive than Spawn Queen's 10)
    // BUT spawns more frequently
    int ships_to_spawn = 8;
    int ships_spawned = 0;
    
    // Mix: 50% Red (aggressive), 30% Green (hunters), 20% Purple (sentinels)
    for (int i = 0; i < ships_to_spawn; i++) {
        if (game->enemy_ship_count >= MAX_ENEMY_SHIPS) {
            fprintf(stdout, "[VOID NEXUS] Hit MAX_ENEMY_SHIPS limit, stopping spawn\n");
            break;
        }
        
        int ship_type = 0;
        int formation_id = -1;
        int formation_size = 1;
        
        // Distribute types: 0-3 = Red, 4-5 = Green, 6-7 = Purple
        if (i < 4) {
            // Red aggressive ships (50%)
            ship_type = 1;
        } else if (i < 6) {
            // Green hunter ships (30%)
            ship_type = 2;
        } else {
            // Purple sentinel ships (20%) - spawn in pairs
            ship_type = 3;
            if (i == 6) {
                formation_id = game->current_wave * 1000 + (int)(game->boss.phase_timer * 100);
                formation_size = 2;
            } else if (i == 7) {
                formation_id = game->current_wave * 1000 + (int)(game->boss.phase_timer * 100);
                formation_size = 2;
            }
        }
        
        // Spawn from random edges
        int edge = (i % 8);
        double speed = 100.0 + (rand() % 60);  // Vary speeds
        
        comet_buster_spawn_enemy_ship_internal(game, screen_width, screen_height,
                                               ship_type, edge, speed,
                                               formation_id, formation_size);
        
        ships_spawned++;
    }
    
    fprintf(stdout, "[VOID NEXUS] Spawned %d ships in wave\n", ships_spawned);
}

// ============================================================================
// VOID NEXUS COLLISION DETECTION
// ============================================================================

bool comet_buster_check_bullet_void_nexus(Bullet *b, BossShip *boss) {
    if (!b || !b->active || !boss || !boss->active) return false;
    
    // Check collision with main body
    if (boss->fragment_count == 0) {
        double dx = boss->x - b->x;
        double dy = boss->y - b->y;
        double dist = sqrt(dx*dx + dy*dy);
        double collision_dist = 30.0;  // Main body collision radius
        
        return (dist < collision_dist);
    }
    
    // Check collision with fragments
    for (int i = 0; i < boss->fragment_count; i++) {
        double dx = boss->fragment_positions[i][0] - b->x;
        double dy = boss->fragment_positions[i][1] - b->y;
        double dist = sqrt(dx*dx + dy*dy);
        double collision_dist = 22.0;  // Fragment collision radius (smaller)
        
        if (dist < collision_dist) {
            return true;
        }
    }
    
    return false;
}

bool comet_buster_hit_void_nexus_fragment(Bullet *b, BossShip *boss, int *fragment_hit) {
    if (!b || !b->active || !boss || !boss->active || !fragment_hit) return false;
    
    // Check which fragment was hit
    for (int i = 0; i < boss->fragment_count; i++) {
        double dx = boss->fragment_positions[i][0] - b->x;
        double dy = boss->fragment_positions[i][1] - b->y;
        double dist = sqrt(dx*dx + dy*dy);
        double collision_dist = 22.0;
        
        if (dist < collision_dist) {
            *fragment_hit = i;
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// VOID NEXUS DAMAGE HANDLING
// ============================================================================

void comet_buster_damage_void_nexus(CometBusterGame *game, int damage, int fragment_id) {
    if (!game || !game->boss_active) return;
    
    BossShip *boss = &game->boss;
    
    if (boss->fragment_count == 0) {
        // Damage main body
        boss->health -= damage;
        boss->damage_flash_timer = 0.1;
        boss->last_damage_time = 0;
        
        fprintf(stdout, "[VOID NEXUS] Main body hit! Health: %d/%d\n", 
                boss->health, boss->max_health);
    } else if (fragment_id >= 0 && fragment_id < boss->fragment_count) {
        // Damage specific fragment
        boss->fragment_health[fragment_id] -= damage;
        boss->damage_flash_timer = 0.1;
        
        fprintf(stdout, "[VOID NEXUS] Fragment %d hit! Health: %d\n", 
                fragment_id, boss->fragment_health[fragment_id]);
        
        // If fragment destroyed, spawn particles and award bonus
        if (boss->fragment_health[fragment_id] <= 0) {
            comet_buster_spawn_explosion(game, boss->fragment_positions[fragment_id][0],
                                        boss->fragment_positions[fragment_id][1], 2, 20);
            
            game->score += 250;
            game->score_multiplier += 0.1;  // Boost multiplier
            
            // Remove destroyed fragment by shifting array
            for (int i = fragment_id; i < boss->fragment_count - 1; i++) {
                boss->fragment_positions[i][0] = boss->fragment_positions[i+1][0];
                boss->fragment_positions[i][1] = boss->fragment_positions[i+1][1];
                boss->fragment_health[i] = boss->fragment_health[i+1];
            }
            boss->fragment_count--;
            
            // If all fragments destroyed, defeat boss
            if (boss->fragment_count == 0) {
                boss->health = 0;
                fprintf(stdout, "[VOID NEXUS] All fragments destroyed! Boss defeated!\n");
            }
        }
    }
}

// ============================================================================
// VOID NEXUS RENDERING
// ============================================================================

void draw_void_nexus_boss(BossShip *boss, cairo_t *cr, int width, int height) {
    (void)width;
    (void)height;
    
    if (!boss || !boss->active) return;
    
    cairo_save(cr);
    
    if (boss->fragment_count == 0) {
        // Draw main body - crystalline octagon with rotating rings
        cairo_translate(cr, boss->x, boss->y);
        cairo_rotate(cr, boss->rotation * M_PI / 180.0);
        
        // Outer energy ring (pulsing)
        double pulse = 0.5 + 0.3 * sin(boss->rotation * M_PI / 180.0 * 0.1);
        cairo_set_source_rgba(cr, 0.2, 0.8, 1.0, pulse);  // Cyan glow
        cairo_arc(cr, 0, 0, 40.0 + pulse * 5.0, 0, 2.0 * M_PI);
        cairo_stroke_preserve(cr);
        cairo_fill(cr);
        
        // Main crystalline body (octagon)
        cairo_set_source_rgb(cr, 0.3, 0.7, 1.0);  // Bright cyan
        double oct_radius = 30.0;
        for (int i = 0; i < 8; i++) {
            double angle = (i * 2.0 * M_PI / 8.0) + (boss->rotation * M_PI / 180.0);
            double x = cos(angle) * oct_radius;
            double y = sin(angle) * oct_radius;
            
            if (i == 0) {
                cairo_move_to(cr, x, y);
            } else {
                cairo_line_to(cr, x, y);
            }
        }
        cairo_close_path(cr);
        cairo_fill_preserve(cr);
        cairo_stroke(cr);
        
        // Core nucleus (bright white)
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_arc(cr, 0, 0, 6.0, 0, 2.0 * M_PI);
        cairo_fill(cr);
        
        // Damage flash
        if (boss->damage_flash_timer > 0) {
            cairo_set_source_rgba(cr, 1.0, 0.5, 0.5, 0.6);
            cairo_arc(cr, 0, 0, 35.0, 0, 2.0 * M_PI);
            cairo_fill(cr);
        }
        
    } else {
        // Draw fragments - smaller crystals
        for (int i = 0; i < boss->fragment_count; i++) {
            cairo_translate(cr, boss->fragment_positions[i][0], boss->fragment_positions[i][1]);
            
            double frag_pulse = 0.3 + 0.2 * sin((boss->rotation + i * 45) * M_PI / 180.0 * 0.1);
            
            // Fragment glow
            cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, frag_pulse);
            cairo_arc(cr, 0, 0, 25.0 + frag_pulse * 3.0, 0, 2.0 * M_PI);
            cairo_stroke_preserve(cr);
            
            // Fragment body (hexagon)
            cairo_set_source_rgb(cr, 0.2, 0.9, 1.0);
            double hex_radius = 20.0;
            for (int j = 0; j < 6; j++) {
                double angle = (j * 2.0 * M_PI / 6.0);
                double x = cos(angle) * hex_radius;
                double y = sin(angle) * hex_radius;
                
                if (j == 0) {
                    cairo_move_to(cr, x, y);
                } else {
                    cairo_line_to(cr, x, y);
                }
            }
            cairo_close_path(cr);
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
            
            // Fragment core
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_arc(cr, 0, 0, 4.0, 0, 2.0 * M_PI);
            cairo_fill(cr);
            
            // Fragment health indicator
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
            cairo_set_font_size(cr, 8);
            char health_text[8];
            snprintf(health_text, sizeof(health_text), "%d", boss->fragment_health[i]);
            cairo_move_to(cr, -5, 3);
            cairo_show_text(cr, health_text);
            
            cairo_translate(cr, -boss->fragment_positions[i][0], -boss->fragment_positions[i][1]);
        }
    }
    
    cairo_restore(cr);
    
    // Draw health bar
    double bar_width = 100.0;
    double bar_height = 8.0;
    double bar_x = boss->x - bar_width / 2.0;
    double bar_y = boss->y - 55.0;
    
    // Background
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_rectangle(cr, bar_x, bar_y, bar_width, bar_height);
    cairo_fill(cr);
    
    // Health bar (cyan)
    double health_percent = (double)boss->health / boss->max_health;
    if (health_percent < 0) health_percent = 0;
    
    cairo_set_source_rgb(cr, 0.0, 1.0, 1.0);
    cairo_rectangle(cr, bar_x, bar_y, bar_width * health_percent, bar_height);
    cairo_fill(cr);
    
    // Border
    cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, bar_x, bar_y, bar_width, bar_height);
    cairo_stroke(cr);
}
