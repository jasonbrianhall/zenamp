#include "visualization.h"
#include <math.h>

// Static variables for animation state
static double time_offset = 0;
static double turtle_x = 50;
static double hare_x = 50;
static double hare_sleep_timer = 0;
static bool hare_sleeping = false;
static double hare_boost_timer = 0;
static bool hare_boosting = false;
static double turtle_sleep_timer = 0;
static bool turtle_sleeping = false;
static double turtle_boost_timer = 0;
static bool turtle_boosting = false;
static double finish_line_x = 0;
static bool race_finished = false;
static int winner = 0; // 0=none, 1=turtle, 2=hare
static double celebration_time = 0;
static double cloud_x[8] = {0};
static double cloud_y[8] = {0};
static bool clouds_initialized = false;

// Food system
#define MAX_FOOD_ITEMS 50
typedef struct {
    double x, y;
    int type; // 0=carrot, 1=lettuce
    bool active;
    double life; // How long before it disappears
    double vy; // Vertical velocity for falling
} FoodItem;

static FoodItem food_items[MAX_FOOD_ITEMS];
static int food_count = 0;

void update_rabbithare(Visualizer *vis, double dt) {
    const double min_dt = 1.0 / 120.0;
    double speed_factor = dt / 0.033;
    
    if (dt < min_dt) {
        dt = min_dt;
        speed_factor = dt / 0.033;
    }
    
    // Handle mouse clicks to drop food
    if (vis->mouse_left_pressed) {
        // Left click: drop carrot
        if (food_count < MAX_FOOD_ITEMS) {
            for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
                if (!food_items[i].active) {
                    food_items[i].x = vis->mouse_x;
                    food_items[i].y = vis->mouse_y;
                    food_items[i].type = 0; // Carrot
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
        // Right click: drop lettuce
        if (food_count < MAX_FOOD_ITEMS) {
            for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
                if (!food_items[i].active) {
                    food_items[i].x = vis->mouse_x;
                    food_items[i].y = vis->mouse_y;
                    food_items[i].type = 1; // Lettuce
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
    // 400px = 0.2x, 800px = 0.4x, 1600px = 0.8x
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
        
        // Turtle movement with sleep and boost state
        if (turtle_sleeping) {
            turtle_sleep_timer -= dt;
            if (turtle_sleep_timer <= 0) {
                turtle_sleeping = false;
            }
            // Turtle doesn't move while sleeping
        } else {
            // Calculate boost multiplier
            double boost_multiplier = 1.0;
            if (turtle_boosting) {
                turtle_boost_timer -= dt;
                boost_multiplier = 2.0; // Double speed while boosted
                if (turtle_boost_timer <= 0) {
                    turtle_boosting = false;
                }
            }
            // Turtle moves steadily, slightly faster with music (slower than before)
            turtle_x += (1.8 + avg_energy * 0.6) * speed_factor * screen_speed_factor * boost_multiplier;
        }
        
        // Hare behavior based on music energy
        if (hare_sleeping) {
            hare_sleep_timer -= dt;
            
            // Very small chance of waking if turtle passes - turtle makes noises!
            double lead = hare_x - turtle_x;
            double wake_chance = 0;
            
            if (lead < -30) {
                wake_chance = 0.03; // 3% chance if turtle is way ahead
            } else if (lead < -10) {
                wake_chance = 0.02; // 2% chance if turtle is ahead
            } else if (lead < 0) {
                wake_chance = 0.01; // 1% chance if turtle just passed
            }
            
            bool noise_wake = (rand() % 1000) < (wake_chance * 1000);
            
            // Hare is a DEEP sleeper - mostly only wakes up when timer runs out
            if (hare_sleep_timer <= 0 || noise_wake) {
                hare_sleeping = false;
            }
        } else {
            // Calculate boost multiplier for hare
            double boost_multiplier = 1.0;
            if (hare_boosting) {
                hare_boost_timer -= dt;
                boost_multiplier = 1.5; // 50% speed boost for rabbit
                if (hare_boost_timer <= 0) {
                    hare_boosting = false;
                }
            }
            
            // Hare runs fast when music is energetic (faster than before)
            if (avg_energy > 0.35) {
                hare_x += (6.0 + avg_energy * 3.0) * speed_factor * screen_speed_factor * boost_multiplier;
            } else {
                // Better minimum speed to stay competitive
                hare_x += 2.5 * speed_factor * screen_speed_factor * boost_multiplier;
            }
            
            // Calculate how far ahead the hare is
            double lead = hare_x - turtle_x;
            
            // Hare gets more confident and lazy the further ahead it is
            // Uses exponential scaling - barely ahead = very cautious, far ahead = very lazy
            double confidence_factor = 1.0;
            double sleep_time_bonus = 0;
            
            if (lead > 150) {
                confidence_factor = 10.0; // Way ahead - extremely overconfident
                sleep_time_bonus = 6.0;   // Very long naps
            } else if (lead > 100) {
                confidence_factor = 7.0;  // Far ahead - very lazy
                sleep_time_bonus = 5.0;
            } else if (lead > 60) {
                confidence_factor = 5.0;  // Comfortable lead - quite lazy
                sleep_time_bonus = 4.0;
            } else if (lead > 30) {
                confidence_factor = 3.0;  // Moderate lead - somewhat lazy
                sleep_time_bonus = 3.0;
            } else if (lead > 15) {
                confidence_factor = 1.5;  // Small lead - bit cautious
                sleep_time_bonus = 1.5;
            } else if (lead > 5) {
                confidence_factor = 0.5;  // Barely ahead - very cautious
                sleep_time_bonus = 0.5;
            }
            // If lead <= 5, confidence_factor stays at 1.0 (minimal sleep chance)
            
            // Base sleep chance is lower, but scales dramatically with confidence
            double sleep_chance = 3 * confidence_factor;  // Was 20
            
            // Hare only sleeps when ahead and conditions are right
            double energy_threshold = 0.50;
            if (lead > 80) energy_threshold = 0.60; // Can sleep even with more energetic music
            
            if (avg_energy < energy_threshold && lead > 5 && (rand() % 100) < sleep_chance) {
                hare_sleeping = true;
                hare_sleep_timer = 2.0 + sleep_time_bonus + (rand() % 2); // 2-4 base + bonus
            }
        }
        
        // Update food items
        double ground_y = vis->height * 0.7;
        double track_y = ground_y - 30; // Center of the track
        
        for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
            if (!food_items[i].active) continue;
            
            // Food lands on the track (where racers are)
            double ground_y_level = track_y;
            
            // Apply gravity
            food_items[i].vy += 300.0 * dt; // Gravity acceleration
            
            // Update position
            food_items[i].y += food_items[i].vy * dt;
            
            // Bounce off ground
            if (food_items[i].y >= ground_y_level) {
                food_items[i].y = ground_y_level;
                food_items[i].vy *= -0.6; // Bounce with damping
                
                // Stop bouncing when velocity is very small
                if (fabs(food_items[i].vy) < 10.0) {
                    food_items[i].vy = 0.0;
                }
            }
            
            food_items[i].life -= dt;
            if (food_items[i].life <= 0) {
                food_items[i].active = false;
                food_count--;
            }
        }
        
        // Check if hare eats lettuce or if turtle eats carrot
        const double collision_distance = 30.0; // Increased from 20 for better detection
        
        for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
            if (!food_items[i].active) continue;
            
            // Check hare collision (hare is at ground_y - 50)
            double hare_y = ground_y - 50;
            double hare_dist = sqrt((hare_x - food_items[i].x) * (hare_x - food_items[i].x) + 
                                   (hare_y - food_items[i].y) * (hare_y - food_items[i].y));
            
            if (hare_dist < collision_distance) {
                if (food_items[i].type == 0) { // Carrot gives rabbit boost
                    hare_boosting = true;
                    hare_boost_timer = 2.5;
                    food_items[i].active = false;
                    food_count--;
                } else if (food_items[i].type == 1) { // Lettuce makes rabbit sleep
                    hare_sleeping = true;
                    hare_sleep_timer = 3.0;
                    food_items[i].active = false;
                    food_count--;
                }
            }
            
            // Check turtle collision
            if (!food_items[i].active) continue; // Food might have been eaten by hare
            
            double turtle_y = ground_y - 45;
            double turtle_dist = sqrt((turtle_x - food_items[i].x) * (turtle_x - food_items[i].x) + 
                                     (turtle_y - food_items[i].y) * (turtle_y - food_items[i].y));
            
            if (turtle_dist < collision_distance) {
                if (food_items[i].type == 0) { // Carrot gives turtle boost
                    turtle_boosting = true;
                    turtle_boost_timer = 2.5;
                    food_items[i].active = false;
                    food_count--;
                } else if (food_items[i].type == 1) { // Lettuce makes turtle sleep
                    turtle_sleeping = true;
                    turtle_sleep_timer = 3.0;
                    food_items[i].active = false;
                    food_count--;
                }
            }
        }
        
        if (turtle_x >= finish_line_x && winner == 0) {
            winner = 1;
            race_finished = true;
            celebration_time = 3.0;
        } else if (hare_x >= finish_line_x && winner == 0) {
            winner = 2;
            race_finished = true;
            celebration_time = 3.0;
        }
    } else {
        celebration_time -= dt;
        if (celebration_time <= 0) {
            // Reset race
            turtle_x = 50;
            hare_x = 50;
            hare_sleeping = false;
            hare_sleep_timer = 0;
            hare_boosting = false;
            hare_boost_timer = 0;
            turtle_sleeping = false;
            turtle_sleep_timer = 0;
            turtle_boosting = false;
            turtle_boost_timer = 0;
            race_finished = false;
            winner = 0;
            celebration_time = 0;
            
            // Clear food items
            for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
                food_items[i].active = false;
            }
            food_count = 0;
        }
    }
}

void draw_rabbithare(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    // Sky gradient background
    cairo_pattern_t *sky = cairo_pattern_create_linear(0, 0, 0, vis->height);
    cairo_pattern_add_color_stop_rgb(sky, 0, 0.4, 0.7, 1.0);
    cairo_pattern_add_color_stop_rgb(sky, 0.7, 0.6, 0.85, 1.0);
    cairo_pattern_add_color_stop_rgb(sky, 1.0, 0.5, 0.8, 0.6);
    cairo_set_source(cr, sky);
    cairo_paint(cr);
    cairo_pattern_destroy(sky);
    
    // Draw clouds
    for (int i = 0; i < 8; i++) {
        cairo_set_source_rgba(cr, 1, 1, 1, 0.7);
        cairo_arc(cr, cloud_x[i], cloud_y[i], 20, 0, 6.28);
        cairo_fill(cr);
        cairo_arc(cr, cloud_x[i] + 15, cloud_y[i] - 5, 15, 0, 6.28);
        cairo_fill(cr);
        cairo_arc(cr, cloud_x[i] + 25, cloud_y[i], 18, 0, 6.28);
        cairo_fill(cr);
    }
    
    // Ground
    double ground_y = vis->height * 0.7;
    cairo_set_source_rgb(cr, 0.4, 0.7, 0.3);
    cairo_rectangle(cr, 0, ground_y, vis->width, vis->height - ground_y);
    cairo_fill(cr);
    
    // Grass texture
    cairo_set_source_rgb(cr, 0.3, 0.6, 0.25);
    for (int gx = 0; gx < vis->width; gx += 10) {
        for (int i = 0; i < 3; i++) {
            double grass_x = gx + (rand() % 8);
            double grass_y = ground_y + (rand() % 30);
            cairo_move_to(cr, grass_x, grass_y);
            cairo_line_to(cr, grass_x, grass_y - 5 - (rand() % 5));
            cairo_set_line_width(cr, 1);
            cairo_stroke(cr);
        }
    }
    
    // Race track
    cairo_set_source_rgb(cr, 0.6, 0.5, 0.4);
    cairo_rectangle(cr, 0, ground_y - 60, vis->width, 80);
    cairo_fill(cr);
    
    // Track lines
    cairo_set_source_rgba(cr, 1, 1, 1, 0.3);
    cairo_set_line_width(cr, 2);
    cairo_move_to(cr, 0, ground_y - 20);
    cairo_line_to(cr, vis->width, ground_y - 20);
    cairo_stroke(cr);
    
    // Dashed center line
    double dashes[] = {10, 10};
    cairo_set_dash(cr, dashes, 2, 0);
    cairo_move_to(cr, 0, ground_y - 40);
    cairo_line_to(cr, vis->width, ground_y - 40);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
    
    // Start line
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_rectangle(cr, 40, ground_y - 60, 5, 80);
    cairo_fill(cr);
    
    // Finish line (checkered)
    for (int i = 0; i < 8; i++) {
        if (i % 2 == 0) {
            cairo_set_source_rgb(cr, 0, 0, 0);
        } else {
            cairo_set_source_rgb(cr, 1, 1, 1);
        }
        cairo_rectangle(cr, finish_line_x, ground_y - 60 + i * 10, 10, 10);
        cairo_fill(cr);
    }
    
    // Draw frequency bars behind racers
    double bar_width = (double)vis->width / VIS_FREQUENCY_BARS;
    // Scale bar height based on screen height - taller screens get taller bars
    double max_bar_height = vis->height * 0.35; // Use 35% of screen height
    if (max_bar_height < 60) max_bar_height = 60; // Minimum height
    
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        double height = vis->frequency_bands[i] * max_bar_height;
        double x = i * bar_width;
        double y = ground_y - 60 - height;
        
        double hue = (double)i / VIS_FREQUENCY_BARS;
        double r = 0.5 + 0.5 * sin(hue * 6.28);
        double g = 0.5 + 0.5 * sin(hue * 6.28 + 2.09);
        double b = 0.5 + 0.5 * sin(hue * 6.28 + 4.18);
        
        cairo_set_source_rgba(cr, r, g, b, 0.3);
        cairo_rectangle(cr, x, y, bar_width - 1, height);
        cairo_fill(cr);
    }
    
    // Draw food items
    for (int i = 0; i < MAX_FOOD_ITEMS; i++) {
        if (!food_items[i].active) continue;
        
        // Fade out as food expires
        double alpha = food_items[i].life / 8.0;
        
        if (food_items[i].type == 0) {
            // Draw carrot (orange)
            cairo_set_source_rgba(cr, 1.0, 0.6, 0.0, alpha);
            
            // Carrot shape - triangle
            cairo_move_to(cr, food_items[i].x, food_items[i].y - 8);
            cairo_line_to(cr, food_items[i].x - 6, food_items[i].y + 6);
            cairo_line_to(cr, food_items[i].x + 6, food_items[i].y + 6);
            cairo_close_path(cr);
            cairo_fill(cr);
            
            // Green top
            cairo_set_source_rgba(cr, 0.0, 0.7, 0.0, alpha);
            cairo_move_to(cr, food_items[i].x - 4, food_items[i].y - 8);
            cairo_line_to(cr, food_items[i].x, food_items[i].y - 14);
            cairo_line_to(cr, food_items[i].x + 4, food_items[i].y - 8);
            cairo_close_path(cr);
            cairo_fill(cr);
        } else {
            // Draw lettuce (green)
            cairo_set_source_rgba(cr, 0.0, 0.8, 0.0, alpha);
            
            // Lettuce is a leaf shape
            cairo_save(cr);
            cairo_translate(cr, food_items[i].x, food_items[i].y);
            
            cairo_arc(cr, 0, 0, 8, 0, 6.28);
            cairo_fill(cr);
            
            // Wavy edge
            cairo_set_source_rgba(cr, 0.0, 0.6, 0.0, alpha);
            cairo_set_line_width(cr, 1.5);
            for (int j = 0; j < 8; j++) {
                double angle = (double)j / 8.0 * 6.28;
                double x1 = cos(angle) * 8;
                double y1 = sin(angle) * 8;
                double x2 = cos(angle + 0.2) * 8;
                double y2 = sin(angle + 0.2) * 8;
                
                cairo_move_to(cr, x1, y1);
                cairo_line_to(cr, x2, y2);
                cairo_stroke(cr);
            }
            
            cairo_restore(cr);
        }
    }
    
    // Draw Turtle
    double turtle_y = ground_y - 45;
    cairo_save(cr);
    cairo_translate(cr, turtle_x, turtle_y);
    
    // Shell
    cairo_set_source_rgb(cr, 0.4, 0.6, 0.3);
    cairo_arc(cr, 0, 0, 18, 0, 6.28);
    cairo_fill(cr);
    
    // Shell pattern
    cairo_set_source_rgb(cr, 0.3, 0.5, 0.25);
    cairo_arc(cr, 0, 0, 12, 0, 6.28);
    cairo_fill(cr);
    for (int i = 0; i < 6; i++) {
        double angle = i * 1.047;
        cairo_arc(cr, cos(angle) * 8, sin(angle) * 8, 4, 0, 6.28);
        cairo_fill(cr);
    }
    
    if (turtle_sleeping) {
        // Sleeping turtle - head retracted
        cairo_set_source_rgb(cr, 0.5, 0.7, 0.4);
        cairo_arc(cr, 5, 5, 6, 0, 6.28);
        cairo_fill(cr);
        
        // Closed eyes
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, 1.5);
        cairo_move_to(cr, 3, 4);
        cairo_line_to(cr, 7, 4);
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
    } else if (turtle_boosting) {
        // Boosted turtle - speed lines and glow
        
        // Head (moving fast)
        double head_bob = sin(time_offset * 6) * 3;
        cairo_set_source_rgb(cr, 0.5, 0.7, 0.4);
        cairo_arc(cr, 20 + head_bob, -5, 8, 0, 6.28);
        cairo_fill(cr);
        
        // Eye
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_arc(cr, 23 + head_bob, -7, 2, 0, 6.28);
        cairo_fill(cr);
        
        // Fast legs
        cairo_set_source_rgb(cr, 0.5, 0.7, 0.4);
        cairo_set_line_width(cr, 4);
        double leg_swing = sin(time_offset * 8) * 4;
        cairo_move_to(cr, -8, 12);
        cairo_line_to(cr, -12, 18 + leg_swing);
        cairo_stroke(cr);
        cairo_move_to(cr, 8, 12);
        cairo_line_to(cr, 12, 18 - leg_swing);
        cairo_stroke(cr);
        
        // Boost glow
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.3);
        cairo_arc(cr, 0, 0, 22, 0, 6.28);
        cairo_fill(cr);
    } else {
        // Head (moving back and forth)
        double head_bob = sin(time_offset * 3) * 2;
        cairo_set_source_rgb(cr, 0.5, 0.7, 0.4);
        cairo_arc(cr, 20 + head_bob, -5, 8, 0, 6.28);
        cairo_fill(cr);
        
        // Eye
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_arc(cr, 23 + head_bob, -7, 2, 0, 6.28);
        cairo_fill(cr);
        
        // Legs
        cairo_set_source_rgb(cr, 0.5, 0.7, 0.4);
        cairo_set_line_width(cr, 4);
        double leg_swing = sin(time_offset * 4) * 3;
        cairo_move_to(cr, -8, 12);
        cairo_line_to(cr, -12, 18 + leg_swing);
        cairo_stroke(cr);
        cairo_move_to(cr, 8, 12);
        cairo_line_to(cr, 12, 18 - leg_swing);
        cairo_stroke(cr);
    }
    
    cairo_restore(cr);
    
    // Draw Hare
    double hare_y = ground_y - 50;
    cairo_save(cr);
    cairo_translate(cr, hare_x, hare_y);
    
    if (hare_sleeping) {
        // Sleeping hare
        cairo_set_source_rgb(cr, 0.7, 0.6, 0.5);
        
        // Body (curled up)
        cairo_arc(cr, 0, 5, 20, 0, 6.28);
        cairo_fill(cr);
        
        // Head
        cairo_arc(cr, 15, 0, 12, 0, 6.28);
        cairo_fill(cr);
        
        // Ears (floppy)
        cairo_arc(cr, 20, -8, 8, 0, 6.28);
        cairo_fill(cr);
        cairo_arc(cr, 28, -6, 7, 0, 6.28);
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
    } else if (hare_boosting) {
        // Boosted hare - speed lines and glow
        cairo_set_source_rgb(cr, 0.7, 0.6, 0.5);
        
        // Body (stretched)
        cairo_save(cr);
        cairo_scale(cr, 1.4, 0.8);
        cairo_arc(cr, 0, 0, 15, 0, 6.28);
        cairo_fill(cr);
        cairo_restore(cr);
        
        // Head
        cairo_arc(cr, 18, -8, 10, 0, 6.28);
        cairo_fill(cr);
        
        // Ears (upright and alert)
        cairo_arc(cr, 20, -18, 6, 0, 6.28);
        cairo_fill(cr);
        cairo_arc(cr, 28, -20, 5, 0, 6.28);
        cairo_fill(cr);
        
        // Eye
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_arc(cr, 22, -10, 2, 0, 6.28);
        cairo_fill(cr);
        
        // Fast running legs
        cairo_set_source_rgb(cr, 0.7, 0.6, 0.5);
        cairo_set_line_width(cr, 3);
        double run_cycle = sin(time_offset * 12) * 10;
        cairo_move_to(cr, -10, 8);
        cairo_line_to(cr, -15, 18 + run_cycle);
        cairo_stroke(cr);
        cairo_move_to(cr, 5, 8);
        cairo_line_to(cr, 8, 18 - run_cycle);
        cairo_stroke(cr);
        
        // Boost glow
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.3);
        cairo_arc(cr, 0, 0, 25, 0, 6.28);
        cairo_fill(cr);
        
        // Speed lines
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.4);
        cairo_set_line_width(cr, 2);
        for (int i = 0; i < 3; i++) {
            cairo_move_to(cr, -25 - i * 8, -5 + i * 3);
            cairo_line_to(cr, -35 - i * 8, -5 + i * 3);
            cairo_stroke(cr);
        }
    } else {
        // Running hare
        cairo_set_source_rgb(cr, 0.7, 0.6, 0.5);
        
        // Body (stretched)
        cairo_save(cr);
        cairo_scale(cr, 1.3, 0.8);
        cairo_arc(cr, 0, 0, 15, 0, 6.28);
        cairo_fill(cr);
        cairo_restore(cr);
        
        // Head
        cairo_arc(cr, 18, -8, 10, 0, 6.28);
        cairo_fill(cr);
        
        // Ears (upright)
        cairo_arc(cr, 20, -18, 6, 0, 6.28);
        cairo_fill(cr);
        cairo_arc(cr, 28, -20, 5, 0, 6.28);
        cairo_fill(cr);
        
        // Eye
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_arc(cr, 22, -10, 2, 0, 6.28);
        cairo_fill(cr);
        
        // Running legs
        cairo_set_source_rgb(cr, 0.7, 0.6, 0.5);
        cairo_set_line_width(cr, 3);
        double run_cycle = sin(time_offset * 8) * 8;
        cairo_move_to(cr, -10, 8);
        cairo_line_to(cr, -15, 18 + run_cycle);
        cairo_stroke(cr);
        cairo_move_to(cr, 5, 8);
        cairo_line_to(cr, 8, 18 - run_cycle);
        cairo_stroke(cr);
        
        // Speed lines
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.4);
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
            cairo_move_to(cr, vis->width/2 - 120, vis->height/2);
            cairo_show_text(cr, "TURTLE WINS!");
        } else {
            cairo_move_to(cr, vis->width/2 - 100, vis->height/2);
            cairo_show_text(cr, "HARE WINS!");
        }
        
        cairo_set_font_size(cr, 18);
        cairo_move_to(cr, vis->width/2 - 80, vis->height/2 + 30);
        cairo_show_text(cr, "Race restarting...");
    }
}
