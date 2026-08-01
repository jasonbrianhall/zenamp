#include "visualization.h"
#include <math.h>

// Static variables for animation state
static double time_offset = 0;
static double penguin1_x = 50;
static double penguin2_x = 50;
static double penguin2_sleep_timer = 0;
static bool penguin2_sleeping = false;
static double penguin2_boost_timer = 0;
static bool penguin2_boosting = false;
static double penguin1_sleep_timer = 0;
static bool penguin1_sleeping = false;
static double penguin1_boost_timer = 0;
static bool penguin1_boosting = false;
static double finish_line_x = 0;
static bool race_finished = false;
static int winner = 0; // 0=none, 1=penguin1, 2=penguin2
static double celebration_time = 0;
static double cloud_x[8] = {0};
static double cloud_y[8] = {0};
static bool clouds_initialized = false;

// Food system
#define MAX_FOOD_ITEMS 50
typedef struct {
    double x, y;
    int type; // 0=fish, 1=krill
    bool active;
    double life; // How long before it disappears
    double vy; // Vertical velocity for falling
} FoodItem;

static FoodItem food_items[MAX_FOOD_ITEMS];
static int food_count = 0;

void update_penguins(Visualizer *vis, double dt) {
    const double min_dt = 1.0 / 120.0;
    double speed_factor = dt / 0.033;
    
    if (dt < min_dt) {
        dt = min_dt;
        speed_factor = dt / 0.033;
    }
    
    // Handle mouse clicks to drop food
    if (vis->mouse_left_pressed) {
        // Left click: drop fish
        if (food_count < MAX_FOOD_ITEMS) {
            for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
                if (!food_items[i].active) {
                    food_items[i].x = vis->mouse_x;
                    food_items[i].y = vis->mouse_y;
                    food_items[i].type = 0; // Fish
                    food_items[i].active = true;
                    food_items[i].life = 8.0;
                    food_items[i].vy = 0.0;
                    food_count++;
                    break;
                }
            }
        }
        vis->mouse_left_pressed = FALSE;
    }
    
    if (vis->mouse_middle_pressed) {
        // Middle click: drop random food
        if (food_count < MAX_FOOD_ITEMS) {
            for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
                if (!food_items[i].active) {
                    food_items[i].x = vis->mouse_x;
                    food_items[i].y = vis->mouse_y;
                    food_items[i].type = rand() % 2;
                    food_items[i].active = true;
                    food_items[i].life = 8.0;
                    food_items[i].vy = 0.0;
                    food_count++;
                    break;
                }
            }
        }
        vis->mouse_middle_pressed = FALSE;
    }
    
    if (vis->mouse_right_pressed) {
        // Right click: drop krill
        if (food_count < MAX_FOOD_ITEMS) {
            for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
                if (!food_items[i].active) {
                    food_items[i].x = vis->mouse_x;
                    food_items[i].y = vis->mouse_y;
                    food_items[i].type = 1; // Krill
                    food_items[i].active = true;
                    food_items[i].life = 8.0;
                    food_items[i].vy = 0.0;
                    food_count++;
                    break;
                }
            }
        }
        vis->mouse_right_pressed = FALSE;
    }
    
    time_offset += 0.05 * speed_factor;
    
    // Initialize clouds
    if (!clouds_initialized) {
        for (int i = 0; i < 8; i++) {
            cloud_x[i] = (rand() % vis->width);
            cloud_y[i] = 30 + (rand() % 80);
        }
        clouds_initialized = true;
    }
    
    // Update clouds
    for (int i = 0; i < 8; i++) {
        cloud_x[i] += 0.3 * speed_factor;
        if (cloud_x[i] > vis->width + 80) {
            cloud_x[i] = -80;
            cloud_y[i] = 30 + (rand() % 80);
        }
    }
    
    // Set finish line
    finish_line_x = vis->width - 100;
    
    // Calculate speed multiplier based on screen width
    double screen_speed_factor = vis->width / 2000.0;
    if (screen_speed_factor < 0.2) screen_speed_factor = 0.2; // Minimum speed
    if (screen_speed_factor > 0.8) screen_speed_factor = 0.8; // Maximum speed
    
    if (!race_finished) {
        // Calculate average energy from frequency bands
        double avg_energy = 0;
        for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
            avg_energy += vis->frequency_bands[i];
        }
        avg_energy /= VIS_FREQUENCY_BARS;
        
        // Penguin 1 movement with sleep and boost state (steady one)
        if (penguin1_sleeping) {
            penguin1_sleep_timer -= dt;
            if (penguin1_sleep_timer <= 0) {
                penguin1_sleeping = false;
            }
            // Penguin doesn't move while sleeping
        } else {
            // Calculate boost multiplier
            double boost_multiplier = 1.0;
            if (penguin1_boosting) {
                penguin1_boost_timer -= dt;
                boost_multiplier = 2.0; // Double speed while boosted
                if (penguin1_boost_timer <= 0) {
                    penguin1_boosting = false;
                }
            }
            // Penguin 1 moves steadily
            penguin1_x += (1.8 + avg_energy * 0.6) * speed_factor * screen_speed_factor * boost_multiplier;
        }
        
        // Penguin 2 behavior based on music energy (lazy one)
        if (penguin2_sleeping) {
            penguin2_sleep_timer -= dt;
            
            // Very small chance of waking if penguin1 passes
            double lead = penguin2_x - penguin1_x;
            double wake_chance = 0;
            
            if (lead < -30) {
                wake_chance = 0.03; // 3% chance if penguin1 is way ahead
            } else if (lead < -10) {
                wake_chance = 0.02; // 2% chance if penguin1 is ahead
            } else if (lead < 0) {
                wake_chance = 0.01; // 1% chance if penguin1 just passed
            }
            
            bool noise_wake = (rand() % 1000) < (wake_chance * 1000);
            
            if (penguin2_sleep_timer <= 0 || noise_wake) {
                penguin2_sleeping = false;
            }
        } else {
            // Calculate boost multiplier for penguin 2
            double boost_multiplier = 1.0;
            if (penguin2_boosting) {
                penguin2_boost_timer -= dt;
                boost_multiplier = 1.5; // 50% speed boost
                if (penguin2_boost_timer <= 0) {
                    penguin2_boosting = false;
                }
            }
            
            // Penguin 2 runs fast when music is energetic
            if (avg_energy > 0.35) {
                penguin2_x += (6.0 + avg_energy * 3.0) * speed_factor * screen_speed_factor * boost_multiplier;
            } else {
                penguin2_x += 2.5 * speed_factor * screen_speed_factor * boost_multiplier;
            }
            
            // Calculate how far ahead penguin 2 is
            double lead = penguin2_x - penguin1_x;
            
            // Gets more confident and lazy the further ahead
            double confidence_factor = 1.0;
            double sleep_time_bonus = 0;
            
            if (lead > 150) {
                confidence_factor = 10.0;
                sleep_time_bonus = 6.0;
            } else if (lead > 100) {
                confidence_factor = 7.0;
                sleep_time_bonus = 5.0;
            } else if (lead > 60) {
                confidence_factor = 5.0;
                sleep_time_bonus = 4.0;
            } else if (lead > 30) {
                confidence_factor = 3.0;
                sleep_time_bonus = 3.0;
            } else if (lead > 15) {
                confidence_factor = 1.5;
                sleep_time_bonus = 1.5;
            } else if (lead > 5) {
                confidence_factor = 0.5;
                sleep_time_bonus = 0.5;
            }
            
            // Random sleep based on confidence
            if ((rand() % 10000) < (200 * confidence_factor)) {
                penguin2_sleeping = true;
                penguin2_sleep_timer = 2.0 + sleep_time_bonus;
            }
        }
        
        // Check if either penguin finished
        if (penguin1_x >= finish_line_x && winner == 0) {
            race_finished = true;
            winner = 1;
            celebration_time = 3.0;
        }
        
        if (penguin2_x >= finish_line_x && winner == 0) {
            race_finished = true;
            winner = 2;
            celebration_time = 3.0;
        }
    } else {
        celebration_time -= dt;
        if (celebration_time <= 0) {
            // Reset race
            penguin1_x = 50;
            penguin2_x = 50;
            race_finished = false;
            winner = 0;
            penguin1_sleeping = false;
            penguin2_sleeping = false;
            penguin1_boosting = false;
            penguin2_boosting = false;
        }
    }
    
    // Update food items
    for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
        if (food_items[i].active) {
            food_items[i].life -= dt;
            if (food_items[i].life <= 0) {
                food_items[i].active = false;
                food_count--;
            } else {
                // Gravity for falling food
                food_items[i].vy += 0.3; // gravity
                food_items[i].y += food_items[i].vy;
                
                // Floor collision
                if (food_items[i].y > vis->height - 30) {
                    food_items[i].y = vis->height - 30;
                    food_items[i].vy = 0;
                }
            }
        }
    }
}

void draw_penguins(Visualizer *vis, cairo_t *cr) {
    double ground_y = vis->height - 50;
    
    // Draw sky gradient
    for (int y = 0; y < ground_y; y++) {
        double t = (double)y / ground_y;
        cairo_set_source_rgb(cr, 0.5 + t * 0.2, 0.7 + t * 0.1, 0.9 - t * 0.2);
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, vis->width, y);
        cairo_stroke(cr);
    }
    
    // Draw clouds
    cairo_set_source_rgba(cr, 1, 1, 1, 0.7);
    for (int i = 0; i < 8; i++) {
        cairo_arc(cr, cloud_x[i], cloud_y[i], 20, 0, 6.28);
        cairo_fill(cr);
        cairo_arc(cr, cloud_x[i] + 15, cloud_y[i] - 5, 25, 0, 6.28);
        cairo_fill(cr);
        cairo_arc(cr, cloud_x[i] + 30, cloud_y[i], 20, 0, 6.28);
        cairo_fill(cr);
    }
    
    // Draw ground
    cairo_set_source_rgb(cr, 1, 1, 1); // Snow
    cairo_rectangle(cr, 0, ground_y, vis->width, vis->height - ground_y);
    cairo_fill(cr);
    
    cairo_set_source_rgb(cr, 0.8, 0.9, 1); // Ice shadow
    for (int i = 0; i < vis->width; i += 30) {
        cairo_rectangle(cr, i, ground_y, 15, 30);
        cairo_fill(cr);
    }
    
    // Draw finish line
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_set_line_width(cr, 4);
    cairo_move_to(cr, finish_line_x, 0);
    cairo_line_to(cr, finish_line_x, vis->height);
    cairo_stroke(cr);
    
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_font_size(cr, 16);
    cairo_move_to(cr, finish_line_x + 10, 30);
    cairo_show_text(cr, "FINISH");
    
    // Draw food items
    for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
        if (food_items[i].active) {
            cairo_save(cr);
            cairo_translate(cr, food_items[i].x, food_items[i].y);
            
            if (food_items[i].type == 0) {
                // Fish
                cairo_set_source_rgb(cr, 1, 0.6, 0.2);
                cairo_arc(cr, 0, 0, 4, 0, 6.28);
                cairo_fill(cr);
                cairo_set_source_rgb(cr, 1, 0.8, 0.4);
                cairo_arc(cr, -3, -1, 2, 0, 6.28);
                cairo_fill(cr);
            } else {
                // Krill
                cairo_set_source_rgb(cr, 1, 0.2, 0.6);
                cairo_arc(cr, 0, 0, 3, 0, 6.28);
                cairo_fill(cr);
            }
            
            cairo_restore(cr);
        }
    }
    
    // Draw Penguin 1 (steady one)
    double penguin1_y = ground_y - 70;
    cairo_save(cr);
    cairo_translate(cr, penguin1_x, penguin1_y);
    
    if (penguin1_sleeping) {
        // Sleeping penguin - on its side
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        
        // Body
        cairo_arc(cr, 0, 5, 18, 0, 6.28);
        cairo_fill(cr);
        
        // Head
        cairo_arc(cr, 15, 0, 12, 0, 6.28);
        cairo_fill(cr);
        
        // White belly
        cairo_set_source_rgb(cr, 0.9, 0.9, 1);
        cairo_arc(cr, 0, 8, 10, 0, 6.28);
        cairo_fill(cr);
        
        // Eye
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, 2);
        cairo_move_to(cr, 18, -2);
        cairo_line_to(cr, 22, -2);
        cairo_stroke(cr);
        
        // ZZZ
        cairo_set_source_rgba(cr, 0.3, 0.3, 0.5, 0.7);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 16);
        double z_bob = sin(time_offset * 2) * 4;
        cairo_move_to(cr, 20, -10 + z_bob);
        cairo_show_text(cr, "Z");
        cairo_move_to(cr, 28, -18 + z_bob * 0.7);
        cairo_show_text(cr, "Z");
        cairo_move_to(cr, 36, -26 + z_bob * 0.5);
        cairo_show_text(cr, "Z");
    } else if (penguin1_boosting) {
        // Boosted penguin - speed lines and glow
        
        // Body
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        cairo_arc(cr, 0, 0, 15, 0, 6.28);
        cairo_fill(cr);
        
        // White belly
        cairo_set_source_rgb(cr, 0.9, 0.9, 1);
        cairo_arc(cr, 0, 3, 8, 0, 6.28);
        cairo_fill(cr);
        
        // Head (fast movement)
        double head_bob = sin(time_offset * 6) * 3;
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        cairo_arc(cr, 12 + head_bob, -12, 10, 0, 6.28);
        cairo_fill(cr);
        
        // Eye
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_arc(cr, 15 + head_bob, -14, 2, 0, 6.28);
        cairo_fill(cr);
        
        // Fast flippers
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        cairo_set_line_width(cr, 4);
        double flipper_swing = sin(time_offset * 8) * 4;
        cairo_move_to(cr, -8, 8);
        cairo_line_to(cr, -14, 14 + flipper_swing);
        cairo_stroke(cr);
        cairo_move_to(cr, 8, 8);
        cairo_line_to(cr, 14, 14 - flipper_swing);
        cairo_stroke(cr);
        
        // Boost glow
        cairo_set_source_rgba(cr, 0.3, 0.7, 1.0, 0.3);
        cairo_arc(cr, 0, 0, 22, 0, 6.28);
        cairo_fill(cr);
    } else {
        // Normal penguin
        // Body
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        cairo_arc(cr, 0, 0, 15, 0, 6.28);
        cairo_fill(cr);
        
        // White belly
        cairo_set_source_rgb(cr, 0.9, 0.9, 1);
        cairo_arc(cr, 0, 3, 8, 0, 6.28);
        cairo_fill(cr);
        
        // Head
        double head_bob = sin(time_offset * 3) * 2;
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        cairo_arc(cr, 12 + head_bob, -12, 10, 0, 6.28);
        cairo_fill(cr);
        
        // Eye
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_arc(cr, 15 + head_bob, -14, 2, 0, 6.28);
        cairo_fill(cr);
        
        // Flippers (waddling)
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        cairo_set_line_width(cr, 4);
        double flipper_swing = sin(time_offset * 4) * 3;
        cairo_move_to(cr, -8, 8);
        cairo_line_to(cr, -14, 14 + flipper_swing);
        cairo_stroke(cr);
        cairo_move_to(cr, 8, 8);
        cairo_line_to(cr, 14, 14 - flipper_swing);
        cairo_stroke(cr);
    }
    
    cairo_restore(cr);
    
    // Draw Penguin 2 (lazy one)
    double penguin2_y = ground_y - 70;
    cairo_save(cr);
    cairo_translate(cr, penguin2_x, penguin2_y);
    
    if (penguin2_sleeping) {
        // Sleeping penguin
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        
        // Body (curled)
        cairo_arc(cr, 0, 5, 18, 0, 6.28);
        cairo_fill(cr);
        
        // Head
        cairo_arc(cr, 15, 0, 12, 0, 6.28);
        cairo_fill(cr);
        
        // White belly
        cairo_set_source_rgb(cr, 0.9, 0.9, 1);
        cairo_arc(cr, 5, 8, 8, 0, 6.28);
        cairo_fill(cr);
        
        // Closed eyes
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, 2);
        cairo_move_to(cr, 18, -2);
        cairo_line_to(cr, 22, -2);
        cairo_stroke(cr);
        
        // ZZZ
        cairo_set_source_rgba(cr, 0.3, 0.3, 0.5, 0.7);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 20);
        double z_bob = sin(time_offset * 2) * 5;
        cairo_move_to(cr, 30, -15 + z_bob);
        cairo_show_text(cr, "Z");
        cairo_move_to(cr, 40, -25 + z_bob * 0.7);
        cairo_show_text(cr, "Z");
        cairo_move_to(cr, 50, -35 + z_bob * 0.5);
        cairo_show_text(cr, "Z");
    } else if (penguin2_boosting) {
        // Boosted penguin - speed lines
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        
        // Body (stretched)
        cairo_save(cr);
        cairo_scale(cr, 1.4, 0.8);
        cairo_arc(cr, 0, 0, 14, 0, 6.28);
        cairo_fill(cr);
        cairo_restore(cr);
        
        // White belly
        cairo_set_source_rgb(cr, 0.9, 0.9, 1);
        cairo_arc(cr, 0, 3, 7, 0, 6.28);
        cairo_fill(cr);
        
        // Head
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        cairo_arc(cr, 14, -12, 9, 0, 6.28);
        cairo_fill(cr);
        
        // Eye
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_arc(cr, 17, -14, 2, 0, 6.28);
        cairo_fill(cr);
        
        // Fast flippers
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        cairo_set_line_width(cr, 3);
        double run_cycle = sin(time_offset * 12) * 8;
        cairo_move_to(cr, -10, 8);
        cairo_line_to(cr, -15, 16 + run_cycle);
        cairo_stroke(cr);
        cairo_move_to(cr, 5, 8);
        cairo_line_to(cr, 8, 16 - run_cycle);
        cairo_stroke(cr);
        
        // Boost glow
        cairo_set_source_rgba(cr, 0.3, 0.7, 1.0, 0.3);
        cairo_arc(cr, 0, 0, 24, 0, 6.28);
        cairo_fill(cr);
        
        // Speed lines
        cairo_set_source_rgba(cr, 0.5, 0.7, 0.9, 0.4);
        cairo_set_line_width(cr, 2);
        for (int i = 0; i < 3; i++) {
            cairo_move_to(cr, -25 - i * 8, -5 + i * 3);
            cairo_line_to(cr, -35 - i * 8, -5 + i * 3);
            cairo_stroke(cr);
        }
    } else {
        // Running penguin
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        
        // Body (stretched)
        cairo_save(cr);
        cairo_scale(cr, 1.3, 0.8);
        cairo_arc(cr, 0, 0, 14, 0, 6.28);
        cairo_fill(cr);
        cairo_restore(cr);
        
        // White belly
        cairo_set_source_rgb(cr, 0.9, 0.9, 1);
        cairo_arc(cr, 0, 3, 7, 0, 6.28);
        cairo_fill(cr);
        
        // Head
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        cairo_arc(cr, 14, -12, 9, 0, 6.28);
        cairo_fill(cr);
        
        // Eye
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_arc(cr, 17, -14, 2, 0, 6.28);
        cairo_fill(cr);
        
        // Running flippers
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.2);
        cairo_set_line_width(cr, 3);
        double run_cycle = sin(time_offset * 8) * 7;
        cairo_move_to(cr, -10, 8);
        cairo_line_to(cr, -15, 16 + run_cycle);
        cairo_stroke(cr);
        cairo_move_to(cr, 5, 8);
        cairo_line_to(cr, 8, 16 - run_cycle);
        cairo_stroke(cr);
        
        // Speed lines
        cairo_set_source_rgba(cr, 0.5, 0.7, 0.9, 0.4);
        cairo_set_line_width(cr, 2);
        for (int i = 0; i < 3; i++) {
            cairo_move_to(cr, -25 - i * 8, -5 + i * 3);
            cairo_line_to(cr, -35 - i * 8, -5 + i * 3);
            cairo_stroke(cr);
        }
    }
    
    cairo_restore(cr);
    
    // Draw winner announcement
    if (race_finished && celebration_time > 0) {
        cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
        cairo_rectangle(cr, vis->width/2 - 150, vis->height/2 - 50, 300, 100);
        cairo_fill(cr);
        
        cairo_set_source_rgb(cr, 1, 1, 0.2);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 36);
        
        if (winner == 1) {
            cairo_move_to(cr, vis->width/2 - 140, vis->height/2);
            cairo_show_text(cr, "PENGUIN 1 WINS!");
        } else {
            cairo_move_to(cr, vis->width/2 - 140, vis->height/2);
            cairo_show_text(cr, "PENGUIN 2 WINS!");
        }
        
        cairo_set_font_size(cr, 18);
        cairo_move_to(cr, vis->width/2 - 80, vis->height/2 + 30);
        cairo_show_text(cr, "Race restarting...");
    }
}
