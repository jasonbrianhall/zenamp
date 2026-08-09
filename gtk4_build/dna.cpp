#ifndef DNAHELIX_H
#define DNAHELIX_H

#include "visualization.h"

void init_dna_system(Visualizer *vis) {
    DNAHelix *dna = &vis->dna_helix;
    
    // Initialize helix parameters
    dna->amplitude = 40.0;           // Base amplitude
    dna->frequency = 4.0;            // Number of twists across the screen
    dna->phase_offset = M_PI;        // 180 degrees apart (opposite phases)
    dna->flow_speed = 50.0;          // Pixels per second flow
    
    // Set strand colors - using complementary colors
    // Strand 1: Cyan/Blue
    dna->strand_colors[0][0] = 0.2; // R
    dna->strand_colors[0][1] = 0.8; // G  
    dna->strand_colors[0][2] = 1.0; // B
    
    // Strand 2: Orange/Red
    dna->strand_colors[1][0] = 1.0; // R
    dna->strand_colors[1][1] = 0.5; // G
    dna->strand_colors[1][2] = 0.1; // B
    
    vis->dna_time_offset = 0.0;
    vis->dna_amplitude_multiplier = 1.0;
    vis->dna_twist_rate = 1.0;
}

void update_dna_helix(Visualizer *vis, double dt) {
    DNAHelix *dna = &vis->dna_helix;
    
    // Update time for flowing animation
    vis->dna_time_offset += dt * dna->flow_speed;
    
    // Map audio to helix parameters
    // Use low frequencies for amplitude (bass makes it bigger)
    double low_freq_sum = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS / 4; i++) {
        low_freq_sum += vis->frequency_bands[i];
    }
    low_freq_sum /= (VIS_FREQUENCY_BARS / 4);
    vis->dna_amplitude_multiplier = 0.5 + low_freq_sum * 2.0;
    
    // Use mid-high frequencies for twist rate
    double mid_freq_sum = 0.0;
    for (int i = VIS_FREQUENCY_BARS / 4; i < 3 * VIS_FREQUENCY_BARS / 4; i++) {
        mid_freq_sum += vis->frequency_bands[i];
    }
    mid_freq_sum /= (VIS_FREQUENCY_BARS / 2);
    vis->dna_twist_rate = 0.5 + mid_freq_sum * 2.0;
    
    // Update flow speed based on overall volume
    dna->flow_speed = 30.0 + vis->volume_level * 100.0;
}

void draw_dna_helix(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    DNAHelix *dna = &vis->dna_helix;
    double center_y = vis->height / 2.0;
    
    // Draw each DNA strand
    for (int strand = 0; strand < DNA_STRANDS; strand++) {
        // Set up for smooth curves
        cairo_set_line_width(cr, 3.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        
        // Calculate points for this strand
        double points_x[DNA_POINTS];
        double points_y[DNA_POINTS];
        double intensities[DNA_POINTS];
        
        for (int i = 0; i < DNA_POINTS; i++) {
            double x = (double)i * vis->width / (DNA_POINTS - 1);
            double wave_x = x + vis->dna_time_offset;
            
            // Calculate the helical wave
            double phase = strand * dna->phase_offset; // Phase difference between strands
            double wave_phase = (wave_x / vis->width) * dna->frequency * 2.0 * M_PI * vis->dna_twist_rate + phase;
            
            double amplitude = dna->amplitude * vis->dna_amplitude_multiplier;
            double y = center_y + sin(wave_phase) * amplitude;
            
            points_x[i] = x;
            points_y[i] = y;
            
            // Calculate intensity based on local frequency data
            int freq_index = (i * VIS_FREQUENCY_BARS) / DNA_POINTS;
            if (freq_index >= VIS_FREQUENCY_BARS) freq_index = VIS_FREQUENCY_BARS - 1;
            intensities[i] = vis->frequency_bands[freq_index];
        }
        
        // Draw the strand with varying intensity
        for (int i = 0; i < DNA_POINTS - 1; i++) {
            double intensity = intensities[i];
            
            // Color intensity based on audio
            double r = dna->strand_colors[strand][0] * (0.3 + intensity * 0.7);
            double g = dna->strand_colors[strand][1] * (0.3 + intensity * 0.7);
            double b = dna->strand_colors[strand][2] * (0.3 + intensity * 0.7);
            double alpha = 0.6 + intensity * 0.4;
            
            cairo_set_source_rgba(cr, r, g, b, alpha);
            
            // Draw line segment
            cairo_move_to(cr, points_x[i], points_y[i]);
            cairo_line_to(cr, points_x[i + 1], points_y[i + 1]);
            cairo_stroke(cr);
            
            // Add glow effect for high intensity
            if (intensity > 0.6) {
                cairo_set_line_width(cr, 6.0);
                cairo_set_source_rgba(cr, r, g, b, (intensity - 0.6) * 0.3);
                cairo_move_to(cr, points_x[i], points_y[i]);
                cairo_line_to(cr, points_x[i + 1], points_y[i + 1]);
                cairo_stroke(cr);
                cairo_set_line_width(cr, 3.0); // Reset line width
            }
        }
        
        // Draw strand particles/nodes at peak points
        for (int i = 0; i < DNA_POINTS; i += 8) { // Every 8th point
            double intensity = intensities[i];
            if (intensity > 0.4) {
                double r = dna->strand_colors[strand][0];
                double g = dna->strand_colors[strand][1];
                double b = dna->strand_colors[strand][2];
                
                cairo_set_source_rgba(cr, r, g, b, intensity);
                
                double radius = 2.0 + intensity * 4.0;
                cairo_arc(cr, points_x[i], points_y[i], radius, 0, 2 * M_PI);
                cairo_fill(cr);
                
                // Inner bright core
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, intensity * 0.5);
                cairo_arc(cr, points_x[i], points_y[i], radius * 0.4, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }
    }
    
    // Draw center reference line (subtle)
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, center_y);
    cairo_line_to(cr, vis->width, center_y);
    cairo_stroke(cr);
    
    // Draw frequency spectrum indicator at bottom
    double spectrum_height = 30.0;
    double spectrum_y = vis->height - spectrum_height - 10;
    double bar_width = vis->width / (double)VIS_FREQUENCY_BARS;
    
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        double height = vis->frequency_bands[i] * spectrum_height;
        double x = i * bar_width;
        
        // Color based on frequency range
        double hue = (double)i / VIS_FREQUENCY_BARS;
        double r = 0.5 + 0.5 * sin(hue * 2 * M_PI);
        double g = 0.5 + 0.5 * sin(hue * 2 * M_PI + 2.0);
        double b = 0.5 + 0.5 * sin(hue * 2 * M_PI + 4.0);
        
        cairo_set_source_rgba(cr, r, g, b, 0.6);
        cairo_rectangle(cr, x, spectrum_y + spectrum_height - height, bar_width - 1, height);
        cairo_fill(cr);
    }
}

#endif

