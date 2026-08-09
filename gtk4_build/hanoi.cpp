// hanoi.c
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "visualization.h"

static HanoiParticle particles[MAX_HANOI_PARTICLES];
static int active_particles = 0;

static void spawn_particle(double x, double y, double r, double g, double b) {
    if (active_particles >= MAX_HANOI_PARTICLES) return;
    
    HanoiParticle *p = &particles[active_particles++];
    p->x = x;
    p->y = y;
    p->vx = (((double)rand() / RAND_MAX) - 0.5) * 100;
    p->vy = (((double)rand() / RAND_MAX) - 0.5) * 100 - 50;
    p->life = 1.0;
    p->r = r;
    p->g = g;
    p->b = b;
    p->size = 2 + ((double)rand() / RAND_MAX) * 3;
}

static void update_particles(double dt) {
    for (int i = 0; i < active_particles; i++) {
        HanoiParticle *p = &particles[i];
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vy += 200 * dt;
        p->life -= dt * 0.8;
        
        if (p->life <= 0) {
            particles[i] = particles[active_particles - 1];
            active_particles--;
            i--;
        }
    }
}

static void draw_particles(cairo_t *cr) {
    for (int i = 0; i < active_particles; i++) {
        HanoiParticle *p = &particles[i];
        cairo_set_source_rgba(cr, p->r, p->g, p->b, p->life);
        cairo_arc(cr, p->x, p->y, p->size, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// Proper iterative Hanoi solution
static void get_hanoi_move_at_step(int step, int total_disks, int *from, int *to) {
    // Binary method: the bit position that changes tells us which disk to move
    int bit_changed = 0;
    int temp = step ^ (step + 1);
    while (temp > 1) {
        temp >>= 1;
        bit_changed++;
    }
    
    // For odd disks: smallest moves 0->2, even disks: smallest moves 0->1
    // Each disk rotates through pegs in a pattern
    int disk = bit_changed;
    
    // Calculate how many times this disk has moved
    int disk_moves = (step + 1) >> (disk + 1);
    
    if (total_disks % 2 == 1) {
        // Odd: pattern is 0->2->1->0 for smallest, reversed for larger
        if (disk % 2 == 0) {
            int positions[] = {0, 2, 1};
            *from = positions[disk_moves % 3];
            *to = positions[(disk_moves + 1) % 3];
        } else {
            int positions[] = {0, 1, 2};
            *from = positions[disk_moves % 3];
            *to = positions[(disk_moves + 1) % 3];
        }
    } else {
        // Even: pattern is 0->1->2->0 for smallest, reversed for larger
        if (disk % 2 == 0) {
            int positions[] = {0, 1, 2};
            *from = positions[disk_moves % 3];
            *to = positions[(disk_moves + 1) % 3];
        } else {
            int positions[] = {0, 2, 1};
            *from = positions[disk_moves % 3];
            *to = positions[(disk_moves + 1) % 3];
        }
    }
}

void init_hanoi_system(Visualizer *vis) {
    HanoiSystem *hanoi = &vis->hanoi;
    memset(hanoi, 0, sizeof(HanoiSystem));
    
    hanoi->total_disks = 5 + (rand() % 6);
    hanoi->beat_threshold = 0.15;
    hanoi->waiting_for_beat = true;
    hanoi->total_moves_needed = (1 << hanoi->total_disks) - 1;
    
    for (int i = 0; i < HANOI_NUM_PEGS; i++) {
        hanoi->pegs[i].disk_count = 0;
        hanoi->pegs[i].x_position = (i + 1) * 0.25;
    }
    
    for (int i = 0; i < hanoi->total_disks; i++) {
        HanoiDisk *disk = &hanoi->pegs[0].disks[i];
        disk->disk_size = hanoi->total_disks - i;
        disk->y_offset = 0.0;
        disk->glow = 0.0;
        hanoi->pegs[0].disk_count++;
    }
    
    hanoi->current_solution_step = 0;
    hanoi->puzzle_complete = false;
    hanoi->current_move.active = false;
    active_particles = 0;
}

static bool hanoi_detect_beat(Visualizer *vis) {
    HanoiSystem *hanoi = &vis->hanoi;
    
    hanoi->volume_history[hanoi->volume_index] = vis->volume_level;
    hanoi->volume_index = (hanoi->volume_index + 1) % 10;
    
    double avg_volume = 0.0;
    for (int i = 0; i < 10; i++) {
        avg_volume += hanoi->volume_history[i];
    }
    avg_volume /= 10.0;
    
    return (vis->volume_level > avg_volume * 1.4 && 
            vis->volume_level > hanoi->beat_threshold);
}

void update_hanoi(Visualizer *vis, double dt) {
    HanoiSystem *hanoi = &vis->hanoi;
    
    update_particles(dt);
    
    if (hanoi->puzzle_complete) {
        hanoi->completion_glow *= 0.95;
        
        if (hanoi->completion_glow < 0.01) {
            hanoi->total_disks = 5 + (rand() % 6);
            init_hanoi_system(vis);
        }
        return;
    }
    
    if (hanoi->current_move.active) {
        hanoi->current_move.progress += dt * 4.0;
        
        if (hanoi->current_move.progress >= 1.0) {
            int from = hanoi->current_move.from_peg;
            int to = hanoi->current_move.to_peg;
            
            HanoiDisk moving_disk = hanoi->pegs[from].disks[hanoi->pegs[from].disk_count - 1];
            hanoi->pegs[from].disk_count--;
            
            hanoi->pegs[to].disks[hanoi->pegs[to].disk_count] = moving_disk;
            hanoi->pegs[to].disks[hanoi->pegs[to].disk_count].glow = 1.0;
            hanoi->pegs[to].disk_count++;
            
            double peg_x = vis->width * hanoi->pegs[to].x_position;
            double disk_y = vis->height * 0.8 - hanoi->pegs[to].disk_count * vis->height * 0.04;
            double hue = (double)moving_disk.disk_size / hanoi->total_disks;
            double r, g, b;
            hsv_to_rgb((hue * 0.7)*360, 0.8, 0.9, &r, &g, &b);
            
            for (int i = 0; i < 10; i++) {
                spawn_particle(peg_x, disk_y, r, g, b);
            }
            
            hanoi->current_move.active = false;
            hanoi->waiting_for_beat = true;
            
            if (hanoi->pegs[2].disk_count == hanoi->total_disks) {
                hanoi->puzzle_complete = true;
                hanoi->completion_glow = 1.0;
                
                for (int i = 0; i < 100; i++) {
                    spawn_particle(vis->width / 2, vis->height / 2, 
                                 1.0, 0.8, 0.0);
                }
            }
        }
    }
    
    for (int p = 0; p < HANOI_NUM_PEGS; p++) {
        for (int d = 0; d < hanoi->pegs[p].disk_count; d++) {
            hanoi->pegs[p].disks[d].glow *= 0.92;
        }
    }
    
    if (!hanoi->current_move.active && hanoi->waiting_for_beat) {
        if (hanoi_detect_beat(vis)) {
            if (hanoi->current_solution_step < hanoi->total_moves_needed) {
                int from, to;
                get_hanoi_move_at_step(hanoi->current_solution_step, hanoi->total_disks, &from, &to);
                
                hanoi->current_move.from_peg = from;
                hanoi->current_move.to_peg = to;
                hanoi->current_move.progress = 0.0;
                hanoi->current_move.active = true;
                hanoi->current_solution_step++;
                hanoi->move_count++;
                hanoi->waiting_for_beat = false;
            }
        }
    }
}

void draw_hanoi(Visualizer *vis, cairo_t *cr) {
    HanoiSystem *hanoi = &vis->hanoi;
    double width = vis->width;
    double height = vis->height;
    
    // Dynamic gradient background based on audio
    cairo_pattern_t *bg_gradient = cairo_pattern_create_linear(0, 0, 0, height);
    double bass = vis->frequency_bands[0] * 0.3;
    cairo_pattern_add_color_stop_rgb(bg_gradient, 0, 0.05 + bass, 0.0, 0.1 + bass * 0.5);
    cairo_pattern_add_color_stop_rgb(bg_gradient, 1, 0.0, 0.0, 0.05);
    cairo_set_source(cr, bg_gradient);
    cairo_paint(cr);
    cairo_pattern_destroy(bg_gradient);
    
    // Audio visualization rings around pegs
    for (int p = 0; p < HANOI_NUM_PEGS; p++) {
        double peg_x = width * hanoi->pegs[p].x_position;
        double intensity = vis->frequency_bands[p * 10];
        
        for (int ring = 0; ring < 3; ring++) {
            double radius = 30 + ring * 20 + intensity * 50;
            cairo_set_source_rgba(cr, 0.3, 0.5, 0.8, (0.3 - ring * 0.1) * intensity);
            cairo_set_line_width(cr, 3);
            cairo_arc(cr, peg_x, height * 0.8, radius, 0, 2 * M_PI);
            cairo_stroke(cr);
        }
    }
    
    // Base platform with glow
    cairo_set_source_rgba(cr, 0.5, 0.3, 0.1, 0.5);
    cairo_rectangle(cr, width * 0.05, height * 0.82, width * 0.9, height * 0.08);
    cairo_fill(cr);
    
    cairo_pattern_t *platform_gradient = cairo_pattern_create_linear(0, height * 0.8, 0, height * 0.88);
    cairo_pattern_add_color_stop_rgb(platform_gradient, 0, 0.6, 0.4, 0.2);
    cairo_pattern_add_color_stop_rgb(platform_gradient, 0.5, 0.8, 0.6, 0.3);
    cairo_pattern_add_color_stop_rgb(platform_gradient, 1, 0.4, 0.3, 0.15);
    cairo_set_source(cr, platform_gradient);
    cairo_rectangle(cr, width * 0.1, height * 0.8, width * 0.8, height * 0.05);
    cairo_fill(cr);
    cairo_pattern_destroy(platform_gradient);
    
    // Pegs with metallic effect
    for (int p = 0; p < HANOI_NUM_PEGS; p++) {
        double peg_x = width * hanoi->pegs[p].x_position;
        double peg_height = height * 0.5;
        double bass = vis->frequency_bands[p * 4] * 0.8;
        
        // Outer glow
        cairo_pattern_t *glow = cairo_pattern_create_radial(peg_x, height * 0.55, 5, peg_x, height * 0.55, 40);
        cairo_pattern_add_color_stop_rgba(glow, 0, 0.8 + bass, 0.6 + bass, 0.2, 0.5);
        cairo_pattern_add_color_stop_rgba(glow, 1, 0.2, 0.1, 0.0, 0.0);
        cairo_set_source(cr, glow);
        cairo_rectangle(cr, peg_x - 40, height * 0.3, 80, peg_height);
        cairo_fill(cr);
        cairo_pattern_destroy(glow);
        
        // Peg with gradient
        cairo_pattern_t *peg_gradient = cairo_pattern_create_linear(peg_x - 8, 0, peg_x + 8, 0);
        cairo_pattern_add_color_stop_rgb(peg_gradient, 0, 0.2, 0.2, 0.2);
        cairo_pattern_add_color_stop_rgb(peg_gradient, 0.5, 0.9, 0.85, 0.7);
        cairo_pattern_add_color_stop_rgb(peg_gradient, 1, 0.2, 0.2, 0.2);
        cairo_set_source(cr, peg_gradient);
        cairo_rectangle(cr, peg_x - 8, height * 0.3, 16, peg_height);
        cairo_fill(cr);
        cairo_pattern_destroy(peg_gradient);
    }
    
    // Disks with shadows and effects
    for (int p = 0; p < HANOI_NUM_PEGS; p++) {
        double peg_x = width * hanoi->pegs[p].x_position;
        
        for (int d = 0; d < hanoi->pegs[p].disk_count; d++) {
            HanoiDisk *disk = &hanoi->pegs[p].disks[d];
            
            double disk_width = (width * 0.16) * ((double)disk->disk_size / hanoi->total_disks);
            double disk_height = height * 0.045;
            double disk_y = height * 0.8 - (d + 1) * disk_height;
            
            double hue = (double)disk->disk_size / hanoi->total_disks;
            double r, g, b;
            hsv_to_rgb((hue * 0.7 + vis->rotation * 0.05)*360, 0.9, 0.95, &r, &g, &b);
            
            // Shadow
            cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
            cairo_rectangle(cr, peg_x - disk_width/2 + 5, disk_y + disk_height - 3, 
                          disk_width, 6);
            cairo_fill(cr);
            
            // Glow effect
            if (disk->glow > 0.0) {
                cairo_pattern_t *glow = cairo_pattern_create_radial(
                    peg_x, disk_y + disk_height/2, disk_width * 0.3,
                    peg_x, disk_y + disk_height/2, disk_width * 0.6);
                cairo_pattern_add_color_stop_rgba(glow, 0, 1.0, 1.0, 0.5, disk->glow);
                cairo_pattern_add_color_stop_rgba(glow, 1, r, g, b, 0);
                cairo_set_source(cr, glow);
                cairo_rectangle(cr, peg_x - disk_width/2 - 15, disk_y - 15,
                              disk_width + 30, disk_height + 30);
                cairo_fill(cr);
                cairo_pattern_destroy(glow);
            }
            
            // Disk body with gradient
            cairo_pattern_t *disk_gradient = cairo_pattern_create_linear(0, disk_y, 0, disk_y + disk_height);
            cairo_pattern_add_color_stop_rgb(disk_gradient, 0, r * 1.3, g * 1.3, b * 1.3);
            cairo_pattern_add_color_stop_rgb(disk_gradient, 0.4, r, g, b);
            cairo_pattern_add_color_stop_rgb(disk_gradient, 1, r * 0.5, g * 0.5, b * 0.5);
            cairo_set_source(cr, disk_gradient);
            
            // Rounded rectangle for disk
            double radius = disk_height * 0.3;
            cairo_new_path(cr);
            cairo_arc(cr, peg_x - disk_width/2 + radius, disk_y + radius, radius, M_PI, 3*M_PI/2);
            cairo_arc(cr, peg_x + disk_width/2 - radius, disk_y + radius, radius, 3*M_PI/2, 0);
            cairo_arc(cr, peg_x + disk_width/2 - radius, disk_y + disk_height - radius, radius, 0, M_PI/2);
            cairo_arc(cr, peg_x - disk_width/2 + radius, disk_y + disk_height - radius, radius, M_PI/2, M_PI);
            cairo_close_path(cr);
            cairo_fill(cr);
            cairo_pattern_destroy(disk_gradient);
            
            // Highlight
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
            cairo_rectangle(cr, peg_x - disk_width/2 + 5, disk_y + 5, 
                          disk_width - 10, disk_height * 0.15);
            cairo_fill(cr);
        }
    }
    
    // Moving disk with dramatic trail
    if (hanoi->current_move.active) {
        int from = hanoi->current_move.from_peg;
        int to = hanoi->current_move.to_peg;
        
        HanoiDisk *disk = &hanoi->pegs[from].disks[hanoi->pegs[from].disk_count - 1];
        
        double from_x = width * hanoi->pegs[from].x_position;
        double to_x = width * hanoi->pegs[to].x_position;
        
        double t = hanoi->current_move.progress;
        double curve_height = height * 0.35;
        double current_x = from_x + (to_x - from_x) * t;
        double current_y = height * 0.8 - (hanoi->pegs[from].disk_count) * height * 0.045
                          - sin(t * M_PI) * curve_height;
        
        double disk_width = (width * 0.16) * ((double)disk->disk_size / hanoi->total_disks);
        double disk_height = height * 0.045;
        
        double hue = (double)disk->disk_size / hanoi->total_disks;
        double r, g, b;
        hsv_to_rgb((hue * 0.7)*360, 0.9, 0.95, &r, &g, &b);
        
        // Motion blur trail
        for (int i = 1; i <= 8; i++) {
            double trail_t = t - i * 0.03;
            if (trail_t < 0) continue;
            
            double trail_x = from_x + (to_x - from_x) * trail_t;
            double trail_y = height * 0.8 - (hanoi->pegs[from].disk_count) * height * 0.045
                            - sin(trail_t * M_PI) * curve_height;
            
            double alpha = (1.0 - i * 0.125) * 0.4;
            cairo_set_source_rgba(cr, r, g, b, alpha);
            cairo_rectangle(cr, trail_x - disk_width/2, trail_y, disk_width, disk_height);
            cairo_fill(cr);
        }
        
        // Bright glow
        cairo_pattern_t *glow = cairo_pattern_create_radial(
            current_x, current_y + disk_height/2, disk_width * 0.2,
            current_x, current_y + disk_height/2, disk_width * 0.8);
        cairo_pattern_add_color_stop_rgba(glow, 0, 1.0, 1.0, 0.3, 0.9);
        cairo_pattern_add_color_stop_rgba(glow, 1, r, g, b, 0);
        cairo_set_source(cr, glow);
        cairo_rectangle(cr, current_x - disk_width, current_y - disk_height,
                       disk_width * 2, disk_height * 3);
        cairo_fill(cr);
        cairo_pattern_destroy(glow);
        
        // Moving disk
        cairo_pattern_t *disk_gradient = cairo_pattern_create_linear(0, current_y, 0, current_y + disk_height);
        cairo_pattern_add_color_stop_rgb(disk_gradient, 0, r * 1.5, g * 1.5, b * 1.5);
        cairo_pattern_add_color_stop_rgb(disk_gradient, 0.5, r * 1.2, g * 1.2, b * 1.2);
        cairo_pattern_add_color_stop_rgb(disk_gradient, 1, r * 0.6, g * 0.6, b * 0.6);
        cairo_set_source(cr, disk_gradient);
        cairo_rectangle(cr, current_x - disk_width/2, current_y, disk_width, disk_height);
        cairo_fill(cr);
        cairo_pattern_destroy(disk_gradient);
    }
    
    // Draw particles
    draw_particles(cr);
    
    // Info text
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.8);
    cairo_set_font_size(cr, 18);
    char info[64];
    snprintf(info, sizeof(info), "Disks: %d | Moves: %d / %d", 
             hanoi->total_disks, hanoi->move_count, hanoi->total_moves_needed);
    cairo_move_to(cr, 12, 28);
    cairo_show_text(cr, info);
    
    cairo_set_source_rgb(cr, 1.0, 0.9, 0.5);
    cairo_move_to(cr, 10, 26);
    cairo_show_text(cr, info);
    
    // Completion celebration
    if (hanoi->puzzle_complete && hanoi->completion_glow > 0.0) {
        cairo_set_source_rgba(cr, 1.0, 0.8, 0.0, hanoi->completion_glow * 0.4);
        cairo_paint(cr);
        
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, hanoi->completion_glow);
        cairo_set_font_size(cr, 56);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, "SOLVED!", &extents);
        
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, hanoi->completion_glow);
        cairo_move_to(cr, (width - extents.width) / 2 + 4, height / 2 + 4);
        cairo_show_text(cr, "SOLVED!");
        
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, hanoi->completion_glow);
        cairo_move_to(cr, (width - extents.width) / 2, height / 2);
        cairo_show_text(cr, "SOLVED!");
    }
}
