#include "visualization.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

typedef struct {
    int wall_type;
    double hit_x;
    double hit_y;
    double dist;
} RayHit;

void init_maze3d_system(Visualizer *vis) {
    Maze3D *maze = &vis->maze3d;
    
    // Seed random for unique maze generation on every restart and solve
    srand((unsigned int)time(NULL) + rand());
    
    // Initialize maze
    memset(maze->cells, 0x0F, sizeof(maze->cells)); // All walls initially
    maze->path_length = 0;
    maze->current_path_index = 0;
    maze->solved = FALSE;
    maze->maze_time = 0.0;
    maze->audio_pulse = 0.0;
    maze->auto_solve = TRUE;
    maze->move_timer = 0.0;
    
    // Initialize player at start position
    maze->player.x = 1.5;
    maze->player.y = 1.5;
    maze->player.angle = 0.0;
    maze->player.target_angle = 0.0;
    maze->player.map_x = 1;
    maze->player.map_y = 1;
    maze->player.move_speed = 2.0;
    maze->player.turn_speed = 3.0;
    
    // Initialize penguin at exit
    maze->exit_x = MAZE_WIDTH - 2;
    maze->exit_y = MAZE_HEIGHT - 2;
    maze->penguin.x = maze->exit_x + 0.5;
    maze->penguin.y = maze->exit_y + 0.5;
    maze->penguin.z = 0.0;
    maze->penguin.rotation = 0.0;
    maze->penguin.bob_offset = 0.0;
    maze->penguin.scale = 1.0;
    maze->penguin.found = FALSE;
    maze->penguin.found_time = 0.0;
    
    // Generate maze first
    generate_maze(maze);
    solve_maze(maze);
    
    // NOW initialize rats with valid positions
    for (int i = 0; i < MAX_RATS; i++) {
        find_valid_spawn_position(maze, &maze->rats[i].x, &maze->rats[i].y, 0.15);
        maze->rats[i].angle = (rand() / (double)RAND_MAX) * 2 * M_PI;
        maze->rats[i].speed = 0.5 + (rand() / (double)RAND_MAX) * 0.5;
        maze->rats[i].bob_offset = 0.0;
        maze->rats[i].active = TRUE;
    }
    
    // NOW initialize elephants with valid positions
    for (int i = 0; i < MAX_ELEPHANTS; i++) {
        find_valid_spawn_position(maze, &maze->elephants[i].x, &maze->elephants[i].y, 0.4);
        maze->elephants[i].angle = (rand() / (double)RAND_MAX) * 2 * M_PI;
        maze->elephants[i].speed = 0.3 + (rand() / (double)RAND_MAX) * 0.2;
        maze->elephants[i].bob_offset = 0.0;
        maze->elephants[i].active = TRUE;
    }
    
    // Initialize wall colors with vibrant, distinct colors for each direction
    // North - Blue
    hsv_to_rgb(210.0, 0.8, 0.9, &maze->wall_colors[0][0], 
               &maze->wall_colors[0][1], &maze->wall_colors[0][2]);
    // East - Orange
    hsv_to_rgb(30.0, 0.85, 0.95, &maze->wall_colors[1][0], 
               &maze->wall_colors[1][1], &maze->wall_colors[1][2]);
    // South - Purple
    hsv_to_rgb(280.0, 0.75, 0.85, &maze->wall_colors[2][0], 
               &maze->wall_colors[2][1], &maze->wall_colors[2][2]);
    // West - Green
    hsv_to_rgb(140.0, 0.8, 0.85, &maze->wall_colors[3][0], 
               &maze->wall_colors[3][1], &maze->wall_colors[3][2]);
}

// Recursive backtracking maze generation
void carve_maze(Maze3D *maze, int x, int y, unsigned char visited[MAZE_HEIGHT][MAZE_WIDTH]) {
    visited[y][x] = 1;
    
    // Directions: N, E, S, W
    int dirs[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    int wall_flags[4] = {WALL_NORTH, WALL_EAST, WALL_SOUTH, WALL_WEST};
    int opposite_flags[4] = {WALL_SOUTH, WALL_WEST, WALL_NORTH, WALL_EAST};
    
    // Shuffle directions
    for (int i = 0; i < 4; i++) {
        int j = rand() % 4;
        int temp_x = dirs[i][0], temp_y = dirs[i][1], temp_flag = wall_flags[i], temp_opp = opposite_flags[i];
        dirs[i][0] = dirs[j][0]; dirs[i][1] = dirs[j][1];
        wall_flags[i] = wall_flags[j]; opposite_flags[i] = opposite_flags[j];
        dirs[j][0] = temp_x; dirs[j][1] = temp_y;
        wall_flags[j] = temp_flag; opposite_flags[j] = temp_opp;
    }
    
    for (int i = 0; i < 4; i++) {
        int nx = x + dirs[i][0];
        int ny = y + dirs[i][1];
        
        if (nx >= 0 && nx < MAZE_WIDTH && ny >= 0 && ny < MAZE_HEIGHT && !visited[ny][nx]) {
            // Remove walls between cells
            maze->cells[y][x] &= ~wall_flags[i];
            maze->cells[ny][nx] &= ~opposite_flags[i];
            carve_maze(maze, nx, ny, visited);
        }
    }
}

void generate_maze(Maze3D *maze) {
    // Fill with walls
    memset(maze->cells, 0x0F, sizeof(maze->cells));
    
    unsigned char visited[MAZE_HEIGHT][MAZE_WIDTH];
    memset(visited, 0, sizeof(visited));
    
    // Generate maze using recursive backtracking
    carve_maze(maze, 1, 1, visited);
}

// BFS maze solving
void solve_maze(Maze3D *maze) {
    int start_x = 1, start_y = 1;
    int end_x = maze->exit_x;
    int end_y = maze->exit_y;
    
    unsigned char visited[MAZE_HEIGHT][MAZE_WIDTH];
    Point parent[MAZE_HEIGHT][MAZE_WIDTH];
    memset(visited, 0, sizeof(visited));
    
    Point queue[MAZE_WIDTH * MAZE_HEIGHT];
    int queue_start = 0, queue_end = 0;
    
    queue[queue_end++] = (Point){start_x, start_y};
    visited[start_y][start_x] = 1;
    parent[start_y][start_x] = (Point){-1, -1};
    
    int dirs[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    int wall_checks[4] = {WALL_NORTH, WALL_EAST, WALL_SOUTH, WALL_WEST};
    
    gboolean found = FALSE;
    
    while (queue_start < queue_end && !found) {
        Point current = queue[queue_start++];
        
        if (current.x == end_x && current.y == end_y) {
            found = TRUE;
            break;
        }
        
        for (int i = 0; i < 4; i++) {
            if (!(maze->cells[current.y][current.x] & wall_checks[i])) {
                int nx = current.x + dirs[i][0];
                int ny = current.y + dirs[i][1];
                
                if (nx >= 0 && nx < MAZE_WIDTH && ny >= 0 && ny < MAZE_HEIGHT && !visited[ny][nx]) {
                    visited[ny][nx] = 1;
                    parent[ny][nx] = current;
                    queue[queue_end++] = (Point){nx, ny};
                }
            }
        }
    }
    
    // Reconstruct path
    if (found) {
        maze->path_length = 0;
        Point current = (Point){end_x, end_y};
        
        while (current.x != -1 && maze->path_length < MAX_PATH_LENGTH) {
            maze->path[maze->path_length++] = current;
            current = parent[current.y][current.x];
        }
        
        // Reverse path to go from start to end
        for (int i = 0; i < maze->path_length / 2; i++) {
            Point temp = maze->path[i];
            maze->path[i] = maze->path[maze->path_length - 1 - i];
            maze->path[maze->path_length - 1 - i] = temp;
        }
        
        maze->solved = TRUE;
    }
}

gboolean is_valid_position(Maze3D *maze, double x, double y, double radius) {
    int map_x = (int)x;
    int map_y = (int)y;
    
    if (map_x < 1 || map_x >= MAZE_WIDTH - 1 || map_y < 1 || map_y >= MAZE_HEIGHT - 1) {
        return FALSE;
    }
    
    double frac_x = x - map_x;
    double frac_y = y - map_y;
    
    // Relax collision checks - creatures can get closer to walls
    if (maze->cells[map_y][map_x] & WALL_NORTH && frac_y - radius < 0.2) return FALSE;
    if (maze->cells[map_y][map_x] & WALL_SOUTH && frac_y + radius > 0.8) return FALSE;
    if (maze->cells[map_y][map_x] & WALL_WEST && frac_x - radius < 0.2) return FALSE;
    if (maze->cells[map_y][map_x] & WALL_EAST && frac_x + radius > 0.8) return FALSE;
    
    return TRUE;
}

void find_valid_spawn_position(Maze3D *maze, double *x, double *y, double radius) {
    int max_attempts = 100;
    for (int attempt = 0; attempt < max_attempts; attempt++) {
        int test_x = 2 + rand() % (MAZE_WIDTH - 4);
        int test_y = 2 + rand() % (MAZE_HEIGHT - 4);
        double test_pos_x = test_x + 0.5;
        double test_pos_y = test_y + 0.5;
        
        if (is_valid_position(maze, test_pos_x, test_pos_y, radius)) {
            *x = test_pos_x;
            *y = test_pos_y;
            return;
        }
    }
    // Fallback to center of map
    *x = MAZE_WIDTH / 2.0;
    *y = MAZE_HEIGHT / 2.0;
}

void update_maze3d(Visualizer *vis, double dt) {
    Maze3D *maze = &vis->maze3d;
    Player *player = &maze->player;
    
    maze->maze_time += dt;
    maze->move_timer += dt;
    
    // Update audio pulse
    double avg_intensity = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        avg_intensity += vis->frequency_bands[i];
    }
    avg_intensity /= VIS_FREQUENCY_BARS;
    maze->audio_pulse = maze->audio_pulse * 0.8 + avg_intensity * 0.2;
    
    // Update rats - random wandering with collision detection
    for (int i = 0; i < MAX_RATS; i++) {
        Creature *rat = &maze->rats[i];
        if (!rat->active) continue;
        
        // Randomly change direction
        if (fmod(maze->maze_time * rat->speed, 3.0) < dt) {
            rat->angle += (rand() / (double)RAND_MAX - 0.5) * M_PI * 0.5;
        }
        
        // Try multiple small steps
        double step_x = cos(rat->angle) * rat->speed * dt;
        double step_y = sin(rat->angle) * rat->speed * dt;
        double new_x = rat->x + step_x;
        double new_y = rat->y + step_y;
        
        // Try full step
        if (is_valid_position(maze, new_x, new_y, 0.1)) {
            rat->x = new_x;
            rat->y = new_y;
        } 
        // Try half step
        else if (is_valid_position(maze, rat->x + step_x * 0.5, rat->y + step_y * 0.5, 0.1)) {
            rat->x += step_x * 0.5;
            rat->y += step_y * 0.5;
        }
        // Can't move, turn
        else {
            rat->angle = (rand() / (double)RAND_MAX) * 2 * M_PI;
        }
        
        rat->bob_offset = sin(maze->maze_time * 4.0 + i) * 0.1;
    }
    
    // Update elephants - slower wandering with collision detection
    for (int i = 0; i < MAX_ELEPHANTS; i++) {
        Creature *elephant = &maze->elephants[i];
        if (!elephant->active) continue;
        
        // Less frequent direction changes
        if (fmod(maze->maze_time * elephant->speed, 5.0) < dt) {
            elephant->angle += (rand() / (double)RAND_MAX - 0.5) * M_PI * 0.3;
        }
        
        // Try multiple small steps
        double step_x = cos(elephant->angle) * elephant->speed * dt;
        double step_y = sin(elephant->angle) * elephant->speed * dt;
        double new_x = elephant->x + step_x;
        double new_y = elephant->y + step_y;
        
        // Try full step
        if (is_valid_position(maze, new_x, new_y, 0.3)) {
            elephant->x = new_x;
            elephant->y = new_y;
        }
        // Try half step
        else if (is_valid_position(maze, elephant->x + step_x * 0.5, elephant->y + step_y * 0.5, 0.3)) {
            elephant->x += step_x * 0.5;
            elephant->y += step_y * 0.5;
        }
        // Can't move, turn
        else {
            elephant->angle = (rand() / (double)RAND_MAX) * 2 * M_PI;
        }
        
        elephant->bob_offset = sin(maze->maze_time * 1.5 + i) * 0.2;
    }
    
    // Auto-solve navigation
    if (maze->auto_solve && maze->solved && maze->move_timer > 0.1) {
        if (maze->current_path_index < maze->path_length - 1) {
            Point target = maze->path[maze->current_path_index + 1];
            double target_x = target.x + 0.5;
            double target_y = target.y + 0.5;
            
            double dx = target_x - player->x;
            double dy = target_y - player->y;
            double distance = sqrt(dx * dx + dy * dy);
            
            if (distance < 0.3) {
                maze->current_path_index++;
            } else {
                double target_angle = atan2(dy, dx);
                double angle_diff = target_angle - player->angle;
                while (angle_diff > M_PI) angle_diff -= 2 * M_PI;
                while (angle_diff < -M_PI) angle_diff += 2 * M_PI;
                
                if (fabs(angle_diff) < 0.1) {
                    player->x += cos(player->angle) * player->move_speed * dt;
                    player->y += sin(player->angle) * player->move_speed * dt;
                } else {
                    player->angle += angle_diff * player->turn_speed * dt;
                }
            }
        }
    }
    
    // Update player grid position
    player->map_x = (int)player->x;
    player->map_y = (int)player->y;
    
    // Check if player found penguin
    double dx = player->x - maze->penguin.x;
    double dy = player->y - maze->penguin.y;
    double dist_to_penguin = sqrt(dx * dx + dy * dy);
    
    if (dist_to_penguin < 1.5 && !maze->penguin.found) {
        maze->penguin.found = TRUE;
        maze->penguin.found_time = maze->maze_time;
    }
    
    // Update penguin animation
    maze->penguin.rotation += dt * 0.5;
    maze->penguin.bob_offset = sin(maze->maze_time * 3.0) * 0.15;
    
    if (maze->penguin.found) {
        double time_since_found = maze->maze_time - maze->penguin.found_time;
        maze->penguin.scale = 1.0 + sin(time_since_found * 5.0) * 0.2;
    }
    
    // Regenerate maze after penguin is found for a while
    if (maze->penguin.found && maze->maze_time - maze->penguin.found_time > 5.0) {
        init_maze3d_system(vis);
    }
}

void draw_maze3d(Visualizer *vis, cairo_t *cr) {
    Maze3D *maze = &vis->maze3d;
    Player *player = &maze->player;
    
    if (vis->width <= 0 || vis->height <= 0) return;
    
    // Disable antialiasing to prevent color blending artifacts
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    
    double half_width = vis->width / 2.0;
    double half_height = vis->height / 2.0;
    
    // Draw textured sky with gradient
    cairo_pattern_t *sky_pattern = cairo_pattern_create_linear(0, 0, 0, half_height);
    cairo_pattern_add_color_stop_rgb(sky_pattern, 0, 0.3, 0.6, 1.0);
    cairo_pattern_add_color_stop_rgb(sky_pattern, 1, 0.6, 0.8, 1.0);
    cairo_set_source(cr, sky_pattern);
    cairo_rectangle(cr, 0, 0, vis->width, half_height);
    cairo_fill(cr);
    cairo_pattern_destroy(sky_pattern);
    
    // Draw clouds in the sky - location aware based on player position
    double player_angle_normalized = player->angle;
    while (player_angle_normalized < 0) player_angle_normalized += 2 * M_PI;
    while (player_angle_normalized > 2 * M_PI) player_angle_normalized -= 2 * M_PI;
    
    // Map player viewing angle to horizontal cloud offset
    double cloud_offset = (player_angle_normalized / (2 * M_PI)) * vis->width * 4;
    
    for (int cloud_idx = 0; cloud_idx < 6; cloud_idx++) {
        // Position clouds based on viewing angle
        int cloud_base_x = cloud_idx * 100;
        int cloud_x = (int)(cloud_base_x - cloud_offset) % (int)(vis->width * 2);
        if (cloud_x < 0) cloud_x += vis->width * 2;
        cloud_x = cloud_x % (int)vis->width;
        
        // Use cloud index as seed for consistent cloud properties
        int cloud_seed = cloud_idx * 12345;
        double cloud_noise = sin(cloud_seed * 0.02) * 0.5 + 0.5;
        
        if (cloud_noise > 0.3) {
            double cloud_y = 40 + sin(cloud_idx * 0.5) * 50 + cloud_noise * 40;
            double cloud_opacity = (cloud_noise - 0.3) * 1.2;
            if (cloud_opacity > 0.6) cloud_opacity = 0.6;
            
            // Vary cloud size based on cloud index
            double cloud_size_mult = 0.8 + cloud_noise * 0.4;
            
            // Draw puffy cloud shapes
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, cloud_opacity);
            cairo_arc(cr, cloud_x - 15 * cloud_size_mult, cloud_y, 12.0 * cloud_size_mult, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_arc(cr, cloud_x, cloud_y - 8, 14.0 * cloud_size_mult, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_arc(cr, cloud_x + 15 * cloud_size_mult, cloud_y, 12.0 * cloud_size_mult, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
    
    // Draw textured floor with gradient
    cairo_pattern_t *floor_pattern = cairo_pattern_create_linear(0, half_height, 0, vis->height);
    cairo_pattern_add_color_stop_rgb(floor_pattern, 0, 0.25, 0.2, 0.15);
    cairo_pattern_add_color_stop_rgb(floor_pattern, 1, 0.15, 0.1, 0.08);
    cairo_set_source(cr, floor_pattern);
    cairo_rectangle(cr, 0, half_height, vis->width, half_height);
    cairo_fill(cr);
    cairo_pattern_destroy(floor_pattern);
    
    // Raycasting parameters
    double fov = M_PI / 3.0; // 60 degrees
    int num_rays = vis->width;  // One ray per pixel column
    
    // Store wall distances for z-buffering
    double *wall_distances = (double*)malloc(num_rays * sizeof(double));
    RayHit *ray_hits = (RayHit*)malloc(num_rays * sizeof(RayHit));
    
    // First pass: raycasting to find walls and store hit data
    for (int ray = 0; ray < num_rays; ray++) {
        double ray_angle = player->angle - fov / 2.0 + (ray / (double)num_rays) * fov;
        
        double ray_dx = cos(ray_angle);
        double ray_dy = sin(ray_angle);
        
        // Simple raycasting - step through until we hit a wall
        double ray_x = player->x;
        double ray_y = player->y;
        double step_size = 0.02;
        double dist = 0.0;
        int hit = 0;
        int wall_type = 0;
        
        while (dist < 20.0 && !hit) {
            ray_x += ray_dx * step_size;
            ray_y += ray_dy * step_size;
            dist += step_size;
            
            int map_x = (int)ray_x;
            int map_y = (int)ray_y;
            
            // Check bounds
            if (map_x < 0 || map_x >= MAZE_WIDTH || map_y < 0 || map_y >= MAZE_HEIGHT) {
                hit = 1;
                wall_type = 0;
                break;
            }
            
            // Get fractional position within cell
            double frac_x = ray_x - map_x;
            double frac_y = ray_y - map_y;
            
            // Check which wall we hit
            if (maze->cells[map_y][map_x] & WALL_NORTH && frac_y < 0.1) {
                hit = 1;
                wall_type = 0;
            } else if (maze->cells[map_y][map_x] & WALL_SOUTH && frac_y > 0.9) {
                hit = 1;
                wall_type = 2;
            } else if (maze->cells[map_y][map_x] & WALL_WEST && frac_x < 0.1) {
                hit = 1;
                wall_type = 3;
            } else if (maze->cells[map_y][map_x] & WALL_EAST && frac_x > 0.9) {
                hit = 1;
                wall_type = 1;
            }
        }
        
        // Fix fish-eye effect
        dist = dist * cos(ray_angle - player->angle);
        if (dist < 0.1) dist = 0.1;
        
        // Store wall distance for z-buffering
        wall_distances[ray] = dist;
        
        // Store ray hit data
        ray_hits[ray].wall_type = wall_type;
        ray_hits[ray].hit_x = ray_x;
        ray_hits[ray].hit_y = ray_y;
        ray_hits[ray].dist = dist;
        
        // Calculate wall height
        double wall_height = (vis->height / dist) * 0.5;
        if (wall_height > vis->height * 2) wall_height = vis->height * 2;
        
        // Round positions to integer pixels
        int top_y = (int)(half_height - wall_height / 2);
        int height = (int)wall_height;
        
        // Distance-based brightness
        double brightness = 1.0 / (1.0 + dist * 0.15);
        brightness *= (1.0 + maze->audio_pulse * 0.3);
        if (brightness > 1.0) brightness = 1.0;
        
        // Get base color for this wall direction
        double r = maze->wall_colors[wall_type][0] * brightness;
        double g = maze->wall_colors[wall_type][1] * brightness;
        double b = maze->wall_colors[wall_type][2] * brightness;
        
        // Draw single pixel wide column
        cairo_set_source_rgb(cr, r, g, b);
        cairo_rectangle(cr, ray, top_y, 1, height);
        cairo_fill(cr);
    }
    
    // Draw wall decorations using stored hit data
    int num_wall_bars = VIS_FREQUENCY_BARS;
    
    for (int ray = 0; ray < num_rays; ray++) {
        if (wall_distances[ray] > 0.5 && wall_distances[ray] < 10.0) {
            int wall_type = ray_hits[ray].wall_type;
            double hit_x = ray_hits[ray].hit_x;
            double hit_y = ray_hits[ray].hit_y;
            double dist = ray_hits[ray].dist;
            
            double wall_height = (vis->height / dist) * 0.5;
            if (wall_height > vis->height * 2) wall_height = vis->height * 2;
            
            int top_y = (int)(half_height - wall_height / 2);
            int center_y = (int)half_height;
            int height = (int)wall_height;
            
            // Use hit position for consistent texture coordinate
            double texture_u = 0;
            
            if (wall_type == 0 || wall_type == 2) { // North or South walls
                texture_u = fmod(hit_x, 1.0);
                if (texture_u < 0) texture_u += 1.0;
            } else { // East or West walls
                texture_u = fmod(hit_y, 1.0);
                if (texture_u < 0) texture_u += 1.0;
            }
            
            // Draw multi-colored bars on SOUTH walls (wall_type == 2)
            if (wall_type == 2) {
                int bar_idx = (int)(texture_u * num_wall_bars) % num_wall_bars;
                double bar_intensity = vis->frequency_bands[bar_idx];
                
                double bar_extend = bar_intensity * (wall_height * 0.35);
                double bar_top = center_y - bar_extend;
                double bar_bottom = center_y + bar_extend;
                
                // Multi-color gradient based on frequency
                double hue = (double)bar_idx / num_wall_bars;
                double bar_r, bar_g, bar_b;
                
                // Create rainbow spectrum
                if (hue < 0.166) {
                    bar_r = 1.0; bar_g = hue / 0.166; bar_b = 0.0;
                } else if (hue < 0.333) {
                    bar_r = 1.0 - (hue - 0.166) / 0.167; bar_g = 1.0; bar_b = 0.0;
                } else if (hue < 0.5) {
                    bar_r = 0.0; bar_g = 1.0; bar_b = (hue - 0.333) / 0.167;
                } else if (hue < 0.666) {
                    bar_r = 0.0; bar_g = 1.0 - (hue - 0.5) / 0.166; bar_b = 1.0;
                } else if (hue < 0.833) {
                    bar_r = (hue - 0.666) / 0.167; bar_g = 0.0; bar_b = 1.0;
                } else {
                    bar_r = 1.0; bar_g = 0.0; bar_b = 1.0 - (hue - 0.833) / 0.167;
                }
                
                // Add glow effect
                cairo_set_source_rgba(cr, bar_r, bar_g, bar_b, bar_intensity * 0.5);
                cairo_rectangle(cr, ray - 1, bar_top - 2, 3, bar_bottom - bar_top + 4);
                cairo_fill(cr);
                
                // Draw bright core
                cairo_set_source_rgba(cr, bar_r, bar_g, bar_b, 0.9);
                cairo_rectangle(cr, ray, bar_top, 1, bar_bottom - bar_top);
                cairo_fill(cr);
            }
            
            // Draw oscilloscope on EAST walls (wall_type == 1)
            else if (wall_type == 1) {
                int sample_idx = (int)(texture_u * VIS_SAMPLES) % VIS_SAMPLES;
                double sample = vis->audio_samples[sample_idx];
                
                double wave_y = center_y + sample * (wall_height * 0.4);
                
                // Draw grid lines
                if ((int)ray % 32 == 0) {
                    cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.3);
                    cairo_move_to(cr, ray, top_y);
                    cairo_line_to(cr, ray, top_y + height);
                    cairo_stroke(cr);
                }
                
                // Draw waveform line
                cairo_set_source_rgba(cr, 0.2, 1.0, 0.8, 0.8);
                cairo_set_line_width(cr, 1.5);
                cairo_move_to(cr, ray, wave_y);
                cairo_line_to(cr, ray + 1, wave_y);
                cairo_stroke(cr);
                
                // Add glow point
                cairo_set_source_rgba(cr, 0.2, 1.0, 0.8, 0.6);
                cairo_arc(cr, ray, wave_y, 1.0, 0, 2 * M_PI);
                cairo_fill(cr);
            }
            
            // Draw vertical bars on WEST walls (wall_type == 3)
            else if (wall_type == 3) {
                int bar_idx = (int)(texture_u * num_wall_bars) % num_wall_bars;
                double bar_intensity = vis->frequency_bands[bar_idx];
                
                // Draw tall vertical bars across the wall
                double bar_width = 2.0;
                double bar_height = bar_intensity * height * 0.6;
                double bar_x = ray;
                double bar_top = center_y - bar_height * 0.5;
                
                // Rainbow colors
                double hue = (double)bar_idx / num_wall_bars;
                double bar_r, bar_g, bar_b;
                
                if (hue < 0.166) {
                    bar_r = 1.0; bar_g = hue / 0.166; bar_b = 0.0;
                } else if (hue < 0.333) {
                    bar_r = 1.0 - (hue - 0.166) / 0.167; bar_g = 1.0; bar_b = 0.0;
                } else if (hue < 0.5) {
                    bar_r = 0.0; bar_g = 1.0; bar_b = (hue - 0.333) / 0.167;
                } else if (hue < 0.666) {
                    bar_r = 0.0; bar_g = 1.0 - (hue - 0.5) / 0.166; bar_b = 1.0;
                } else if (hue < 0.833) {
                    bar_r = (hue - 0.666) / 0.167; bar_g = 0.0; bar_b = 1.0;
                } else {
                    bar_r = 1.0; bar_g = 0.0; bar_b = 1.0 - (hue - 0.833) / 0.167;
                }
                
                cairo_set_source_rgba(cr, bar_r, bar_g, bar_b, 0.7);
                cairo_rectangle(cr, bar_x, bar_top, bar_width, bar_height);
                cairo_fill(cr);
            }
            
            // Draw waveform on NORTH walls (wall_type == 0)
            else if (wall_type == 0) {
                // Map texture coordinate to audio sample index
                int sample_idx = (int)(texture_u * VIS_SAMPLES) % VIS_SAMPLES;
                double sample = vis->audio_samples[sample_idx];
                
                // Draw dark background
                cairo_set_source_rgb(cr, 0.05, 0.05, 0.1);
                cairo_rectangle(cr, ray, top_y, 1, height);
                cairo_fill(cr);
                
                // Draw centered waveform line
                double wave_y = center_y + sample * (height * 0.35);
                
                // Draw white waveform with proper path
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
                cairo_set_line_width(cr, 1.0);
                cairo_move_to(cr, ray, wave_y);
                cairo_line_to(cr, ray + 1, wave_y);
                cairo_stroke(cr);
            }
        }
    }
    
// In draw_maze3d function, replace the creature drawing calls with these:

// Draw rats (replace existing rat drawing code)
for (int i = 0; i < MAX_RATS; i++) {
    if (!maze->rats[i].active) continue;
    
    double dx = maze->rats[i].x - player->x;
    double dy = maze->rats[i].y - player->y;
    double rat_dist = sqrt(dx * dx + dy * dy);
    
    if (rat_dist < 0.2) continue; // Too close
    if (rat_dist > 15.0) continue; // Too far
    
    double rat_angle = atan2(dy, dx);
    double angle_diff = rat_angle - player->angle;
    while (angle_diff > M_PI) angle_diff -= 2 * M_PI;
    while (angle_diff < -M_PI) angle_diff += 2 * M_PI;
    
    if (fabs(angle_diff) < fov / 2.0) {
        double screen_x = half_width + (angle_diff / (fov / 2.0)) * half_width;
        int rat_ray = (int)screen_x;
        
        if (rat_ray >= 0 && rat_ray < num_rays && rat_dist < wall_distances[rat_ray]) {
            draw_rat_3d(cr, screen_x, rat_dist, maze->rats[i].angle, maze->rats[i].bob_offset, half_height);
        }
    }
}

// Draw elephants (replace existing elephant drawing code)
for (int i = 0; i < MAX_ELEPHANTS; i++) {
    if (!maze->elephants[i].active) continue;
    
    double dx = maze->elephants[i].x - player->x;
    double dy = maze->elephants[i].y - player->y;
    double elephant_dist = sqrt(dx * dx + dy * dy);
    
    if (elephant_dist < 0.5) continue; // Too close
    if (elephant_dist > 15.0) continue; // Too far
    
    double elephant_angle = atan2(dy, dx);
    double angle_diff = elephant_angle - player->angle;
    while (angle_diff > M_PI) angle_diff -= 2 * M_PI;
    while (angle_diff < -M_PI) angle_diff += 2 * M_PI;
    
    if (fabs(angle_diff) < fov / 2.0) {
        double screen_x = half_width + (angle_diff / (fov / 2.0)) * half_width;
        int elephant_ray = (int)screen_x;
        
        if (elephant_ray >= 0 && elephant_ray < num_rays && elephant_dist < wall_distances[elephant_ray]) {
            draw_elephant_3d(cr, screen_x, elephant_dist, maze->elephants[i].angle, maze->elephants[i].bob_offset, half_height);
        }
    }
}

// Draw penguin (replace existing penguin drawing code)
double dx = maze->penguin.x - player->x;
double dy = maze->penguin.y - player->y;
double penguin_dist = sqrt(dx * dx + dy * dy);
double penguin_angle = atan2(dy, dx);

double angle_diff = penguin_angle - player->angle;
while (angle_diff > M_PI) angle_diff -= 2 * M_PI;
while (angle_diff < -M_PI) angle_diff += 2 * M_PI;

if (fabs(angle_diff) < fov / 2.0 && penguin_dist < 15.0) {
    double screen_x = half_width + (angle_diff / (fov / 2.0)) * half_width;
    int penguin_ray = (int)screen_x;
    
    // Only draw penguin if it's closer than the wall at this position
    if (penguin_ray >= 0 && penguin_ray < num_rays && penguin_dist < wall_distances[penguin_ray]) {
        draw_penguin_3d(cr, screen_x, penguin_dist, maze->penguin.scale,
                       maze->penguin.rotation, maze->penguin.bob_offset,
                       maze->penguin.found, maze->audio_pulse, half_height);
    }
}
    // Free the z-buffer and ray hits
    free(wall_distances);
    free(ray_hits);
    
    // Draw minimap
    double minimap_size = 200;
    double minimap_x = vis->width - minimap_size - 20;
    double minimap_y = 20;
    double cell_size = minimap_size / MAZE_WIDTH;
    
    // Draw minimap background with transparency
    cairo_set_source_rgba(cr, 0, 0, 0, 0.3);
    cairo_rectangle(cr, minimap_x - 5, minimap_y - 5, minimap_size + 10, minimap_size + 10);
    cairo_fill(cr);
    
    // Draw all cells and walls
    for (int y = 0; y < MAZE_HEIGHT; y++) {
        for (int x = 0; x < MAZE_WIDTH; x++) {
            double px = minimap_x + x * cell_size;
            double py = minimap_y + y * cell_size;
            
            // Draw floor with transparency
            cairo_set_source_rgba(cr, 0.15, 0.15, 0.2, 0.4);
            cairo_rectangle(cr, px, py, cell_size, cell_size);
            cairo_fill(cr);
            
            // Draw walls as lines with transparency
            cairo_set_line_width(cr, 2.0);
            cairo_set_source_rgba(cr, 0.8, 0.8, 0.9, 0.6);
            
            if (maze->cells[y][x] & WALL_NORTH) {
                cairo_move_to(cr, px, py);
                cairo_line_to(cr, px + cell_size, py);
                cairo_stroke(cr);
            }
            if (maze->cells[y][x] & WALL_SOUTH) {
                cairo_move_to(cr, px, py + cell_size);
                cairo_line_to(cr, px + cell_size, py + cell_size);
                cairo_stroke(cr);
            }
            if (maze->cells[y][x] & WALL_WEST) {
                cairo_move_to(cr, px, py);
                cairo_line_to(cr, px, py + cell_size);
                cairo_stroke(cr);
            }
            if (maze->cells[y][x] & WALL_EAST) {
                cairo_move_to(cr, px + cell_size, py);
                cairo_line_to(cr, px + cell_size, py + cell_size);
                cairo_stroke(cr);
            }
        }
    }
    
    // Draw rats on minimap
    for (int i = 0; i < MAX_RATS; i++) {
        if (!maze->rats[i].active) continue;
        cairo_set_source_rgb(cr, 0.8, 0.3, 0.3);
        cairo_arc(cr, minimap_x + maze->rats[i].x * cell_size,
                  minimap_y + maze->rats[i].y * cell_size, 2, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    
    // Draw elephants on minimap
    for (int i = 0; i < MAX_ELEPHANTS; i++) {
        if (!maze->elephants[i].active) continue;
        cairo_set_source_rgb(cr, 0.7, 0.5, 0.2);
        cairo_arc(cr, minimap_x + maze->elephants[i].x * cell_size,
                  minimap_y + maze->elephants[i].y * cell_size, 3, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    
    // Draw player on minimap
    cairo_set_source_rgb(cr, 1.0, 0, 0);
    cairo_arc(cr, minimap_x + player->x * cell_size, 
              minimap_y + player->y * cell_size, 3, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Draw player direction
    cairo_move_to(cr, minimap_x + player->x * cell_size, 
                  minimap_y + player->y * cell_size);
    cairo_line_to(cr, minimap_x + (player->x + cos(player->angle) * 0.5) * cell_size,
                  minimap_y + (player->y + sin(player->angle) * 0.5) * cell_size);
    cairo_stroke(cr);
    
    // Draw penguin on minimap
    cairo_set_source_rgb(cr, 0, 1.0, 1.0);
    cairo_arc(cr, minimap_x + maze->penguin.x * cell_size,
              minimap_y + maze->penguin.y * cell_size, 3, 0, 2 * M_PI);
    cairo_fill(cr);
}

void draw_rat_3d(cairo_t *cr, double screen_x, double distance, double rotation, 
                 double bob_offset, double half_height) {
    double size = (80.0 / distance);
    if (size > 150) size = 150;
    if (size < 5) size = 5;
    
    // Ground plane positioning - creatures sit on the horizon
    double screen_y = half_height + (20.0 / (distance + 0.5)) + bob_offset * 30;
    
    cairo_save(cr);
    cairo_translate(cr, screen_x, screen_y);
    
    // Body
    cairo_set_source_rgb(cr, 0.4, 0.35, 0.3);
    cairo_save(cr);
    cairo_scale(cr, 1.2, 0.8);
    cairo_arc(cr, 0, 0, size * 0.3, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
    
    // Head
    cairo_set_source_rgb(cr, 0.45, 0.4, 0.35);
    cairo_arc(cr, size * 0.25, -size * 0.1, size * 0.25, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Ears
    cairo_arc(cr, size * 0.15, -size * 0.35, size * 0.1, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, size * 0.35, -size * 0.35, size * 0.1, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Eyes
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_arc(cr, size * 0.2, -size * 0.15, size * 0.05, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Nose
    cairo_set_source_rgb(cr, 0.8, 0.3, 0.3);
    cairo_arc(cr, size * 0.35, -size * 0.08, size * 0.08, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Tail
    cairo_set_source_rgb(cr, 0.35, 0.3, 0.25);
    cairo_set_line_width(cr, size * 0.08);
    cairo_move_to(cr, -size * 0.35, 0);
    cairo_curve_to(cr, -size * 0.5, size * 0.2 + sin(rotation) * size * 0.15,
                   -size * 0.4, size * 0.4 + sin(rotation) * size * 0.2,
                   -size * 0.2, size * 0.35 + sin(rotation) * size * 0.1);
    cairo_stroke(cr);
    
    cairo_restore(cr);
}

void draw_elephant_3d(cairo_t *cr, double screen_x, double distance, double rotation, 
                      double bob_offset, double half_height) {
    double size = (250.0 / distance);
    if (size > 400) size = 400;
    if (size < 10) size = 10;
    
    // Ground plane positioning
    double screen_y = half_height + (20.0 / (distance + 0.5)) + bob_offset * 40;
    
    cairo_save(cr);
    cairo_translate(cr, screen_x, screen_y);
    
    // Body
    cairo_set_source_rgb(cr, 0.6, 0.55, 0.5);
    cairo_save(cr);
    cairo_scale(cr, 1.3, 1.0);
    cairo_arc(cr, 0, size * 0.05, size * 0.35, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
    
    // Head
    cairo_set_source_rgb(cr, 0.63, 0.58, 0.53);
    cairo_arc(cr, -size * 0.15, -size * 0.25, size * 0.28, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Ears
    cairo_set_source_rgb(cr, 0.6, 0.55, 0.5);
    cairo_save(cr);
    cairo_translate(cr, -size * 0.35, -size * 0.1);
    cairo_scale(cr, 0.6, 1.0);
    cairo_arc(cr, 0, 0, size * 0.3, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
    
    cairo_save(cr);
    cairo_translate(cr, size * 0.05, -size * 0.1);
    cairo_scale(cr, 0.6, 1.0);
    cairo_arc(cr, 0, 0, size * 0.3, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
    
    // Trunk
    cairo_set_source_rgb(cr, 0.55, 0.5, 0.45);
    cairo_set_line_width(cr, size * 0.12);
    cairo_move_to(cr, -size * 0.1, size * 0.02);
    double trunk_curve = sin(rotation * 0.5) * size * 0.15;
    cairo_curve_to(cr, -size * 0.1 + trunk_curve * 0.5, size * 0.2,
                   -size * 0.05 + trunk_curve, size * 0.4,
                   -size * 0.02 + trunk_curve * 0.8, size * 0.5);
    cairo_stroke(cr);
    
    // Eyes
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_arc(cr, -size * 0.28, -size * 0.32, size * 0.08, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Tusks
    cairo_set_source_rgb(cr, 0.95, 0.95, 0.9);
    cairo_set_line_width(cr, size * 0.06);
    cairo_move_to(cr, -size * 0.08, size * 0.08);
    cairo_line_to(cr, -size * 0.12, size * 0.2);
    cairo_stroke(cr);
    
    cairo_move_to(cr, size * 0.02, size * 0.08);
    cairo_line_to(cr, size * 0.08, size * 0.2);
    cairo_stroke(cr);
    
    // Legs
    cairo_set_source_rgb(cr, 0.55, 0.5, 0.45);
    double leg_positions[4] = {-size * 0.25, -size * 0.08, size * 0.08, size * 0.25};
    for (int i = 0; i < 4; i++) {
        cairo_rectangle(cr, leg_positions[i] - size * 0.08, size * 0.35, size * 0.16, size * 0.25);
        cairo_fill(cr);
    }
    
    // Tail
    cairo_set_source_rgb(cr, 0.5, 0.45, 0.4);
    cairo_set_line_width(cr, size * 0.08);
    cairo_move_to(cr, size * 0.35, size * 0.15);
    cairo_curve_to(cr, size * 0.5, size * 0.25 + sin(rotation * 0.3) * size * 0.1,
                   size * 0.45, size * 0.45,
                   size * 0.35, size * 0.55);
    cairo_stroke(cr);
    
    cairo_restore(cr);
}

void draw_penguin_3d(cairo_t *cr, double screen_x, double distance, double scale,
                     double rotation, double bob_offset, gboolean found, double pulse, double half_height) {
    double size = (200.0 / distance) * scale;
    if (size > 300) size = 300;
    
    // Ground plane positioning
    double screen_y = half_height + (20.0 / (distance + 0.5)) + bob_offset * 50;
    
    cairo_save(cr);
    cairo_translate(cr, screen_x, screen_y);
    
    // Glow effect if found
    if (found) {
        double glow = 0.5 + pulse * 0.5;
        cairo_set_source_rgba(cr, 0, 1.0, 1.0, glow * 0.3);
        cairo_arc(cr, 0, 0, size * 0.8, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    
    // Body
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_save(cr);
    cairo_scale(cr, 1.0, 1.3);
    cairo_arc(cr, 0, 0, size * 0.4, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
    
    // White belly
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_save(cr);
    cairo_scale(cr, 0.7, 1.0);
    cairo_arc(cr, 0, size * 0.1, size * 0.25, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
    
    // Eyes
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_arc(cr, -size * 0.12, -size * 0.15, size * 0.08, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, size * 0.12, -size * 0.15, size * 0.08, 0, 2 * M_PI);
    cairo_fill(cr);
    
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_arc(cr, -size * 0.12, -size * 0.15, size * 0.04, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, size * 0.12, -size * 0.15, size * 0.04, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Beak
    cairo_set_source_rgb(cr, 1.0, 0.6, 0);
    cairo_move_to(cr, 0, -size * 0.05);
    cairo_line_to(cr, -size * 0.08, size * 0.05);
    cairo_line_to(cr, size * 0.08, size * 0.05);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // Flippers
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_save(cr);
    cairo_translate(cr, -size * 0.35, size * 0.1);
    cairo_rotate(cr, -0.3 + sin(rotation * 2) * 0.2);
    cairo_scale(cr, 0.4, 1.0);
    cairo_arc(cr, 0, 0, size * 0.2, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
    
    cairo_save(cr);
    cairo_translate(cr, size * 0.35, size * 0.1);
    cairo_rotate(cr, 0.3 - sin(rotation * 2) * 0.2);
    cairo_scale(cr, 0.4, 1.0);
    cairo_arc(cr, 0, 0, size * 0.2, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);
    
    // Feet
    cairo_set_source_rgb(cr, 1.0, 0.6, 0);
    cairo_arc(cr, -size * 0.15, size * 0.45, size * 0.12, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, size * 0.15, size * 0.45, size * 0.12, 0, 2 * M_PI);
    cairo_fill(cr);
    
    cairo_restore(cr);
}
