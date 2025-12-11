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

void comet_buster_update_enemy_ships(CometBusterGame *game, double dt, int width, int height, Visualizer *visualizer) {
    if (!game) return;
    
    for (int i = 0; i < game->enemy_ship_count; i++) {
        EnemyShip *ship = &game->enemy_ships[i];
        
        if (!ship->active) continue;
        
        // Update shield impact timer
        if (ship->shield_impact_timer > 0) {
            ship->shield_impact_timer -= dt;
        }
        
        if (ship->ship_type == 1) {
            // AGGRESSIVE RED SHIP: Chase player with smooth turning
            double dx = game->ship_x - ship->x;
            double dy = game->ship_y - ship->y;
            double dist_to_player = sqrt(dx*dx + dy*dy);
            
            if (dist_to_player > 0.1) {
                // Move toward player at constant speed with smooth turning
                double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                if (base_speed < 1.0) base_speed = 100.0;  // Default speed if not set
                
                double target_vx = (dx / dist_to_player) * base_speed;
                double target_vy = (dy / dist_to_player) * base_speed;
                
                // Smooth turning: red ships turn faster when chasing
                double turn_rate = 0.20;  // Red ships are aggressive, turn quickly
                ship->vx = ship->vx * (1.0 - turn_rate) + target_vx * turn_rate;
                ship->vy = ship->vy * (1.0 - turn_rate) + target_vy * turn_rate;
                
                // Update angle to face player
                ship->angle = atan2(ship->vy, ship->vx);
            }
        } else if (ship->ship_type == 2) {
            // HUNTER GREEN SHIP: Follow sine wave, but chase player if close
            double dx = game->ship_x - ship->x;
            double dy = game->ship_y - ship->y;
            double dist_to_player = sqrt(dx*dx + dy*dy);
            
            double chase_range = 300.0;  // Switch to chasing if player within 300px
            
            if (dist_to_player < chase_range && dist_to_player > 0.1) {
                // Chase player - with smooth turning
                double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                if (base_speed < 1.0) base_speed = 90.0;
                
                double target_vx = (dx / dist_to_player) * base_speed;
                double target_vy = (dy / dist_to_player) * base_speed;
                
                // Smooth turning: blend current velocity with target direction
                double turn_rate = 0.15;  // 0.0-1.0, higher = snappier, lower = smoother
                ship->vx = ship->vx * (1.0 - turn_rate) + target_vx * turn_rate;
                ship->vy = ship->vy * (1.0 - turn_rate) + target_vy * turn_rate;
                ship->angle = atan2(ship->vy, ship->vx);
            } else {
                // Update patrol behavior
                ship->patrol_behavior_timer += dt;
                if (ship->patrol_behavior_timer >= ship->patrol_behavior_duration) {
                    // Time to change behavior
                    ship->patrol_behavior_timer = 0.0;
                    ship->patrol_behavior_duration = 2.0 + (rand() % 30) / 10.0;  // 2-5 seconds
                    
                    int behavior_roll = rand() % 100;
                    if (behavior_roll < 70) {
                        ship->patrol_behavior_type = 0;  // 70% straight movement
                    } else if (behavior_roll < 90) {
                        ship->patrol_behavior_type = 1;  // 20% circular movement
                        // Set circle center ahead of current direction
                        double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                        if (base_speed > 0.1) {
                            ship->patrol_circle_center_x = ship->x + (ship->base_vx / base_speed) * 150.0;
                            ship->patrol_circle_center_y = ship->y + (ship->base_vy / base_speed) * 150.0;
                        }
                        ship->patrol_circle_angle = 0.0;
                    } else {
                        ship->patrol_behavior_type = 2;  // 10% sudden direction change
                        // Pick a new random direction
                        double rand_angle = (rand() % 360) * (M_PI / 180.0);
                        double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                        if (base_speed < 1.0) base_speed = 90.0;
                        ship->base_vx = cos(rand_angle) * base_speed;
                        ship->base_vy = sin(rand_angle) * base_speed;
                    }
                }
                
                // Apply patrol behavior
                double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                double target_vx, target_vy;
                
                if (ship->patrol_behavior_type == 0) {
                    // Straight movement with gentle sine wave
                    if (base_speed > 0.1) {
                        double dir_x = ship->base_vx / base_speed;
                        double dir_y = ship->base_vy / base_speed;
                        double perp_x = -dir_y;
                        double perp_y = dir_x;
                        double wave_amplitude = 40.0;
                        double wave_frequency = 1.2;
                        double sine_offset = sin(ship->path_time * wave_frequency * M_PI) * wave_amplitude;
                        target_vx = dir_x * base_speed + perp_x * sine_offset;
                        target_vy = dir_y * base_speed + perp_y * sine_offset;
                    } else {
                        target_vx = ship->vx;
                        target_vy = ship->vy;
                    }
                } else if (ship->patrol_behavior_type == 1) {
                    // Circular movement - move smoothly instead of teleporting
                    ship->patrol_circle_angle += (base_speed / ship->patrol_circle_radius) * dt;
                    
                    // Calculate target position on circle
                    double target_x = ship->patrol_circle_center_x + cos(ship->patrol_circle_angle) * ship->patrol_circle_radius;
                    double target_y = ship->patrol_circle_center_y + sin(ship->patrol_circle_angle) * ship->patrol_circle_radius;
                    
                    // Move toward target instead of teleporting
                    double dx_circle = target_x - ship->x;
                    double dy_circle = target_y - ship->y;
                    double dist_to_target = sqrt(dx_circle*dx_circle + dy_circle*dy_circle);
                    
                    if (dist_to_target > 0.1) {
                        target_vx = (dx_circle / dist_to_target) * base_speed;
                        target_vy = (dy_circle / dist_to_target) * base_speed;
                    } else {
                        target_vx = -sin(ship->patrol_circle_angle) * base_speed;
                        target_vy = cos(ship->patrol_circle_angle) * base_speed;
                    }
                    
                    ship->angle = atan2(target_vy, target_vx);  // Face direction of movement
                } else {
                    // Evasive turns - use base_vx/base_vy for new direction
                    target_vx = ship->base_vx;
                    target_vy = ship->base_vy;
                }
                
                // Smooth turning towards target velocity
                double turn_rate = 0.12;  // Smoother for patrol behaviors
                ship->vx = ship->vx * (1.0 - turn_rate) + target_vx * turn_rate;
                ship->vy = ship->vy * (1.0 - turn_rate) + target_vy * turn_rate;
                ship->angle = atan2(ship->vy, ship->vx);
            }
        } else if (ship->ship_type == 3) {
            // SENTINEL PURPLE SHIP: Formation-based with occasional coordinated maneuvers
            
            // Update patrol behavior timer
            ship->patrol_behavior_timer += dt;
            if (ship->patrol_behavior_timer >= ship->patrol_behavior_duration) {
                ship->patrol_behavior_timer = 0.0;
                ship->patrol_behavior_duration = 3.0 + (rand() % 30) / 10.0;  // 3-6 seconds (slower changes)
                
                int behavior_roll = rand() % 100;
                if (behavior_roll < 75) {
                    ship->patrol_behavior_type = 0;  // 75% formation maintenance
                } else if (behavior_roll < 90) {
                    ship->patrol_behavior_type = 1;  // 15% coordinated circular movement
                    double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                    if (base_speed > 0.1) {
                        ship->patrol_circle_center_x = ship->x + (ship->base_vx / base_speed) * 120.0;
                        ship->patrol_circle_center_y = ship->y + (ship->base_vy / base_speed) * 120.0;
                    }
                    ship->patrol_circle_angle = 0.0;
                } else {
                    ship->patrol_behavior_type = 2;  // 10% formation-wide direction change
                    double rand_angle = (rand() % 360) * (M_PI / 180.0);
                    double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                    if (base_speed < 1.0) base_speed = 60.0;
                    ship->base_vx = cos(rand_angle) * base_speed;
                    ship->base_vy = sin(rand_angle) * base_speed;
                }
            }
            
            // Find formation center for positioning
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
            
            double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
            if (base_speed < 1.0) base_speed = 60.0;
            
            double target_vx, target_vy;
            
            if (ship->patrol_behavior_type == 0) {
                // Standard formation movement with gentle spacing correction
                double dx_formation = formation_center_x - ship->x;
                double dy_formation = formation_center_y - ship->y;
                double dist_to_center = sqrt(dx_formation*dx_formation + dy_formation*dy_formation);
                
                double correction_factor = 0.08 * ship->formation_cohesion;
                if (dist_to_center > 100.0 && dist_to_center > 0.1) {
                    target_vx = ship->base_vx * 0.7 + (dx_formation / dist_to_center) * base_speed * correction_factor;
                    target_vy = ship->base_vy * 0.7 + (dy_formation / dist_to_center) * base_speed * correction_factor;
                } else {
                    target_vx = ship->base_vx * 0.7;
                    target_vy = ship->base_vy * 0.7;
                }
            } else if (ship->patrol_behavior_type == 1) {
                // Coordinated circular movement around formation center - move smoothly
                ship->patrol_circle_angle += (base_speed / ship->patrol_circle_radius) * dt;
                
                // Calculate target position on circle
                double target_x = ship->patrol_circle_center_x + cos(ship->patrol_circle_angle) * ship->patrol_circle_radius;
                double target_y = ship->patrol_circle_center_y + sin(ship->patrol_circle_angle) * ship->patrol_circle_radius;
                
                // Move toward target instead of teleporting
                double dx_circle = target_x - ship->x;
                double dy_circle = target_y - ship->y;
                double dist_to_target = sqrt(dx_circle*dx_circle + dy_circle*dy_circle);
                
                if (dist_to_target > 0.1) {
                    target_vx = (dx_circle / dist_to_target) * base_speed * 0.8;
                    target_vy = (dy_circle / dist_to_target) * base_speed * 0.8;
                } else {
                    target_vx = -sin(ship->patrol_circle_angle) * base_speed * 0.8;
                    target_vy = cos(ship->patrol_circle_angle) * base_speed * 0.8;
                }
                
                ship->angle = atan2(target_vy, target_vx);  // Face direction of movement
            } else {
                // Coordinated direction change
                target_vx = ship->base_vx * 0.8;
                target_vy = ship->base_vy * 0.8;
            }
            
            // Smooth turning towards target velocity
            double turn_rate = 0.10;  // Smooth turning for formation flights
            ship->vx = ship->vx * (1.0 - turn_rate) + target_vx * turn_rate;
            ship->vy = ship->vy * (1.0 - turn_rate) + target_vy * turn_rate;
            ship->angle = atan2(ship->vy, ship->vx);
        } else if (ship->ship_type == 4) {
            // BROWN COAT ELITE BLUE SHIP
            comet_buster_update_brown_coat_ship(game, i, dt, visualizer);
        } else {
            // PATROL BLUE SHIP: More dynamic patrol with occasional evasive maneuvers
            ship->patrol_behavior_timer += dt;
            if (ship->patrol_behavior_timer >= ship->patrol_behavior_duration) {
                // Time to change behavior
                ship->patrol_behavior_timer = 0.0;
                ship->patrol_behavior_duration = 2.0 + (rand() % 30) / 10.0;  // 2-5 seconds
                
                int behavior_roll = rand() % 100;
                if (behavior_roll < 60) {
                    ship->patrol_behavior_type = 0;  // 60% straight movement
                } else if (behavior_roll < 80) {
                    ship->patrol_behavior_type = 1;  // 20% circular movement
                    // Set circle center perpendicular to current direction
                    double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                    if (base_speed > 0.1) {
                        double perp_x = -ship->base_vy / base_speed;
                        double perp_y = ship->base_vx / base_speed;
                        ship->patrol_circle_center_x = ship->x + perp_x * 100.0;
                        ship->patrol_circle_center_y = ship->y + perp_y * 100.0;
                    }
                    ship->patrol_circle_angle = 0.0;
                } else {
                    ship->patrol_behavior_type = 2;  // 20% sudden direction change
                    // Pick a new random direction
                    double rand_angle = (rand() % 360) * (M_PI / 180.0);
                    double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
                    if (base_speed < 1.0) base_speed = 80.0;
                    ship->base_vx = cos(rand_angle) * base_speed;
                    ship->base_vy = sin(rand_angle) * base_speed;
                }
            }
            
            // Apply patrol behavior
            double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
            double target_vx, target_vy;
            
            if (ship->patrol_behavior_type == 0) {
                // Straight movement with sine wave oscillation
                if (base_speed > 0.1) {
                    double dir_x = ship->base_vx / base_speed;
                    double dir_y = ship->base_vy / base_speed;
                    double perp_x = -dir_y;
                    double perp_y = dir_x;
                    double wave_amplitude = 50.0;
                    double wave_frequency = 1.5;
                    double sine_offset = sin(ship->path_time * wave_frequency * M_PI) * wave_amplitude;
                    target_vx = dir_x * base_speed + perp_x * sine_offset;
                    target_vy = dir_y * base_speed + perp_y * sine_offset;
                } else {
                    target_vx = ship->vx;
                    target_vy = ship->vy;
                }
                ship->path_time += dt;
            } else if (ship->patrol_behavior_type == 1) {
                // Circular movement - move smoothly along circle, don't teleport
                ship->patrol_circle_angle += (base_speed / ship->patrol_circle_radius) * dt;
                
                // Calculate target position on circle
                double target_x = ship->patrol_circle_center_x + cos(ship->patrol_circle_angle) * ship->patrol_circle_radius;
                double target_y = ship->patrol_circle_center_y + sin(ship->patrol_circle_angle) * ship->patrol_circle_radius;
                
                // Move toward target position instead of teleporting
                double dx_circle = target_x - ship->x;
                double dy_circle = target_y - ship->y;
                double dist_to_target = sqrt(dx_circle*dx_circle + dy_circle*dy_circle);
                
                if (dist_to_target > 0.1) {
                    target_vx = (dx_circle / dist_to_target) * base_speed;
                    target_vy = (dy_circle / dist_to_target) * base_speed;
                } else {
                    target_vx = -sin(ship->patrol_circle_angle) * base_speed;
                    target_vy = cos(ship->patrol_circle_angle) * base_speed;
                }
                
                ship->angle = atan2(target_vy, target_vx);  // Face direction of movement
            } else {
                // Evasive turns
                target_vx = ship->base_vx;
                target_vy = ship->base_vy;
            }
            
            // Smooth turning towards target velocity
            double turn_rate = 0.14;  // Slightly sharper for blue ships, less formation-locked
            ship->vx = ship->vx * (1.0 - turn_rate) + target_vx * turn_rate;
            ship->vy = ship->vy * (1.0 - turn_rate) + target_vy * turn_rate;
            ship->angle = atan2(ship->vy, ship->vx);
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
                    
                    // Pass ship index (i) so bullet knows who fired it
                    comet_buster_spawn_enemy_bullet_from_ship(game, ship->x, ship->y, vx, vy, i);
                    
                    // Play alien fire sound
#ifdef ExternalSound
                    if (visualizer && visualizer->audio.sfx_alien_fire && !game->splash_screen_active) {
                        //audio_play_sound(&visualizer->audio, visualizer->audio.sfx_alien_fire);
                    }
#endif
                    
                    // Aggressive ships shoot more frequently
                    ship->shoot_cooldown = 0.3 + (rand() % 50) / 100.0;  // 0.3-0.8 sec (faster)
                }
            }
        } else if (ship->ship_type == 2) {
            // GREEN SHIPS: Shoot at blue ships if close (to provoke them), OR at nearest comet VERY fast, OR at player if close
            double provoke_range = 200.0;  // Range to shoot at blue ships
            double chase_range = 300.0;    // Range to start shooting at player
            double dx_player = game->ship_x - ship->x;
            double dy_player = game->ship_y - ship->y;
            double dist_to_player = sqrt(dx_player*dx_player + dy_player*dy_player);
            
            // Priority 1: Try to shoot at nearby blue ships to provoke them
            bool found_blue_ship = false;
            int nearest_blue_idx = -1;
            double nearest_blue_dist = 1e9;
            
            for (int j = 0; j < game->enemy_ship_count; j++) {
                EnemyShip *target_ship = &game->enemy_ships[j];
                if (!target_ship->active || target_ship->ship_type != 0) continue;  // Only target blue ships (type 0)
                
                double dx = target_ship->x - ship->x;
                double dy = target_ship->y - ship->y;
                double dist = sqrt(dx*dx + dy*dy);
                
                if (dist < provoke_range && dist < nearest_blue_dist) {
                    nearest_blue_dist = dist;
                    nearest_blue_idx = j;
                    found_blue_ship = true;
                }
            }
            
            if (found_blue_ship) {
                // Shoot at the blue ship to provoke it
                ship->shoot_cooldown -= dt;
                if (ship->shoot_cooldown <= 0) {
                    EnemyShip *target = &game->enemy_ships[nearest_blue_idx];
                    double dx = target->x - ship->x;
                    double dy = target->y - ship->y;
                    double dist = sqrt(dx*dx + dy*dy);
                    
                    if (dist > 0.01) {
                        double bullet_speed = 150.0;
                        double vx = (dx / dist) * bullet_speed;
                        double vy = (dy / dist) * bullet_speed;
                        
                        comet_buster_spawn_enemy_bullet_from_ship(game, ship->x, ship->y, vx, vy, i);
                        
                        // Play alien fire sound
#ifdef ExternalSound
                        if (visualizer && visualizer->audio.sfx_alien_fire && !game->splash_screen_active) {
                            audio_play_sound(&visualizer->audio, visualizer->audio.sfx_alien_fire);
                        }
#endif
                        
                        // Green ships shoot VERY fast when provoking
                        ship->shoot_cooldown = 0.2 + (rand() % 25) / 100.0;  // 0.2-0.45 sec
                    }
                }
            }
            // Priority 2: Check if player is within range
            else if (dist_to_player < chase_range) {
                // Shoot at player
                ship->shoot_cooldown -= dt;
                if (ship->shoot_cooldown <= 0) {
                    if (dist_to_player > 0.01) {
                        double bullet_speed = 150.0;
                        double vx = (dx_player / dist_to_player) * bullet_speed;
                        double vy = (dy_player / dist_to_player) * bullet_speed;
                        
                        comet_buster_spawn_enemy_bullet_from_ship(game, ship->x, ship->y, vx, vy, i);
                        
                        // Play alien fire sound
#ifdef ExternalSound
                        if (visualizer && visualizer->audio.sfx_alien_fire && !game->splash_screen_active) {
                            audio_play_sound(&visualizer->audio, visualizer->audio.sfx_alien_fire);
                        }
#endif
                        
                        // Green ships shoot VERY fast at player too
                        ship->shoot_cooldown = 0.15 + (rand() % 25) / 100.0;  // 0.15-0.4 sec (very fast!)
                    }
                }
            }
            // Priority 3: Shoot at nearest comet
            else if (game->comet_count > 0) {
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
                            
                            comet_buster_spawn_enemy_bullet_from_ship(game, ship->x, ship->y, vx, vy, i);
                            
                            // Play alien fire sound
#ifdef ExternalSound
                            if (visualizer && visualizer->audio.sfx_alien_fire && !game->splash_screen_active) {
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
        } else if (ship->ship_type == 3) {
            // PURPLE SENTINEL SHIPS: Shoot at blue ships if close (to provoke them), OR at nearest comet
            double provoke_range = 200.0;  // Range to shoot at blue ships
            
            // Priority 1: Try to shoot at nearby blue ships to provoke them
            bool found_blue_ship = false;
            int nearest_blue_idx = -1;
            double nearest_blue_dist = 1e9;
            
            for (int j = 0; j < game->enemy_ship_count; j++) {
                EnemyShip *target_ship = &game->enemy_ships[j];
                if (!target_ship->active || target_ship->ship_type != 0) continue;  // Only target blue ships (type 0)
                
                double dx = target_ship->x - ship->x;
                double dy = target_ship->y - ship->y;
                double dist = sqrt(dx*dx + dy*dy);
                
                if (dist < provoke_range && dist < nearest_blue_dist) {
                    nearest_blue_dist = dist;
                    nearest_blue_idx = j;
                    found_blue_ship = true;
                }
            }
            
            if (found_blue_ship) {
                // Shoot at the blue ship to provoke it
                ship->shoot_cooldown -= dt;
                if (ship->shoot_cooldown <= 0) {
                    EnemyShip *target = &game->enemy_ships[nearest_blue_idx];
                    double dx = target->x - ship->x;
                    double dy = target->y - ship->y;
                    double dist = sqrt(dx*dx + dy*dy);
                    
                    if (dist > 0.01) {
                        double bullet_speed = 150.0;
                        double vx = (dx / dist) * bullet_speed;
                        double vy = (dy / dist) * bullet_speed;
                        
                        comet_buster_spawn_enemy_bullet_from_ship(game, ship->x, ship->y, vx, vy, i);
                        
                        // Play alien fire sound
#ifdef ExternalSound
                        if (visualizer && visualizer->audio.sfx_alien_fire && !game->splash_screen_active) {
                            audio_play_sound(&visualizer->audio, visualizer->audio.sfx_alien_fire);
                        }
#endif
                        
                        // Purple ships shoot fairly fast when provoking
                        ship->shoot_cooldown = 0.4 + (rand() % 30) / 100.0;  // 0.4-0.7 sec
                    }
                }
            }
            // Priority 2: Shoot at nearest comet
            else if (game->comet_count > 0) {
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
                            
                            comet_buster_spawn_enemy_bullet_from_ship(game, ship->x, ship->y, vx, vy, i);
                            
                            // Play alien fire sound
#ifdef ExternalSound
                            if (visualizer && visualizer->audio.sfx_alien_fire && !game->splash_screen_active) {
                                audio_play_sound(&visualizer->audio, visualizer->audio.sfx_alien_fire);
                            }
#endif
                            
                            // Purple ships shoot at moderate speed
                            ship->shoot_cooldown = 0.5 + (rand() % 30) / 100.0;  // 0.5-0.8 sec
                        }
                    } else {
                        // Reload even if no target in range
                        ship->shoot_cooldown = 0.5;
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
                            
                            comet_buster_spawn_enemy_bullet_from_ship(game, ship->x, ship->y, vx, vy, i);
                            
                            // Play alien fire sound
#ifdef ExternalSound
                            if (visualizer && visualizer->audio.sfx_alien_fire && !game->splash_screen_active) {
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

void update_comet_buster(Visualizer *visualizer, double dt) {
    if (!visualizer) return;
    
    CometBusterGame *game = &visualizer->comet_buster;

#ifdef ExternalSound

    // Update joystick hardware state and sync to visualizer fields
    joystick_manager_update(&visualizer->joystick_manager);
    update_visualizer_joystick(visualizer);

    // Handle splash screen
    if (game->splash_screen_active) {
        comet_buster_update_splash_screen(game, dt, visualizer->width, visualizer->height, visualizer);
        
        // Check for input to start game
        if (comet_buster_splash_screen_input_detected(visualizer)) {
            comet_buster_exit_splash_screen(game);
            fprintf(stdout, "[SPLASH] Splash screen exited, game starting\n");
        }
        return;  // Don't update game yet
    }
#endif

    
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
    // Get joystick state
    JoystickState *active_joystick = joystick_manager_get_active(&visualizer->joystick_manager);
    bool joystick_connected = (active_joystick && active_joystick->connected);
    
    // Check if ANY joystick input is active (buttons, sticks, triggers)
    bool joystick_any_input = false;
    if (joystick_connected) {
        // Check stick movement
        if (fabs(active_joystick->axis_x) > 0.3 || fabs(active_joystick->axis_y) > 0.3) {
            joystick_any_input = true;
        }
        // Check triggers
        if (active_joystick->axis_lt > 0.1 || active_joystick->axis_rt > 0.1) {
            joystick_any_input = true;
        }
        // Check any button
        if (active_joystick->button_a || active_joystick->button_b || 
            active_joystick->button_x || active_joystick->button_y ||
            active_joystick->button_lb || active_joystick->button_rb ||
            active_joystick->button_start || active_joystick->button_back ||
            active_joystick->button_left_stick || active_joystick->button_right_stick) {
            joystick_any_input = true;
        }
    }
  
    bool joy_active=false;  
    if (joystick_connected && joystick_any_input) {
         joy_active=true;
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
    
    // Disable mouse if ANY joystick input is active
    bool mouse_active = visualizer->mouse_just_moved && !keyboard_active && !joy_active;
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
    if (game->boss_active && game->spawn_queen.active && game->spawn_queen.is_spawn_queen && game->current_wave % 20 == 10) {
        comet_buster_update_spawn_queen(game, dt, width, height);
    } else if (game->boss_active && game->boss.active) {
        // Route to correct boss based on wave
        if (game->current_wave % 20 == 5) {
            comet_buster_update_boss(game, dt, width, height);        // Death Star (wave 5, 25, 45, etc)
        } else if (game->current_wave % 20 == 15) {
            comet_buster_update_void_nexus(game, dt, width, height);  // Void Nexus (wave 15, 35, 55, etc)
        } else if (game->current_wave % 20 == 0) {
            comet_buster_update_harbinger(game, dt, width, height);   // Harbinger (wave 20, 40, 60, etc)
        }
    } 
    
    // Spawn boss on waves 5, 10, 15, 20, etc. (every 5 waves starting at wave 5)
    // But only if the boss hasn't already been defeated this wave (game->wave_complete_timer == 0)
    /*if ((game->current_wave % 15 == 5) && !game->boss_active && game->comet_count == 0 && !game->boss.active && game->wave_complete_timer == 0) {
        fprintf(stdout, "[UPDATE] Conditions met to spawn boss: Wave=%d, BossActive=%d, CometCount=%d\n",
                game->current_wave, game->boss_active, game->comet_count);
        comet_buster_spawn_boss(game, width, height);
    }*/
    
    // Update fuel system
    comet_buster_update_fuel(game, dt);
    
    // Handle wave completion and progression (only if not already counting down)
    // BUT: Don't progress if boss is active (boss must be defeated first)
    if (game->wave_complete_timer <= 0 && !game->boss_active) {
        comet_buster_update_wave_progression(game);
        
        // NEW: Play wave complete sound when timer just started
        if (game->wave_complete_timer > 0) {
#ifdef ExternalSound
            if (visualizer && visualizer->audio.sfx_wave_complete) {
                audio_play_sound(&visualizer->audio, visualizer->audio.sfx_wave_complete);
                fprintf(stdout, "[AUDIO] Playing wave complete sound\n");
            }
#endif
        }
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
    
    // Check enemy bullets hitting enemy ships (friendly fire)
    for (int i = 0; i < game->enemy_ship_count; i++) {
        EnemyShip *target_ship = &game->enemy_ships[i];
        if (!target_ship->active) continue;
        
        for (int j = 0; j < game->enemy_bullet_count; j++) {
            Bullet *bullet = &game->enemy_bullets[j];
            if (!bullet->active) continue;
            
            // CRITICAL: Skip if bullet came from this same ship
            if (bullet->owner_ship_id == i) continue;
            
            double dx = target_ship->x - bullet->x;
            double dy = target_ship->y - bullet->y;
            double dist = sqrt(dx*dx + dy*dy);
            double collision_dist = 15.0;  // Enemy ship collision radius
            
            if (dist < collision_dist) {
                // Try to provoke blue ships first
                bool was_provoked = comet_buster_hit_enemy_ship_provoke(game, i);
                
                if (!was_provoked) {
                    // Bullet destroys on impact
                    bullet->active = false;
                    
                    // Ship takes damage from friendly fire (from OTHER ships)
                    if (target_ship->shield_health > 0) {
                        target_ship->shield_health--;
                        target_ship->shield_impact_angle = atan2(target_ship->y - bullet->y,
                                                                  target_ship->x - bullet->x);
                        target_ship->shield_impact_timer = 0.2;
                    } else {
                        // No shield - ship is destroyed by friendly fire
                        comet_buster_destroy_enemy_ship(game, i, width, height, visualizer);
                        
                        // Award player points for friendly fire destruction
                        game->score += (int)(150 * game->score_multiplier);
                        break;  // Exit inner loop since we destroyed the ship
                    }
                    break;  // Bullet hit this ship, move to next bullet
                } else {
                    // Blue ship was provoked, bullet destroys on impact
                    bullet->active = false;
                    break;
                }
            }
        }
    }
    
    // Check enemy bullet-ship collisions
    for (int i = 0; i < game->enemy_bullet_count; i++) {
        if (comet_buster_check_enemy_bullet_ship(game, &game->enemy_bullets[i])) {
            fprintf(stdout, "[COLLISION] Enemy bullet hit player ship! Bullet removed.\n");
            comet_buster_on_ship_hit(game, visualizer);
            // Bullet disappears on impact with player ship
            game->enemy_bullets[i].active = false;
            // Swap with last and remove
            if (i != game->enemy_bullet_count - 1) {
                game->enemy_bullets[i] = game->enemy_bullets[game->enemy_bullet_count - 1];
            }
            game->enemy_bullet_count--;
            i--;
            continue;
        }
    }
    
    // Check enemy ship-enemy ship collisions (ships destroy each other on contact)
    for (int i = 0; i < game->enemy_ship_count; i++) {
        EnemyShip *ship1 = &game->enemy_ships[i];
        if (!ship1->active) continue;
        
        for (int j = i + 1; j < game->enemy_ship_count; j++) {
            EnemyShip *ship2 = &game->enemy_ships[j];
            if (!ship2->active) continue;
            
            double dx = ship2->x - ship1->x;
            double dy = ship2->y - ship1->y;
            double dist = sqrt(dx*dx + dy*dy);
            double collision_dist = 15.0 + 15.0;  // Both have ~15px radius
            
            if (dist < collision_dist) {
                // Both ships are destroyed
                ship1->active = false;
                ship2->active = false;
                
                // Spawn explosion at midpoint
                double ex = (ship1->x + ship2->x) / 2.0;
                double ey = (ship1->y + ship2->y) / 2.0;
                comet_buster_spawn_explosion(game, ex, ey, 1, 15);  // Mid-frequency explosion
                
                // Award points for mutual destruction
                game->score += (int)(250 * game->score_multiplier);
                
                break;  // Exit inner loop after collision
            }
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
        // Check boss-comet collisions first (comets can damage boss)
        for (int j = 0; j < game->comet_count; j++) {
            Comet *comet = &game->comets[j];
            if (!comet->active) continue;
            
            // Check against regular boss
            if (game->boss.active) {
                BossShip *boss = &game->boss;
                double dx = boss->x - comet->x;
                double dy = boss->y - comet->y;
                double dist = sqrt(dx*dx + dy*dy);
                double collision_dist = 50.0 + comet->radius;  // Boss is big (~50px)
                
                if (dist < collision_dist) {
                    // Boss takes damage from comet collision
                    // Damage scales with comet size
                    int comet_damage = 2;  // Base damage
                    switch (comet->size) {
                        case COMET_SMALL:   comet_damage = 1; break;
                        case COMET_MEDIUM:  comet_damage = 2; break;
                        case COMET_LARGE:   comet_damage = 3; break;
                        case COMET_MEGA:    comet_damage = 4; break;
                        case COMET_SPECIAL: comet_damage = 4; break;
                        default:            comet_damage = 2; break;
                    }
                    
                    if (boss->shield_active && boss->shield_health > 0) {
                        // Shield absorbs damage from smaller comets, reduces damage from larger ones
                        boss->shield_health--;  // Shield always takes 1 hit
                        boss->shield_impact_angle = atan2(boss->y - comet->y,
                                                           boss->x - comet->x);
                        boss->shield_impact_timer = 0.2;
                        
                        // Large comets penetrate shields
                        if (comet->size >= COMET_LARGE) {
                            boss->health -= 1;  // Penetrating damage
                        }
                    } else {
                        // Shield down - full damage scaled by comet size
                        boss->health -= comet_damage;
                    }
                    
                    boss->damage_flash_timer = 0.1;
                    
                    // On splash screen, don't apply collision knockback to boss
                    // The comet is still destroyed normally, but boss doesn't move backwards
                    if (!game->splash_screen_active) {
                        // Apply collision impulse to boss to push it back (normal gameplay)
                        double nx = (boss->x - comet->x) / dist;
                        double ny = (boss->y - comet->y) / dist;
                        boss->vx += nx * comet->radius;  // Push boss away from comet
                        boss->vy += ny * comet->radius;
                    }
                    
                    // Comet is destroyed
                    comet_buster_destroy_comet(game, j, width, height, visualizer);
                    
                    if (boss->health <= 0) {
                        comet_buster_destroy_boss(game, width, height, visualizer);
                    }
                    break;
                }
            }
            // Also check Spawn Queen - NOTE: Spawn Queen is IMMUNE to asteroid damage!
            // She can only be damaged by player bullets
            else if (game->spawn_queen.active && game->spawn_queen.is_spawn_queen) {
                SpawnQueenBoss *queen = &game->spawn_queen;
                double dx = queen->x - comet->x;
                double dy = queen->y - comet->y;
                double dist = sqrt(dx*dx + dy*dy);
                double collision_dist = 60.0 + comet->radius;  // Queen is larger (~60px)
                
                // Spawn Queen is IMMUNE to asteroid damage - asteroids just pass through
                // IMPORTANT: The Spawn Queen (Mothership) cannot be damaged by asteroids,
                // only by direct player gunfire. This is by design - asteroids are her weapon!
                if (dist < collision_dist) {
                    // Simply destroy the comet, but do NOT damage the queen
                    comet_buster_destroy_comet(game, j, width, height, visualizer);
                    break;
                }
            }
        }
        
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
                
                // Push player away from Spawn Queen to prevent clipping inside
                if (dist > 0.1) {
                    double nx = dx / dist;  // Normal vector
                    double ny = dy / dist;
                    
                    // Push player to safe distance
                    double push_distance = collision_dist + 5.0;  // Extra buffer
                    game->ship_x = game->spawn_queen.x + nx * push_distance;
                    game->ship_y = game->spawn_queen.y + ny * push_distance;
                    
                    // Also apply knockback velocity
                    game->ship_vx = nx * 200.0;  // Push away at 200 px/s
                    game->ship_vy = ny * 200.0;
                }
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
                
                // Push player away from boss to prevent clipping inside
                if (dist > 0.1) {
                    double nx = dx / dist;  // Normal vector
                    double ny = dy / dist;
                    
                    // Push player to safe distance
                    double push_distance = collision_dist + 5.0;  // Extra buffer
                    game->ship_x = game->boss.x + nx * push_distance;
                    game->ship_y = game->boss.y + ny * push_distance;
                    
                    // Also apply knockback velocity
                    game->ship_vx = nx * 200.0;  // Push away at 200 px/s
                    game->ship_vy = ny * 200.0;
                }
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

void comet_buster_update_brown_coat_ship(CometBusterGame *game, int ship_index, double dt, Visualizer *visualizer) {
    if (!game || ship_index < 0 || ship_index >= game->enemy_ship_count) return;
    
    EnemyShip *ship = &game->enemy_ships[ship_index];
    if (!ship->active || ship->ship_type != 4) return;
    
    // Update shield impact timer
    if (ship->shield_impact_timer > 0) {
        ship->shield_impact_timer -= dt;
    }
    
    // Brown Coats are aggressive chasers with fast fire rate
    double dx = game->ship_x - ship->x;
    double dy = game->ship_y - ship->y;
    double dist_to_player = sqrt(dx*dx + dy*dy);
    
    // Chase player aggressively (larger range than red ships)
    double chase_range = 400.0;  // Larger detection range
    
    if (dist_to_player > 0.1) {
        // Move toward player with smooth turning
        double base_speed = sqrt(ship->base_vx*ship->base_vx + ship->base_vy*ship->base_vy);
        if (base_speed < 1.0) base_speed = 120.0;  // Faster than regular ships
        
        double target_vx = (dx / dist_to_player) * base_speed;
        double target_vy = (dy / dist_to_player) * base_speed;
        
        // Very responsive turning for tactical maneuvering
        double turn_rate = 0.25;  // Faster than all other types
        ship->vx = ship->vx * (1.0 - turn_rate) + target_vx * turn_rate;
        ship->vy = ship->vy * (1.0 - turn_rate) + target_vy * turn_rate;
        
        // Update angle to face target
        ship->angle = atan2(ship->vy, ship->vx);
    }
    
    // Proximity detection for burst attack
    ship->proximity_detection_timer += dt;
    if (ship->proximity_detection_timer >= 0.3) {  // Check every 0.3 seconds
        ship->proximity_detection_timer = 0.0;
        
        // Check if player OR comet is nearby for burst trigger
        bool trigger_burst = false;
        
        // Check player distance
        if (dist_to_player < 250.0) {
            trigger_burst = true;
        }
        
        // Check for nearby comets
        if (!trigger_burst && game->comet_count > 0) {
            for (int j = 0; j < game->comet_count; j++) {
                Comet *comet = &game->comets[j];
                if (!comet->active) continue;
                
                double dx_comet = comet->x - ship->x;
                double dy_comet = comet->y - ship->y;
                double dist_to_comet = sqrt(dx_comet*dx_comet + dy_comet*dy_comet);
                
                if (dist_to_comet < 280.0) {
                    trigger_burst = true;
                    break;
                }
            }
        }
        
        // Fire burst if triggered and cooldown ready
        if (trigger_burst && ship->burst_fire_cooldown <= 0) {
            comet_buster_brown_coat_fire_burst(game, ship_index);
            // Burst cooldown: fires every 2-3 seconds
            ship->burst_fire_cooldown = 2.0 + (rand() % 20) / 10.0;
        }
    }
    
    // Update burst cooldown
    if (ship->burst_fire_cooldown > 0) {
        ship->burst_fire_cooldown -= dt;
    }
    
    // Standard rapid fire (3x faster than other ships)
    ship->shoot_cooldown -= dt;
    if (ship->shoot_cooldown <= 0) {
        comet_buster_brown_coat_standard_fire(game, ship_index, visualizer);
        // Fire rate: every 0.1 seconds (3 shots per 0.3 seconds of other ships)
        ship->shoot_cooldown = 0.1 + (rand() % 10) / 100.0;  // 0.1-0.2 sec
    }
}

// Standard single-target fire for Brown Coats
void comet_buster_brown_coat_standard_fire(CometBusterGame *game, int ship_index, Visualizer *visualizer) {
    if (!game || ship_index < 0 || ship_index >= game->enemy_ship_count) return;
    
    EnemyShip *ship = &game->enemy_ships[ship_index];
    if (!ship->active) return;
    
    double dx = game->ship_x - ship->x;
    double dy = game->ship_y - ship->y;
    double dist = sqrt(dx*dx + dy*dy);
    
    if (dist > 0.01) {
        double bullet_speed = 200.0;
        double vx = (dx / dist) * bullet_speed;
        double vy = (dy / dist) * bullet_speed;
        
        comet_buster_spawn_enemy_bullet_from_ship(game, ship->x, ship->y, vx, vy, ship_index);
        
        // Sound effect (same as other aggressive ships)
        #ifdef ExternalSound
            if (visualizer && visualizer->audio.sfx_fire) {
                audio_play_sound(&visualizer->audio, visualizer->audio.sfx_fire);
            }
        #endif
    }
}

// Omnidirectional burst attack (8 directions)
void comet_buster_brown_coat_fire_burst(CometBusterGame *game, int ship_index) {
    if (!game || ship_index < 0 || ship_index >= game->enemy_ship_count) return;
    
    EnemyShip *ship = &game->enemy_ships[ship_index];
    if (!ship->active) return;
    
    // Fire 8 bullets in omnidirectional pattern
    int num_directions = 8;
    double angle_step = 2.0 * M_PI / num_directions;
    double bullet_speed = 250.0;  // Faster
    
    // Rotate pattern each burst for visual variety
    ship->last_burst_direction = (ship->last_burst_direction + 1) % 4;
    double pattern_offset = ship->last_burst_direction * (M_PI / 4.0);
    
    for (int i = 0; i < num_directions; i++) {
        double angle = pattern_offset + (i * angle_step);
        double vx = cos(angle) * bullet_speed;
        double vy = sin(angle) * bullet_speed;
        
        comet_buster_spawn_enemy_bullet_from_ship(game, ship->x, ship->y, vx, vy, ship_index);
    }
    
    ship->burst_count_this_wave++;
    
    // Sound effect for burst (more dramatic than standard fire)
    #ifdef ExternalSound
    // Burst sound would play here
    #endif
}
