#include <cairo.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cometbuster.h"
#include "visualization.h"

// Initialize splash screen
void comet_buster_init_splash_screen(CometBusterGame *game, int width, int height) {
    if (!game) return;
    
    game->splash_screen_active = true;
    game->splash_timer = 0.0;
    
    // Set wave to 32 so spawn function calculates 32 comets normally
    game->current_wave = 32;
    
    // Use the normal spawn function with wave 32
    comet_buster_spawn_wave(game, width, height);
    
    fprintf(stdout, "[SPLASH] Splash screen initialized with %d comets\n", 
            game->comet_count);
}

// Update splash screen
void comet_buster_update_splash_screen(CometBusterGame *game, double dt, int width, int height) {
    if (!game || !game->splash_screen_active) return;
    
    game->splash_timer += dt;
    
    // Use actual game physics engine for comets
    comet_buster_update_comets(game, dt, width, height);
}

// Draw splash screen using existing game rendering functions
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
    
    // Use existing game functions to draw comets and ships
    draw_comet_buster_comets(game, cr, width, height);
    draw_comet_buster_enemy_ships(game, cr, width, height);
    
    // Draw title "COMET BUSTER"
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 120.0);
    
    // Text measurement for centering
    cairo_text_extents_t extents;
    cairo_text_extents(cr, "COMET BUSTER", &extents);
    double title_x = (width - extents.width) / 2.0;
    double title_y = height / 2.0;
    
    // Draw glowing text
    for (int i = 5; i > 0; i--) {
        double alpha = 0.1 * (5 - i) / 5.0;
        cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, alpha);
        cairo_move_to(cr, title_x, title_y);
        cairo_show_text(cr, "COMET BUSTER");
    }
    
    // Draw bright main text
    cairo_set_source_rgb(cr, 0.0, 1.0, 1.0);
    cairo_move_to(cr, title_x, title_y);
    cairo_show_text(cr, "COMET BUSTER");
    
    // Draw subtitle
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 28.0);
    
    cairo_text_extents(cr, "Press any key to start", &extents);
    double subtitle_x = (width - extents.width) / 2.0;
    double subtitle_y = title_y + 80;
    
    // Blinking text effect
    double blink_alpha = 0.5 + 0.5 * sin(game->splash_timer * 3.0);
    cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, blink_alpha);
    cairo_move_to(cr, subtitle_x, subtitle_y);
    cairo_show_text(cr, "Press any key to start");
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
    if (visualizer->mouse_left_pressed) {
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
