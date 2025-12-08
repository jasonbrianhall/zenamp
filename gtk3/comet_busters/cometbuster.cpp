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


bool comet_buster_check_bullet_boss(Bullet *b, BossShip *boss) {
    if (!b || !b->active || !boss || !boss->active) return false;
    
    double dx = boss->x - b->x;
    double dy = boss->y - b->y;
    double dist = sqrt(dx*dx + dy*dy);
    double collision_dist = 35.0;  // Boss collision radius
    
    return (dist < collision_dist);
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
        } else if (ship->ship_type == 3) {
            // Sentinel purple ship
            cairo_set_source_rgb(cr, 0.8, 0.2, 1.0);  // Bright purple
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
            } else if (ship->ship_type == 3) {
                // Sentinel purple shield: bright purple
                cairo_set_source_rgba(cr, 0.8, 0.4, 1.0, 0.5);
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
        
        // Draw formation connection lines between Sentinel ships
        if (ship->ship_type == 3) {
            cairo_save(cr);
            
            // Find and draw lines to other sentinels in formation
            for (int j = i + 1; j < game->enemy_ship_count; j++) {
                EnemyShip *other = &game->enemy_ships[j];
                if (other->active && other->ship_type == 3 && other->formation_id == ship->formation_id) {
                    cairo_set_source_rgba(cr, 0.8, 0.4, 1.0, 0.3);  // Purple line
                    cairo_set_line_width(cr, 1.0);
                    cairo_move_to(cr, ship->x, ship->y);
                    cairo_line_to(cr, other->x, other->y);
                    cairo_stroke(cr);
                }
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
