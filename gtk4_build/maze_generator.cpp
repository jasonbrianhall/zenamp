#include "visualization.h"

void init_maze_generator_system(Visualizer *vis) {
    // Initialize with a 40x30 maze
    maze_init(&vis->maze, 40, 30);
    
    // Start generation
    maze_generate(&vis->maze);
    
    vis->maze_regenerate_timer = 0.0;
    vis->maze_audio_reactive = true;
}

void update_maze_generator(Visualizer *vis, double dt) {
    // Update maze animation
    maze_update(&vis->maze, dt);
    
    // Check if maze generation just completed
    if (!vis->maze.generating) {
        // Add to completion timer
        vis->maze.generation_timer += dt;
        
        // Wait 1 second, then regenerate
        if (vis->maze.generation_timer > 1.0) {
            maze_reset_generation(&vis->maze);
            maze_generate(&vis->maze);
        }
    } else {
        // Reset completion timer while generating
        vis->maze.generation_timer = 0.0;
    }
    
    // Make speed responsive to audio
    if (vis->maze_audio_reactive && vis->frequency_bands) {
        // Focus on lower frequency bands (beat/bass detection)
        // Bands 0-4 represent bass frequencies
        double bass_intensity = 0.0;
        for (int i = 0; i < 5 && i < VIS_FREQUENCY_BARS; i++) {
            bass_intensity += vis->frequency_bands[i];
        }
        bass_intensity /= 5.0;
        
        // Add some mid-range for presence
        double mid_intensity = 0.0;
        for (int i = 5; i < 10 && i < VIS_FREQUENCY_BARS; i++) {
            mid_intensity += vis->frequency_bands[i];
        }
        mid_intensity /= 5.0;
        
        // Combine with emphasis on bass (80% bass, 20% mid)
        double combined_intensity = (bass_intensity * 0.8) + (mid_intensity * 0.2);
        
        // Speed range: 25.0 to 50.0 cells per second for beat sync
        // Base 25.0 cells/sec + up to 25.0 cells/sec based on beat intensity
        vis->maze.cell_animation_speed = 25.0 + (combined_intensity * 25.0);
    }
}

void draw_maze_generator(Visualizer *vis, cairo_t *cr) {
    // Use your color scheme
    draw_maze_complete(cr, &vis->maze, vis->width, vis->height,
                       vis->fg_r, vis->fg_g, vis->fg_b,        // Wall color
                       vis->bg_r, vis->bg_g, vis->bg_b,        // Path color
                       vis->accent_r, vis->accent_g, vis->accent_b,  // Snake color
                       vis->accent_r * 0.6, vis->accent_g * 0.6, vis->accent_b * 0.6);
}

// Draw the maze grid with walls in beautiful gradients
void draw_maze_grid(cairo_t *cr, Maze *maze, double cell_size, double offset_x, double offset_y,
                    double wall_r, double wall_g, double wall_b, double path_r, double path_g, double path_b) {
    cairo_set_line_width(cr, 1.5);
    
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            double cx = offset_x + x * cell_size;
            double cy = offset_y + y * cell_size;
            
            MazeCell *cell = &maze->cells[y][x];
            
            // Colorful walls - gradient based on position
            double hue = fmod((x * 0.15 + y * 0.15), 1.0);
            double h = hue * 6.0;
            double c = 0.8;  // Saturation for walls
            double x_val = c * (1.0 - fabs(fmod(h, 2.0) - 1.0));
            
            int hi = (int)h;
            double r, g, b;
            switch(hi) {
                case 0: r = c; g = x_val; b = 0; break;           // Red-Yellow
                case 1: r = x_val; g = c; b = 0; break;           // Yellow-Green
                case 2: r = 0; g = c; b = x_val; break;           // Green-Cyan
                case 3: r = 0; g = x_val; b = c; break;           // Cyan-Blue
                case 4: r = x_val; g = 0; b = c; break;           // Blue-Magenta
                default: r = c; g = 0; b = x_val; break;          // Magenta-Red
            }
            
            // Draw north wall
            if (cell->walls[DIR_NORTH]) {
                cairo_set_source_rgb(cr, r, g, b);
                cairo_move_to(cr, cx, cy);
                cairo_line_to(cr, cx + cell_size, cy);
                cairo_stroke(cr);
            }
            
            // Draw east wall
            if (cell->walls[DIR_EAST]) {
                cairo_set_source_rgb(cr, r, g, b);
                cairo_move_to(cr, cx + cell_size, cy);
                cairo_line_to(cr, cx + cell_size, cy + cell_size);
                cairo_stroke(cr);
            }
            
            // Draw south wall
            if (cell->walls[DIR_SOUTH]) {
                cairo_set_source_rgb(cr, r, g, b);
                cairo_move_to(cr, cx, cy + cell_size);
                cairo_line_to(cr, cx + cell_size, cy + cell_size);
                cairo_stroke(cr);
            }
            
            // Draw west wall
            if (cell->walls[DIR_WEST]) {
                cairo_set_source_rgb(cr, r, g, b);
                cairo_move_to(cr, cx, cy);
                cairo_line_to(cr, cx, cy + cell_size);
                cairo_stroke(cr);
            }
        }
    }
}

// Draw the generating algorithm snake with orthogonal-only paths
void draw_generation_snake(cairo_t *cr, Maze *maze, double cell_size, double offset_x, double offset_y,
                           double snake_r, double snake_g, double snake_b, double alpha) {
    if (maze->snake_length < 1) return;
    
    cairo_set_line_width(cr, cell_size * 0.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    
    // Draw the trail with orthogonal (grid-aligned) paths
    for (int i = 0; i < maze->snake_length - 1; i++) {
        double fade = (double)(i + 1) / maze->snake_length;
        double current_alpha = alpha * fade;
        
        // Colorful trail - cycle through rainbow
        double hue = fmod(i * 0.15 + maze->generation_progress * 2.0, 1.0);
        double h = hue * 6.0;
        double c = 1.0;  // Full saturation
        double x_val = c * (1.0 - fabs(fmod(h, 2.0) - 1.0));
        
        int hi = (int)h;
        double r, g, b;
        switch(hi) {
            case 0: r = c; g = x_val; b = 0; break;
            case 1: r = x_val; g = c; b = 0; break;
            case 2: r = 0; g = c; b = x_val; break;
            case 3: r = 0; g = x_val; b = c; break;
            case 4: r = x_val; g = 0; b = c; break;
            default: r = c; g = 0; b = x_val; break;
        }
        
        // Always draw orthogonal paths (only horizontal or vertical, never diagonal)
        int x1 = maze->snake_x[i];
        int y1 = maze->snake_y[i];
        int x2 = maze->snake_x[i + 1];
        int y2 = maze->snake_y[i + 1];
        
        double px1 = offset_x + (x1 + 0.5) * cell_size;
        double py1 = offset_y + (y1 + 0.5) * cell_size;
        double px2 = offset_x + (x2 + 0.5) * cell_size;
        double py2 = offset_y + (y2 + 0.5) * cell_size;
        
        cairo_set_source_rgba(cr, r, g, b, current_alpha);
        
        // Draw orthogonal path: move in X first, then Y (grid-aligned)
        if (x1 != x2) {
            // Horizontal movement
            cairo_move_to(cr, px1, py1);
            cairo_line_to(cr, px2, py1);
            cairo_stroke(cr);
        }
        if (y1 != y2) {
            // Vertical movement
            cairo_move_to(cr, px2, py1);
            cairo_line_to(cr, px2, py2);
            cairo_stroke(cr);
        }
    }
    
    // Draw snake head with colorful glow
    if (maze->snake_length > 0) {
        int head_idx = maze->snake_length - 1;
        double head_x = offset_x + (maze->snake_x[head_idx] + 0.5) * cell_size;
        double head_y = offset_y + (maze->snake_y[head_idx] + 0.5) * cell_size;
        double head_radius = cell_size * 0.25;
        
        // Rainbow glow effect
        double hue = fmod(maze->generation_progress * 3.0, 1.0);
        double h = hue * 6.0;
        double c = 1.0;
        double x_val = c * (1.0 - fabs(fmod(h, 2.0) - 1.0));
        
        int hi = (int)h;
        double r, g, b;
        switch(hi) {
            case 0: r = c; g = x_val; b = 0; break;
            case 1: r = x_val; g = c; b = 0; break;
            case 2: r = 0; g = c; b = x_val; break;
            case 3: r = 0; g = x_val; b = c; break;
            case 4: r = x_val; g = 0; b = c; break;
            default: r = c; g = 0; b = x_val; break;
        }
        
        // Glow layers
        for (int g_layer = 3; g_layer > 0; g_layer--) {
            double glow_radius = head_radius + (3.0 - g_layer) * 3.0;
            double glow_alpha = alpha * 0.3 / g_layer;
            cairo_set_source_rgba(cr, r, g, b, glow_alpha);
            cairo_arc(cr, head_x, head_y, glow_radius, 0, 2 * M_PI);
            cairo_fill(cr);
        }
        
        // Head circle - bright color
        cairo_set_source_rgba(cr, r, g, b, alpha);
        cairo_arc(cr, head_x, head_y, head_radius, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

// Draw cells marked as "in path" during generation with vibrant colors
void draw_active_path(cairo_t *cr, Maze *maze, double cell_size, double offset_x, double offset_y,
                      double active_r, double active_g, double active_b, double alpha) {
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            if (maze->cells[y][x].in_path) {
                double cx = offset_x + x * cell_size;
                double cy = offset_y + y * cell_size;
                
                // Bright vibrant colors for the active generation path
                // Cycle through bright colors
                double color_idx = fmod((x * 0.3 + y * 0.3 + maze->generation_progress * 5.0), 6.0);
                double r, g, b;
                
                if (color_idx < 1.0) { r = 1.0; g = color_idx; b = 0; }           // Red to Yellow
                else if (color_idx < 2.0) { r = 2.0 - color_idx; g = 1.0; b = 0; }  // Yellow to Green
                else if (color_idx < 3.0) { r = 0; g = 1.0; b = color_idx - 2.0; }  // Green to Cyan
                else if (color_idx < 4.0) { r = 0; g = 4.0 - color_idx; b = 1.0; }  // Cyan to Blue
                else if (color_idx < 5.0) { r = color_idx - 4.0; g = 0; b = 1.0; }  // Blue to Magenta
                else { r = 1.0; g = 0; b = 6.0 - color_idx; }                       // Magenta to Red
                
                cairo_set_source_rgba(cr, r, g, b, alpha * 0.9);
                cairo_rectangle(cr, cx + 1, cy + 1, cell_size - 2, cell_size - 2);
                cairo_fill(cr);
            }
        }
    }
}

// Draw visited cells with beautiful gradient colors
void draw_visited_cells(cairo_t *cr, Maze *maze, double cell_size, double offset_x, double offset_y,
                        double visited_r, double visited_g, double visited_b, double alpha) {
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            if (maze->cells[y][x].visited && !maze->cells[y][x].in_path) {
                double cx = offset_x + x * cell_size;
                double cy = offset_y + y * cell_size;
                
                // Create rainbow colors based on position
                double hue = fmod((x + y) * 0.05, 1.0);  // Cycle through hues
                double r, g, b;
                
                // HSV to RGB conversion for rainbow effect
                double h = hue * 6.0;
                double c = 0.6;  // Saturation
                double x_val = c * (1.0 - fabs(fmod(h, 2.0) - 1.0));
                
                int hi = (int)h;
                switch(hi) {
                    case 0: r = c; g = x_val; b = 0; break;
                    case 1: r = x_val; g = c; b = 0; break;
                    case 2: r = 0; g = c; b = x_val; break;
                    case 3: r = 0; g = x_val; b = c; break;
                    case 4: r = x_val; g = 0; b = c; break;
                    default: r = c; g = 0; b = x_val; break;
                }
                
                cairo_set_source_rgba(cr, r, g, b, alpha * 0.7);
                cairo_rectangle(cr, cx + 2, cy + 2, cell_size - 4, cell_size - 4);
                cairo_fill(cr);
            }
        }
    }
}

// Complete maze visualization with all elements
void draw_maze_complete(cairo_t *cr, Maze *maze, 
                        int width, int height,
                        double wall_r, double wall_g, double wall_b,
                        double path_r, double path_g, double path_b,
                        double snake_r, double snake_g, double snake_b,
                        double active_r, double active_g, double active_b) {
    
    // Calculate cell size to fit maze in viewport
    double cell_size_x = (width * 0.9) / maze->width;
    double cell_size_y = (height * 0.9) / maze->height;
    double cell_size = (cell_size_x < cell_size_y) ? cell_size_x : cell_size_y;
    
    // Center the maze
    double offset_x = (width - maze->width * cell_size) / 2.0;
    double offset_y = (height - maze->height * cell_size) / 2.0;
    
    // Ensure minimum visible size
    if (cell_size < 2.0) cell_size = 2.0;
    
    // Draw background
    cairo_set_source_rgb(cr, 0.05, 0.05, 0.08);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    
    // Draw visited cells (optional, subtle background)
    draw_visited_cells(cr, maze, cell_size, offset_x, offset_y, 
                       0.15, 0.15, 0.2, 0.3);
    
    // Draw active path during generation
    draw_active_path(cr, maze, cell_size, offset_x, offset_y,
                     active_r * 0.6, active_g * 0.6, active_b * 0.6, 0.4);
    
    // Draw maze grid
    draw_maze_grid(cr, maze, cell_size, offset_x, offset_y,
                   wall_r, wall_g, wall_b, path_r, path_g, path_b);
    
    // Draw the snake trail
    draw_generation_snake(cr, maze, cell_size, offset_x, offset_y,
                          snake_r, snake_g, snake_b, 0.8);
    
    // Draw generation progress text
    if (maze->generating) {
        cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 16);
        cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
        
        char progress_text[256];
        snprintf(progress_text, sizeof(progress_text), 
                 "Generating... %.1f%%", maze->generation_progress * 100.0);
        
        cairo_move_to(cr, 20, height - 30);
        cairo_show_text(cr, progress_text);
    } else {
        cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 16);
        cairo_set_source_rgb(cr, 0.4, 0.9, 0.4);
        
        char complete_text[256];
        snprintf(complete_text, sizeof(complete_text), 
                 "Maze Complete - %d x %d", maze->width, maze->height);
        
        cairo_move_to(cr, 20, height - 30);
        cairo_show_text(cr, complete_text);
    }
}

// Simplified maze visualization (just walls and snake)
void draw_maze_simple(cairo_t *cr, Maze *maze, int width, int height) {
    double cell_size_x = (width * 0.9) / maze->width;
    double cell_size_y = (height * 0.9) / maze->height;
    double cell_size = (cell_size_x < cell_size_y) ? cell_size_x : cell_size_y;
    
    double offset_x = (width - maze->width * cell_size) / 2.0;
    double offset_y = (height - maze->height * cell_size) / 2.0;
    
    // Draw background
    cairo_set_source_rgb(cr, 0.05, 0.05, 0.08);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    
    // Draw maze
    draw_maze_grid(cr, maze, cell_size, offset_x, offset_y,
                   0.9, 0.9, 0.9, 0.0, 0.0, 0.0);
    
    // Draw snake
    draw_generation_snake(cr, maze, cell_size, offset_x, offset_y,
                          0.2, 0.8, 1.0, 0.9);
}

// Initialize maze with all walls intact
void maze_init(Maze *maze, int width, int height) {
    maze->width = width > MAX_MAZE_WIDTH ? MAX_MAZE_WIDTH : width;
    maze->height = height > MAX_MAZE_HEIGHT ? MAX_MAZE_HEIGHT : height;
    maze->snake_length = 0;
    maze->snake_progress = 0.0;
    maze->stack_size = 0;
    maze->generating = true;
    maze->generation_progress = 0.0;
    maze->cells_visited = 0;
    maze->cell_animation_speed = 25.0;  // 25 cells per second - fast!
    maze->generation_timer = 0.0;
    
    // Initialize all cells with all walls intact
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            MazeCell *cell = &maze->cells[y][x];
            cell->walls[DIR_NORTH] = true;
            cell->walls[DIR_EAST] = true;
            cell->walls[DIR_SOUTH] = true;
            cell->walls[DIR_WEST] = true;
            cell->visited = false;
            cell->in_path = false;
        }
    }
    
    // Seed random number generator
    srand((unsigned int)time(NULL));
}

// Clean up maze resources
void maze_cleanup(Maze *maze) {
    memset(maze, 0, sizeof(Maze));
}

// Reset generation for creating a new maze
void maze_reset_generation(Maze *maze) {
    maze->snake_length = 0;
    maze->snake_progress = 0.0;
    maze->stack_size = 0;
    maze->generating = true;
    maze->generation_progress = 0.0;
    maze->cells_visited = 0;
    maze->generation_timer = 0.0;
    
    // Clear all visited and path markers
    for (int y = 0; y < maze->height; y++) {
        for (int x = 0; x < maze->width; x++) {
            maze->cells[y][x].visited = false;
            maze->cells[y][x].in_path = false;
            maze->cells[y][x].walls[DIR_NORTH] = true;
            maze->cells[y][x].walls[DIR_EAST] = true;
            maze->cells[y][x].walls[DIR_SOUTH] = true;
            maze->cells[y][x].walls[DIR_WEST] = true;
        }
    }
}

// Get the opposite direction
int maze_get_opposite_direction(int dir) {
    return (dir + 2) % 4;
}

// Add a position to the snake trail
void maze_add_snake_step(Maze *maze, int x, int y) {
    if (maze->snake_length < MAX_SNAKE_LENGTH) {
        maze->snake_x[maze->snake_length] = x;
        maze->snake_y[maze->snake_length] = y;
        maze->snake_length++;
    }
}

// Remove wall between two cells
void maze_remove_wall(Maze *maze, int x, int y, Direction dir) {
    if (x < 0 || x >= maze->width || y < 0 || y >= maze->height) {
        return;
    }
    
    MazeCell *current = &maze->cells[y][x];
    current->walls[dir] = false;
    
    // Remove opposite wall from neighbor
    int nx = x;
    int ny = y;
    
    if (dir == DIR_NORTH) ny--;
    else if (dir == DIR_SOUTH) ny++;
    else if (dir == DIR_EAST) nx++;
    else if (dir == DIR_WEST) nx--;
    
    if (nx >= 0 && nx < maze->width && ny >= 0 && ny < maze->height) {
        MazeCell *neighbor = &maze->cells[ny][nx];
        neighbor->walls[maze_get_opposite_direction(dir)] = false;
    }
}

// Check if a cell has unvisited neighbors
bool maze_has_unvisited_neighbor(Maze *maze, int x, int y, Direction *out_dir) {
    // Create array of possible directions
    Direction directions[4] = {DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST};
    
    // Shuffle directions (Fisher-Yates shuffle)
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        Direction temp = directions[i];
        directions[i] = directions[j];
        directions[j] = temp;
    }
    
    // Check each direction
    for (int i = 0; i < 4; i++) {
        Direction dir = directions[i];
        int nx = x;
        int ny = y;
        
        if (dir == DIR_NORTH) ny--;
        else if (dir == DIR_SOUTH) ny++;
        else if (dir == DIR_EAST) nx++;
        else if (dir == DIR_WEST) nx--;
        
        // Check bounds and if unvisited
        if (nx >= 0 && nx < maze->width && ny >= 0 && ny < maze->height) {
            if (!maze->cells[ny][nx].visited) {
                *out_dir = dir;
                return true;
            }
        }
    }
    
    return false;
}

// Carve a passage between two cells
void maze_carve_passage(Maze *maze, int from_x, int from_y, Direction dir) {
    // Add current cell to snake
    maze_add_snake_step(maze, from_x, from_y);
    
    // Remove wall
    maze_remove_wall(maze, from_x, from_y, dir);
    
    // Calculate neighbor position
    int to_x = from_x;
    int to_y = from_y;
    
    if (dir == DIR_NORTH) to_y--;
    else if (dir == DIR_SOUTH) to_y++;
    else if (dir == DIR_EAST) to_x++;
    else if (dir == DIR_WEST) to_x--;
    
    // Mark neighbor as visited
    if (to_x >= 0 && to_x < maze->width && to_y >= 0 && to_y < maze->height) {
        maze->cells[to_y][to_x].visited = true;
    }
}

// Recursive backtracker algorithm - one step
void maze_generate_step(Maze *maze) {
    if (!maze->generating || maze->stack_size == 0) {
        return;
    }
    
    // Get current cell from stack
    int current_x = maze->stack_x[maze->stack_size - 1];
    int current_y = maze->stack_y[maze->stack_size - 1];
    
    MazeCell *current = &maze->cells[current_y][current_x];
    current->in_path = true;
    
    Direction next_dir;
    
    // Check for unvisited neighbors
    if (maze_has_unvisited_neighbor(maze, current_x, current_y, &next_dir)) {
        // Calculate neighbor position
        int nx = current_x;
        int ny = current_y;
        
        if (next_dir == DIR_NORTH) ny--;
        else if (next_dir == DIR_SOUTH) ny++;
        else if (next_dir == DIR_EAST) nx++;
        else if (next_dir == DIR_WEST) nx--;
        
        // Mark neighbor as visited
        maze->cells[ny][nx].visited = true;
        
        // Carve passage
        maze_carve_passage(maze, current_x, current_y, next_dir);
        
        // Push neighbor onto stack
        maze->stack_x[maze->stack_size] = nx;
        maze->stack_y[maze->stack_size] = ny;
        maze->stack_size++;
        
        maze->cells_visited++;
    } else {
        // Backtrack
        current->in_path = false;
        maze->stack_size--;
    }
}

// Complete maze generation
void maze_generate(Maze *maze) {
    // Start from top-left corner
    int start_x = 0;
    int start_y = 0;
    
    maze->cells[start_y][start_x].visited = true;
    maze->stack_x[0] = start_x;
    maze->stack_y[0] = start_y;
    maze->stack_size = 1;
    maze->cells_visited = 1;
    maze->generating = true;
    maze->generation_progress = 0.0;
    
    // DON'T generate here! Let maze_update() do it step by step
}

// Update maze animation
void maze_update(Maze *maze, double dt) {
    if (maze->generating) {
        maze->generation_timer += dt;
        
        // Process one cell per animation frame based on speed
        double cells_to_process = maze->cell_animation_speed * dt;
        
        while (cells_to_process >= 1.0 && maze->generating) {
            maze_generate_step(maze);
            cells_to_process -= 1.0;
            
            if (maze->stack_size == 0) {
                maze->generating = false;
            }
        }
        
        // Update generation progress
        int total_cells = maze->width * maze->height;
        maze->generation_progress = (double)maze->cells_visited / total_cells;
    }
    
    // Update snake animation progress
    if (maze->snake_length > 0) {
        maze->snake_progress += dt * 2.0;  // 2.0 = 2 units per second
        
        // Loop animation
        if (maze->snake_progress > maze->snake_length) {
            maze->snake_progress = 0.0;
        }
    }
}
