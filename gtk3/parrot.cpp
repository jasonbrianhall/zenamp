#include "visualization.h"

// Parrot visualization

void init_parrot_system(Visualizer *vis) {
    vis->parrot_state.mouth_open = 0.0;
    vis->parrot_state.blink_timer = 0.0;
    vis->parrot_state.eye_closed = FALSE;
    vis->parrot_state.head_bob_offset = 0.0;
    vis->parrot_state.body_bounce = 0.0;
    vis->parrot_state.wing_flap_angle = 0.0;
    vis->parrot_state.tail_sway = 0.0;
    vis->parrot_state.pupil_x = 0.0;
    vis->parrot_state.pupil_y = 0.0;
    vis->parrot_state.chest_scale = 1.0;
    vis->parrot_state.foot_tap = 0.0;
    vis->parrot_state.right_foot_tap = 0.0;
    vis->parrot_state.glow_intensity = 0.0;
    vis->parrot_state.last_beat_time = 0.0;
}

void update_parrot(Visualizer *vis, double dt) {
    double target_mouth = vis->volume_level * 3.0;
    if (target_mouth > 1.0) target_mouth = 1.0;
    
    double high_freq_energy = 0.0;
    for (int i = VIS_FREQUENCY_BARS/2; i < VIS_FREQUENCY_BARS; i++) {
        high_freq_energy += vis->frequency_bands[i];
    }
    high_freq_energy /= (VIS_FREQUENCY_BARS / 2);
    
    // Bass frequencies for beat detection
    double bass_energy = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS/4; i++) {
        bass_energy += vis->frequency_bands[i];
    }
    bass_energy /= (VIS_FREQUENCY_BARS / 4);
    
    double speed_multiplier = 8.0 + high_freq_energy * 12.0;
    vis->parrot_state.mouth_open += (target_mouth - vis->parrot_state.mouth_open) * speed_multiplier * dt;
    
    if (vis->parrot_state.mouth_open > 0.3) {
        vis->parrot_state.mouth_open += sin(vis->time_offset * 20) * 0.02;
    }
    
    if (vis->parrot_state.mouth_open < 0.0) vis->parrot_state.mouth_open = 0.0;
    if (vis->parrot_state.mouth_open > 1.0) vis->parrot_state.mouth_open = 1.0;
    
    // Eye blinking
    vis->parrot_state.blink_timer += dt;
    if (!vis->parrot_state.eye_closed && vis->parrot_state.blink_timer > 3.0 + (rand() % 2000) / 1000.0) {
        vis->parrot_state.eye_closed = TRUE;
        vis->parrot_state.blink_timer = 0.0;
    } else if (vis->parrot_state.eye_closed && vis->parrot_state.blink_timer > 0.15) {
        vis->parrot_state.eye_closed = FALSE;
        vis->parrot_state.blink_timer = 0.0;
    }
    
    // Head bobbing to the beat (bass frequencies) - only when music is playing
    double target_bob = 0.0;
    if (vis->volume_level > 0.05) {
        target_bob = sin(vis->time_offset * 3.0) * bass_energy * 15.0;
    }
    vis->parrot_state.head_bob_offset += (target_bob - vis->parrot_state.head_bob_offset) * 6.0 * dt;
    
    // Body bounce - only when music is playing
    double target_bounce = 0.0;
    if (vis->volume_level > 0.05) {
        target_bounce = vis->volume_level * 8.0;
    }
    vis->parrot_state.body_bounce += (target_bounce - vis->parrot_state.body_bounce) * 5.0 * dt;
    
    // Wing flapping
    double target_wing = sin(vis->time_offset * 4.0) * vis->volume_level * 25.0;
    vis->parrot_state.wing_flap_angle += (target_wing - vis->parrot_state.wing_flap_angle) * 8.0 * dt;
    
    // Tail feather sway
    vis->parrot_state.tail_sway = sin(vis->time_offset * 2.5) * 10.0 * vis->volume_level;
    
    // Pupil tracking (follows music notes) - only when music is playing
    double target_pupil_x = 0.0;
    double target_pupil_y = 0.0;
    if (vis->volume_level > 0.1) {
        target_pupil_x = -sin(vis->time_offset * 3.0) * 8.0;
        target_pupil_y = cos(vis->time_offset * 2.0) * 5.0;
    }
    vis->parrot_state.pupil_x += (target_pupil_x - vis->parrot_state.pupil_x) * 4.0 * dt;
    vis->parrot_state.pupil_y += (target_pupil_y - vis->parrot_state.pupil_y) * 4.0 * dt;
    
    // Chest breathing
    double target_chest = 1.0 + vis->volume_level * 0.15;
    vis->parrot_state.chest_scale += (target_chest - vis->parrot_state.chest_scale) * 7.0 * dt;
    
    // Foot tapping - both feet alternating
    if (vis->volume_level > 0.05 && bass_energy > 0.4) {
        double target_left_tap = sin(vis->time_offset * 8.0) * 8.0;
        double target_right_tap = sin(vis->time_offset * 8.0 + M_PI) * 8.0; // Offset by PI for alternating
        vis->parrot_state.foot_tap += (target_left_tap - vis->parrot_state.foot_tap) * 10.0 * dt;
        vis->parrot_state.right_foot_tap += (target_right_tap - vis->parrot_state.right_foot_tap) * 10.0 * dt;
    } else {
        vis->parrot_state.foot_tap *= 0.9;
        vis->parrot_state.right_foot_tap *= 0.9;
    }
    
    // Glow intensity
    double target_glow = bass_energy * 0.6;
    vis->parrot_state.glow_intensity += (target_glow - vis->parrot_state.glow_intensity) * 10.0 * dt;
    
    // Beat detection for particles
    if (bass_energy > 0.6 && (vis->time_offset - vis->parrot_state.last_beat_time) > 0.3) {
        vis->parrot_state.last_beat_time = vis->time_offset;
    }
}

void draw_music_notes(Visualizer *vis, cairo_t *cr, double cx, double cy, double scale) {
    // Draw musical notes that shoot out from the middle of the beak
    // Adjusted position - lower and more to the left
    double mouth_x = cx - 165 * scale;
    double mouth_y = cy + 5 * scale;
    
    for (int i = 0; i < VIS_FREQUENCY_BARS; i += 2) {
        double intensity = vis->frequency_bands[i];
        
        if (intensity > 0.2) {
            // Time-based position
            double time_progress = fmod(vis->time_offset * 60 + i * 12, 400.0);
            
            // Shoot left in a slight arc
            double note_x = mouth_x - time_progress * scale;
            double note_y = mouth_y + sin(time_progress * 0.05) * 20 * scale;
            
            // Fade based on distance
            double distance_fade = 1.0 - (time_progress / 400.0);
            
            // Color based on frequency
            double hue = (double)i / VIS_FREQUENCY_BARS;
            double r, g, b;
            hsv_to_rgb(hue * 0.8, 0.8, 1.0, &r, &g, &b);
            
            // Opacity based on audio intensity AND distance
            double alpha = intensity * distance_fade;
            cairo_set_source_rgba(cr, r, g, b, alpha);
            
            double note_scale = scale * 0.7;
            
            // Vary note types based on intensity
            if (intensity > 0.7) {
                // Whole note
                cairo_arc(cr, note_x, note_y, 8 * note_scale, 0, 2 * M_PI);
                cairo_stroke(cr);
            } else if (intensity > 0.5) {
                // Quarter note
                cairo_arc(cr, note_x, note_y, 6 * note_scale, 0, 2 * M_PI);
                cairo_fill(cr);
                
                cairo_set_line_width(cr, 2 * note_scale);
                cairo_move_to(cr, note_x + 5 * note_scale, note_y);
                cairo_line_to(cr, note_x + 5 * note_scale, note_y - 20 * note_scale);
                cairo_stroke(cr);
                
                cairo_move_to(cr, note_x + 5 * note_scale, note_y - 20 * note_scale);
                cairo_curve_to(cr,
                    note_x + 15 * note_scale, note_y - 18 * note_scale,
                    note_x + 18 * note_scale, note_y - 10 * note_scale,
                    note_x + 12 * note_scale, note_y - 5 * note_scale);
                cairo_fill(cr);
            } else {
                // Eighth note
                cairo_arc(cr, note_x, note_y, 5 * note_scale, 0, 2 * M_PI);
                cairo_fill(cr);
                
                cairo_set_line_width(cr, 1.5 * note_scale);
                cairo_move_to(cr, note_x + 4 * note_scale, note_y);
                cairo_line_to(cr, note_x + 4 * note_scale, note_y - 15 * note_scale);
                cairo_stroke(cr);
            }
        }
    }
}

void draw_particles(Visualizer *vis, cairo_t *cr, double cx, double cy, double scale) {
    // Draw burst particles on beat
    double time_since_beat = vis->time_offset - vis->parrot_state.last_beat_time;
    
    if (time_since_beat < 0.5) {
        for (int i = 0; i < 20; i++) {
            double angle = (double)i / 20.0 * 2.0 * M_PI;
            double distance = time_since_beat * 200.0 * scale;
            double px = cx + cos(angle) * distance;
            double py = cy + sin(angle) * distance;
            
            double fade = 1.0 - (time_since_beat / 0.5);
            
            double hue = (double)i / 20.0;
            double r, g, b;
            hsv_to_rgb(hue, 0.9, 1.0, &r, &g, &b);
            
            cairo_set_source_rgba(cr, r, g, b, fade * 0.8);
            cairo_arc(cr, px, py, 4 * scale, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
    
    // Sparkles around the parrot
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        if (vis->frequency_bands[i] > 0.5) {
            double angle = (double)i / VIS_FREQUENCY_BARS * 2.0 * M_PI;
            double sparkle_dist = 150 * scale + sin(vis->time_offset * 5.0 + i) * 30 * scale;
            double sx = cx + cos(angle) * sparkle_dist;
            double sy = cy + sin(angle) * sparkle_dist;
            
            double hue = (double)i / VIS_FREQUENCY_BARS;
            double r, g, b;
            hsv_to_rgb(hue * 0.8, 0.7, 1.0, &r, &g, &b);
            
            cairo_set_source_rgba(cr, r, g, b, vis->frequency_bands[i] * 0.6);
            cairo_arc(cr, sx, sy, 3 * scale, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
}

void draw_audio_bars_around_parrot(Visualizer *vis, cairo_t *cr, double cx, double cy, double scale) {
    double radius = 220 * scale;
    int num_bars = VIS_FREQUENCY_BARS;
    
    for (int i = 0; i < num_bars; i++) {
        double angle = (double)i / num_bars * 2.0 * M_PI - M_PI / 2;
        double bar_height = vis->frequency_bands[i] * 80 * scale;
        
        double x1 = cx + cos(angle) * radius;
        double y1 = cy + sin(angle) * radius;
        double x2 = cx + cos(angle) * (radius + bar_height);
        double y2 = cy + sin(angle) * (radius + bar_height);
        
        double hue = (double)i / num_bars;
        double r, g, b;
        hsv_to_rgb(hue * 0.8, 0.6, 0.8, &r, &g, &b);
        
        cairo_set_source_rgba(cr, r, g, b, 0.5);
        cairo_set_line_width(cr, 4 * scale);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }
}

void draw_parrot(Visualizer *vis, cairo_t *cr) {
    double bird_height = vis->height * 0.9;
    double scale = bird_height / 400.0;
    
    double cx = vis->width / 2.0;
    double cy = vis->height / 2.0 - 30 * scale + vis->parrot_state.body_bounce;
    
    // Draw glow effect
    if (vis->parrot_state.glow_intensity > 0.1) {
        cairo_pattern_t *glow = cairo_pattern_create_radial(
            cx, cy, 50 * scale,
            cx, cy, 200 * scale);
        
        double avg_freq = 0.0;
        for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
            avg_freq += vis->frequency_bands[i];
        }
        avg_freq /= VIS_FREQUENCY_BARS;
        
        double hue = fmod(vis->time_offset * 0.2, 1.0);
        double r, g, b;
        hsv_to_rgb(hue, 0.7, 1.0, &r, &g, &b);
        
        cairo_pattern_add_color_stop_rgba(glow, 0, r, g, b, vis->parrot_state.glow_intensity * 0.4);
        cairo_pattern_add_color_stop_rgba(glow, 1, r, g, b, 0);
        
        cairo_set_source(cr, glow);
        cairo_arc(cr, cx, cy, 200 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(glow);
    }
    
    // Tail feathers with sway
    for (int i = 0; i < 8; i++) {
        double y_offset = i * 12 * scale - 50 * scale;
        double sway_offset = sin(vis->time_offset * 2.0 + i * 0.5) * vis->parrot_state.tail_sway * scale;
        
        // Feather shimmer
        double shimmer = 0.6 + sin(vis->time_offset * 10.0 + i) * 0.1 * vis->frequency_bands[i % VIS_FREQUENCY_BARS];
        cairo_set_source_rgb(cr, shimmer + i * 0.05, 0.85 - i * 0.03, 0.2);
        
        cairo_move_to(cr, cx + 80 * scale, cy + y_offset);
        cairo_curve_to(cr,
            cx + 120 * scale + sway_offset, cy + y_offset + 10 * scale,
            cx + 160 * scale + sway_offset * 1.5, cy + y_offset + 30 * scale,
            cx + 180 * scale + sway_offset * 2, cy + y_offset + 60 * scale);
        cairo_line_to(cr, cx + 175 * scale + sway_offset * 2, cy + y_offset + 65 * scale);
        cairo_curve_to(cr,
            cx + 155 * scale + sway_offset * 1.5, cy + y_offset + 35 * scale,
            cx + 115 * scale + sway_offset, cy + y_offset + 15 * scale,
            cx + 80 * scale, cy + y_offset + 5 * scale);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
    
    // Main body
    cairo_set_source_rgb(cr, 0.1, 0.75, 0.2);
    cairo_arc(cr, cx + 30 * scale, cy, 90 * scale, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Wing with flapping animation
    cairo_save(cr);
    cairo_translate(cr, cx + 60 * scale, cy);
    cairo_rotate(cr, vis->parrot_state.wing_flap_angle * M_PI / 180.0);
    cairo_translate(cr, -(cx + 60 * scale), -cy);
    
    cairo_set_source_rgb(cr, 0.05, 0.6, 0.15);
    cairo_move_to(cr, cx + 40 * scale, cy - 40 * scale);
    cairo_curve_to(cr,
        cx + 90 * scale, cy - 20 * scale,
        cx + 100 * scale, cy + 30 * scale,
        cx + 80 * scale, cy + 70 * scale);
    cairo_curve_to(cr,
        cx + 60 * scale, cy + 60 * scale,
        cx + 40 * scale, cy + 30 * scale,
        cx + 40 * scale, cy - 40 * scale);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // Wing feather lines
    cairo_set_source_rgba(cr, 0.0, 0.4, 0.1, 0.6);
    cairo_set_line_width(cr, 3 * scale);
    for (int i = 0; i < 6; i++) {
        double start_y = cy - 30 * scale + i * 18 * scale;
        cairo_move_to(cr, cx + 45 * scale, start_y);
        cairo_line_to(cr, cx + 85 * scale, start_y + 10 * scale);
        cairo_stroke(cr);
    }
    
    cairo_restore(cr);
    
    // Chest accent with breathing
    cairo_set_source_rgb(cr, 0.95, 0.15, 0.55);
    cairo_save(cr);
    cairo_translate(cr, cx - 20 * scale, cy + 20 * scale);
    cairo_scale(cr, vis->parrot_state.chest_scale, vis->parrot_state.chest_scale);
    cairo_arc(cr, 0, 0, 50 * scale, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
    
    // Legs and feet with tapping
    cairo_set_source_rgb(cr, 1.0, 0.6, 0.0);
    cairo_set_line_width(cr, 6 * scale);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    
    // Left leg (tapping foot)
    double left_foot_offset = vis->parrot_state.foot_tap;
    cairo_move_to(cr, cx + 10 * scale, cy + 70 * scale);
    cairo_line_to(cr, cx + 5 * scale, cy + 110 * scale - left_foot_offset * scale);
    cairo_stroke(cr);
    
    // Left foot toes
    cairo_move_to(cr, cx + 5 * scale, cy + 110 * scale - left_foot_offset * scale);
    cairo_line_to(cr, cx - 5 * scale, cy + 120 * scale - left_foot_offset * scale);
    cairo_stroke(cr);
    cairo_move_to(cr, cx + 5 * scale, cy + 110 * scale - left_foot_offset * scale);
    cairo_line_to(cr, cx + 5 * scale, cy + 122 * scale - left_foot_offset * scale);
    cairo_stroke(cr);
    cairo_move_to(cr, cx + 5 * scale, cy + 110 * scale - left_foot_offset * scale);
    cairo_line_to(cr, cx + 15 * scale, cy + 120 * scale - left_foot_offset * scale);
    cairo_stroke(cr);
    
    // Right leg (tapping foot)
    double right_foot_offset = vis->parrot_state.right_foot_tap;
    cairo_move_to(cr, cx + 35 * scale, cy + 75 * scale);
    cairo_line_to(cr, cx + 35 * scale, cy + 115 * scale - right_foot_offset * scale);
    cairo_stroke(cr);
    
    // Right foot toes
    cairo_move_to(cr, cx + 35 * scale, cy + 115 * scale - right_foot_offset * scale);
    cairo_line_to(cr, cx + 25 * scale, cy + 125 * scale - right_foot_offset * scale);
    cairo_stroke(cr);
    cairo_move_to(cr, cx + 35 * scale, cy + 115 * scale - right_foot_offset * scale);
    cairo_line_to(cr, cx + 35 * scale, cy + 127 * scale - right_foot_offset * scale);
    cairo_stroke(cr);
    cairo_move_to(cr, cx + 35 * scale, cy + 115 * scale - right_foot_offset * scale);
    cairo_line_to(cr, cx + 45 * scale, cy + 125 * scale - right_foot_offset * scale);
    cairo_stroke(cr);
    
    // Blue head with bobbing
    double head_x = cx - 80 * scale;
    double head_y = cy - 60 * scale + vis->parrot_state.head_bob_offset * scale;
    
    cairo_set_source_rgb(cr, 0.0, 0.45, 1.0);
    cairo_arc(cr, head_x, head_y, 70 * scale, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // White eye ring
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_arc(cr, head_x, head_y - 10 * scale, 28 * scale, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Black pupil with tracking (or closed eye)
    if (vis->parrot_state.eye_closed) {
        // Draw closed eye as a horizontal line
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_line_width(cr, 4 * scale);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, head_x - 15 * scale, head_y - 10 * scale);
        cairo_line_to(cr, head_x + 15 * scale, head_y - 10 * scale);
        cairo_stroke(cr);
    } else {
        // Draw open eye with pupil tracking
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_arc(cr, 
            head_x - 5 * scale + vis->parrot_state.pupil_x * scale, 
            head_y - 10 * scale + vis->parrot_state.pupil_y * scale, 
            15 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Eye shine
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
        cairo_arc(cr, 
            head_x + 2 * scale + vis->parrot_state.pupil_x * scale, 
            head_y - 15 * scale + vis->parrot_state.pupil_y * scale, 
            6 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    
    // BEAK
    double mouth_gap = vis->parrot_state.mouth_open * 35 * scale;
    
    // Upper beak
    cairo_set_source_rgb(cr, 1.0, 0.55, 0.0);
    cairo_move_to(cr, head_x - 20 * scale, head_y + 15 * scale - mouth_gap * 0.3);
    cairo_curve_to(cr,
        head_x - 60 * scale, head_y + 10 * scale - mouth_gap * 0.2,
        head_x - 85 * scale, head_y + 25 * scale,
        head_x - 90 * scale, head_y + 50 * scale);
    cairo_curve_to(cr,
        head_x - 85 * scale, head_y + 60 * scale,
        head_x - 65 * scale, head_y + 55 * scale,
        head_x - 40 * scale, head_y + 40 * scale);
    cairo_curve_to(cr,
        head_x - 25 * scale, head_y + 30 * scale - mouth_gap * 0.2,
        head_x - 20 * scale, head_y + 20 * scale - mouth_gap * 0.25,
        head_x - 20 * scale, head_y + 15 * scale - mouth_gap * 0.3);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // Upper beak highlight
    cairo_set_source_rgba(cr, 1.0, 0.75, 0.2, 0.6);
    cairo_move_to(cr, head_x - 30 * scale, head_y + 20 * scale - mouth_gap * 0.3);
    cairo_curve_to(cr,
        head_x - 60 * scale, head_y + 17 * scale - mouth_gap * 0.2,
        head_x - 75 * scale, head_y + 30 * scale,
        head_x - 78 * scale, head_y + 48 * scale);
    cairo_line_to(cr, head_x - 70 * scale, head_y + 52 * scale);
    cairo_curve_to(cr,
        head_x - 55 * scale, head_y + 45 * scale,
        head_x - 35 * scale, head_y + 35 * scale - mouth_gap * 0.2,
        head_x - 30 * scale, head_y + 20 * scale - mouth_gap * 0.3);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // Lower beak
    cairo_set_source_rgb(cr, 0.9, 0.5, 0.0);
    cairo_move_to(cr, head_x - 20 * scale, head_y + 25 * scale + mouth_gap * 0.7);
    cairo_curve_to(cr,
        head_x - 55 * scale, head_y + 30 * scale + mouth_gap * 0.8,
        head_x - 80 * scale, head_y + 45 * scale + mouth_gap * 0.6,
        head_x - 88 * scale, head_y + 65 * scale + mouth_gap * 0.4);
    cairo_curve_to(cr,
        head_x - 83 * scale, head_y + 75 * scale + mouth_gap * 0.3,
        head_x - 65 * scale, head_y + 75 * scale + mouth_gap * 0.3,
        head_x - 45 * scale, head_y + 65 * scale + mouth_gap * 0.5);
    cairo_curve_to(cr,
        head_x - 28 * scale, head_y + 50 * scale + mouth_gap * 0.65,
        head_x - 22 * scale, head_y + 35 * scale + mouth_gap * 0.68,
        head_x - 20 * scale, head_y + 25 * scale + mouth_gap * 0.7);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // Mouth interior
    if (vis->parrot_state.mouth_open > 0.15) {
        cairo_set_source_rgba(cr, 0.1, 0.0, 0.0, vis->parrot_state.mouth_open * 0.85);
        cairo_move_to(cr, head_x - 20 * scale, head_y + 20 * scale);
        cairo_line_to(cr, head_x - 60 * scale, head_y + 35 * scale + mouth_gap * 0.3);
        cairo_line_to(cr, head_x - 75 * scale, head_y + 55 * scale + mouth_gap * 0.5);
        cairo_line_to(cr, head_x - 60 * scale, head_y + 65 * scale + mouth_gap * 0.5);
        cairo_line_to(cr, head_x - 25 * scale, head_y + 45 * scale + mouth_gap * 0.4);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
    
    // Tongue
    if (vis->parrot_state.mouth_open > 0.4) {
        cairo_set_source_rgba(cr, 0.9, 0.2, 0.2, vis->parrot_state.mouth_open * 0.7);
        cairo_arc(cr, head_x - 50 * scale, head_y + 50 * scale + mouth_gap * 0.4, 12 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    
    draw_music_notes(vis, cr, cx, cy, scale);
    draw_particles(vis, cr, cx, cy, scale);
    draw_audio_bars_around_parrot(vis, cr, cx, cy, scale);
}
