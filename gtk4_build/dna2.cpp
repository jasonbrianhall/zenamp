#ifndef DNAHELIX_H
#define DNAHELIX_H

#include "visualization.h"

void init_dna2_system(Visualizer *vis) {
    vis->dna_segment_count = MAX_DNA_SEGMENTS;
    vis->dna_rotation = 0.0;
    vis->dna_twist_speed = 0.02;
    vis->dna_spine_offset = 0.0;
    
    // Initialize base pulse arrays
    for (int i = 0; i < DNA_BASE_PAIRS; i++) {
        vis->dna_base_pulse[i] = 0.0;
    }
    
    // Initialize DNA segments
    for (int i = 0; i < MAX_DNA_SEGMENTS; i++) {
        DNASegment *seg = &vis->dna_segments[i];
        
        // Position along the helix spine
        double spine_pos = (double)i / MAX_DNA_SEGMENTS;
        seg->y = spine_pos;
        seg->z = 0.0; // Will be calculated during drawing
        
        // Assign base pair type cyclically with some randomness
        seg->base_type = i % DNA_BASE_PAIRS;
        
        seg->intensity = 0.0;
        seg->connection_strength = 1.0;
        seg->twist_offset = (double)i / MAX_DNA_SEGMENTS * 4.0 * M_PI; // 2 full twists
        seg->active = TRUE;
    }
}

void update_dna2_helix(Visualizer *vis, double dt) {
    // Update rotation
    vis->dna_rotation += vis->dna_twist_speed;
    if (vis->dna_rotation > 2.0 * M_PI) {
        vis->dna_rotation -= 2.0 * M_PI;
    }
    
    // Update spine offset for flowing effect
    vis->dna_spine_offset += dt * 0.5;
    if (vis->dna_spine_offset > 1.0) {
        vis->dna_spine_offset -= 1.0;
    }
    
    // Map frequency bands to base types and update pulses
    int bands_per_base = VIS_FREQUENCY_BARS / DNA_BASE_PAIRS;
    
    for (int base = 0; base < DNA_BASE_PAIRS; base++) {
        double total_intensity = 0.0;
        
        // Sum up frequency bands for this base type
        for (int band = 0; band < bands_per_base; band++) {
            int freq_index = base * bands_per_base + band;
            if (freq_index < VIS_FREQUENCY_BARS) {
                total_intensity += vis->frequency_bands[freq_index];
            }
        }
        
        total_intensity /= bands_per_base;
        
        // Smooth the pulse with decay
        vis->dna_base_pulse[base] = fmax(total_intensity, vis->dna_base_pulse[base] * 0.95);
    }
    
    // Update individual segments
    for (int i = 0; i < vis->dna_segment_count; i++) {
        DNASegment *seg = &vis->dna_segments[i];
        
        // Update intensity based on base type
        int base_type = (int)seg->base_type;
        seg->intensity = vis->dna_base_pulse[base_type];
        
        // Update connection strength based on overall volume
        seg->connection_strength = 0.5 + vis->volume_level * 1.5;
        if (seg->connection_strength > 2.0) seg->connection_strength = 2.0;
        
        // Calculate 3D position
        double total_twist = seg->twist_offset + vis->dna_rotation;
        double spine_y = seg->y + vis->dna_spine_offset;
        
        // Wrap around vertically
        if (spine_y > 1.0) spine_y -= 1.0;
        
        seg->y = spine_y;
        seg->z = sin(total_twist) * 0.3; // Depth for 3D effect
    }
}

void draw_dna2_helix(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double center_x = vis->width / 2.0;
    double helix_width = vis->width * 0.6;
    double helix_radius = helix_width / 4.0;
    
    // Draw background spiral guides (faint)
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 1.0);
    
    // Draw two spiral backbones
    for (int spine = 0; spine < 2; spine++) {
        cairo_move_to(cr, center_x, 0);
        
        for (int i = 0; i < MAX_DNA_SEGMENTS; i++) {
            double y = (double)i / MAX_DNA_SEGMENTS * vis->height;
            double twist = (double)i / MAX_DNA_SEGMENTS * 4.0 * M_PI + vis->dna_rotation;
            
            if (spine == 1) twist += M_PI; // Opposite side
            
            double x = center_x + cos(twist) * helix_radius;
            
            if (i == 0) {
                cairo_move_to(cr, x, y);
            } else {
                cairo_line_to(cr, x, y);
            }
        }
        cairo_stroke(cr);
    }
    
    // Draw DNA segments (base pairs)
    for (int i = 0; i < vis->dna_segment_count; i++) {
        DNASegment *seg = &vis->dna_segments[i];
        if (!seg->active) continue;
        
        double y = seg->y * vis->height;
        if (y < 0 || y > vis->height) continue;
        
        double twist = seg->twist_offset + vis->dna_rotation;
        
        // Calculate positions for both sides of the helix
        double x1 = center_x + cos(twist) * helix_radius;
        double x2 = center_x + cos(twist + M_PI) * helix_radius;
        
        // Scale based on depth (z-position) for 3D effect
        double depth_scale = 0.7 + seg->z * 0.3;
        double line_width = 2.0 * depth_scale * seg->connection_strength;
        
        // Get base pair colors
        double r1, g1, b1, r2, g2, b2;
        int base_type = (int)seg->base_type;
        get_base_color(base_type, seg->intensity, &r1, &g1, &b1);
        get_base_color((base_type + 2) % DNA_BASE_PAIRS, seg->intensity, &r2, &g2, &b2); // Complementary base
        
        // Draw connection line between base pairs
        cairo_set_line_width(cr, line_width);
        
        // Create gradient for the connection
        cairo_pattern_t *gradient = cairo_pattern_create_linear(x1, y, x2, y);
        cairo_pattern_add_color_stop_rgba(gradient, 0.0, r1, g1, b1, 0.8 * depth_scale);
        cairo_pattern_add_color_stop_rgba(gradient, 0.3, r1, g1, b1, 0.9 * depth_scale);
        cairo_pattern_add_color_stop_rgba(gradient, 0.7, r2, g2, b2, 0.9 * depth_scale);
        cairo_pattern_add_color_stop_rgba(gradient, 1.0, r2, g2, b2, 0.8 * depth_scale);
        
        cairo_set_source(cr, gradient);
        cairo_move_to(cr, x1, y);
        cairo_line_to(cr, x2, y);
        cairo_stroke(cr);
        cairo_pattern_destroy(gradient);
        
        // Draw base pair "atoms" as circles
        double atom_radius = 3.0 * depth_scale * (0.8 + seg->intensity * 0.4);
        
        // Left base
        cairo_set_source_rgba(cr, r1, g1, b1, 0.9 * depth_scale);
        cairo_arc(cr, x1, y, atom_radius, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Right base
        cairo_set_source_rgba(cr, r2, g2, b2, 0.9 * depth_scale);
        cairo_arc(cr, x2, y, atom_radius, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Add glow effect for high intensity
        if (seg->intensity > 0.7) {
            double glow_radius = atom_radius * 2.0;
            double glow_alpha = (seg->intensity - 0.7) * 0.3;
            
            cairo_set_source_rgba(cr, r1, g1, b1, glow_alpha * depth_scale);
            cairo_arc(cr, x1, y, glow_radius, 0, 2 * M_PI);
            cairo_fill(cr);
            
            cairo_set_source_rgba(cr, r2, g2, b2, glow_alpha * depth_scale);
            cairo_arc(cr, x2, y, glow_radius, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
    
    // Draw intensity indicators for each base type
    double indicator_size = 20.0;
    double indicator_y = vis->height - 40.0;
    
    for (int base = 0; base < DNA_BASE_PAIRS; base++) {
        double x = 20.0 + base * (indicator_size + 10.0);
        double intensity = vis->dna_base_pulse[base];
        
        double r, g, b;
        get_base_color(base, intensity, &r, &g, &b);
        
        // Background circle
        cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.8);
        cairo_arc(cr, x, indicator_y, indicator_size / 2, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Intensity fill
        if (intensity > 0.01) {
            cairo_set_source_rgba(cr, r, g, b, 0.9);
            cairo_arc(cr, x, indicator_y, indicator_size / 2 * intensity, 0, 2 * M_PI);
            cairo_fill(cr);
        }
        
        // Base letter labels
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 10);
        
        const char* base_labels[] = {"A", "T", "G", "C"};
        cairo_text_extents_t extents;
        cairo_text_extents(cr, base_labels[base], &extents);
        cairo_move_to(cr, x - extents.width / 2, indicator_y + extents.height / 2);
        cairo_show_text(cr, base_labels[base]);
    }
}

void get_base_color(int base_type, double intensity, double *r, double *g, double *b) {
    // DNA base colors: A=Red, T=Blue, G=Green, C=Yellow
    double base_colors[DNA_BASE_PAIRS][3] = {
        {1.0, 0.2, 0.2}, // A - Red
        {0.2, 0.4, 1.0}, // T - Blue  
        {0.2, 1.0, 0.3}, // G - Green
        {1.0, 0.8, 0.2}  // C - Yellow
    };
    
    double brightness = 0.3 + intensity * 0.7; // Base brightness + intensity boost
    
    *r = base_colors[base_type][0] * brightness;
    *g = base_colors[base_type][1] * brightness;
    *b = base_colors[base_type][2] * brightness;
}

#endif

