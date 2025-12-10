#include <cairo.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cometbuster.h"
#include "visualization.h"

// ============================================================================
// OPENING CRAWL TEXT - PROPER LINE BY LINE
// ============================================================================

static const char *OPENING_CRAWL_LINES[] = {
    "",
    "",
    "COMET BUSTER",
    "",
    "In the not so distant future in a galaxy not so far away",
    "",
    "",
    "The Kepler-442 Asteroid Field, once a",
    "treasure trove of minerals, now lies in ruin.",
    "Asteroids fracture, comets drift, factions clash.",
    "",
    "Red warships hunt without mercy.",
    "Blue patrols guard with fragile honor.",
    "Green drones strip-mine with ruthless speed.",
    "And now... the PURPLE SENTINELS arrive—",
    "enigmatic guardians with unknown intent.",
    "",
    "You fly the DESTINY—",
    "an ancient warship of unknown origin,",
    "reborn as a mining vessel,",
    "armed with rapid-fire cannons,",
    "advanced thrusters, and omnidirectional fire.",
    "",
    "It is fragile, yet fierce.",
    "It carries no banner, no allegiance,",
    "only the will to survive.",
    "",
    "But survival is not enough.",
    "Beyond the factions loom colossal threats:",
    "MEGA BOSS SHIPS, engines of annihilation,",
    "whose presence darkens the field itself.",
    "",
    "And deeper still, from the void,",
    "alien forces gather—",
    "a tide that consumes all in its path.",
    "",
    "Your mission: endure the chaos,",
    "outwit rival factions,",
    "and face the horrors that await.",
    "",
    "The asteroid field is no longer a mine.",
    "It is a crucible of war.",
    "",
    "Survive. Score. Ascend.",
    "",
    "",
};

#define NUM_CRAWL_LINES (sizeof(OPENING_CRAWL_LINES) / sizeof(OPENING_CRAWL_LINES[0]))

// Initialize splash screen with lots of objects for impressive visuals
void comet_buster_init_splash_screen(CometBusterGame *game, int width, int height) {
    if (!game) return;
    
    game->splash_screen_active = true;
    game->splash_timer = 0.0;
    
    // Directly spawn lots of comets for impressive splash screen visuals
    // Instead of relying on wave calculations, we spawn 32 comets directly
    comet_buster_spawn_random_comets(game, 32, width, height);
    
    // Spawn a boss to make the splash screen look dramatic and dynamic
    //comet_buster_spawn_boss(game, width, height);
    
    // Spawn 3 enemy ships for additional visual variety
    for (int i = 0; i < 3; i++) {
        comet_buster_spawn_enemy_ship(game, width, height);
    }
    
    fprintf(stdout, "[SPLASH] Splash screen initialized:\n");
    fprintf(stdout, "  - %d comets\n", game->comet_count);
    fprintf(stdout, "  - Boss active: %d\n", game->boss_active);
    fprintf(stdout, "  - %d enemy ships\n", game->enemy_ship_count);
}

// Update splash screen - now includes enemy ship and boss animation
void comet_buster_update_splash_screen(CometBusterGame *game, double dt, int width, int height, Visualizer *visualizer) {
    if (!game || !game->splash_screen_active) return;
    
    game->splash_timer += dt;
    
    // Use actual game physics engine for comets
    comet_buster_update_comets(game, dt, width, height);
    
    // Also update enemy ships so they move and animate on the splash screen
    // Note: visualizer parameter is required for enemy ship update logic
    comet_buster_update_enemy_ships(game, dt, width, height, visualizer);
    
    // Update enemy bullets fired by ships
    comet_buster_update_enemy_bullets(game, dt, width, height, visualizer);
    
    // Update boss if active (so it animates on splash screen)
    if (game->boss_active) {
        comet_buster_update_boss(game, dt, width, height);
    }
    
    // Update particles (explosions, etc.)
    comet_buster_update_particles(game, dt);
    
    // Check collisions between enemy bullets and comets for visual effects
    for (int i = 0; i < game->enemy_bullet_count; i++) {
        if (!game->enemy_bullets[i].active) continue;
        
        Bullet *bullet = &game->enemy_bullets[i];
        
        for (int j = 0; j < game->comet_count; j++) {
            if (!game->comets[j].active) continue;
            
            Comet *comet = &game->comets[j];
            
            if (comet_buster_check_bullet_comet(bullet, comet)) {
                // Create visual impact
                bullet->active = false;
                
                // Spawn particles at collision point
                double px = (bullet->x + comet->x) / 2.0;
                double py = (bullet->y + comet->y) / 2.0;
                comet_buster_spawn_explosion(game, px, py, comet->frequency_band, 8);
                
                break;
            }
        }
    }
}

// Draw splash screen with proper line-by-line scrolling crawl
void comet_buster_draw_splash_screen(CometBusterGame *game, cairo_t *cr, int width, int height) {
    if (!game || !game->splash_screen_active) return;
    
    // Draw background (dark space)
    cairo_set_source_rgb(cr, 0.04, 0.06, 0.15);
    cairo_paint(cr);

    // Draw grid
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
    
    // Use existing game functions to draw all animated objects
    draw_comet_buster_comets(game, cr, width, height);
    draw_comet_buster_enemy_ships(game, cr, width, height);
    draw_comet_buster_enemy_bullets(game, cr, width, height);
    draw_comet_buster_particles(game, cr, width, height);
    
    // Draw boss if active
    if (game->boss_active) {
        draw_comet_buster_boss(&game->boss, cr, width, height);
    }
    
    // Dim the background with overlay for text visibility
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
    cairo_paint(cr);
    
    // ===== OPENING CRAWL PHASE =====
    // Duration: approximately 0-38 seconds (much longer to account for extended fade zones)
    double scroll_speed = 1.0;  // seconds per line - SLOWER

    if (game->splash_timer < 38.0) {
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 24.0);
        
        // Yellow Star Wars style text
        cairo_set_source_rgb(cr, 1.0, 0.95, 0.0);
        
        // Calculate line height
        cairo_text_extents_t extents;
        cairo_text_extents(cr, "A", &extents);
        double line_height = extents.height * 1.8;
        
        // Each line appears and scrolls from bottom to top SLOWLY
        // Scroll speed: one line per 0.6 seconds (slower than before)
        double lines_visible = (height + 200.0) / line_height;
        
        // Calculate which lines should be visible
        double current_line_offset = (game->splash_timer / scroll_speed);
        double fractional_offset = fmod(game->splash_timer, scroll_speed) / scroll_speed;
        
        // Draw all lines that could be visible
        for (int i = 0; i < (int)lines_visible + 2; i++) {
            int line_index = (int)current_line_offset + i;
            if (line_index < 0 || line_index >= (int)NUM_CRAWL_LINES) continue;
            
            // Calculate Y position (lines scroll up from bottom to top)
            double y_pos = height - (fractional_offset * line_height) + (i * line_height) - (current_line_offset * line_height);
            
            // Calculate fade for lines entering (bottom) and leaving (top) - MUCH LONGER FADE
            double alpha = 1.0;
            if (y_pos < 200.0) {
                alpha = y_pos / 200.0;  // Fade in at top - MUCH LONGER (200px instead of 120px)
            } else if (y_pos > height - 200.0) {
                alpha = (height - y_pos) / 200.0;  // Fade out at bottom - MUCH LONGER
            }
            
            if (alpha < 0.0) alpha = 0.0;
            if (alpha > 1.0) alpha = 1.0;
            
            cairo_set_source_rgba(cr, 1.0, 0.95, 0.0, alpha);
            
            // Center the text
            cairo_text_extents(cr, OPENING_CRAWL_LINES[line_index], &extents);
            double x_pos = (width - extents.width) / 2.0;
            
            cairo_move_to(cr, x_pos, y_pos);
            cairo_show_text(cr, OPENING_CRAWL_LINES[line_index]);
        }
    }
    // ===== TITLE PHASE =====
    // After crawl ends, show big GALAXY RAIDERS title
    else if (game->splash_timer < 43.0) {
        // Fade in the title
        double phase_timer = game->splash_timer - 38.0;
        double title_alpha = phase_timer / 2.0;  // Fade in over 2 seconds
        if (title_alpha > 1.0) title_alpha = 1.0;
        
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 120.0);
        
        // Draw glowing text effect
        for (int i = 5; i > 0; i--) {
            double alpha = 0.1 * (5 - i) / 5.0 * title_alpha;
            cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, alpha);
            
            cairo_text_extents_t extents;
            cairo_text_extents(cr, "COMET BUSTERS", &extents);
            double title_x = (width - extents.width) / 2.0;
            double title_y = height / 2.0 + 20;
            
            cairo_move_to(cr, title_x, title_y);
            cairo_show_text(cr, "COMET BUSTERS");
        }
        
        // Draw bright main title text
        cairo_text_extents_t extents;
        cairo_text_extents(cr, "COMET BUSTERS", &extents);
        double title_x = (width - extents.width) / 2.0;
        double title_y = height / 2.0 + 20;
        
        cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, title_alpha);
        cairo_move_to(cr, title_x, title_y);
        cairo_show_text(cr, "COMET BUSTERS");
        
        // Draw subtitle
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 28.0);
        
        cairo_text_extents(cr, "Press fire key to start", &extents);
        double subtitle_x = (width - extents.width) / 2.0;
        double subtitle_y = title_y + 80;
        
        // Blinking text effect for subtitle
        double blink_alpha = 0.5 + 0.5 * sin(game->splash_timer * 3.0);
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, blink_alpha * title_alpha);
        cairo_move_to(cr, subtitle_x, subtitle_y);
        cairo_show_text(cr, "Press fire key to start");
    }
    // ===== WAIT PHASE =====
    // Just show the title and wait for input
    else {
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 120.0);
        
        // Full brightness
        cairo_set_source_rgb(cr, 0.0, 1.0, 1.0);
        
        cairo_text_extents_t extents;
        cairo_text_extents(cr, "COMET BUSTERS", &extents);
        double title_x = (width - extents.width) / 2.0;
        double title_y = height / 2.0 + 20;
        
        cairo_move_to(cr, title_x, title_y);
        cairo_show_text(cr, "COMET BUSTERS");
        
        // Draw subtitle
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 28.0);
        
        cairo_text_extents(cr, "Press fire key to start", &extents);
        double subtitle_x = (width - extents.width) / 2.0;
        double subtitle_y = title_y + 80;
        
        // Blinking text effect for subtitle
        double blink_alpha = 0.5 + 0.5 * sin(game->splash_timer * 3.0);
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, blink_alpha);
        cairo_move_to(cr, subtitle_x, subtitle_y);
        cairo_show_text(cr, "Press fire key to start");
    }
}

// Check if splash screen should exit (any key pressed)
bool comet_buster_splash_screen_input_detected(Visualizer *visualizer) {
    if (!visualizer) return false;
    
    // Any keyboard key
    if (visualizer->key_a_pressed || visualizer->key_d_pressed ||
        visualizer->key_w_pressed || visualizer->key_s_pressed ||
        visualizer->key_z_pressed || visualizer->key_x_pressed ||
        visualizer->key_space_pressed || visualizer->key_ctrl_pressed) {
        return true;
    }
    
    // Any joystick button
    JoystickState *js = joystick_manager_get_active(&visualizer->joystick_manager);
    if (js && js->connected) {
        if (js->button_a || js->button_b || js->button_x || js->button_y ||
            js->button_start || js->button_back) {
            return true;
        }
    }
    
    // Any mouse click
    if (visualizer->mouse_left_pressed || visualizer->mouse_right_pressed || 
        visualizer->mouse_middle_pressed) {
        return true;
    }
    
    return false;
}

// Exit splash screen and start game
void comet_buster_exit_splash_screen(CometBusterGame *game) {
    if (!game) return;
    
    fprintf(stdout, "[SPLASH] Exiting splash screen, starting game\n");
    
    game->splash_screen_active = false;
    game->splash_timer = 0.0;
    
    // Clear all objects to start fresh game
    game->comet_count = 0;
    game->bullet_count = 0;
    game->particle_count = 0;
    game->floating_text_count = 0;
    game->high_score_count = 0;
    game->enemy_ship_count = 0;
    game->enemy_bullet_count = 0;
    
    game->boss_active = false;
    game->boss.active = false;
    game->spawn_queen.active = false;
    game->spawn_queen.is_spawn_queen = false;
    game->boss_spawn_timer = 0;
    game->last_boss_wave = 0;
    
    game->ship_x = 400.0;
    game->ship_y = 300.0;
    game->ship_vx = 0;
    game->ship_vy = 0;
    game->ship_angle = 0;
    game->ship_speed = 0;
    game->ship_lives = 3;
    game->invulnerability_time = 0;
    
    game->shield_health = 3;
    game->max_shield_health = 3;
    game->shield_regen_timer = 0;
    game->shield_regen_delay = 3.0;
    game->shield_regen_rate = 0.5;
    game->shield_impact_angle = 0;
    game->shield_impact_timer = 0;
    
    game->score = 0;
    game->comets_destroyed = 0;
    game->score_multiplier = 1.0;
    game->consecutive_hits = 0;
    game->current_wave = 1;
    game->wave_comets = 0;
    game->last_life_milestone = 0;
    game->game_over = false;
    game->game_won = false;
    
    game->spawn_timer = 1.0;
    game->base_spawn_rate = 1.0;
    game->beat_fire_cooldown = 0;
    game->last_beat_time = -1;
    game->difficulty_timer = 0;
    game->enemy_ship_spawn_timer = 5.0;
    game->enemy_ship_spawn_rate = 8.0;
    
    game->mouse_left_pressed = false;
    game->mouse_fire_cooldown = 0;
    game->mouse_right_pressed = false;
    game->mouse_middle_pressed = false;
    game->omni_fire_cooldown = 0;
    
    game->keyboard.key_a_pressed = false;
    game->keyboard.key_d_pressed = false;
    game->keyboard.key_w_pressed = false;
    game->keyboard.key_s_pressed = false;
    game->keyboard.key_z_pressed = false;
    game->keyboard.key_x_pressed = false;
    game->keyboard.key_space_pressed = false;
    game->keyboard.key_ctrl_pressed = false;
    
    game->energy_amount = 100.0;
    game->max_energy = 100.0;
    game->energy_burn_rate = 25.0;
    game->energy_recharge_rate = 10.0;
    game->boost_multiplier = 2.5;
    game->is_boosting = false;
    game->boost_thrust_timer = 0.0;
    
    comet_buster_load_high_scores(game);
    comet_buster_spawn_wave(game, 1920, 1080);
}
