#include "visualization.h"

void init_blockstack_system(void *vis_ptr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    BlockStackSystem *bs = &vis->blockstack;
    
    // Initialize all columns
    for (int i = 0; i < BLOCK_COLUMNS; i++) {
        bs->columns[i].block_count = 0;
        bs->columns[i].column_height = 0.0;
        bs->columns[i].spawn_timer = 0.0;
        bs->columns[i].last_spawn_intensity = 0.0;
    }
    
    // System settings
    bs->beat_threshold = 0.15;
    bs->spawn_cooldown = 0.0;
    bs->block_width = 0.0; // Will be calculated based on screen size
    bs->block_min_height = 10.0;
    bs->block_max_height = 40.0;
    bs->fall_delay = 2.0; // Blocks stay for 2 seconds before falling
    
    // Beat detection
    for (int i = 0; i < 10; i++) {
        bs->volume_history[i] = 0.0;
    }
    bs->volume_index = 0;
}

gboolean blockstack_detect_beat(void *vis_ptr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    BlockStackSystem *bs = &vis->blockstack;
    
    // Store current volume
    bs->volume_history[bs->volume_index] = vis->volume_level;
    bs->volume_index = (bs->volume_index + 1) % 10;
    
    // Calculate average
    double avg = 0.0;
    for (int i = 0; i < 10; i++) {
        avg += bs->volume_history[i];
    }
    avg /= 10.0;
    
    // Beat if current significantly exceeds average
    return (vis->volume_level > avg * 1.6 && vis->volume_level > bs->beat_threshold);
}

void get_block_color(int frequency_band, double intensity, double *r, double *g, double *b) {
    // Color based on frequency band (low = red/orange, mid = green/yellow, high = blue/cyan)
    double hue = (double)frequency_band / BLOCK_COLUMNS;
    
    if (hue < 0.33) {
        // Low frequencies: Red to Orange
        *r = 1.0;
        *g = 0.3 + hue * 2.0;
        *b = 0.1;
    } else if (hue < 0.67) {
        // Mid frequencies: Yellow to Green
        *r = 1.0 - (hue - 0.33) * 2.0;
        *g = 1.0;
        *b = 0.2;
    } else {
        // High frequencies: Cyan to Blue
        *r = 0.1;
        *g = 0.8 - (hue - 0.67) * 1.5;
        *b = 1.0;
    }
    
    // Adjust brightness based on intensity
    double brightness = 0.6 + intensity * 0.4;
    *r *= brightness;
    *g *= brightness;
    *b *= brightness;
}

void spawn_block(void *vis_ptr, int column, double intensity, int frequency_band) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    BlockStackSystem *bs = &vis->blockstack;
    
    if (column < 0 || column >= BLOCK_COLUMNS) return;
    
    BlockColumn *col = &bs->columns[column];
    
    // Don't spawn if column is full
    if (col->block_count >= MAX_BLOCKS_PER_COLUMN) return;
    
    Block *block = &col->blocks[col->block_count++];
    
    // Block properties based on intensity
    block->height = bs->block_min_height + intensity * (bs->block_max_height - bs->block_min_height);
    block->y_position = -block->height; // Start above screen
    block->target_y = col->column_height;
    block->velocity = 0.0;
    block->lifetime = 0.0;
    block->fade_alpha = 1.0;
    block->state = BLOCK_STATE_RISING;
    block->intensity_level = (int)(intensity * 10);
    
    // Get color
    get_block_color(frequency_band, intensity, &block->r, &block->g, &block->b);
    
    // Update column height
    col->column_height += block->height;
}

void update_block_physics(Block *block, double dt, double ground_level) {
    switch (block->state) {
        case BLOCK_STATE_RISING:
            // Rise up to target position
            block->y_position += 300.0 * dt; // Rise speed
            
            if (block->y_position >= block->target_y) {
                block->y_position = block->target_y;
                block->state = BLOCK_STATE_STACKED;
                block->lifetime = 0.0;
            }
            break;
            
        case BLOCK_STATE_STACKED:
            // Just sitting there, aging
            block->lifetime += dt;
            
            // Start falling after delay
            if (block->lifetime > 2.0) {
                block->state = BLOCK_STATE_FALLING;
                block->velocity = 0.0;
            }
            break;
            
        case BLOCK_STATE_FALLING:
            // Fall with gravity
            block->velocity += 400.0 * dt; // Gravity
            block->y_position += block->velocity * dt;
            
            // Fade out while falling
            block->fade_alpha -= dt * 0.8;
            if (block->fade_alpha < 0.0) block->fade_alpha = 0.0;
            
            break;
    }
}

void update_blockstack(void *vis_ptr, double dt) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    BlockStackSystem *bs = &vis->blockstack;
    
    // Update spawn cooldown
    if (bs->spawn_cooldown > 0.0) {
        bs->spawn_cooldown -= dt;
    }
    
    // Detect beats and spawn blocks
    gboolean beat = blockstack_detect_beat(vis);
    
    if (beat && bs->spawn_cooldown <= 0.0) {
        // Spawn blocks in columns based on frequency bands
        for (int i = 0; i < BLOCK_COLUMNS; i++) {
            double intensity = vis->frequency_bands[i];
            
            if (intensity > 0.3) {
                spawn_block(vis, i, intensity, i);
            }
        }
        bs->spawn_cooldown = 0.1; // Cooldown between spawns
    }
    
    // Update all blocks in all columns
    for (int col = 0; col < BLOCK_COLUMNS; col++) {
        BlockColumn *column = &bs->columns[col];
        
        // Update each block
        for (int i = 0; i < column->block_count; i++) {
            Block *block = &column->blocks[i];
            
            update_block_physics(block, dt, vis->height);
            
            // Remove blocks that have fallen off screen
            if (block->state == BLOCK_STATE_FALLING && block->fade_alpha <= 0.0) {
                // Remove this block
                for (int j = i; j < column->block_count - 1; j++) {
                    column->blocks[j] = column->blocks[j + 1];
                }
                column->block_count--;
                i--;
                
                // Recalculate column height
                column->column_height = 0.0;
                for (int j = 0; j < column->block_count; j++) {
                    if (column->blocks[j].state == BLOCK_STATE_STACKED) {
                        column->blocks[j].target_y = column->column_height;
                        column->column_height += column->blocks[j].height;
                    }
                }
            }
        }
    }
}

void draw_blockstack(void *vis_ptr, cairo_t *cr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    BlockStackSystem *bs = &vis->blockstack;
    
    if (vis->width <= 0 || vis->height <= 0) return;
    
    // Calculate block width based on screen size
    bs->block_width = (double)vis->width / BLOCK_COLUMNS;
    double padding = bs->block_width * 0.1;
    double actual_width = bs->block_width - padding * 2;
    
    // Draw all columns
    for (int col = 0; col < BLOCK_COLUMNS; col++) {
        BlockColumn *column = &bs->columns[col];
        double x = col * bs->block_width + padding;
        
        // Draw each block in the column
        for (int i = 0; i < column->block_count; i++) {
            Block *block = &column->blocks[i];
            
            // Calculate Y position from bottom
            double y = vis->height - block->y_position - block->height;
            
            // Draw block shadow
            cairo_set_source_rgba(cr, 0, 0, 0, 0.3 * block->fade_alpha);
            cairo_rectangle(cr, x + 2, y + 2, actual_width, block->height);
            cairo_fill(cr);
            
            // Draw main block with gradient
            cairo_pattern_t *gradient = cairo_pattern_create_linear(x, y, x, y + block->height);
            cairo_pattern_add_color_stop_rgba(gradient, 0, 
                block->r * 1.2, block->g * 1.2, block->b * 1.2, block->fade_alpha);
            cairo_pattern_add_color_stop_rgba(gradient, 1, 
                block->r * 0.6, block->g * 0.6, block->b * 0.6, block->fade_alpha);
            
            cairo_set_source(cr, gradient);
            cairo_rectangle(cr, x, y, actual_width, block->height);
            cairo_fill(cr);
            cairo_pattern_destroy(gradient);
            
            // Draw block outline
            cairo_set_source_rgba(cr, 
                block->r * 1.5, block->g * 1.5, block->b * 1.5, block->fade_alpha * 0.8);
            cairo_set_line_width(cr, 1.5);
            cairo_rectangle(cr, x, y, actual_width, block->height);
            cairo_stroke(cr);
            
            // Draw intensity indicator (small lines inside block)
            if (block->intensity_level > 5 && block->state != BLOCK_STATE_FALLING) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4 * block->fade_alpha);
                cairo_set_line_width(cr, 1.0);
                
                int num_lines = (block->intensity_level - 5) / 2;
                double line_spacing = block->height / (num_lines + 1);
                
                for (int l = 0; l < num_lines; l++) {
                    double line_y = y + line_spacing * (l + 1);
                    cairo_move_to(cr, x + actual_width * 0.2, line_y);
                    cairo_line_to(cr, x + actual_width * 0.8, line_y);
                }
                cairo_stroke(cr);
            }
        }
    }
    
    // Draw ground line
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.3);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, 0, vis->height);
    cairo_line_to(cr, vis->width, vis->height);
    cairo_stroke(cr);
    
    // Draw column separators (subtle)
    cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 0.2);
    cairo_set_line_width(cr, 1.0);
    for (int col = 1; col < BLOCK_COLUMNS; col++) {
        double x = col * bs->block_width;
        cairo_move_to(cr, x, vis->height - 100);
        cairo_line_to(cr, x, vis->height);
        cairo_stroke(cr);
    }
}
