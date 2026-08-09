#include "visualization.h"

void draw_birthday_candles(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    
    // Pulsating background with beat
    double bg_pulse = 1.0 + vis->volume_level * 0.3;
    cairo_pattern_t *bg_pattern = cairo_pattern_create_radial(
        center_x, center_y, 0,
        center_x, center_y, fmax(vis->width, vis->height) * bg_pulse
    );
    cairo_pattern_add_color_stop_rgba(bg_pattern, 0, 0.15, 0.1, 0.2, 1.0);
    cairo_pattern_add_color_stop_rgba(bg_pattern, 0.5, 0.1, 0.05, 0.15, 1.0);
    cairo_pattern_add_color_stop_rgba(bg_pattern, 1.0, 0.05, 0.02, 0.1, 1.0);
    cairo_set_source(cr, bg_pattern);
    cairo_paint(cr);
    cairo_pattern_destroy(bg_pattern);
    
    // Sparkles/stars in background
    for (int i = 0; i < 50; i++) {
        double sparkle_x = (sin(vis->time_offset * 0.5 + i * 0.7) * 0.5 + 0.5) * vis->width;
        double sparkle_y = (cos(vis->time_offset * 0.3 + i * 0.9) * 0.5 + 0.5) * vis->height;
        double sparkle_size = 1 + sin(vis->time_offset * 4 + i) * 1.5;
        double sparkle_alpha = (sin(vis->time_offset * 3 + i) * 0.5 + 0.5) * 0.7;
        
        double hue = fmod((double)i / 50.0 + vis->time_offset * 0.2, 1.0);
        double r, g, b;
        hsv_to_rgb(hue, 0.8, 1.0, &r, &g, &b);
        
        cairo_set_source_rgba(cr, r, g, b, sparkle_alpha);
        cairo_arc(cr, sparkle_x, sparkle_y, sparkle_size, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Star rays
        if (sparkle_size > 2.0) {
            cairo_set_line_width(cr, 1.0);
            for (int ray = 0; ray < 4; ray++) {
                double angle = (M_PI / 2.0) * ray + vis->time_offset;
                cairo_move_to(cr, sparkle_x, sparkle_y);
                cairo_line_to(cr, 
                    sparkle_x + cos(angle) * sparkle_size * 3,
                    sparkle_y + sin(angle) * sparkle_size * 3);
                cairo_stroke(cr);
            }
        }
    }
    
    // Number of candles based on frequency bars
    int num_candles = 9;
    double candle_spacing = vis->width / (num_candles + 1);
    double base_y = vis->height * 0.7;
    
    // Draw cake base with rotation
    double cake_width = vis->width * 0.85;
    double cake_height = vis->height * 0.18;
    double cake_x = center_x - cake_width / 2;
    double cake_y = base_y;
    
    // Cake layers with beat pulse and slight bounce
    double beat_pulse = 1.0 + vis->volume_level * 0.15;
    double cake_bounce = sin(vis->time_offset * 2) * 5 * vis->volume_level;
    cake_y += cake_bounce;
    
    // Shadow under cake (ellipse using scale transform)
    cairo_save(cr);
    cairo_translate(cr, center_x, cake_y + cake_height);
    cairo_scale(cr, cake_width * 0.5, 20);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.3);
    cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
    
    // Bottom layer (chocolate) with texture
    cairo_pattern_t *choc_pattern = cairo_pattern_create_linear(
        cake_x, cake_y, cake_x, cake_y + cake_height * 0.6
    );
    cairo_pattern_add_color_stop_rgb(choc_pattern, 0, 0.5, 0.25, 0.15);
    cairo_pattern_add_color_stop_rgb(choc_pattern, 0.5, 0.4, 0.2, 0.1);
    cairo_pattern_add_color_stop_rgb(choc_pattern, 1, 0.35, 0.18, 0.08);
    cairo_set_source(cr, choc_pattern);
    cairo_rectangle(cr, cake_x, cake_y, cake_width, cake_height * 0.6);
    cairo_fill(cr);
    cairo_pattern_destroy(choc_pattern);
    
    // Top layer (vanilla) with gradient
    cairo_pattern_t *vanilla_pattern = cairo_pattern_create_linear(
        cake_x, cake_y - cake_height * 0.4, cake_x, cake_y
    );
    cairo_pattern_add_color_stop_rgb(vanilla_pattern, 0, 1.0, 0.95, 0.85);
    cairo_pattern_add_color_stop_rgb(vanilla_pattern, 1, 0.95, 0.85, 0.7);
    cairo_set_source(cr, vanilla_pattern);
    cairo_rectangle(cr, cake_x, cake_y - cake_height * 0.4, cake_width, cake_height * 0.4);
    cairo_fill(cr);
    cairo_pattern_destroy(vanilla_pattern);
    
    // Animated frosting decorations (scalloped edge)
    int num_scallops = 15;
    for (int i = 0; i < num_scallops; i++) {
        double x = cake_x + (cake_width / num_scallops) * i + (cake_width / num_scallops / 2);
        double y = cake_y - cake_height * 0.4 + 4;
        double size = 8 + sin(vis->time_offset * 3 + i * 0.5) * 2;
        
        // Alternate frosting colors
        if (i % 2 == 0) {
            cairo_set_source_rgba(cr, 0.95, 0.4, 0.6, 0.9);
        } else {
            cairo_set_source_rgba(cr, 0.4, 0.7, 0.95, 0.9);
        }
        
        cairo_arc(cr, x, y, size * beat_pulse, 0, M_PI);
        cairo_fill(cr);
    }
    
    // Decorative sprinkles on cake
    for (int i = 0; i < 30; i++) {
        double sprinkle_x = cake_x + 20 + (i % 10) * (cake_width - 40) / 10 + sin(vis->time_offset + i) * 5;
        double sprinkle_y = cake_y - cake_height * 0.3 + (i / 10) * 10;
        double rotation = vis->time_offset * 2 + i;
        
        cairo_save(cr);
        cairo_translate(cr, sprinkle_x, sprinkle_y);
        cairo_rotate(cr, rotation);
        
        double hue = (double)i / 30.0;
        double r, g, b;
        hsv_to_rgb(hue, 1.0, 1.0, &r, &g, &b);
        cairo_set_source_rgba(cr, r, g, b, 0.8);
        
        cairo_rectangle(cr, -1, -4, 2, 8);
        cairo_fill(cr);
        cairo_restore(cr);
    }
    
    // Draw candles with more dynamic effects
    for (int i = 0; i < num_candles; i++) {
        double candle_x = candle_spacing * (i + 1);
        double candle_y = cake_y - cake_height * 0.4 + cake_bounce;
        
        // Get audio intensity for this candle
        int freq_idx = (i * VIS_FREQUENCY_BARS) / num_candles;
        double intensity = vis->frequency_bands[freq_idx];
        
        // Candle dimensions with individual wobble
        double candle_width = 18;
        double candle_height = 70 + intensity * 30;
        double wobble = sin(vis->time_offset * 4 + i * 0.8) * 2 * intensity;
        
        // Candle body with beat pulse and wobble
        double pulse = beat_pulse * (1.0 + intensity * 0.3);
        
        // Candle glow
        cairo_pattern_t *candle_glow = cairo_pattern_create_radial(
            candle_x, candle_y - candle_height * pulse / 2,
            0,
            candle_x, candle_y - candle_height * pulse / 2,
            candle_width * 2
        );
        
        // Color based on position
        double hue = (double)i / num_candles;
        double r, g, b;
        hsv_to_rgb(hue, 0.7, 1.0, &r, &g, &b);
        
        cairo_pattern_add_color_stop_rgba(candle_glow, 0, r, g, b, 0.5 * intensity);
        cairo_pattern_add_color_stop_rgba(candle_glow, 1, r, g, b, 0);
        cairo_set_source(cr, candle_glow);
        cairo_arc(cr, candle_x, candle_y - candle_height * pulse / 2, candle_width * 2, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(candle_glow);
        
        // Candle body with gradient
        cairo_pattern_t *candle_gradient = cairo_pattern_create_linear(
            candle_x - candle_width/2, candle_y,
            candle_x + candle_width/2, candle_y
        );
        cairo_pattern_add_color_stop_rgb(candle_gradient, 0, r * 0.7, g * 0.7, b * 0.7);
        cairo_pattern_add_color_stop_rgb(candle_gradient, 0.5, r, g, b);
        cairo_pattern_add_color_stop_rgb(candle_gradient, 1, r * 0.7, g * 0.7, b * 0.7);
        cairo_set_source(cr, candle_gradient);
        
        cairo_rectangle(cr, 
                       candle_x - candle_width/2 + wobble, 
                       candle_y - candle_height * pulse, 
                       candle_width, 
                       candle_height * pulse);
        cairo_fill(cr);
        cairo_pattern_destroy(candle_gradient);
        
        // Candle shine/highlight
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
        cairo_rectangle(cr, 
                       candle_x - candle_width/2 + 3 + wobble, 
                       candle_y - candle_height * pulse + 5, 
                       3, 
                       candle_height * pulse - 10);
        cairo_fill(cr);
        
        // Wick
        cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.9);
        cairo_set_line_width(cr, 2.5);
        cairo_move_to(cr, candle_x + wobble, candle_y - candle_height * pulse);
        cairo_line_to(cr, candle_x + wobble, candle_y - candle_height * pulse - 10);
        cairo_stroke(cr);
        
        // Flame - much more dynamic
        double flame_y = candle_y - candle_height * pulse - 10;
        double flame_height = 30 + intensity * 25;
        double flame_width = 16 + intensity * 12;
        
        // Multiple flame flicker components
        double flicker1 = sin(vis->time_offset * 12 + i) * 0.15;
        double flicker2 = sin(vis->time_offset * 8 + i * 1.5) * 0.1;
        double flicker = (flicker1 + flicker2 + 1.0);
        flame_height *= flicker;
        
        // Flame sway
        double flame_sway = sin(vis->time_offset * 3 + i * 0.7) * 3 * intensity;
        
        // Outer flame glow (large radius)
        cairo_pattern_t *outer_glow = cairo_pattern_create_radial(
            candle_x + wobble + flame_sway, flame_y - flame_height * 0.3,
            0,
            candle_x + wobble + flame_sway, flame_y - flame_height * 0.3,
            flame_width * 2 * pulse
        );
        cairo_pattern_add_color_stop_rgba(outer_glow, 0, 1.0, 0.9, 0.3, 0.6 * intensity);
        cairo_pattern_add_color_stop_rgba(outer_glow, 0.5, 1.0, 0.6, 0.0, 0.3 * intensity);
        cairo_pattern_add_color_stop_rgba(outer_glow, 1.0, 1.0, 0.3, 0.0, 0);
        
        cairo_set_source(cr, outer_glow);
        cairo_arc(cr, candle_x + wobble + flame_sway, flame_y - flame_height * 0.3, 
                  flame_width * 2 * pulse, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(outer_glow);
        
        // Main flame body
        cairo_pattern_t *flame_pattern = cairo_pattern_create_radial(
            candle_x + wobble + flame_sway, flame_y - flame_height * 0.4,
            0,
            candle_x + wobble + flame_sway, flame_y - flame_height * 0.4,
            flame_width * pulse
        );
        cairo_pattern_add_color_stop_rgba(flame_pattern, 0, 1.0, 1.0, 0.9, 1.0);
        cairo_pattern_add_color_stop_rgba(flame_pattern, 0.3, 1.0, 0.95, 0.4, 0.95);
        cairo_pattern_add_color_stop_rgba(flame_pattern, 0.6, 1.0, 0.6, 0.1, 0.8);
        cairo_pattern_add_color_stop_rgba(flame_pattern, 1.0, 1.0, 0.3, 0.0, 0.3);
        
        cairo_set_source(cr, flame_pattern);
        
        // Flame teardrop shape with sway
        cairo_move_to(cr, candle_x + wobble + flame_sway, flame_y);
        cairo_curve_to(cr, 
                      candle_x + wobble + flame_sway - flame_width/2 * pulse, flame_y - flame_height * 0.3,
                      candle_x + wobble + flame_sway - flame_width/2.5 * pulse, flame_y - flame_height * 0.7,
                      candle_x + wobble + flame_sway + flame_sway * 0.5, flame_y - flame_height);
        cairo_curve_to(cr, 
                      candle_x + wobble + flame_sway + flame_width/2.5 * pulse, flame_y - flame_height * 0.7,
                      candle_x + wobble + flame_sway + flame_width/2 * pulse, flame_y - flame_height * 0.3,
                      candle_x + wobble + flame_sway, flame_y);
        cairo_fill(cr);
        cairo_pattern_destroy(flame_pattern);
        
        // Hot white center
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
        double inner_height = flame_height * 0.4;
        double inner_width = flame_width * 0.3;
        
        cairo_move_to(cr, candle_x + wobble + flame_sway, flame_y - 5);
        cairo_curve_to(cr, 
                      candle_x + wobble + flame_sway - inner_width/2 * pulse, flame_y - inner_height * 0.3,
                      candle_x + wobble + flame_sway - inner_width/2 * pulse, flame_y - inner_height * 0.7,
                      candle_x + wobble + flame_sway, flame_y - inner_height);
        cairo_curve_to(cr, 
                      candle_x + wobble + flame_sway + inner_width/2 * pulse, flame_y - inner_height * 0.7,
                      candle_x + wobble + flame_sway + inner_width/2 * pulse, flame_y - inner_height * 0.3,
                      candle_x + wobble + flame_sway, flame_y - 5);
        cairo_fill(cr);
        
        // Smoke particles rising with more variety
        if (intensity > 0.2) {
            for (int s = 0; s < 5; s++) {
                double smoke_offset = fmod(vis->time_offset * 40 + s * 25, 120.0);
                double smoke_y = flame_y - flame_height - smoke_offset;
                double smoke_x = candle_x + wobble + sin(vis->time_offset * 2.5 + s * 0.8) * 15;
                double smoke_size = 2 + s * 1.5 + sin(vis->time_offset * 5 + s) * 1;
                double smoke_alpha = 0.4 * (1.0 - smoke_offset / 120.0) * intensity;
                
                cairo_set_source_rgba(cr, 0.6, 0.6, 0.6, smoke_alpha);
                cairo_arc(cr, smoke_x, smoke_y, smoke_size, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }
        
        // Heat distortion waves above flame
        if (intensity > 0.4) {
            cairo_set_line_width(cr, 1.5);
            for (int w = 0; w < 3; w++) {
                double wave_y = flame_y - flame_height - 10 - w * 8;
                double wave_offset = fmod(vis->time_offset * 4 + w * 0.5, 1.0);
                cairo_set_source_rgba(cr, 1.0, 0.8, 0.4, (1.0 - wave_offset) * 0.3 * intensity);
                
                cairo_move_to(cr, candle_x + wobble - 10, wave_y);
                for (int p = 0; p < 5; p++) {
                    double px = candle_x + wobble - 10 + p * 5;
                    double py = wave_y + sin(vis->time_offset * 6 + p + w) * 3;
                    cairo_line_to(cr, px, py);
                }
                cairo_stroke(cr);
            }
        }
    }
    
    // "Happy Birthday" text with sequential fly-in animation and explosion effect
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 56);
    
    const char *text = "Happy Birthday!";
    double text_base_y = vis->height * 0.12;
    int text_length = strlen(text);
    
    // Animation phases: 0 = fly in, 1 = formed, 2 = explode
    double animation_cycle = fmod(vis->time_offset, 10.0); // 10 second cycle
    int phase = 0;
    double phase_time = 0;
    
    if (animation_cycle < 4.0) {
        phase = 0; // Fly in phase (0-4 seconds, sequential)
        phase_time = animation_cycle / 4.0; // 0 to 1
    } else if (animation_cycle < 8.0) {
        phase = 1; // Formed phase (4-8 seconds)
        phase_time = (animation_cycle - 4.0) / 4.0; // 0 to 1
    } else {
        phase = 2; // Explode phase (8-10 seconds)
        phase_time = (animation_cycle - 8.0) / 2.0; // 0 to 1
    }
    
    // Detect if we should trigger explosion (beat during formed phase)
    bool should_explode = false;
    if (phase == 1 && vis->volume_level > 0.6) {
        should_explode = true;
        phase = 2;
        phase_time = 0.0;
    }
    
    // Draw each letter individually
    double letter_x = center_x - 250;
    
    for (int i = 0; text[i] != '\0'; i++) {
        char letter[2] = {text[i], '\0'};
        cairo_text_extents_t extents;
        cairo_text_extents(cr, letter, &extents);
        
        double target_x = letter_x + extents.width / 2;
        double target_y = text_base_y;
        double current_x = target_x;
        double current_y = target_y;
        double alpha = 1.0;
        double scale = 1.0;
        double rotation = 0.0;
        
        if (phase == 0) {
            // Sequential fly in - each letter has its own timing
            double letter_start_time = (double)i / text_length; // When this letter starts (0 to 1)
            double letter_duration = 0.15; // Each letter takes 15% of the phase to fly in
            double letter_phase = (phase_time - letter_start_time) / letter_duration;
            
            if (letter_phase < 0) {
                // Letter hasn't started flying in yet - don't draw
                letter_x += extents.x_advance + 2;
                continue;
            } else if (letter_phase > 1.0) {
                // Letter has finished flying in - hold in position
                letter_phase = 1.0;
            }
            
            // Ease in cubic for smooth deceleration
            double eased = 1.0 - pow(1.0 - letter_phase, 3);
            
            // Fly in from random directions
            double start_angle = (i * 0.7 + 2.1) * M_PI; // Different angle for each letter
            double distance = 800 * (1.0 - eased);
            
            current_x = target_x + cos(start_angle) * distance;
            current_y = target_y + sin(start_angle) * distance;
            
            // Spinning while flying in
            rotation = (1.0 - letter_phase) * M_PI * 4;
            alpha = letter_phase;
            scale = 0.3 + letter_phase * 0.7;
            
        } else if (phase == 1) {
            // Formed - gentle wave motion
            double wave_offset = sin(vis->time_offset * 4 + i * 0.6) * 20;
            current_y = target_y + wave_offset;
            rotation = sin(vis->time_offset * 2 + i * 0.5) * 0.15;
            
        } else if (phase == 2) {
            // Explode outward all at once
            double explode_angle = (i * 0.5 + 1.3) * M_PI;
            double explode_distance = phase_time * phase_time * 500; // Accelerate
            
            current_x = target_x + cos(explode_angle) * explode_distance;
            current_y = target_y + sin(explode_angle) * explode_distance;
            
            // Spinning while exploding
            rotation = phase_time * M_PI * 6;
            alpha = 1.0 - phase_time;
            scale = 1.0 + phase_time * 2.0;
        }
        
        // Rainbow color based on position and time
        double hue = fmod(vis->time_offset * 0.4 + (double)i / text_length, 1.0);
        double r, g, b;
        hsv_to_rgb(hue, 0.9, 1.0, &r, &g, &b);
        
        cairo_save(cr);
        cairo_translate(cr, current_x, current_y);
        cairo_rotate(cr, rotation);
        cairo_scale(cr, scale, scale);
        cairo_translate(cr, -(extents.width / 2), 0);
        
        // Multi-layer glow effect with beat pulse
        if (phase == 1) {
            for (int glow = 0; glow < 4; glow++) {
                cairo_set_source_rgba(cr, r, g, b, 0.5 * vis->volume_level / (glow + 1) * alpha);
                cairo_move_to(cr, glow, glow);
                cairo_show_text(cr, letter);
            }
        }
        
        // Explosion particles
        if (phase == 2) {
            for (int p = 0; p < 8; p++) {
                double particle_angle = (p / 8.0) * 2 * M_PI;
                double particle_dist = phase_time * 60;
                double px = (extents.width / 2) + cos(particle_angle) * particle_dist;
                double py = sin(particle_angle) * particle_dist;
                double particle_size = 3 * (1.0 - phase_time);
                
                cairo_set_source_rgba(cr, r, g, b, alpha * 0.8);
                cairo_arc(cr, px, py, particle_size, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }
        
        // Main letter with outline
        cairo_set_source_rgba(cr, r, g, b, alpha);
        cairo_move_to(cr, 0, 0);
        cairo_text_path(cr, letter);
        cairo_fill_preserve(cr);
        
        // White outline
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8 * alpha);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
        
        cairo_restore(cr);
        
        // Move to next letter position
        letter_x += extents.x_advance + 2;
    }
    
    // Enhanced confetti with more variety
    for (int c = 0; c < 60; c++) {
        double confetti_x = fmod(sin(vis->time_offset * 0.6 + c * 0.3) * vis->width * 0.6 + vis->width * 0.5, vis->width);
        double fall_speed = 0.3 + (c % 5) * 0.1;
        double confetti_y = fmod(vis->time_offset * (60 * fall_speed) + c * 20, vis->height + 50);
        double rotation = vis->time_offset * (4 + (c % 3)) + c;
        double size = 3 + (c % 3) * 2;
        
        cairo_save(cr);
        cairo_translate(cr, confetti_x, confetti_y);
        cairo_rotate(cr, rotation);
        
        // Varied confetti shapes and colors
        double hue = (double)(c % 12) / 12.0;
        double r, g, b;
        hsv_to_rgb(hue, 0.9, 1.0, &r, &g, &b);
        cairo_set_source_rgba(cr, r, g, b, 0.85);
        
        if (c % 3 == 0) {
            // Rectangle
            cairo_rectangle(cr, -size/2, -size*2, size, size*4);
        } else if (c % 3 == 1) {
            // Circle
            cairo_arc(cr, 0, 0, size, 0, 2 * M_PI);
        } else {
            // Triangle
            cairo_move_to(cr, 0, -size * 1.5);
            cairo_line_to(cr, -size, size * 1.5);
            cairo_line_to(cr, size, size * 1.5);
            cairo_close_path(cr);
        }
        cairo_fill(cr);
        
        cairo_restore(cr);
    }
    
    // Balloons floating around
    for (int b = 0; b < 6; b++) {
        double balloon_x = (b * vis->width / 7) + 50 + sin(vis->time_offset * 0.8 + b) * 40;
        double balloon_y = 80 + sin(vis->time_offset * 1.2 + b * 0.7) * 30;
        double balloon_size = 30 + sin(vis->time_offset * 2 + b) * 5;
        
        double hue = (double)b / 6.0;
        double r, g, b_col;
        hsv_to_rgb(hue, 0.8, 1.0, &r, &b_col, &b_col);
        
        // Balloon string
        cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 0.6);
        cairo_set_line_width(cr, 1.5);
        cairo_move_to(cr, balloon_x, balloon_y + balloon_size);
        cairo_curve_to(cr,
            balloon_x + sin(vis->time_offset + b) * 10, balloon_y + balloon_size + 30,
            balloon_x - sin(vis->time_offset + b) * 10, balloon_y + balloon_size + 60,
            balloon_x, balloon_y + balloon_size + 80);
        cairo_stroke(cr);
        
        // Balloon gradient
        cairo_pattern_t *balloon_grad = cairo_pattern_create_radial(
            balloon_x - balloon_size * 0.3, balloon_y - balloon_size * 0.3, 0,
            balloon_x, balloon_y, balloon_size
        );
        cairo_pattern_add_color_stop_rgba(balloon_grad, 0, 
            fmin(r * 1.5, 1.0), fmin(b_col * 1.5, 1.0), fmin(b_col * 1.5, 1.0), 0.9);
        cairo_pattern_add_color_stop_rgba(balloon_grad, 0.7, r, b_col, b_col, 0.9);
        cairo_pattern_add_color_stop_rgba(balloon_grad, 1, r * 0.6, b_col * 0.6, b_col * 0.6, 0.9);
        
        cairo_set_source(cr, balloon_grad);
        
        // Balloon shape
        cairo_arc(cr, balloon_x, balloon_y, balloon_size, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(balloon_grad);
        
        // Balloon highlight
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.6);
        cairo_arc(cr, balloon_x - balloon_size * 0.3, balloon_y - balloon_size * 0.3, 
                  balloon_size * 0.3, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Balloon knot
        cairo_set_source_rgba(cr, r * 0.5, b_col * 0.5, b_col * 0.5, 0.8);
        cairo_arc(cr, balloon_x, balloon_y + balloon_size, 3, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}
