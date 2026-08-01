#include "visualization.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

// Game state
#define MAX_PENGUINS 10
#define MAX_BOUNCES 4
#define PENGUIN_SIZE 8

typedef struct {
    double x, y;
    double vx, vy;
    bool active;
    int bounces_remaining;
    double scale; // Gets smaller with each bounce
} FallingPenguin;

typedef struct {
    double x, y;
    double vx, vy;
    bool active;
} Spark;

#define MAX_SPARKS 100
static Spark sparks[MAX_SPARKS];

static FallingPenguin penguins[MAX_PENGUINS];
static double stretcher_x = 0;
static double stretcher_y = 0;
static double stretcher_width = 80;
static double stretcher_height = 8;
static double building_x = 0;
static double building_height = 0;
static int score = 0;
static int lives = 3;
static int level = 1;
static double game_time = 0;
static double level_timer = 0;
static bool game_over = false;
static double game_over_time = 0;
static int penguins_caught = 0;
static int penguins_missed = 0;
static double spawn_timer = 0;
static double spawn_rate = 2.0;
static double base_spawn_rate = 2.0;
static double multi_spawn_timer = 0;
static bool fire_animation = false;
static double fire_time = 0;
static int penguins_in_ambulance = 0;

void create_spark(double x, double y, double vx, double vy) {
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (!sparks[i].active) {
            sparks[i].x = x;
            sparks[i].y = y;
            sparks[i].vx = vx;
            sparks[i].vy = vy;
            sparks[i].active = true;
            break;
        }
    }
}

void init_penguins_game(Visualizer *vis) {
    stretcher_x = vis->width / 2;
    stretcher_y = vis->height - 80;
    building_x = 50;
    building_height = vis->height * 0.6;
    
    score = 0;
    lives = 3;
    level = 1;
    game_time = 0;
    level_timer = 0;
    penguins_caught = 0;
    penguins_missed = 0;
    spawn_timer = 0;
    base_spawn_rate = 2.0;
    spawn_rate = base_spawn_rate;
    multi_spawn_timer = 0;
    game_over = false;
    game_over_time = 0;
    penguins_in_ambulance = 0;
    
    for (int i = 0; i < MAX_PENGUINS; i++) {
        penguins[i].active = false;
    }
    
    for (int i = 0; i < MAX_SPARKS; i++) {
        sparks[i].active = false;
    }
}

void update_bouncing_penguins(Visualizer *vis, double dt) {
    if (!game_over) {
        game_time += dt;
        level_timer += dt;
        fire_time += dt;
        
        // Update difficulty every 30 seconds
        if (level_timer > 30.0) {
            level++;
            level_timer = 0;
            base_spawn_rate = 2.0 - (level - 1) * 0.25;
            if (base_spawn_rate < 0.3) base_spawn_rate = 0.3;
            spawn_rate = base_spawn_rate;
        }
        
        // Update stretcher position - follow mouse X
        stretcher_x = vis->mouse_x;
        if (stretcher_x < stretcher_width/2) stretcher_x = stretcher_width/2;
        if (stretcher_x > vis->width - stretcher_width/2) stretcher_x = vis->width - stretcher_width/2;
        
        // Spawn penguins from building
        spawn_timer -= dt;
        
        if (spawn_timer <= 0) {
            // Determine how many penguins to spawn based on level
            int num_penguins = 1;
            if (level > 5) num_penguins = 2;
            if (level > 10) num_penguins = 3;
            
            for (int n = 0; n < num_penguins; n++) {
                for (int i = 0; i < MAX_PENGUINS; i++) {
                    if (!penguins[i].active) {
                        penguins[i].x = building_x + 20;
                        penguins[i].y = building_height - 30;
                        // Initial velocity - tossed from building
                        penguins[i].vx = 150 + (rand() % 50);
                        penguins[i].vy = -200 - (level * 20); // Gets faster with level
                        penguins[i].active = true;
                        penguins[i].bounces_remaining = MAX_BOUNCES;
                        penguins[i].scale = 1.0;
                        fire_animation = true;
                        fire_time = 0;
                        break;
                    }
                }
            }
            
            spawn_timer = spawn_rate;
        }
        
        // Update penguins
        for (int i = 0; i < MAX_PENGUINS; i++) {
            if (penguins[i].active) {
                // Apply gravity
                penguins[i].vy += 400 * dt;
                penguins[i].y += penguins[i].vy * dt;
                penguins[i].x += penguins[i].vx * dt;
                
                // Wall bouncing
                if (penguins[i].x < PENGUIN_SIZE * penguins[i].scale) {
                    penguins[i].x = PENGUIN_SIZE * penguins[i].scale;
                    penguins[i].vx = fabs(penguins[i].vx);
                }
                if (penguins[i].x > vis->width - PENGUIN_SIZE * penguins[i].scale) {
                    penguins[i].x = vis->width - PENGUIN_SIZE * penguins[i].scale;
                    penguins[i].vx = -fabs(penguins[i].vx);
                }
                
                // Check if caught by stretcher
                double dx = penguins[i].x - stretcher_x;
                double dy = penguins[i].y - stretcher_y;
                double dist = sqrt(dx*dx + dy*dy);
                double catch_radius = (stretcher_width/2 + PENGUIN_SIZE * penguins[i].scale);
                
                if (dist < catch_radius && penguins[i].y >= stretcher_y - 20 && penguins[i].vy > 0) {
                    // Bounce upward!
                    penguins[i].vy = -400 - (level * 30);
                    penguins[i].vx *= 0.8; // Slight air resistance
                    penguins[i].bounces_remaining--;
                    penguins[i].scale = 1.0 - ((MAX_BOUNCES - penguins[i].bounces_remaining) * 0.2);
                    
                    penguins_caught++;
                    
                    // Sparks effect
                    for (int s = 0; s < 8; s++) {
                        double angle = (s / 8.0) * 6.28;
                        double speed = 200;
                        create_spark(penguins[i].x, penguins[i].y, 
                                   cos(angle) * speed, sin(angle) * speed);
                    }
                    
                    // Check if bounces exhausted
                    if (penguins[i].bounces_remaining <= 0) {
                        penguins[i].active = false;
                    }
                }
                
                // Check if reached ambulance (right side)
                if (penguins[i].x > vis->width - 100 && penguins[i].y > vis->height - 100) {
                    penguins[i].active = false;
                    score += (100 + (MAX_BOUNCES - penguins[i].bounces_remaining) * 50 + level * 10);
                    penguins_in_ambulance++;
                }
                
                // Check if missed (hit ground without being in ambulance)
                if (penguins[i].y > vis->height) {
                    penguins[i].active = false;
                    lives--;
                    penguins_missed++;
                    
                    if (lives <= 0) {
                        game_over = true;
                        game_over_time = 0;
                    }
                }
            }
        }
        
        // Update sparks
        for (int i = 0; i < MAX_SPARKS; i++) {
            if (sparks[i].active) {
                sparks[i].vy += 300 * dt;
                sparks[i].y += sparks[i].vy * dt;
                sparks[i].x += sparks[i].vx * dt;
                
                if (sparks[i].y > vis->height) {
                    sparks[i].active = false;
                }
            }
        }
    } else {
        game_over_time += dt;
        if (game_over_time > 3.0 && vis->mouse_left_pressed) {
            // Restart game
            init_penguins_game(vis);
            vis->mouse_left_pressed = FALSE;
        }
    }
}

void draw_bouncing_penguins(Visualizer *vis, cairo_t *cr) {
    // Draw sky
    cairo_set_source_rgb(cr, 0.3, 0.5, 0.8);
    cairo_rectangle(cr, 0, 0, vis->width, vis->height);
    cairo_fill(cr);
    
    // Draw ground
    cairo_set_source_rgb(cr, 0.2, 0.6, 0.2);
    cairo_rectangle(cr, 0, vis->height - 60, vis->width, 60);
    cairo_fill(cr);
    
    // Draw building on left side (on fire!)
    double building_x_pos = 30;
    cairo_set_source_rgb(cr, 0.4, 0.3, 0.2);
    cairo_rectangle(cr, building_x_pos - 40, 0, 80, building_height);
    cairo_fill(cr);
    
    // Windows
    cairo_set_source_rgb(cr, 1, 1, 0.3);
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 2; col++) {
            double wx = building_x_pos - 30 + col * 30;
            double wy = 30 + row * 25;
            if (wy < building_height - 20) {
                cairo_rectangle(cr, wx, wy, 16, 16);
                cairo_fill(cr);
            }
        }
    }
    
    // Fire at top of building
    double fire_bob = sin(fire_time * 8) * 4;
    cairo_set_source_rgba(cr, 1, 0.4, 0, 0.7);
    cairo_arc(cr, building_x_pos, building_height - 30 + fire_bob, 25, 0, 6.28);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1, 0.8, 0, 0.5);
    cairo_arc(cr, building_x_pos, building_height - 35 + fire_bob, 15, 0, 6.28);
    cairo_fill(cr);
    
    // Draw ambulance on right side
    double ambulance_x = vis->width - 70;
    double ambulance_y = vis->height - 50;
    cairo_set_source_rgb(cr, 1, 0.2, 0.2);
    cairo_rectangle(cr, ambulance_x, ambulance_y, 100, 40);
    cairo_fill(cr);
    
    // Ambulance cross
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 3);
    cairo_move_to(cr, ambulance_x + 50, ambulance_y + 15);
    cairo_line_to(cr, ambulance_x + 50, ambulance_y + 25);
    cairo_stroke(cr);
    cairo_move_to(cr, ambulance_x + 45, ambulance_y + 20);
    cairo_line_to(cr, ambulance_x + 55, ambulance_y + 20);
    cairo_stroke(cr);
    
    // Wheels
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_arc(cr, ambulance_x + 20, ambulance_y + 42, 6, 0, 6.28);
    cairo_fill(cr);
    cairo_arc(cr, ambulance_x + 80, ambulance_y + 42, 6, 0, 6.28);
    cairo_fill(cr);
    
    // Draw stretcher (player controlled)
    cairo_set_source_rgb(cr, 0.8, 0.4, 0.2);
    cairo_rectangle(cr, stretcher_x - stretcher_width/2, stretcher_y, stretcher_width, stretcher_height);
    cairo_fill(cr);
    
    // Stretcher handles
    cairo_set_source_rgb(cr, 0.6, 0.3, 0.1);
    cairo_set_line_width(cr, 4);
    cairo_move_to(cr, stretcher_x - stretcher_width/2 - 8, stretcher_y - 8);
    cairo_line_to(cr, stretcher_x - stretcher_width/2, stretcher_y);
    cairo_stroke(cr);
    cairo_move_to(cr, stretcher_x + stretcher_width/2 + 8, stretcher_y - 8);
    cairo_line_to(cr, stretcher_x + stretcher_width/2, stretcher_y);
    cairo_stroke(cr);
    
    // Draw sparks
    for (int i = 0; i < MAX_SPARKS; i++) {
        if (sparks[i].active) {
            cairo_set_source_rgba(cr, 1, 0.6, 0, 0.6);
            cairo_arc(cr, sparks[i].x, sparks[i].y, 2, 0, 6.28);
            cairo_fill(cr);
        }
    }
    
    // Draw falling penguins
    for (int i = 0; i < MAX_PENGUINS; i++) {
        if (penguins[i].active) {
            double size = PENGUIN_SIZE * penguins[i].scale;
            
            cairo_save(cr);
            cairo_translate(cr, penguins[i].x, penguins[i].y);
            
            // Rotation based on velocity
            double angle = atan2(penguins[i].vy, penguins[i].vx);
            cairo_rotate(cr, angle);
            
            // Body
            cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
            cairo_arc(cr, 0, 0, size, 0, 6.28);
            cairo_fill(cr);
            
            // White belly
            cairo_set_source_rgb(cr, 0.9, 0.9, 1);
            cairo_arc(cr, 0, size*0.3, size*0.6, 0, 6.28);
            cairo_fill(cr);
            
            // Head
            cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
            cairo_arc(cr, size*1.2, -size*0.3, size*0.7, 0, 6.28);
            cairo_fill(cr);
            
            // Eye
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_arc(cr, size*1.5, -size*0.4, size*0.3, 0, 6.28);
            cairo_fill(cr);
            
            cairo_restore(cr);
        }
    }
    
    // Draw HUD
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 20);
    
    char score_text[64];
    snprintf(score_text, sizeof(score_text), "SCORE: %d", score);
    cairo_move_to(cr, 20, 30);
    cairo_show_text(cr, score_text);
    
    char level_text[64];
    snprintf(level_text, sizeof(level_text), "LEVEL: %d", level);
    cairo_move_to(cr, vis->width / 2 - 50, 30);
    cairo_show_text(cr, level_text);
    
    char lives_text[64];
    snprintf(lives_text, sizeof(lives_text), "LIVES: %d", lives);
    cairo_move_to(cr, vis->width - 200, 30);
    cairo_show_text(cr, lives_text);
    
    // Draw game over screen
    if (game_over) {
        cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
        cairo_rectangle(cr, 0, 0, vis->width, vis->height);
        cairo_fill(cr);
        
        cairo_set_source_rgb(cr, 1, 0.8, 0.2);
        cairo_set_font_size(cr, 48);
        cairo_move_to(cr, vis->width/2 - 150, vis->height/2 - 50);
        cairo_show_text(cr, "GAME OVER");
        
        cairo_set_font_size(cr, 24);
        char final_score[64];
        snprintf(final_score, sizeof(final_score), "Final Score: %d", score);
        cairo_move_to(cr, vis->width/2 - 100, vis->height/2 + 20);
        cairo_show_text(cr, final_score);
        
        char stats[128];
        snprintf(stats, sizeof(stats), "Saved: %d  Missed: %d", penguins_caught, penguins_missed);
        cairo_move_to(cr, vis->width/2 - 120, vis->height/2 + 60);
        cairo_show_text(cr, stats);
        
        cairo_set_font_size(cr, 20);
        cairo_move_to(cr, vis->width/2 - 120, vis->height/2 + 120);
        cairo_show_text(cr, "Click to restart");
    }
}
