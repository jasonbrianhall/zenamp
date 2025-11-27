#include "minesweeper.h"
#include "visualization.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void place_mines_avoiding_cell(Visualizer *vis, int avoid_x, int avoid_y) {
    MinesweeperGame *game = &vis->minesweeper_game;
    
    // Place mines randomly, avoiding the specified cell
    int mines_placed = 0;
    while (mines_placed < game->total_mines) {
        int x = rand() % game->grid_size;
        int y = rand() % game->grid_size;
        
        // Skip if this is the avoided cell or already has a mine
        if ((x == avoid_x && y == avoid_y) || game->grid[y][x].is_mine) {
            continue;
        }
        
        game->grid[y][x].is_mine = true;
        mines_placed++;
    }
    
    // Calculate adjacent mines for each cell
    for (int y = 0; y < game->grid_size; y++) {
        for (int x = 0; x < game->grid_size; x++) {
            if (!game->grid[y][x].is_mine) {
                int count = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ny = y + dy;
                        int nx = x + dx;
                        if (nx >= 0 && nx < game->grid_size &&
                            ny >= 0 && ny < game->grid_size &&
                            game->grid[ny][nx].is_mine) {
                            count++;
                        }
                    }
                }
                game->grid[y][x].adjacent_mines = count;
            }
        }
    }
}

void init_minesweeper(Visualizer *vis) {
    MinesweeperGame *game = &vis->minesweeper_game;
    
    // Set grid size and mine count based on difficulty
    switch (game->current_difficulty) {
        case DIFFICULTY_EASY:
            game->grid_size = 8;
            game->total_mines = 10;
            break;
        case DIFFICULTY_MEDIUM:
            game->grid_size = 10;
            game->total_mines = 20;
            break;
        case DIFFICULTY_HARD:
            game->grid_size = 12;
            game->total_mines = 40;
            break;
        default:
            game->grid_size = 10;
            game->total_mines = 20;
            game->current_difficulty = DIFFICULTY_MEDIUM;
    }
    
    game->total_cells = game->grid_size * game->grid_size;
    
    // Initialize grid (no mines yet)
    memset(game->grid, 0, sizeof(game->grid));
    
    // Initialize all cells as hidden with no mines
    for (int y = 0; y < game->grid_size; y++) {
        for (int x = 0; x < game->grid_size; x++) {
            game->grid[y][x].state = CELL_HIDDEN;
            game->grid[y][x].is_mine = false;
            game->grid[y][x].reveal_animation = 0.0;
            game->grid[y][x].pulse_intensity = 0.0;
            game->grid[y][x].beat_phase = 0.0;
            game->grid[y][x].distance_glow = 0.0;
        }
    }
    
    game->game_over = false;
    game->game_won = false;
    game->flags_placed = 0;
    game->cells_revealed = 0;
    game->game_over_time = 0.0;
    game->beat_glow = 0.0;
    game->last_revealed_x = -1;
    game->last_revealed_y = -1;
    game->show_difficulty_menu = false;
    game->first_click_made = false;
    
    // Initialize beat properties
    game->beat_magnitude = 0.0;
    game->bass_energy = 0.0;
    game->mid_energy = 0.0;
    game->high_energy = 0.0;
    game->beat_time = 0.0;
    game->last_beat_x = game->grid_size / 2;
    game->last_beat_y = game->grid_size / 2;
    game->wave_expansion = 0.0;
    
    // Initialize explosion system
    game->explosion_system.particle_count = 0;
    
    // Initialize timer
    game->elapsed_time = 0.0;
    
    // Initialize idle hint system
    game->idle_time = 0.0;
    game->idle_threshold = 8.0;  // Show hints after 8 seconds of idle
    game->hint_intensity = 0.0;
}

void minesweeper_reveal_cell(Visualizer *vis, int x, int y) {
    MinesweeperGame *game = &vis->minesweeper_game;
    
    if (x < 0 || x >= game->grid_size || y < 0 || y >= game->grid_size) {
        return;
    }
    
    MinesweeperCell *cell = &game->grid[y][x];
    
    if (cell->state != CELL_HIDDEN) {
        return;
    }
    
    // Reset idle timer on move
    game->idle_time = 0.0;
    
    // If this is the first click, place mines avoiding this cell
    if (!game->first_click_made) {
        game->first_click_made = true;
        place_mines_avoiding_cell(vis, x, y);
    }
    
    game->last_revealed_x = x;
    game->last_revealed_y = y;
    
    if (cell->is_mine) {
        cell->state = CELL_REVEALED_MINE;
        game->game_over = true;
        game->game_over_time = 3.0;
        
        // Reveal all mines on game over
        for (int cy = 0; cy < game->grid_size; cy++) {
            for (int cx = 0; cx < game->grid_size; cx++) {
                if (game->grid[cy][cx].is_mine) {
                    game->grid[cy][cx].state = CELL_REVEALED_MINE;
                }
            }
        }
    } else {
        cell->state = CELL_REVEALED_EMPTY;
        if (cell->adjacent_mines > 0) {
            cell->state = CELL_REVEALED_NUMBER;
        }
        game->cells_revealed++;
        
        // Flood fill if no adjacent mines
        if (cell->adjacent_mines == 0) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int ny = y + dy;
                    int nx = x + dx;
                    if (nx >= 0 && nx < game->grid_size &&
                        ny >= 0 && ny < game->grid_size) {
                        minesweeper_reveal_cell(vis, nx, ny);
                    }
                }
            }
        }
        
        // Check win condition
        int unrevealed = game->total_cells - game->total_mines - game->cells_revealed;
        if (unrevealed == 0) {
            game->game_won = true;
            game->game_over_time = 3.0;
            game->game_over = true;
        }
    }
}

void minesweeper_flag_cell(Visualizer *vis, int x, int y) {
    MinesweeperGame *game = &vis->minesweeper_game;
    
    if (x < 0 || x >= game->grid_size || y < 0 || y >= game->grid_size) {
        return;
    }
    
    MinesweeperCell *cell = &game->grid[y][x];
    
    // Reset idle timer on move
    game->idle_time = 0.0;
    
    if (cell->state == CELL_HIDDEN) {
        cell->state = CELL_FLAGGED;
        game->flags_placed++;
    } else if (cell->state == CELL_FLAGGED) {
        cell->state = CELL_HIDDEN;
        game->flags_placed--;
    }
}

void minesweeper_middle_click(Visualizer *vis, int x, int y) {
    MinesweeperGame *game = &vis->minesweeper_game;
    
    if (x < 0 || x >= game->grid_size || y < 0 || y >= game->grid_size) {
        return;
    }
    
    MinesweeperCell *cell = &game->grid[y][x];
    
    // Only works on revealed number cells
    if (cell->state != CELL_REVEALED_NUMBER) {
        return;
    }
    
    // Count adjacent flags
    int flag_count = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int ny = y + dy;
            int nx = x + dx;
            if (nx >= 0 && nx < game->grid_size &&
                ny >= 0 && ny < game->grid_size) {
                if (game->grid[ny][nx].state == CELL_FLAGGED) {
                    flag_count++;
                }
            }
        }
    }
    
    // If flag count matches adjacent mine count, reveal all adjacent cells
    if (flag_count == cell->adjacent_mines) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int ny = y + dy;
                int nx = x + dx;
                if (nx >= 0 && nx < game->grid_size &&
                    ny >= 0 && ny < game->grid_size) {
                    minesweeper_reveal_cell(vis, nx, ny);
                }
            }
        }
    }
}

void spawn_explosion(MinesweeperGame *game, double beat_magnitude, double bass_energy, double width, double height) {
    // Spawn 1-15 particles based on beat magnitude
    int num_particles = 1 + (int)(beat_magnitude * 14.0);
    if (num_particles > 15) num_particles = 15;
    
    ExplosionSystem *sys = &game->explosion_system;
    
    for (int i = 0; i < num_particles; i++) {
        if (sys->particle_count >= MAX_EXPLOSION_PARTICLES) break;
        
        ExplosionParticle *p = &sys->particles[sys->particle_count++];
        
        // Random position on screen
        p->x = (rand() % (int)width);
        p->y = (rand() % (int)height);
        
        // Random velocity in all directions
        double angle = (rand() / (double)RAND_MAX) * 2.0 * M_PI;
        double speed = 100.0 + (bass_energy * 200.0);  // Bass affects speed
        p->vx = cos(angle) * speed;
        p->vy = sin(angle) * speed;
        
        // Random particle type
        int type_rand = rand() % 3;
        p->type = (ParticleType)type_rand;
        
        // Life based on frequency - bass particles live longer
        p->life = 0.8 + (bass_energy * 0.5);
    }
}

void minesweeper_update(Visualizer *vis, double dt) {
    MinesweeperGame *game = &vis->minesweeper_game;
    
    // ===== FREQUENCY ANALYSIS FOR BEAT RESPONSIVENESS =====
    // Break frequency spectrum into bass, mid, and high
    int bass_start = 0;
    int bass_end = VIS_FREQUENCY_BARS / 4;        // Lower 25%
    int mid_start = bass_end;
    int mid_end = (VIS_FREQUENCY_BARS * 3) / 4;   // Middle 50%
    int high_start = mid_end;
    int high_end = VIS_FREQUENCY_BARS;             // Upper 25%
    
    // Calculate band energies
    double bass_total = 0, mid_total = 0, high_total = 0;
    for (int i = bass_start; i < bass_end; i++) {
        bass_total += vis->frequency_bands[i];
    }
    for (int i = mid_start; i < mid_end; i++) {
        mid_total += vis->frequency_bands[i];
    }
    for (int i = high_start; i < high_end; i++) {
        high_total += vis->frequency_bands[i];
    }
    
    game->bass_energy = bass_total / (bass_end - bass_start);
    game->mid_energy = mid_total / (mid_end - mid_start);
    game->high_energy = high_total / (high_end - high_start);
    
    // Overall beat magnitude (weighted average)
    game->beat_magnitude = (game->bass_energy * 0.5 + game->mid_energy * 0.3 + game->high_energy * 0.2);
    
    // ===== BEAT DETECTION WITH HYSTERESIS =====
    double beat_threshold = vis->beat_threshold * 0.8;
    bool is_beat = game->beat_magnitude > beat_threshold;
    
    if (is_beat && game->beat_time > 0.1) {  // Prevent beat spam
        game->beat_time = 0.0;
        game->beat_glow = 1.0;
        game->wave_expansion = 0.0;
        
        // Spawn background explosions
        spawn_explosion(game, game->beat_magnitude, game->bass_energy, vis->width, vis->height);
        
        // New beat epicenter (random or recent interaction)
        if (game->last_revealed_x >= 0) {
            game->last_beat_x = game->last_revealed_x;
            game->last_beat_y = game->last_revealed_y;
        } else {
            game->last_beat_x = game->grid_size / 2;
            game->last_beat_y = game->grid_size / 2;
        }
    }
    
    game->beat_time += dt;
    
    // ===== UPDATE ALL CELLS =====
    for (int y = 0; y < game->grid_size; y++) {
        for (int x = 0; x < game->grid_size; x++) {
            MinesweeperCell *cell = &game->grid[y][x];
            
            // Reveal animation
            if (cell->state != CELL_HIDDEN && cell->state != CELL_FLAGGED) {
                if (cell->reveal_animation < 1.0) {
                    cell->reveal_animation += dt * 4.0;
                    if (cell->reveal_animation > 1.0) {
                        cell->reveal_animation = 1.0;
                    }
                }
            }
            
            // ===== BEAT-WAVE PROPAGATION =====
            // Calculate distance from beat epicenter
            int dx = x - game->last_beat_x;
            int dy = y - game->last_beat_y;
            double distance = sqrt(dx * dx + dy * dy);
            
            // Wave that expands from epicenter
            double wave_distance = game->wave_expansion;
            double wave_width = 1.5;  // How "thick" the wave is
            
            if (distance < wave_distance + wave_width && distance > wave_distance - wave_width) {
                // Cell is in the wave zone
                double wave_intensity = 1.0 - fabs(distance - wave_distance) / wave_width;
                cell->pulse_intensity = fmax(cell->pulse_intensity, wave_intensity * game->beat_magnitude);
            }
            
            // ===== FREQUENCY-SPECIFIC CELL RESPONSE =====
            // Bass cells: Hidden cells pulse with bass
            if (cell->state == CELL_HIDDEN) {
                cell->pulse_intensity += game->bass_energy * 0.15;
            }
            // Mid cells: Number cells react to mids
            else if (cell->state == CELL_REVEALED_NUMBER) {
                cell->pulse_intensity += game->mid_energy * 0.1;
            }
            // High cells: Flagged cells react to highs
            else if (cell->state == CELL_FLAGGED) {
                cell->pulse_intensity += game->high_energy * 0.15;
            }
            
            // Clamp and decay pulse
            if (cell->pulse_intensity > 1.0) cell->pulse_intensity = 1.0;
            cell->pulse_intensity *= 0.90;  // Decay
            
            // Distance glow: stronger closer to epicenter
            if (game->beat_glow > 0.1) {
                double dist_to_center = sqrt(dx * dx + dy * dy);
                double max_dist = sqrt(game->grid_size * game->grid_size + game->grid_size * game->grid_size) / 2.0;
                cell->distance_glow = (1.0 - (dist_to_center / max_dist)) * game->beat_glow;
            }
            cell->distance_glow *= 0.92;
            
            // Beat phase for oscillations
            cell->beat_phase += dt * (2.0 + game->beat_magnitude * 5.0);
        }
    }
    
    // ===== OVERALL BEAT EFFECTS =====
    game->beat_glow *= 0.95;
    game->wave_expansion += dt * (10.0 + game->beat_magnitude * 15.0);  // Wave speed increases with beat
    
    // ===== UPDATE PARTICLES =====
    ExplosionSystem *sys = &game->explosion_system;
    int alive_count = 0;
    for (int i = 0; i < sys->particle_count; i++) {
        ExplosionParticle *p = &sys->particles[i];
        
        // Update position
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        
        // Apply gravity/deceleration
        p->vy += 200.0 * dt;  // Gravity
        p->vx *= 0.98;        // Air resistance
        p->vy *= 0.98;
        
        // Fade out life
        p->life -= dt * 2.0;
        if (p->life < 0.0) p->life = 0.0;
        
        // Keep particles on screen
        if (p->x < 0) p->x = 0;
        if (p->x > vis->width) p->x = vis->width;
        if (p->y < 0) p->y = 0;
        if (p->y > vis->height) p->y = vis->height;
        
        // Only keep alive particles
        if (p->life > 0.01) {
            alive_count++;
        }
    }
    
    // Remove dead particles by shifting array
    if (alive_count < sys->particle_count) {
        int write_idx = 0;
        for (int i = 0; i < sys->particle_count; i++) {
            if (sys->particles[i].life > 0.01) {
                sys->particles[write_idx] = sys->particles[i];
                write_idx++;
            }
        }
        sys->particle_count = write_idx;
    }
    
    // ===== UPDATE TIMER =====
    if (!game->game_over) {
        game->elapsed_time += dt;
    }
    
    // ===== UPDATE IDLE HINT SYSTEM =====
    if (!game->game_over) {
        game->idle_time += dt;
    }
    
    // Calculate hint intensity based on idle time
    if (game->idle_time > game->idle_threshold) {
        // After threshold, slowly increase hint intensity
        double time_over_threshold = game->idle_time - game->idle_threshold;
        game->hint_intensity = fmin(1.0, time_over_threshold / 22.0);  // Full opacity after 22 more seconds (30 total)
    } else {
        // Cool down while player is active
        game->hint_intensity = fmax(0.0, game->hint_intensity - dt / 3.0);  // Fully cool after 3 seconds
    }
    
    // Handle game over timer
    if (game->game_over) {
        game->game_over_time -= dt;
        if (game->game_over_time <= 0) {
            init_minesweeper(vis);
        }
    }
    
    // Calculate cell size responsively based on window dimensions
    int cell_size_w = (vis->width / game->grid_size);
    int cell_size_h = ((vis->height - 60) / game->grid_size);
    int cell_size = (cell_size_w < cell_size_h) ? cell_size_w : cell_size_h;
    
    // Calculate grid offset
    int total_width = cell_size * game->grid_size;
    int total_height = cell_size * game->grid_size;
    int offset_x = (vis->width - total_width) / 2;
    int offset_y = 60 + (vis->height - 60 - total_height) / 2;  // Account for button area
    
    // Update difficulty button hover states
    double button_y = 10;
    double button_height = 40;
    double button_width = 80;
    double button_spacing = 10;
    double buttons_start_x = 10;  // Left side instead of centered
    
    for (int i = 0; i < 3; i++) {
        game->difficulty_buttons[i].x = buttons_start_x + i * (button_width + button_spacing);
        game->difficulty_buttons[i].y = button_y;
        game->difficulty_buttons[i].width = button_width;
        game->difficulty_buttons[i].height = button_height;
        game->difficulty_buttons[i].difficulty = (MineSweepDifficulty)i;
        
        // Check if mouse is hovering (fixed logic)
        game->difficulty_buttons[i].hovered = 
            vis->mouse_x >= game->difficulty_buttons[i].x &&
            vis->mouse_x < game->difficulty_buttons[i].x + button_width &&
            vis->mouse_y >= game->difficulty_buttons[i].y &&
            vis->mouse_y < game->difficulty_buttons[i].y + button_height;
    }
    
    // Handle difficulty button clicks
    if (vis->mouse_left_pressed && vis->mouse_over) {
        for (int i = 0; i < 3; i++) {
            if (game->difficulty_buttons[i].hovered) {
                game->current_difficulty = (MineSweepDifficulty)i;
                init_minesweeper(vis);
                vis->mouse_left_pressed = FALSE;
                return;  // Don't process grid clicks this frame
            }
        }
    }
    
    // Only process grid clicks if game is not over and difficulty menu is not showing
    if (game->game_over || game->show_difficulty_menu) {
        vis->mouse_left_pressed = FALSE;
        vis->mouse_right_pressed = FALSE;
        vis->mouse_middle_pressed = FALSE;
        return;
    }
    
    // Handle mouse clicks
    if (vis->mouse_left_pressed && vis->mouse_over) {
        // Calculate which cell was clicked (subtract offset first)
        int grid_x = (vis->mouse_x - offset_x) / cell_size;
        int grid_y = (vis->mouse_y - offset_y) / cell_size;
        
        if (grid_x >= 0 && grid_x < game->grid_size &&
            grid_y >= 0 && grid_y < game->grid_size) {
            minesweeper_reveal_cell(vis, grid_x, grid_y);
        }
        vis->mouse_left_pressed = FALSE;
    }
    
    if (vis->mouse_right_pressed && vis->mouse_over) {
        // Calculate which cell was right-clicked (subtract offset first)
        int grid_x = (vis->mouse_x - offset_x) / cell_size;
        int grid_y = (vis->mouse_y - offset_y) / cell_size;
        
        if (grid_x >= 0 && grid_x < game->grid_size &&
            grid_y >= 0 && grid_y < game->grid_size) {
            minesweeper_flag_cell(vis, grid_x, grid_y);
        }
        vis->mouse_right_pressed = FALSE;
    }
    
    if (vis->mouse_middle_pressed && vis->mouse_over) {
        // Calculate which cell was middle-clicked (subtract offset first)
        int grid_x = (vis->mouse_x - offset_x) / cell_size;
        int grid_y = (vis->mouse_y - offset_y) / cell_size;
        
        if (grid_x >= 0 && grid_x < game->grid_size &&
            grid_y >= 0 && grid_y < game->grid_size) {
            minesweeper_middle_click(vis, grid_x, grid_y);
        }
        vis->mouse_middle_pressed = FALSE;
    }
}

void minesweeper_draw(Visualizer *vis, cairo_t *cr) {
    MinesweeperGame *game = &vis->minesweeper_game;
    
    // Background
    cairo_set_source_rgb(cr, 0.1, 0.12, 0.15);
    cairo_paint(cr);
    
    // ===== DRAW BACKGROUND EXPLOSION PARTICLES =====
    ExplosionSystem *sys = &game->explosion_system;
    for (int i = 0; i < sys->particle_count; i++) {
        ExplosionParticle *p = &sys->particles[i];
        
        // Color and size based on particle type
        double size = 2.0;
        double r, g, b, a;
        
        a = p->life;  // Fade out as life decreases
        
        switch (p->type) {
            case PARTICLE_SPARK:
                // Yellow/orange sparks
                r = 1.0;
                g = 0.7 + (p->life * 0.3);
                b = 0.0;
                size = 2.5;
                break;
            case PARTICLE_DEBRIS:
                // Gray debris
                r = 0.5;
                g = 0.5;
                b = 0.5;
                size = 3.5;
                break;
            case PARTICLE_SMOKE:
                // White/gray smoke
                r = 0.8 + (p->life * 0.2);
                g = 0.8 + (p->life * 0.2);
                b = 0.8 + (p->life * 0.2);
                size = 4.0 + ((1.0 - p->life) * 2.0);  // Grows as it fades
                break;
        }
        
        cairo_set_source_rgba(cr, r, g, b, a * 0.8);
        cairo_arc(cr, p->x, p->y, size, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    
    // Draw difficulty buttons at the top left
    double button_y = 10;
    double button_height = 40;
    double button_width = 80;
    double button_spacing = 10;
    double buttons_start_x = 10;  // Left side
    
    const char *difficulty_names[] = {"Easy", "Medium", "Hard"};
    
    for (int i = 0; i < 3; i++) {
        double btn_x = buttons_start_x + i * (button_width + button_spacing);
        
        // Draw button background
        if (game->difficulty_buttons[i].hovered) {
            cairo_set_source_rgb(cr, 0.3, 0.5, 0.8);
        } else if (game->current_difficulty == i) {
            cairo_set_source_rgb(cr, 0.2, 0.4, 0.7);
        } else {
            cairo_set_source_rgb(cr, 0.15, 0.25, 0.4);
        }
        
        cairo_rectangle(cr, btn_x, button_y, button_width, button_height);
        cairo_fill(cr);
        
        // Draw button border
        cairo_set_source_rgb(cr, 0.5, 0.6, 0.8);
        cairo_rectangle(cr, btn_x, button_y, button_width, button_height);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
        
        // Draw button text
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 14);
        
        cairo_text_extents_t extents;
        cairo_text_extents(cr, difficulty_names[i], &extents);
        double text_x = btn_x + (button_width - extents.width) / 2.0;
        double text_y = button_y + (button_height + extents.height) / 2.0;
        cairo_move_to(cr, text_x, text_y);
        cairo_show_text(cr, difficulty_names[i]);
    }
    
    // Calculate cell size responsively based on window dimensions
    int cell_size_w = (vis->width / game->grid_size);
    int cell_size_h = ((vis->height - 60) / game->grid_size);
    int cell_size = (cell_size_w < cell_size_h) ? cell_size_w : cell_size_h;
    
    // Calculate offset to center the grid
    int total_width = cell_size * game->grid_size;
    int total_height = cell_size * game->grid_size;
    int offset_x = (vis->width - total_width) / 2;
    int offset_y = 60 + (vis->height - 60 - total_height) / 2;
    
    // Draw grid
    for (int y = 0; y < game->grid_size; y++) {
        for (int x = 0; x < game->grid_size; x++) {
            MinesweeperCell *cell = &game->grid[y][x];
            
            double px = offset_x + x * cell_size;
            double py = offset_y + y * cell_size;
            
            // Draw cell background
            if ((x + y) % 2 == 0) {
                cairo_set_source_rgb(cr, 0.15, 0.17, 0.22);
            } else {
                cairo_set_source_rgb(cr, 0.12, 0.14, 0.18);
            }
            cairo_rectangle(cr, px, py, cell_size, cell_size);
            cairo_fill(cr);
            
            // Draw cell border
            cairo_set_source_rgba(cr, 0.3, 0.4, 0.5, 0.3);
            cairo_rectangle(cr, px, py, cell_size, cell_size);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
            
            // Draw cell content based on state
            if (cell->state == CELL_HIDDEN) {
                // 3D button effect
                double brightness = 0.5;
                cairo_set_source_rgb(cr, brightness * 0.8, brightness * 0.9, brightness);
                cairo_rectangle(cr, px + 3, py + 3, cell_size - 6, cell_size - 6);
                cairo_fill(cr);
                
                // Idle hint: glow mines red if player is idle too long
                if (game->hint_intensity > 0.01 && cell->is_mine) {
                    cairo_set_source_rgba(cr, 1.0, 0.2, 0.2, game->hint_intensity);
                    cairo_rectangle(cr, px + 3, py + 3, cell_size - 6, cell_size - 6);
                    cairo_fill(cr);
                }
                
                // Highlight
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.2);
                cairo_move_to(cr, px + 4, py + cell_size - 4);
                cairo_line_to(cr, px + 4, py + 4);
                cairo_line_to(cr, px + cell_size - 4, py + 4);
                cairo_set_line_width(cr, 1.0);
                cairo_stroke(cr);
                
            } else if (cell->state == CELL_FLAGGED) {
                // Draw flag (no rotation)
                double flag_x = px + cell_size / 2.0;
                double flag_y = py + cell_size / 2.0;
                
                cairo_set_source_rgb(cr, 1.0, 0.2, 0.2);
                
                // Flag pole
                cairo_move_to(cr, flag_x, flag_y - cell_size * 0.3);
                cairo_line_to(cr, flag_x, flag_y + cell_size * 0.3);
                cairo_set_line_width(cr, 2.0);
                cairo_stroke(cr);
                
                // Flag triangle (static, no rotation)
                cairo_move_to(cr, flag_x, flag_y - cell_size * 0.2);
                cairo_line_to(cr, flag_x + cell_size * 0.25, flag_y - cell_size * 0.35);
                cairo_line_to(cr, flag_x, flag_y - cell_size * 0.5);
                cairo_close_path(cr);
                cairo_fill(cr);
                
            } else if (cell->state == CELL_REVEALED_EMPTY) {
                // Empty cell - very very transparent
                cairo_set_source_rgba(cr, 0.25, 0.28, 0.35, 0.05);
                cairo_rectangle(cr, px + 2, py + 2, cell_size - 4, cell_size - 4);
                cairo_fill(cr);
                
            } else if (cell->state == CELL_REVEALED_NUMBER) {
                // Number cell - very very transparent
                cairo_set_source_rgba(cr, 0.25, 0.28, 0.35, 0.05);
                cairo_rectangle(cr, px + 2, py + 2, cell_size - 4, cell_size - 4);
                cairo_fill(cr);
                
                // Draw number
                char num_str[2];
                sprintf(num_str, "%d", cell->adjacent_mines);
                
                double colors[][3] = {
                    {0.2, 0.2, 0.8},  // 1 - blue
                    {0.2, 0.8, 0.2},  // 2 - green
                    {0.8, 0.2, 0.2},  // 3 - red
                    {0.2, 0.2, 0.5},  // 4 - dark blue
                    {0.8, 0.2, 0.2},  // 5 - dark red
                    {0.2, 0.8, 0.8},  // 6 - cyan
                    {0.2, 0.2, 0.2},  // 7 - black
                    {0.5, 0.5, 0.5},  // 8 - gray
                };
                
                int num = cell->adjacent_mines;
                if (num > 0 && num <= 8) {
                    cairo_set_source_rgb(cr, colors[num-1][0], colors[num-1][1], colors[num-1][2]);
                } else {
                    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
                }
                
                cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, cell_size * 0.6);
                
                cairo_text_extents_t extents;
                cairo_text_extents(cr, num_str, &extents);
                
                double text_x = px + (cell_size - extents.width) / 2.0;
                double text_y = py + cell_size * 0.65;
                
                cairo_move_to(cr, text_x, text_y);
                cairo_show_text(cr, num_str);
                
            } else if (cell->state == CELL_REVEALED_MINE) {
                // Mine explosion effect with beat reactivity
                double pulse = game->beat_glow * 0.8;
                double bass_pulse = game->bass_energy * 0.4;
                
                // Red background intensifies with beat
                cairo_set_source_rgb(cr, 0.9 + pulse * 0.1 + bass_pulse * 0.1, 0.1, 0.1);
                cairo_rectangle(cr, px + 2, py + 2, cell_size - 4, cell_size - 4);
                cairo_fill(cr);
                
                // Draw mine circle
                cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
                cairo_arc(cr, px + cell_size / 2.0, py + cell_size / 2.0, cell_size * 0.3, 0, 2 * M_PI);
                cairo_fill(cr);
                
                // Mine spikes - dynamic based on beat with slower rotation
                int spike_count = 8 + (int)(game->beat_magnitude * 4);  // More spikes with beat
                for (int spike = 0; spike < spike_count; spike++) {
                    double angle = (spike / (double)spike_count) * 2.0 * M_PI + (cell->beat_phase * 0.5);  // Slower: 0.5 instead of 2.0
                    
                    // Spike length pulses with beat
                    double base_len = cell_size * 0.35;
                    double spike_len = base_len + (game->bass_energy * cell_size * 0.15);
                    
                    double sx = px + cell_size / 2.0 + cos(angle) * spike_len;
                    double sy = py + cell_size / 2.0 + sin(angle) * spike_len;
                    cairo_move_to(cr, px + cell_size / 2.0, py + cell_size / 2.0);
                    cairo_line_to(cr, sx, sy);
                    
                    // Spike color varies: yellow on high energy, red on low
                    cairo_set_source_rgb(cr, 0.3 + game->high_energy * 0.7, 0.2, 0.1);
                    cairo_set_line_width(cr, 2.0 + game->beat_magnitude * 1.5);
                    cairo_stroke(cr);
                }
            }
        }
    }
    
    // Draw game over overlay
    if (game->game_over) {
        double fade = game->game_over_time / 3.0;
        if (fade < 0) fade = 0;
        if (fade > 1) fade = 1;
        
        cairo_set_source_rgba(cr, 0, 0, 0, 0.6 * fade);
        cairo_rectangle(cr, 0, 0, vis->width, vis->height);
        cairo_fill(cr);
        
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 64);
        
        const char *message = game->game_won ? "YOU WIN!" : "GAME OVER!";
        double r = game->game_won ? 0.2 : 1.0;
        double g = game->game_won ? 1.0 : 0.2;
        double b = game->game_won ? 0.2 : 0.2;
        
        cairo_set_source_rgba(cr, r, g, b, fade);
        
        cairo_text_extents_t extents;
        cairo_text_extents(cr, message, &extents);
        
        double text_x = (vis->width - extents.width) / 2.0;
        double text_y = (vis->height - extents.height) / 2.0;
        
        cairo_move_to(cr, text_x, text_y);
        cairo_show_text(cr, message);
    }
    
    // Draw stats at bottom
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    
    char stats[64];
    sprintf(stats, "Flags: %d/%d", game->flags_placed, game->total_mines);
    cairo_move_to(cr, 10, vis->height - 20);
    cairo_show_text(cr, stats);
    
    sprintf(stats, "Revealed: %d/%d", game->cells_revealed, 
            game->total_cells - game->total_mines);
    cairo_move_to(cr, 10, vis->height - 5);
    cairo_show_text(cr, stats);
    
    // Draw timer at bottom right
    int minutes = (int)game->elapsed_time / 60;
    int seconds = (int)game->elapsed_time % 60;
    char timer_str[16];
    sprintf(timer_str, "%02d:%02d", minutes, seconds);
    
    cairo_text_extents_t timer_extents;
    cairo_text_extents(cr, timer_str, &timer_extents);
    cairo_move_to(cr, vis->width - timer_extents.width - 10, vis->height - 5);
    cairo_show_text(cr, timer_str);
}
