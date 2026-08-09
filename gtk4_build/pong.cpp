#include "pong.h"
#include "visualization.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// Helper function to handle screen resize
static void pong_handle_screen_resize(PongGame *game, Visualizer *vis) {
    if (game->width != vis->width || game->height != vis->height) {
        // Scale ball
        double new_base_size = PONG_BALL_SIZE * (vis->height / 720.0);
        game->ball.size = new_base_size;
        game->ball.base_size = new_base_size;
        
        // Reposition paddles with new screen dimensions
        double paddle_offset = vis->width * 0.05;
        double new_paddle_height = PONG_PADDLE_HEIGHT * (vis->height / 720.0);
        
        // Update player paddle
        game->player.x = paddle_offset;
        game->player.height = new_paddle_height;
        if (game->player.y + game->player.height > vis->height) {
            game->player.y = vis->height - game->player.height;
        }
        game->player.target_y = game->player.y;
        
        // Update AI paddle
        game->ai.x = vis->width - paddle_offset - game->player.width;
        game->ai.height = new_paddle_height;
        if (game->ai.y + game->ai.height > vis->height) {
            game->ai.y = vis->height - game->ai.height;
        }
        game->ai.target_y = game->ai.y;
        
        // Update game dimensions
        game->width = vis->width;
        game->height = vis->height;
        game->last_width = vis->width;
        game->last_height = vis->height;
    }
}

// Helper function to detect beat from audio
static gboolean pong_detect_beat(Visualizer *vis) {
    if (!vis || !vis->frequency_bands) return FALSE;
    
    // Sum all frequency bands to get overall intensity
    double total_intensity = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        if (vis->frequency_bands[i] > 0) {
            total_intensity += vis->frequency_bands[i];
        }
    }
    
    double avg_intensity = total_intensity / VIS_FREQUENCY_BARS;
    
    // Use beat threshold from visualizer
    double threshold = vis->beat_threshold > 0 ? vis->beat_threshold : 0.5;
    
    return (avg_intensity > threshold) ? TRUE : FALSE;
}

// Get RGB color based on beat intensity (hue cycling)
static void pong_get_beat_color(double hue, double intensity, double *r, double *g, double *b) {
    // HSV to RGB conversion
    double h = fmod(hue + intensity * 120.0, 360.0);
    double s = 0.8 + intensity * 0.2;  // Saturation increases with beat
    double v = 0.7 + intensity * 0.3;  // Value increases with beat
    
    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;
    
    if (h < 60) {
        *r = c; *g = x; *b = 0;
    } else if (h < 120) {
        *r = x; *g = c; *b = 0;
    } else if (h < 180) {
        *r = 0; *g = c; *b = x;
    } else if (h < 240) {
        *r = 0; *g = x; *b = c;
    } else if (h < 300) {
        *r = x; *g = 0; *b = c;
    } else {
        *r = c; *g = 0; *b = x;
    }
    
    *r += m;
    *g += m;
    *b += m;
}

void pong_init(void *vis_ptr) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    PongGame *game = &vis->pong_game;
    
    int width = vis->width;
    int height = vis->height;
    
    game->width = width;
    game->height = height;
    game->last_width = width;
    game->last_height = height;
    game->game_time = 0;
    game->reset_timer = 0;
    game->ai_difficulty = 7;
    game->last_beat_time = 0;
    game->game_over = FALSE;
    game->winner = -1;
    game->game_over_display_time = 0;
    
    // Ball - unscaled speed for simplicity
    double base_size = PONG_BALL_SIZE * (height / 720.0);
    game->ball.x = width / 2.0;
    game->ball.y = height / 2.0;
    game->ball.vx = PONG_BALL_SPEED;
    game->ball.vy = PONG_BALL_SPEED * 0.5;
    game->ball.size = base_size;
    game->ball.base_size = base_size;
    game->ball.current_speed = PONG_BALL_SPEED;
    game->ball.hit_count = 0;
    game->ball.beat_glow = 0;
    game->ball.beat_color_hue = 0;
    
    // Player (left) - scale position with screen width
    double paddle_offset = width * 0.05;  // 5% from edge
    game->player.x = paddle_offset;
    game->player.y = (height - PONG_PADDLE_HEIGHT) / 2.0;
    game->player.width = PONG_PADDLE_WIDTH;
    game->player.height = PONG_PADDLE_HEIGHT * (height / 720.0);
    game->player.target_y = game->player.y;
    game->player.glow = 0;
    
    // AI (right) - scale position with screen width
    game->ai.x = width - paddle_offset - game->player.width;
    game->ai.y = (height - PONG_PADDLE_HEIGHT) / 2.0;
    game->ai.width = PONG_PADDLE_WIDTH;
    game->ai.height = PONG_PADDLE_HEIGHT * (height / 720.0);
    game->ai.target_y = game->ai.y;
    game->ai.glow = 0;
}

void pong_update(void *vis_ptr, double dt) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    PongGame *game = &vis->pong_game;
    
    if (dt > 0.05) dt = 0.05;  // Cap dt
    game->game_time += dt;
    
    // Handle screen resize
    pong_handle_screen_resize(game, vis);
    
    // If game is over, count down display time
    if (game->game_over) {
        game->game_over_display_time -= dt;
        if (game->game_over_display_time <= 0) {
            // Reset for new game
            pong_init(vis);
        }
        return;  // Don't update game while over
    }
    
    // Beat detection - update ball color and glow
    if (pong_detect_beat(vis)) {
        game->ball.beat_glow = 0.4;  // Reduced from 1.0 for less intense flashing
        game->last_beat_time = game->game_time;
        // Cycle hue on beat
        game->ball.beat_color_hue = fmod(game->ball.beat_color_hue + 60.0, 360.0);
    }
    
    // Decay beat glow slower for smoother effect
    game->ball.beat_glow *= 0.92;
    
    // Clamp mouse_y to valid range
    int player_target = vis->mouse_y;
    if (player_target < 0) player_target = 0;
    if (player_target > game->height) player_target = game->height;
    
    // Move player paddle toward mouse
    double player_center = game->player.y + game->player.height / 2.0;
    double player_diff = player_target - player_center;
    
    double move_speed = PONG_PADDLE_SPEED;
    double move_amount = move_speed * dt;
    
    // Move toward target, but don't overshoot
    if (fabs(player_diff) > 0.1) {
        if (player_diff > 0) {
            game->player.y += fmin(move_amount, player_diff);
        } else {
            game->player.y -= fmin(move_amount, -player_diff);
        }
    }
    
    // Clamp player paddle to screen
    if (game->player.y < 0) game->player.y = 0;
    if (game->player.y + game->player.height > game->height) {
        game->player.y = game->height - game->player.height;
    }
    
    // Ball physics
    game->ball.x += game->ball.vx * game->width/1024 * dt;
    game->ball.y += game->ball.vy * game->width/1024 * dt;
    
    // Ball bounce off top/bottom
    if (game->ball.y - game->ball.size < 0) {
        game->ball.y = game->ball.size;
        game->ball.vy = -game->ball.vy;
    }
    if (game->ball.y + game->ball.size > game->height) {
        game->ball.y = game->height - game->ball.size;
        game->ball.vy = -game->ball.vy;
    }
    
    // AI paddle logic - predict ball
    if (game->ball.vx > 0) {  // Ball moving toward AI
        double time_to_paddle = (game->ai.x - game->ball.x) / game->ball.vx;
        if (time_to_paddle > 0 && time_to_paddle < 5) {
            double predicted_y = game->ball.y + game->ball.vy * time_to_paddle;
            
            // Add some error based on difficulty
            double error = (10.0 - game->ai_difficulty) * 20;
            predicted_y += (rand() % (int)(error * 2)) - error;
            
            game->ai.target_y = predicted_y;
        }
    }
    
    // Move AI paddle toward target
    double ai_center = game->ai.y + game->ai.height / 2.0;
    double ai_diff = game->ai.target_y - ai_center;
    
    double ai_move_speed = PONG_PADDLE_SPEED * (game->ai_difficulty / 10.0);
    double ai_move_amount = ai_move_speed * dt;
    
    // Move toward target, but don't overshoot
    if (fabs(ai_diff) > 0.1) {
        if (ai_diff > 0) {
            game->ai.y += fmin(ai_move_amount, ai_diff);
        } else {
            game->ai.y -= fmin(ai_move_amount, -ai_diff);
        }
    }
    
    // Clamp AI paddle
    if (game->ai.y < 0) game->ai.y = 0;
    if (game->ai.y + game->ai.height > game->height) {
        game->ai.y = game->height - game->ai.height;
    }
    
    // Player paddle collision
    if (game->ball.vx < 0 &&
        game->ball.x - game->ball.size < game->player.x + game->player.width &&
        game->ball.y > game->player.y &&
        game->ball.y < game->player.y + game->player.height) {
        
        game->ball.vx = -game->ball.vx;
        game->ball.x = game->player.x + game->player.width + game->ball.size;
        
        // Calculate hit position on paddle (0.0 = top, 1.0 = bottom)
        double hit_pos = (game->ball.y - game->player.y) / game->player.height;
        hit_pos = (hit_pos < 0) ? 0 : (hit_pos > 1) ? 1 : hit_pos;
        
        // Convert to angle: -60 degrees at top, +60 degrees at bottom
        double angle = (hit_pos - 0.5) * 120.0 * M_PI / 180.0;  // Convert to radians
        double speed = sqrt(game->ball.vx * game->ball.vx + game->ball.vy * game->ball.vy);
        
        // Apply angle
        game->ball.vx = speed * cos(angle);
        game->ball.vy = speed * sin(angle);
        
        // Increase ball speed
        game->ball.hit_count++;
        game->ball.current_speed += PONG_BALL_SPEED_INCREMENT;
        if (game->ball.current_speed > PONG_MAX_BALL_SPEED) {
            game->ball.current_speed = PONG_MAX_BALL_SPEED;
        }
        
        // Normalize velocity and apply new speed
        double new_speed = sqrt(game->ball.vx * game->ball.vx + game->ball.vy * game->ball.vy);
        if (new_speed > 0) {
            game->ball.vx = (game->ball.vx / new_speed) * game->ball.current_speed;
            game->ball.vy = (game->ball.vy / new_speed) * game->ball.current_speed;
        }
        
        game->player.glow = 1.0;
    }
    
    // AI paddle collision
    if (game->ball.vx > 0 &&
        game->ball.x + game->ball.size > game->ai.x &&
        game->ball.y > game->ai.y &&
        game->ball.y < game->ai.y + game->ai.height) {
        
        game->ball.vx = -game->ball.vx;
        game->ball.x = game->ai.x - game->ball.size;
        
        // Calculate hit position on paddle (0.0 = top, 1.0 = bottom)
        double hit_pos = (game->ball.y - game->ai.y) / game->ai.height;
        hit_pos = (hit_pos < 0) ? 0 : (hit_pos > 1) ? 1 : hit_pos;
        
        // Convert to angle: -60 degrees at top, +60 degrees at bottom
        double angle = (hit_pos - 0.5) * 120.0 * M_PI / 180.0;  // Convert to radians
        double speed = sqrt(game->ball.vx * game->ball.vx + game->ball.vy * game->ball.vy);
        
        // Apply angle (reverse for AI paddle direction)
        game->ball.vx = -speed * cos(angle);
        game->ball.vy = speed * sin(angle);
        
        // Increase ball speed
        game->ball.hit_count++;
        game->ball.current_speed += PONG_BALL_SPEED_INCREMENT;
        if (game->ball.current_speed > PONG_MAX_BALL_SPEED) {
            game->ball.current_speed = PONG_MAX_BALL_SPEED;
        }
        
        // Normalize velocity and apply new speed
        double new_speed = sqrt(game->ball.vx * game->ball.vx + game->ball.vy * game->ball.vy);
        if (new_speed > 0) {
            game->ball.vx = (game->ball.vx / new_speed) * game->ball.current_speed;
            game->ball.vy = (game->ball.vy / new_speed) * game->ball.current_speed;
        }
        
        game->ai.glow = 1.0;
    }
    
    // Scoring
    if (game->ball.x < -game->ball.size) {
        game->ai.score++;
        // Check for win condition (first to 10)
        if (game->ai.score >= 10) {
            game->game_over = TRUE;
            game->winner = 1;  // AI wins
            game->game_over_display_time = 3.0;  // Show message for 3 seconds
            game->player.score=0;
            game->ai.score=0;            
        } else {
            // Reset ball for next round - AI scored, so AI serves
            game->ball.x = game->width / 2.0;
            game->ball.y = game->height / 2.0;
            game->ball.vx = -PONG_BALL_SPEED;  // AI serves toward player (leftward)
            game->ball.vy = PONG_BALL_SPEED * 0.5;
            game->ball.current_speed = PONG_BALL_SPEED;
            game->ball.hit_count = 0;
        }
    }
    if (game->ball.x > game->width + game->ball.size) {
        game->player.score++;
        // Check for win condition (first to 10)
        if (game->player.score >= 10) {
            game->game_over = TRUE;
            game->winner = 0;  // Player wins
            game->game_over_display_time = 3.0;  // Show message for 3 seconds
            game->player.score=0;
            game->ai.score=0;            
        } else {
            // Reset ball for next round - Player scored, so player serves
            game->ball.x = game->width / 2.0;
            game->ball.y = game->height / 2.0;
            game->ball.vx = PONG_BALL_SPEED;  // Player serves toward AI (rightward)
            game->ball.vy = PONG_BALL_SPEED * 0.5;
            game->ball.current_speed = PONG_BALL_SPEED;
            game->ball.hit_count = 0;
        }
    }
    
    // Update glow decay
    game->player.glow *= 0.9;
    game->ai.glow *= 0.9;
}

void pong_draw(void *vis_ptr, cairo_t *cr) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    PongGame *game = &vis->pong_game;
    // Background
    cairo_set_source_rgb(cr, 0.05, 0.08, 0.15);
    cairo_paint(cr);
    
    // Center line
    cairo_set_source_rgba(cr, 0.3, 0.4, 0.6, 0.5);
    cairo_set_line_width(cr, 1.5);
    
    double y = 0;
    while (y < game->height) {
        cairo_move_to(cr, game->width / 2.0, y);
        cairo_line_to(cr, game->width / 2.0, y + 10);
        cairo_stroke(cr);
        y += 20;
    }
    
    // Draw ball with beat-reactive colors and glow
    double glow = 0.2 + 0.15 * sin(game->game_time * 5);
    double beat_intensity = game->ball.beat_glow;
    
    // Get beat-reactive color
    double ball_r, ball_g, ball_b;
    pong_get_beat_color(game->ball.beat_color_hue, beat_intensity, &ball_r, &ball_g, &ball_b);
    
    // Ball glow rings with beat color
    cairo_set_source_rgba(cr, ball_r * 0.5, ball_g * 0.5, ball_b * 0.5, 0.1 * glow * (1.0 + beat_intensity * 0.5));
    cairo_arc(cr, game->ball.x, game->ball.y, game->ball.size * 2.5, 0, 2 * M_PI);
    cairo_fill(cr);
    
    cairo_set_source_rgba(cr, ball_r * 0.7, ball_g * 0.7, ball_b * 0.7, 0.15 * glow * (1.0 + beat_intensity * 0.5));
    cairo_arc(cr, game->ball.x, game->ball.y, game->ball.size * 1.8, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Ball core - interpolate between default and beat color (more subtle)
    double default_r = 0.4, default_g = 0.8, default_b = 1.0;
    double final_r = default_r * (1.0 - beat_intensity * 0.3) + ball_r * beat_intensity * 0.3;
    double final_g = default_g * (1.0 - beat_intensity * 0.3) + ball_g * beat_intensity * 0.3;
    double final_b = default_b * (1.0 - beat_intensity * 0.3) + ball_b * beat_intensity * 0.3;
    
    cairo_set_source_rgb(cr, final_r, final_g, final_b);
    cairo_arc(cr, game->ball.x, game->ball.y, game->ball.size, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Draw paddles
    // Player paddle (red/pink)
    cairo_set_source_rgba(cr, 1.0, 0.2, 0.3, 0.3 * game->player.glow);
    cairo_rectangle(cr, game->player.x - 10, game->player.y - 8,
                   game->player.width + 20, game->player.height + 16);
    cairo_fill(cr);
    
    cairo_set_source_rgb(cr, 1.0, 0.3, 0.4);
    cairo_rectangle(cr, game->player.x, game->player.y,
                   game->player.width, game->player.height);
    cairo_fill(cr);
    
    // AI paddle (blue)
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.3 * game->ai.glow);
    cairo_rectangle(cr, game->ai.x - 10, game->ai.y - 8,
                   game->ai.width + 20, game->ai.height + 16);
    cairo_fill(cr);
    
    cairo_set_source_rgb(cr, 0.2, 0.7, 1.0);
    cairo_rectangle(cr, game->ai.x, game->ai.y,
                   game->ai.width, game->ai.height);
    cairo_fill(cr);
    
    // Draw scores
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 72);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    
    char score_str[20];
    sprintf(score_str, "%d", game->player.score);
    cairo_move_to(cr, game->width / 4 - 30, 100);
    cairo_show_text(cr, score_str);
    
    sprintf(score_str, "%d", game->ai.score);
    cairo_move_to(cr, 3 * game->width / 4 - 30, 100);
    cairo_show_text(cr, score_str);
    
    // Draw game over message if applicable
    if (game->game_over) {
        double fade = game->game_over_display_time / 3.0;  // Fade out as time decreases
        if (fade < 0) fade = 0;
        if (fade > 1) fade = 1;
        
        cairo_set_source_rgba(cr, 0, 0, 0, 0.7 * fade);
        cairo_rectangle(cr, 0, 0, game->width, game->height);
        cairo_fill(cr);
        
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 96);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, fade);
        
        const char *winner_text = (game->winner == 0) ? "YOU WIN!" : "COMPUTER WINS!";
        cairo_text_extents_t extents;
        cairo_text_extents(cr, winner_text, &extents);
        double text_x = (game->width - extents.width) / 2.0;
        double text_y = (game->height - extents.height) / 2.0;
        
        cairo_move_to(cr, text_x, text_y);
        cairo_show_text(cr, winner_text);
    }
}
