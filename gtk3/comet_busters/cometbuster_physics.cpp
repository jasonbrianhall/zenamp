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
        } else if (ship->ship_type == 3) {
            // SENTINEL PURPLE SHIP: Defensive formation behavior
            // Moves slowly in original direction while maintaining formation spacing
            
            // First, find any other sentinels in the same formation
            double formation_center_x = ship->x;
            double formation_center_y = ship->y;
            int formation_count = 0;
            
            for (int j = 0; j < game->enemy_ship_count; j++) {
                EnemyShip *other = &game->enemy_ships[j];
                if (other->active && other->ship_type == 3 && other->formation_id == ship->formation_id) {
                    formation_center_x += other->x;
                    formation_center_y += other->y;
                    formation_count++;
                }
            }
            
            if (formation_count > 0) {
                formation_center_x /= formation_count;
                formation_center_y /= formation_count;
            }
            
            // Maintain formation spacing - gently move toward formation center
            double dx_formation = formation_center_x - ship->x;
            double dy_formation = formation_center_y - ship->y;
            double dist_to_center = sqrt(dx_formation*dx_formation + dy_formation*dy_formation);
            
            double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
            if (base_speed < 1.0) base_speed = 60.0;  // Sentinels move slower
            
            // Small correction toward formation center
            double correction_factor = 0.1 * ship->formation_cohesion;
            if (dist_to_center > 100.0 && dist_to_center > 0.1) {
                ship->vx += (dx_formation / dist_to_center) * base_speed * correction_factor;
                ship->vy += (dy_formation / dist_to_center) * base_speed * correction_factor;
            }
            
            // Maintain base velocity direction but slightly slower
            ship->vx = ship->base_vx * 0.7;
            ship->vy = ship->base_vy * 0.7;
            
            // Add formation correction
            ship->vx += (dx_formation / (dist_to_center + 1.0)) * base_speed * correction_factor;
            ship->vy += (dy_formation / (dist_to_center + 1.0)) * base_speed * correction_factor;
            
            ship->angle = atan2(ship->vy, ship->vx);
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

    // Update joystick hardware state and sync to visualizer fields
    joystick_manager_update(&visualizer->joystick_manager);
    update_visualizer_joystick(visualizer);
    
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
    
    // ========== JOYSTICK INPUT ==========
    // Override keyboard with joystick if joystick is being used
    JoystickState *active_joystick = joystick_manager_get_active(&visualizer->joystick_manager);
    bool joystick_active = (active_joystick && active_joystick->connected);
    
    if (joystick_active) {
        // Left stick movement
        if (active_joystick->axis_x < -0.5) {
            game->keyboard.key_a_pressed = true;  // Turn left
        }
        if (active_joystick->axis_x > 0.5) {
            game->keyboard.key_d_pressed = true;  // Turn right
        }
        if (active_joystick->axis_y > 0.5) {
            game->keyboard.key_w_pressed = true;  // Forward thrust
        }
        if (active_joystick->axis_y < -0.5) {
            game->keyboard.key_s_pressed = true;  // Backward thrust
        }
        
        // Firing (triggers and B button)
        if (active_joystick->axis_lt > 0.3 || 
            active_joystick->axis_rt > 0.3 || 
            active_joystick->button_b) {
            game->keyboard.key_ctrl_pressed = true;  // Fire
        }
        
        // Special abilities (X/LB for boost)
        if (active_joystick->button_x || active_joystick->button_lb) {
            game->keyboard.key_space_pressed = true;  // Boost
        }
        
        // Omnidirectional fire (Y/RB)
        if (active_joystick->button_y || active_joystick->button_rb) {
            game->keyboard.key_z_pressed = true;  // Omnidirectional fire
        }
    }
    // ====================================
    
    // If ANY keyboard movement key is pressed, disable mouse control immediately
    bool keyboard_active = visualizer->key_a_pressed || visualizer->key_d_pressed || 
                          visualizer->key_w_pressed || visualizer->key_s_pressed;
    
    // Also disable mouse if joystick is being used for movement
    bool joystick_movement_active = joystick_active && (
        active_joystick->axis_x < -0.5 || active_joystick->axis_x > 0.5 ||
        active_joystick->axis_y > 0.5 || active_joystick->axis_y < -0.5
    );
    
    bool mouse_active = visualizer->mouse_just_moved && !keyboard_active && !joystick_movement_active;
    
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
    if (game->boss_active && game->spawn_queen.active && game->spawn_queen.is_spawn_queen) {
        comet_buster_update_spawn_queen(game, dt, width, height);
    } else if (game->boss_active && game->boss.active) {
        comet_buster_update_boss(game, dt, width, height);
    }    
    
    // Spawn boss on waves 5, 10, 15, 20, etc. (every 5 waves starting at wave 5)
    // But only if the boss hasn't already been defeated this wave (game->wave_complete_timer == 0)
    if ((game->current_wave % 10 == 5) && !game->boss_active && game->comet_count == 0 && !game->boss.active && game->wave_complete_timer == 0) {
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
    
    // Check player bullets hitting boss (either Spawn Queen or regular boss)
    if (game->boss_active) {
        // Check Spawn Queen collision first
        if (game->spawn_queen.active && game->spawn_queen.is_spawn_queen) {
            for (int j = 0; j < game->bullet_count; j++) {
                if (comet_buster_check_bullet_spawn_queen(&game->bullets[j], &game->spawn_queen)) {
                    game->bullets[j].active = false;  // Consume bullet
                    game->spawn_queen.damage_flash_timer = 0.1;
                    game->consecutive_hits++;
                    
                    // Shield reduces damage
                    if (game->spawn_queen.shield_health > 0) {
                        game->spawn_queen.shield_health--;
                    } else {
                        // No shield - direct damage
                        game->spawn_queen.health--;
                    }
                    
                    fprintf(stdout, "[COLLISION] Spawn Queen hit! Health: %d, Shield: %d\n", 
                            game->spawn_queen.health, game->spawn_queen.shield_health);
                    
                    if (game->spawn_queen.health <= 0) {
                        comet_buster_destroy_spawn_queen(game, width, height, visualizer);
                    }
                    break;
                }
            }
            
            // Check Spawn Queen-player ship collision
            double dx = game->ship_x - game->spawn_queen.x;
            double dy = game->ship_y - game->spawn_queen.y;
            double dist = sqrt(dx*dx + dy*dy);
            double collision_dist = 20.0 + 50.0;  // Player ~20px, Queen ~50px
            
            if (dist < collision_dist) {
                comet_buster_on_ship_hit(game, visualizer);
            }
        }
        // Regular Death Star boss collision
        else if (game->boss.active) {
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
