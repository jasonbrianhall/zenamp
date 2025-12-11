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

void init_comet_buster_system(Visualizer *visualizer) {
    
    comet_buster_reset_game(&visualizer->comet_buster);    
}

void comet_buster_cleanup(CometBusterGame *game) {
    if (!game) return;
    
    memset(game, 0, sizeof(CometBusterGame));
}

void comet_buster_reset_game(CometBusterGame *game) {
    comet_buster_reset_game_with_splash(game, true);
}

void comet_buster_reset_game_with_splash(CometBusterGame *game, bool show_splash) {
    if (!game) return;
    
    // PHASE 1: Initialize all game state variables FIRST
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
    
    // PHASE 2: Clear object arrays
    // IMPORTANT: Do this BEFORE splash screen spawning so we have a clean slate
    game->comet_count = 0;
    game->bullet_count = 0;
    game->particle_count = 0;
    game->floating_text_count = 0;
    // NOTE: high_score_count is NOT reset - high scores persist from disk load
    game->enemy_ship_count = 0;
    game->enemy_bullet_count = 0;
    
    // Initialize boss as inactive (before splash screen so it can be activated by splash)
    game->boss_active = false;
    game->boss.active = false;
    game->spawn_queen.active = false;
    game->spawn_queen.is_spawn_queen = false;
    game->boss_spawn_timer = 0;
    game->last_boss_wave = 0;  // Track which wave had the boss
    
    // PHASE 3: Initialize splash screen only if requested
    if (show_splash) {
#ifdef ExternalSound
        comet_buster_init_splash_screen(game, 1920, 1080);
#endif
    }
    
}
