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
