#ifndef MATRIX_H
#define MATRIX_H
 
#include "visualization.h"

// ============================================================================
// Local Static Data - No Visualizer Struct Modifications Needed
// ============================================================================
#define MAX_TRAIL_PARTICLES 200
#define MAX_INTERACTION_POINTS 3

typedef struct {
    double x, y;
    double vx, vy;
    double lifetime;
    double max_lifetime;
    double size;
    double r, g, b;
    double alpha;
} MatrixTrailParticle;

typedef struct {
    double x, y;
    double influence_radius;
    gboolean active;
    double intensity;
} InteractionPoint;

// Static local arrays - no dynamic allocation, no Visualizer modifications
static MatrixTrailParticle trail_particles[MAX_TRAIL_PARTICLES];
static int trail_particle_count = 0;
static InteractionPoint interaction_points[MAX_INTERACTION_POINTS];

// Enhanced ASCII character sets for more variety
const char* get_random_matrix_char(void) {
    static const char* matrix_chars[] = {
        // Numbers (weighted more heavily for that digital feel)
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", // Doubled for higher frequency
        // Uppercase letters
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
        "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
        "U", "V", "W", "X", "Y", "Z",
        // Lowercase letters
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
        "k", "l", "m", "n", "o", "p", "q", "r", "s", "t",
        "u", "v", "w", "x", "y", "z",
        // Special symbols and tech characters
        "!", "@", "#", "$", "%", "^", "&", "*", "(", ")",
        "[", "]", "{", "}", "|", "\\", "/", "?", "<", ">",
        "=", "+", "-", "_", "~", "`", ":", ";", ".", ",",
        // Extra digital/matrix-style characters
        "§", "±", "°", "µ", "π", "Σ", "Ω", "∞", "≈", "≠",
        "≤", "≥", "÷", "×", "√", "∫", "∆", "∇", "∂", "∋"
    };
    
    int num_chars = sizeof(matrix_chars) / sizeof(matrix_chars[0]);
    int index = rand() % num_chars;
    return matrix_chars[index];
}

// Get special "power" characters for intense moments
static const char* get_power_matrix_char(void) {
    static const char* power_chars[] = {
        "★", "✦", "◆", "◇", "◈", "◉", "◎", "◍", "◎", "◆",
        "◑", "◒", "◓", "▲", "△", "▼", "▽", "◄", "►", "♦",
        "♠", "♣", "♥", "♪", "♫", "☆", "✦", "✧", "✩", "✪"
    };
    
    int num_chars = sizeof(power_chars) / sizeof(power_chars[0]);
    int index = rand() % num_chars;
    return power_chars[index];
}

// ============================================================================
// Spawn Trail Particles from Character Positions
// ============================================================================
static void spawn_matrix_trail_particle(double x, double y, double intensity, 
                                        double r, double g, double b) {
    if (trail_particle_count >= MAX_TRAIL_PARTICLES) return;
    
    MatrixTrailParticle *p = &trail_particles[trail_particle_count];
    
    p->x = x + (rand() % 20 - 10);
    p->y = y + (rand() % 20 - 10);
    p->vx = (rand() % 200 - 100) / 100.0;
    p->vy = (rand() % 200 - 100) / 100.0 - 50; // Bias upward
    p->lifetime = 0.5 + (rand() / (double)RAND_MAX) * 0.5;
    p->max_lifetime = p->lifetime;
    p->size = 1.0 + (rand() / (double)RAND_MAX) * 2.0;
    p->r = r;
    p->g = g;
    p->b = b;
    p->alpha = intensity;
    
    trail_particle_count++;
}

// ============================================================================
// Update Trail Particles
// ============================================================================
static void update_matrix_trail_particles(double dt) {
    for (int i = 0; i < trail_particle_count; i++) {
        MatrixTrailParticle *p = &trail_particles[i];
        
        // Physics
        p->vx *= 0.98; // Air resistance
        p->vy += 50 * dt; // Gravity
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        
        // Lifetime
        p->lifetime -= dt;
        if (p->lifetime <= 0) {
            // Remove particle
            trail_particles[i] = trail_particles[trail_particle_count - 1];
            trail_particle_count--;
            i--;
            continue;
        }
        
        // Fade
        p->alpha = (p->lifetime / p->max_lifetime) * p->alpha;
        p->size *= 0.99;
    }
}

// ============================================================================
// Mouse-Based Interaction
// ============================================================================
static void update_matrix_interactions(Visualizer *vis) {
    // Fixed repel radius (not based on sensitivity)
    double radius = 150.0;
    
    // Update interaction point from current mouse position
    if (vis->mouse_over) {
        // Use the first slot for continuous mouse tracking (for repel)
        interaction_points[0].x = vis->mouse_x;
        interaction_points[0].y = vis->mouse_y;
        interaction_points[0].influence_radius = radius;
        interaction_points[0].intensity = 0.8;  // Fixed intensity
        interaction_points[0].active = TRUE;
        
        // Different effects based on which button is pressed
        if (vis->mouse_left_pressed) {
            // Left click: Green burst (normal matrix particles)
            for (int i = 0; i < 15; i++) {
                spawn_matrix_trail_particle(vis->mouse_x, vis->mouse_y, 1.0, 0.0, 1.0, 0.0);
            }
        } else if (vis->mouse_middle_pressed) {
            // Middle click: Blue burst (different color)
            for (int i = 0; i < 20; i++) {
                spawn_matrix_trail_particle(vis->mouse_x, vis->mouse_y, 1.0, 0.0, 0.5, 1.0);
            }
        } else if (vis->mouse_right_pressed) {
            // Right click: Red burst (intensity effect)
            for (int i = 0; i < 25; i++) {
                spawn_matrix_trail_particle(vis->mouse_x, vis->mouse_y, 1.5, 1.0, 0.0, 0.0);
            }
        }
    } else {
        // Fade out when mouse leaves
        interaction_points[0].intensity *= 0.9;
        if (interaction_points[0].intensity < 0.05) {
            interaction_points[0].active = FALSE;
        }
    }
}

// ============================================================================
// Apply Interaction Distortion to Columns
// ============================================================================
static void apply_interaction_to_column(MatrixColumn *col) {
    // Check distance to each interaction point
    for (int i = 0; i < MAX_INTERACTION_POINTS; i++) {
        if (!interaction_points[i].active) continue;
        
        InteractionPoint *ip = &interaction_points[i];
        double dx = col->x - ip->x;
        double dy = col->y - ip->y;
        double dist = sqrt(dx*dx + dy*dy);
        
        if (dist < ip->influence_radius) {
            // Influence intensity decreases with distance
            double influence = (1.0 - dist / ip->influence_radius) * ip->intensity;
            
            // Repel the column - fixed repulsion force
            if (dist > 0.1) {
                double angle = atan2(dy, dx);
                col->x += cos(angle) * influence * 2.0;
                col->y += sin(angle) * influence * 2.0;
            }
            
            // Increase character morphing near interaction
            col->intensity = fmin(1.0, col->intensity + influence * 0.5);
        }
    }
}

void init_matrix_system(Visualizer *vis) {
    vis->matrix_column_count = 0;
    vis->matrix_spawn_timer = 0.0;
    vis->matrix_char_size = 12;
    
    // Initialize all columns as inactive
    for (int i = 0; i < MAX_MATRIX_COLUMNS; i++) {
        vis->matrix_columns[i].active = FALSE;
        vis->matrix_columns[i].x = 0;
        vis->matrix_columns[i].y = 0;
        vis->matrix_columns[i].speed = 0;
        vis->matrix_columns[i].length = 0;
        vis->matrix_columns[i].intensity = 0;
        vis->matrix_columns[i].frequency_band = 0;
    }
    
    // Initialize local static arrays (one-time only)
    static gboolean initialized = FALSE;
    if (!initialized) {
        trail_particle_count = 0;
        for (int i = 0; i < MAX_INTERACTION_POINTS; i++) {
            interaction_points[i].active = FALSE;
        }
        initialized = TRUE;
    }
}

void create_matrix_column_at_position(Visualizer *vis, int x_position) {
    if (vis->matrix_column_count >= MAX_MATRIX_COLUMNS) return;
    
    // Find inactive column slot
    int slot = -1;
    for (int i = 0; i < MAX_MATRIX_COLUMNS; i++) {
        if (!vis->matrix_columns[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return;
    
    MatrixColumn *col = &vis->matrix_columns[slot];
    
    // Set the exact X position provided
    col->x = x_position;
    
    // Make sure we don't go off screen
    if (col->x < 0) col->x = 0;
    if (col->x > vis->width - vis->matrix_char_size) {
        col->x = vis->width - vis->matrix_char_size;
    }
    
    // Start above screen
    col->y = -vis->matrix_char_size * (1 + rand() % 3);
    
    // Random properties - speed affected by volume level for beat synchronization
    double beat_speed_boost = 1.0 + (vis->volume_level * 2.0);
    col->speed = (50 + (rand() % 150)) * beat_speed_boost;
    col->intensity = 0.4 + (rand() / (double)RAND_MAX) * 0.6;
    col->length = 8 + (rand() % 18);
    if (col->length > MAX_CHARS_PER_COLUMN) col->length = MAX_CHARS_PER_COLUMN;
    
    // Random frequency band for audio reactivity
    col->frequency_band = rand() % VIS_FREQUENCY_BARS;
    
    // Generate characters
    for (int i = 0; i < col->length; i++) {
        col->chars[i] = get_random_matrix_char();
        double position_factor = 1.0 - (double)i / col->length;
        col->char_ages[i] = position_factor * position_factor;
    }
    
    col->active = TRUE;
    vis->matrix_column_count++;
}

void update_matrix(Visualizer *vis, double dt) {
    vis->matrix_spawn_timer += dt;
    
    // Update interactions
    update_matrix_interactions(vis);
    
    // Update trail particles
    update_matrix_trail_particles(dt);
    
    // Calculate audio energy
    double total_energy = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        total_energy += vis->frequency_bands[i];
    }
    total_energy /= VIS_FREQUENCY_BARS;
    
    // SYSTEMATIC spawning across the screen width
    static int screen_section = 0;
    const int NUM_SECTIONS = 8; // Divide screen into sections for even distribution
    
    // Detect beat - significant increase in audio energy
    static double last_total_energy = 0.0;
    gboolean is_beat = (total_energy > last_total_energy * 1.5) && (total_energy > 0.3);
    last_total_energy = total_energy * 0.9 + last_total_energy * 0.1; // Smooth average
    
    // Base spawn interval, but faster on beats
    double spawn_interval = 0.15;
    if (is_beat) {
        spawn_interval = 0.05; // Much faster spawn rate on beat
    }
    
    if (vis->matrix_spawn_timer > spawn_interval) {
        
        // More columns spawn on beats
        int columns_to_spawn = is_beat ? (4 + rand() % 4) : (2 + (rand() % 3));
        
        for (int spawn = 0; spawn < columns_to_spawn; spawn++) {
            // Cycle through screen sections to ensure even distribution
            screen_section = (screen_section + 1) % NUM_SECTIONS;
            
            // Calculate X position in this section
            int section_width = vis->width / NUM_SECTIONS;
            int section_start = screen_section * section_width;
            int x_pos = section_start + (rand() % section_width);
            
            create_matrix_column_at_position(vis, x_pos);
        }
        
        // Additional random spawning for variety
        for (int random_spawn = 0; random_spawn < 2; random_spawn++) {
            int random_x = rand() % vis->width;
            create_matrix_column_at_position(vis, random_x);
        }
        
        vis->matrix_spawn_timer = 0.0;
    }
    
    // Audio-reactive burst spawning
    if (total_energy > 0.3) {
        int burst_columns = (int)(total_energy * 5);
        for (int burst = 0; burst < burst_columns; burst++) {
            int random_x = rand() % vis->width;
            create_matrix_column_at_position(vis, random_x);
        }
    }
    
    // Update existing columns
    for (int i = 0; i < MAX_MATRIX_COLUMNS; i++) {
        if (!vis->matrix_columns[i].active) continue;
        
        MatrixColumn *col = &vis->matrix_columns[i];
        
        // Apply mouse interactions
        apply_interaction_to_column(col);
        
        // Update position
        col->y += col->speed * dt;
        
        // Update intensity based on audio
        double current_audio = vis->frequency_bands[col->frequency_band];
        col->intensity = fmax(col->intensity * 0.98, 0.3 + current_audio * 0.7);
        
        // Character morphing
        if (rand() / (double)RAND_MAX < dt * 4.0) {
            int char_to_change = rand() % col->length;
            if (current_audio > 0.6 && char_to_change < 3 && (rand() % 3 == 0)) {
                col->chars[char_to_change] = get_power_matrix_char();
            } else {
                col->chars[char_to_change] = get_random_matrix_char();
            }
        }
        
        // Age characters
        for (int j = 0; j < col->length; j++) {
            double fade_rate = (j < 3) ? 0.4 : 0.8;
            col->char_ages[j] -= dt * fade_rate;
            if (col->char_ages[j] < 0) col->char_ages[j] = 0;
        }
        
        // Remove when off screen
        if (col->y > vis->height + col->length * vis->matrix_char_size) {
            col->active = FALSE;
            vis->matrix_column_count--;
        }
    }
}

void draw_matrix(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    // Initialize matrix system on first call
    static gboolean matrix_initialized = FALSE;
    if (!matrix_initialized) {
        init_matrix_system(vis);
        matrix_initialized = TRUE;
    }
    
    update_matrix(vis, 0.033); // ~30 FPS
    
    // Animated Background Grid with Depth
    cairo_set_source_rgba(cr, 0.0, 0.1, 0.0, 0.15);
    cairo_set_line_width(cr, 0.5);
    
    double grid_offset = fmod(vis->time_offset * 20, vis->matrix_char_size * 3);
    for (int x = -grid_offset; x < vis->width; x += vis->matrix_char_size * 3) {
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, vis->height);
        cairo_stroke(cr);
    }
    
    // Horizontal grid lines (animated)
    double h_offset = fmod(vis->time_offset * 15, vis->matrix_char_size * 4);
    for (int y = -h_offset; y < vis->height; y += vis->matrix_char_size * 4) {
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, vis->width, y);
        cairo_stroke(cr);
    }
    
    // Set up font
    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, vis->matrix_char_size);
    
    // Draw Columns with Enhanced Visual Effects
    for (int i = 0; i < MAX_MATRIX_COLUMNS; i++) {
        if (!vis->matrix_columns[i].active) continue;
        
        MatrixColumn *col = &vis->matrix_columns[i];
        
        // Draw each character in the column
        for (int j = 0; j < col->length; j++) {
            double char_y = col->y - j * vis->matrix_char_size;
            
            // Skip if off screen
            if (char_y < -vis->matrix_char_size || char_y > vis->height + vis->matrix_char_size) continue;
            
            double brightness = col->char_ages[j] * col->intensity;
            if (brightness < 0.05) continue;
            
            // Spawn trail particles on bright characters
            if (brightness > 0.5 && rand() % 100 < 15) {
                double r = 0.0, g = brightness, b = 0.0;
                spawn_matrix_trail_particle(col->x, char_y, brightness * 0.7, r, g, b);
            }
            
            // Color based on position in column
            if (j == 0 && brightness > 0.7) {
                // Bright head with slight pulse
                double pulse = 0.8 + 0.2 * sin(vis->time_offset * 5);
                cairo_set_source_rgba(cr, 0.9 * pulse, 1.0, 0.9 * pulse, brightness);
            } else {
                // Matrix green with subtle gradient
                double color_intensity = brightness * (0.7 + 0.3 * (1.0 - (double)j / col->length));
                cairo_set_source_rgba(cr, 0, color_intensity, 0, brightness);
            }
            
            // Draw character
            cairo_move_to(cr, col->x, char_y);
            cairo_show_text(cr, col->chars[j]);
            
            // Enhanced glow effect
            if (brightness > 0.6) {
                cairo_set_source_rgba(cr, 0, brightness * 0.6, 0, brightness * 0.4);
                cairo_move_to(cr, col->x - 1.5, char_y);
                cairo_show_text(cr, col->chars[j]);
                cairo_move_to(cr, col->x + 1.5, char_y);
                cairo_show_text(cr, col->chars[j]);
                cairo_move_to(cr, col->x, char_y - 1);
                cairo_show_text(cr, col->chars[j]);
            }
        }
    }
    
    // Draw Trail Particles
    for (int i = 0; i < trail_particle_count; i++) {
        MatrixTrailParticle *p = &trail_particles[i];
        
        cairo_set_source_rgba(cr, p->r, p->g, p->b, p->alpha * 0.6);
        cairo_arc(cr, p->x, p->y, p->size, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Outer glow
        cairo_set_source_rgba(cr, p->r, p->g, p->b, p->alpha * 0.2);
        cairo_arc(cr, p->x, p->y, p->size * 2.5, 0, 2 * M_PI);
        cairo_stroke(cr);
    }
    

    

    
    // Multi-Layer Scanning Line Effect
    static double scan_y = 0;
    static double scan_y2 = 0;
    scan_y += vis->height * 0.008;
    scan_y2 += vis->height * 0.012;
    if (scan_y > vis->height) scan_y = 0;
    if (scan_y2 > vis->height) scan_y2 = 0;
    
    cairo_set_source_rgba(cr, 0, 0.7, 0.3, 0.1);
    cairo_rectangle(cr, 0, scan_y, vis->width, 2);
    cairo_fill(cr);
    
    cairo_set_source_rgba(cr, 0, 0.5, 0.2, 0.08);
    cairo_rectangle(cr, 0, scan_y2, vis->width, 3);
    cairo_fill(cr);
}

#endif
