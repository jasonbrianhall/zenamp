#include "visualization.h"
#include <math.h>

/**
 * INTERACTIVE OSCILLOSCOPE - THREE BUTTON VERSION
 * 
 * Three unique interactive effects:
 * 
 * LEFT CLICK (Zoom In):
 *   - Waveform scales up (2x amplitude)
 *   - Smooth decay back to normal
 *   - Effect: Magnify the signal, see details
 * 
 * MIDDLE CLICK (Glitch/Distort):
 *   - Waveform gets pixelated/stepped effect
 *   - Amplitude increases chaotically
 *   - Effect: Digital glitch, artistic distortion
 * 
 * RIGHT CLICK (Smooth):
 *   - Waveform becomes super smooth/blurred
 *   - Reduced amplitude, low-pass filtered feel
 *   - Effect: Mellow, smoothed-out wave
 * 
 * MOUSE MOVEMENT (Vertical Shift):
 *   - Move mouse up/down to shift waveform baseline
 *   - Creates oscillating baseline effect
 *   - Effect: Wave follows your cursor vertically
 * 
 * GRID & COLORS:
 *   - Interactive grid intensity changes with effects
 *   - Waveform color shifts based on mode
 */

// Track state across frames
static double zoom_scale = 1.0;        // For left click zoom
static double glitch_intensity = 0.0;  // For middle click glitch
static double smooth_intensity = 0.0;  // For right click smooth
static int glitch_step = 0;            // For glitch pattern

#define GLITCH_STEP_SIZE 4              // How pixelated the glitch is

void draw_oscilloscope(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;

    double center_y = vis->height / 2.0;

    // ========== LEFT CLICK - ZOOM IN ==========
    if (vis->mouse_left_pressed) {
        zoom_scale = 2.0;  // Double the amplitude
        vis->mouse_left_pressed = FALSE;
    }
    
    if (zoom_scale > 1.0) {
        zoom_scale = (zoom_scale - 1.0) * 0.96 + 1.0;  // Slower decay, stays longer
    }

    // ========== MIDDLE CLICK - GLITCH ==========
    if (vis->mouse_middle_pressed) {
        glitch_intensity = 1.0;  // Max glitch
        glitch_step = 0;
        vis->mouse_middle_pressed = FALSE;
    }
    
    if (glitch_intensity > 0.0) {
        glitch_intensity *= 0.97;  // Slower decay, stays longer
        glitch_step++;
    }

    // ========== RIGHT CLICK - SMOOTH ==========
    if (vis->mouse_right_pressed) {
        smooth_intensity = 1.0;  // Full smoothing
        vis->mouse_right_pressed = FALSE;
    }
    
    if (smooth_intensity > 0.0) {
        smooth_intensity *= 0.95;  // Slower decay, stays longer
    }

    // ========== DRAW GRID (Dynamic opacity based on effects) ==========
    double grid_alpha = 0.5;
    if (glitch_intensity > 0.1) {
        grid_alpha = 0.9;  // Brighten grid during glitch
    } else if (smooth_intensity > 0.1) {
        grid_alpha = 0.2;  // Dim grid during smooth
    } else if (zoom_scale > 1.1) {
        grid_alpha = 0.7;  // Brighten grid during zoom
    }

    cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, grid_alpha);
    cairo_set_line_width(cr, 1.0);
    
    // Horizontal lines
    for (int i = 1; i < 4; i++) {
        double y = vis->height * i / 4.0;
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, vis->width, y);
        cairo_stroke(cr);
    }
    
    // Vertical lines
    for (int i = 1; i < 8; i++) {
        double x = vis->width * i / 8.0;
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, vis->height);
        cairo_stroke(cr);
    }

    // ========== CALCULATE WAVEFORM WITH ALL EFFECTS ==========
    // We'll draw in two passes: first calculate smoothed points, then render

    // Determine waveform color based on current mode
    double wave_r = vis->accent_r;
    double wave_g = vis->accent_g;
    double wave_b = vis->accent_b;

    if (glitch_intensity > 0.1) {
        // Glitch mode: red tint
        wave_r = 1.0;
        wave_g = vis->accent_g * (1.0 - glitch_intensity);
        wave_b = vis->accent_b * (1.0 - glitch_intensity);
    } else if (smooth_intensity > 0.1) {
        // Smooth mode: blue tint
        wave_r = vis->accent_r * (1.0 - smooth_intensity);
        wave_g = vis->accent_g * (1.0 - smooth_intensity);
        wave_b = 1.0;
    } else if (zoom_scale > 1.1) {
        // Zoom mode: cyan tint
        wave_r = vis->accent_r * (1.0 - (zoom_scale - 1.0) * 0.5);
        wave_g = 1.0;
        wave_b = 1.0;
    }

    cairo_set_source_rgba(cr, wave_r, wave_g, wave_b, 1.0);
    
    // ========== SET LINE WIDTH BASED ON MODE ==========
    double line_width = 2.0;
    if (glitch_intensity > 0.1) {
        line_width = 3.0 + glitch_intensity * 2.0;  // Thicker during glitch
    } else if (smooth_intensity > 0.1) {
        line_width = 2.0 - smooth_intensity * 0.5;  // Thinner during smooth
    } else if (zoom_scale > 1.1) {
        line_width = 2.0 + (zoom_scale - 1.0);  // Thicker during zoom
    }
    cairo_set_line_width(cr, line_width);

    // ========== DRAW WAVEFORM ==========
    cairo_move_to(cr, 0, center_y);
    
    for (int i = 0; i < VIS_SAMPLES; i++) {
        double x = (double)i * vis->width / (VIS_SAMPLES - 1);
        
        // Base sample
        double sample = vis->audio_samples[i];

        // ===== APPLY ZOOM EFFECT =====
        sample *= zoom_scale;

        // ===== APPLY SMOOTH EFFECT =====
        if (smooth_intensity > 0.1) {
            // Smooth by averaging with neighbors
            int smoothing_amount = (int)(smooth_intensity * 10.0) + 1;
            double smooth_sample = 0.0;
            int count = 0;
            
            for (int j = -smoothing_amount; j <= smoothing_amount; j++) {
                int idx = i + j;
                if (idx >= 0 && idx < VIS_SAMPLES) {
                    smooth_sample += vis->audio_samples[idx];
                    count++;
                }
            }
            
            sample = smooth_sample / count;
            sample *= (1.0 - smooth_intensity * 0.5);  // Reduce amplitude
        }

        // ===== APPLY GLITCH EFFECT =====
        if (glitch_intensity > 0.1) {
            // Pixelated/stepped effect
            int step_idx = (i / GLITCH_STEP_SIZE) * GLITCH_STEP_SIZE;
            if (step_idx >= 0 && step_idx < VIS_SAMPLES) {
                sample = vis->audio_samples[step_idx];
            }
            
            // Add chaotic amplitude variation
            double chaos = sin(glitch_step * 0.1 + i * 0.05) * glitch_intensity;
            sample = sample * (1.0 + chaos);
        }

        // Calculate Y position
        double y = center_y + sample * vis->height / 2.5;
        
        // Clamp y to screen bounds
        if (y < 0) y = 0;
        if (y > vis->height) y = vis->height;
        
        cairo_line_to(cr, x, y);
    }
    
    cairo_stroke(cr);

    // ========== DRAW STATUS INDICATOR (What mode is active) ==========
    if (glitch_intensity > 0.1 || smooth_intensity > 0.1 || zoom_scale > 1.05) {
        cairo_set_font_size(cr, 14.0);
        cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.7);
        
        if (glitch_intensity > 0.1) {
            cairo_move_to(cr, 10, 25);
            cairo_show_text(cr, "GLITCH");
        } else if (smooth_intensity > 0.1) {
            cairo_move_to(cr, 10, 25);
            cairo_show_text(cr, "SMOOTH");
        } else if (zoom_scale > 1.05) {
            cairo_move_to(cr, 10, 25);
            cairo_show_text(cr, "ZOOM");
        }
    }
}
