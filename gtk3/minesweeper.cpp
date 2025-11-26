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
            game->grid[y][x].adjacent_mines = 0;
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
    game->first_click_made = false;  // Reset first click flag
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

void minesweeper_update(Visualizer *vis, double dt) {
    MinesweeperGame *game = &vis->minesweeper_game;
    
    // Update animations
    for (int y = 0; y < game->grid_size; y++) {
        for (int x = 0; x < game->grid_size; x++) {
            MinesweeperCell *cell = &game->grid[y][x];
            
            // Reveal animation
            if (cell->state != CELL_HIDDEN && cell->state != CELL_FLAGGED) {
                if (cell->reveal_animation < 1.0) {
                    cell->reveal_animation += dt * 4.0;  // 0.25 second reveal
                    if (cell->reveal_animation > 1.0) {
                        cell->reveal_animation = 1.0;
                    }
                }
            }
            
            // Pulse animation decay
            cell->pulse_intensity *= 0.92;
        }
    }
    
    // Beat detection for pulse effect
    double total_intensity = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        if (vis->frequency_bands[i] > 0) {
            total_intensity += vis->frequency_bands[i];
        }
    }
    double avg_intensity = total_intensity / VIS_FREQUENCY_BARS;
    
    if (avg_intensity > vis->beat_threshold) {
        game->beat_glow = 0.8;
        // Pulse the last revealed cell
        if (game->last_revealed_x >= 0 && game->last_revealed_y >= 0) {
            game->grid[game->last_revealed_y][game->last_revealed_x].pulse_intensity = 1.0;
        }
    }
    
    game->beat_glow *= 0.94;
    
    // Handle game over timer
    if (game->game_over) {
        game->game_over_time -= dt;
        if (game->game_over_time <= 0) {
            init_minesweeper(vis);  // Reset game
        }
    }
    
    // Calculate cell size responsively based on window dimensions
    int cell_size_w = (vis->width / game->grid_size);
    int cell_size_h = ((vis->height - 60) / game->grid_size);  // Leave space for buttons
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
            
            // Beat glow effect
            if (cell->pulse_intensity > 0.01) {
                double glow_intensity = cell->pulse_intensity * 0.3;
                cairo_set_source_rgba(cr, 0.2, 0.8, 1.0, glow_intensity);
                cairo_rectangle(cr, px + 2, py + 2, cell_size - 4, cell_size - 4);
                cairo_fill(cr);
            }
            
            // Draw cell content based on state
            if (cell->state == CELL_HIDDEN) {
                // 3D button effect
                double brightness = 0.5;
                cairo_set_source_rgb(cr, brightness * 0.8, brightness * 0.9, brightness);
                cairo_rectangle(cr, px + 3, py + 3, cell_size - 6, cell_size - 6);
                cairo_fill(cr);
                
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
                // Empty cell - light background
                cairo_set_source_rgb(cr, 0.25, 0.28, 0.35);
                cairo_rectangle(cr, px + 2, py + 2, cell_size - 4, cell_size - 4);
                cairo_fill(cr);
                
            } else if (cell->state == CELL_REVEALED_NUMBER) {
                // Number cell
                cairo_set_source_rgb(cr, 0.25, 0.28, 0.35);
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
                // Mine explosion effect
                double pulse = game->beat_glow * 0.5;
                
                cairo_set_source_rgb(cr, 0.8 + pulse * 0.2, 0.1, 0.1);
                cairo_rectangle(cr, px + 2, py + 2, cell_size - 4, cell_size - 4);
                cairo_fill(cr);
                
                // Draw mine circle
                cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
                cairo_arc(cr, px + cell_size / 2.0, py + cell_size / 2.0, cell_size * 0.3, 0, 2 * M_PI);
                cairo_fill(cr);
                
                // Mine spikes
                cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
                for (int spike = 0; spike < 8; spike++) {
                    double angle = (spike / 8.0) * 2.0 * M_PI;
                    double spike_len = cell_size * 0.35;
                    double sx = px + cell_size / 2.0 + cos(angle) * spike_len;
                    double sy = py + cell_size / 2.0 + sin(angle) * spike_len;
                    cairo_move_to(cr, px + cell_size / 2.0, py + cell_size / 2.0);
                    cairo_line_to(cr, sx, sy);
                    cairo_set_line_width(cr, 2.0);
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
}
