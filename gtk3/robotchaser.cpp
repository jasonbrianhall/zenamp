#include "visualization.h"

void init_robot_chaser_system(Visualizer *vis) {
    // Initialize robot colors
    vis->robot_chaser_robot_colors[0][0] = 1.0; vis->robot_chaser_robot_colors[0][1] = 0.0; vis->robot_chaser_robot_colors[0][2] = 0.0;
    vis->robot_chaser_robot_colors[1][0] = 0.0; vis->robot_chaser_robot_colors[1][1] = 1.0; vis->robot_chaser_robot_colors[1][2] = 0.0;
    vis->robot_chaser_robot_colors[2][0] = 0.0; vis->robot_chaser_robot_colors[2][1] = 0.0; vis->robot_chaser_robot_colors[2][2] = 1.0;
    vis->robot_chaser_robot_colors[3][0] = 0.8; vis->robot_chaser_robot_colors[3][1] = 0.0; vis->robot_chaser_robot_colors[3][2] = 0.8;
    
    // Initialize player
    vis->robot_chaser_player.grid_x = 12;
    vis->robot_chaser_player.grid_y = 11;
    vis->robot_chaser_player.x = vis->robot_chaser_player.grid_x;
    vis->robot_chaser_player.y = vis->robot_chaser_player.grid_y;
    vis->robot_chaser_player.direction = CHASER_RIGHT;
    vis->robot_chaser_player.next_direction = CHASER_RIGHT;
    vis->robot_chaser_player.mouth_angle = 0.0;
    vis->robot_chaser_player.size_multiplier = 1.0;
    vis->robot_chaser_player.moving = TRUE;
    vis->robot_chaser_player.speed = 3.0;
    vis->robot_chaser_player.beat_pulse = 0.0;
    
    // Initialize robots
    vis->robot_chaser_robot_count = 4;
    int robot_positions[4][2] = {{11, 3}, {12, 3}, {13, 3}, {12, 5}};
    
    for (int i = 0; i < vis->robot_chaser_robot_count; i++) {
        ChaserRobot *robot = &vis->robot_chaser_robots[i];
        robot->grid_x = robot_positions[i][0];
        robot->grid_y = robot_positions[i][1];
        robot->x = robot->grid_x;
        robot->y = robot->grid_y;
        robot->direction = (ChaserDirection)(rand() % 4);
        robot->color_index = i % ROBOT_CHASER_ROBOT_COLORS;
        robot->hue = 0.0;
        robot->target_hue = 0.0;
        robot->size_multiplier = 1.0;
        robot->speed = 2.0;
        robot->scared_timer = 0.0;
        robot->scared = FALSE;
        robot->visible = TRUE;
        robot->blink_timer = 0.0;
        robot->frequency_band = (i * VIS_FREQUENCY_BARS) / vis->robot_chaser_robot_count;
        robot->audio_intensity = 0.0;
    }
    
    vis->robot_chaser_beat_timer = 0.0;
    vis->robot_chaser_power_pellet_timer = 0.0;
    vis->robot_chaser_power_mode = FALSE;
    vis->robot_chaser_move_timer = 0.0;
    
    // Start at level 0
    vis->robot_chaser_current_level = 0;
    
    robot_chaser_init_maze(vis);
    robot_chaser_calculate_layout(vis);
    robot_chaser_init_game_state(vis);
    
    // ===== MOUSE CONTROL INIT =====
    vis->robot_chaser_mouse_enabled = TRUE;
    vis->robot_chaser_mouse_control_mode = 0;
    vis->robot_chaser_has_mouse_target = FALSE;
    vis->robot_chaser_mouse_left_pressed_prev = FALSE;
    vis->robot_chaser_mouse_last_x = 0;
    vis->robot_chaser_mouse_last_y = 0;
    vis->robot_chaser_mouse_inactivity_timer = 0.0;
    // ==============================
}


void robot_chaser_calculate_layout(Visualizer *vis) {
    double padding = 20.0;
    double available_width = vis->width - (2 * padding);
    double available_height = vis->height - (2 * padding);
    
    double cell_width = available_width / ROBOT_CHASER_MAZE_WIDTH;
    double cell_height = available_height / ROBOT_CHASER_MAZE_HEIGHT;
    
    vis->robot_chaser_cell_size = fmin(cell_width, cell_height);
    
    double total_maze_width = vis->robot_chaser_cell_size * ROBOT_CHASER_MAZE_WIDTH;
    double total_maze_height = vis->robot_chaser_cell_size * ROBOT_CHASER_MAZE_HEIGHT;
    
    vis->robot_chaser_offset_x = (vis->width - total_maze_width) / 2.0;
    vis->robot_chaser_offset_y = (vis->height - total_maze_height) / 2.0;
}

void robot_chaser_init_maze(Visualizer *vis) {
    // Load the current level
    memcpy(vis->robot_chaser_maze, robot_chaser_levels[vis->robot_chaser_current_level], 
           sizeof(robot_chaser_maze_template));
    
    vis->robot_chaser_pellet_count = 0;
    for (int y = 0; y < ROBOT_CHASER_MAZE_HEIGHT; y++) {
        for (int x = 0; x < ROBOT_CHASER_MAZE_WIDTH; x++) {
            if (vis->robot_chaser_maze[y][x] == CHASER_PELLET || 
                vis->robot_chaser_maze[y][x] == CHASER_POWER_PELLET) {
                ChaserPellet *pellet = &vis->robot_chaser_pellets[vis->robot_chaser_pellet_count];
                pellet->grid_x = x;
                pellet->grid_y = y;
                pellet->active = TRUE;
                pellet->pulse_phase = ((double)rand() / RAND_MAX) * 2.0 * M_PI;
                pellet->size_multiplier = 1.0;
                pellet->is_power_pellet = (vis->robot_chaser_maze[y][x] == CHASER_POWER_PELLET);
                pellet->hue = pellet->is_power_pellet ? 60.0 : 45.0;
                pellet->frequency_band = (vis->robot_chaser_pellet_count * VIS_FREQUENCY_BARS) / MAX_ROBOT_CHASER_PELLETS;
                vis->robot_chaser_pellet_count++;
            }
        }
    }
}

gboolean robot_chaser_can_move(Visualizer *vis, int grid_x, int grid_y) {
    if (grid_x < 0 || grid_x >= ROBOT_CHASER_MAZE_WIDTH || 
        grid_y < 0 || grid_y >= ROBOT_CHASER_MAZE_HEIGHT) {
        return FALSE;
    }
    return vis->robot_chaser_maze[grid_y][grid_x] != CHASER_WALL;
}

void robot_chaser_consume_pellet(Visualizer *vis, int grid_x, int grid_y) {
    for (int i = 0; i < vis->robot_chaser_pellet_count; i++) {
        ChaserPellet *pellet = &vis->robot_chaser_pellets[i];
        if (pellet->active && pellet->grid_x == grid_x && pellet->grid_y == grid_y) {
            pellet->active = FALSE;
            
            if (pellet->is_power_pellet) {
                vis->robot_chaser_score += 50; 
                vis->robot_chaser_power_mode = TRUE;
                vis->robot_chaser_power_pellet_timer = 5.0;
                
                for (int j = 0; j < vis->robot_chaser_robot_count; j++) {
                    vis->robot_chaser_robots[j].scared = TRUE;
                    vis->robot_chaser_robots[j].scared_timer = 5.0;
                }
            } else {
                vis->robot_chaser_score += 10;
            }
            break;
        }
    }
}

gboolean robot_chaser_detect_beat(Visualizer *vis) {
    static double last_volume = 0.0;
    static double beat_cooldown = 0.0;
    
    beat_cooldown -= 0.033;
    if (beat_cooldown < 0) beat_cooldown = 0;
    
    gboolean beat = (vis->volume_level > 0.15 && 
                     vis->volume_level > last_volume * 1.2 && 
                     beat_cooldown <= 0);
    
    if (beat) {
        beat_cooldown = 0.2;
    }
    
    last_volume = vis->volume_level;
    return beat;
}

void robot_chaser_update_pellets(Visualizer *vis, double dt) {
    for (int i = 0; i < vis->robot_chaser_pellet_count; i++) {
        ChaserPellet *pellet = &vis->robot_chaser_pellets[i];
        if (!pellet->active) continue;
        
        pellet->pulse_phase += dt * 4.0;
        if (pellet->pulse_phase > 2.0 * M_PI) pellet->pulse_phase -= 2.0 * M_PI;
        
        double audio_factor = vis->frequency_bands[pellet->frequency_band];
        pellet->size_multiplier = 1.0 + audio_factor * 0.5;
        
        if (pellet->is_power_pellet) {
            pellet->size_multiplier *= 1.0 + 0.3 * sin(pellet->pulse_phase);
        }
    }
}

void draw_robot_chaser_maze(Visualizer *vis, cairo_t *cr) {
    double wall_glow = vis->volume_level * 0.5;
    cairo_set_source_rgba(cr, 0.0 + wall_glow, 0.0 + wall_glow, 1.0, 0.8 + wall_glow * 0.2);
    
    for (int y = 0; y < ROBOT_CHASER_MAZE_HEIGHT; y++) {
        for (int x = 0; x < ROBOT_CHASER_MAZE_WIDTH; x++) {
            if (vis->robot_chaser_maze[y][x] == CHASER_WALL) {
                double wall_x = vis->robot_chaser_offset_x + x * vis->robot_chaser_cell_size;
                double wall_y = vis->robot_chaser_offset_y + y * vis->robot_chaser_cell_size;
                
                cairo_rectangle(cr, wall_x, wall_y, vis->robot_chaser_cell_size, vis->robot_chaser_cell_size);
                cairo_fill(cr);
            }
        }
    }
}

void draw_robot_chaser_pellets(Visualizer *vis, cairo_t *cr) {
    for (int i = 0; i < vis->robot_chaser_pellet_count; i++) {
        ChaserPellet *pellet = &vis->robot_chaser_pellets[i];
        if (!pellet->active) continue;
        
        double pellet_x = vis->robot_chaser_offset_x + pellet->grid_x * vis->robot_chaser_cell_size + vis->robot_chaser_cell_size / 2;
        double pellet_y = vis->robot_chaser_offset_y + pellet->grid_y * vis->robot_chaser_cell_size + vis->robot_chaser_cell_size / 2;
        
        if (pellet->is_power_pellet) {
            // POWER CORE - Large spinning energy crystal
            double size = vis->robot_chaser_cell_size * 0.25 * pellet->size_multiplier;
            double rotation = pellet->pulse_phase;
            
            // Outer energy field
            cairo_set_source_rgba(cr, 1.0, 0.0, 1.0, 0.3);
            cairo_arc(cr, pellet_x, pellet_y, size * 1.8, 0, 2 * M_PI);
            cairo_fill(cr);
            
            // Main crystal body - diamond shape
            cairo_save(cr);
            cairo_translate(cr, pellet_x, pellet_y);
            cairo_rotate(cr, rotation);
            
            // Crystal facets
            cairo_set_source_rgba(cr, 0.8, 0.2, 1.0, 0.9);
            cairo_new_path(cr);
            cairo_move_to(cr, 0, -size);
            cairo_line_to(cr, size * 0.7, 0);
            cairo_line_to(cr, 0, size);
            cairo_line_to(cr, -size * 0.7, 0);
            cairo_close_path(cr);
            cairo_fill(cr);
            
            // Inner light core
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
            cairo_arc(cr, 0, 0, size * 0.3, 0, 2 * M_PI);
            cairo_fill(cr);
            
            // Energy sparks around crystal
            cairo_set_source_rgba(cr, 1.0, 0.5, 1.0, 0.7);
            cairo_set_line_width(cr, 2.0);
            for (int spark = 0; spark < 6; spark++) {
                double spark_angle = (spark * M_PI / 3.0) + rotation * 0.5;
                double spark_dist = size * 1.2;
                double spark_x = cos(spark_angle) * spark_dist;
                double spark_y = sin(spark_angle) * spark_dist;
                cairo_arc(cr, spark_x, spark_y, size * 0.1, 0, 2 * M_PI);
                cairo_fill(cr);
            }
            
            cairo_restore(cr);
            
        } else {
            // ENERGY ORB - Small floating geometric shape
            double size = vis->robot_chaser_cell_size * 0.08 * pellet->size_multiplier;
            double float_offset = sin(pellet->pulse_phase) * size * 0.3;
            
            // Energy trail/glow
            cairo_set_source_rgba(cr, 0.0, 1.0, 0.8, 0.4);
            cairo_arc(cr, pellet_x, pellet_y + float_offset, size * 2.5, 0, 2 * M_PI);
            cairo_fill(cr);
            
            // Main orb - octagonal energy cell
            cairo_save(cr);
            cairo_translate(cr, pellet_x, pellet_y + float_offset);
            cairo_rotate(cr, pellet->pulse_phase * 0.5);
            
            // Octagon shape
            cairo_new_path(cr);
            for (int i = 0; i < 8; i++) {
                double angle = i * M_PI / 4.0;
                double oct_x = size * cos(angle);
                double oct_y = size * sin(angle);
                if (i == 0) {
                    cairo_move_to(cr, oct_x, oct_y);
                } else {
                    cairo_line_to(cr, oct_x, oct_y);
                }
            }
            cairo_close_path(cr);
            
            // Gradient fill
            cairo_set_source_rgba(cr, 0.0, 0.8, 1.0, 0.9);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 0.7);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
            
            // Inner energy dot
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
            cairo_arc(cr, 0, 0, size * 0.4, 0, 2 * M_PI);
            cairo_fill(cr);
            
            // Energy pulse rings
            double pulse_alpha = 0.5 + 0.3 * sin(pellet->pulse_phase * 2.0);
            cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, pulse_alpha * 0.5);
            cairo_set_line_width(cr, 1.5);
            cairo_arc(cr, 0, 0, size * 1.5, 0, 2 * M_PI);
            cairo_stroke(cr);
            
            cairo_restore(cr);
        }
    }
}

void draw_robot_chaser_player(Visualizer *vis, cairo_t *cr) {
    ChaserPlayer *player = &vis->robot_chaser_player;
    
    double player_x = vis->robot_chaser_offset_x + player->x * vis->robot_chaser_cell_size + vis->robot_chaser_cell_size / 2;
    double player_y = vis->robot_chaser_offset_y + player->y * vis->robot_chaser_cell_size + vis->robot_chaser_cell_size / 2;
    
    double size = vis->robot_chaser_cell_size * 0.35 * player->size_multiplier;
    
    // ROBOTIC ENERGY COLLECTOR - Blue/cyan theme
    // Main body - hexagonal robot
    cairo_new_path(cr);
    double hex_radius = size * 0.8;
    for (int i = 0; i < 6; i++) {
        double angle = i * M_PI / 3.0;
        double hex_x = player_x + hex_radius * cos(angle);
        double hex_y = player_y + hex_radius * sin(angle);
        if (i == 0) {
            cairo_move_to(cr, hex_x, hex_y);
        } else {
            cairo_line_to(cr, hex_x, hex_y);
        }
    }
    cairo_close_path(cr);
    
    // Robot body gradient - metallic blue
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.9);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.0, 0.3, 0.8, 0.8);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    
    // Central core - pulsing energy
    double core_pulse = 0.7 + 0.3 * sin(player->mouth_angle * 2.0);
    cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, core_pulse);
    cairo_arc(cr, player_x, player_y, size * 0.4, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Energy collection beam in movement direction
    cairo_set_line_width(cr, size * 0.2);
    cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 0.6 + 0.4 * sin(player->mouth_angle));
    
    double beam_length = size * 1.5;
    double beam_end_x = player_x;
    double beam_end_y = player_y;
    
    switch (player->direction) {
        case CHASER_RIGHT:
            beam_end_x += beam_length;
            break;
        case CHASER_DOWN:
            beam_end_y += beam_length;
            break;
        case CHASER_LEFT:
            beam_end_x -= beam_length;
            break;
        case CHASER_UP:
            beam_end_y -= beam_length;
            break;
    }
    
    cairo_move_to(cr, player_x, player_y);
    cairo_line_to(cr, beam_end_x, beam_end_y);
    cairo_stroke(cr);
    
    // Robot "eyes" - two small rectangles
    double eye_size = size * 0.15;
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
    cairo_rectangle(cr, player_x - size * 0.3, player_y - size * 0.2, eye_size, eye_size);
    cairo_rectangle(cr, player_x + size * 0.15, player_y - size * 0.2, eye_size, eye_size);
    cairo_fill(cr);
    
    // Antenna/sensor on top
    cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.9);
    cairo_set_line_width(cr, 3.0);
    cairo_move_to(cr, player_x, player_y - size * 0.8);
    cairo_line_to(cr, player_x, player_y - size * 1.2);
    cairo_stroke(cr);
    
    // Antenna tip - blinking light
    double antenna_pulse = fmod(player->mouth_angle, 2.0 * M_PI) < M_PI ? 1.0 : 0.3;
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, antenna_pulse);
    cairo_arc(cr, player_x, player_y - size * 1.2, size * 0.1, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Add beat pulse glow effect
    if (player->beat_pulse > 0) {
        cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, player->beat_pulse * 0.5);
        cairo_arc(cr, player_x, player_y, size * (1.5 + player->beat_pulse * 0.5), 0, 2 * M_PI);
        cairo_fill(cr);
    }
    
    // Add directional movement indicators - small triangles
    cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.7);
    double indicator_size = size * 0.2;
    
    for (int i = 0; i < 3; i++) {
        double offset = (i - 1) * indicator_size * 0.8;
        double triangle_x = player_x;
        double triangle_y = player_y;
        
        switch (player->direction) {
            case CHASER_RIGHT:
                triangle_x += size * 1.2 + offset;
                cairo_move_to(cr, triangle_x, triangle_y);
                cairo_line_to(cr, triangle_x + indicator_size, triangle_y - indicator_size * 0.5);
                cairo_line_to(cr, triangle_x + indicator_size, triangle_y + indicator_size * 0.5);
                break;
            case CHASER_DOWN:
                triangle_y += size * 1.2 + offset;
                cairo_move_to(cr, triangle_x, triangle_y);
                cairo_line_to(cr, triangle_x - indicator_size * 0.5, triangle_y + indicator_size);
                cairo_line_to(cr, triangle_x + indicator_size * 0.5, triangle_y + indicator_size);
                break;
            case CHASER_LEFT:
                triangle_x -= size * 1.2 - offset;
                cairo_move_to(cr, triangle_x, triangle_y);
                cairo_line_to(cr, triangle_x - indicator_size, triangle_y - indicator_size * 0.5);
                cairo_line_to(cr, triangle_x - indicator_size, triangle_y + indicator_size * 0.5);
                break;
            case CHASER_UP:
                triangle_y -= size * 1.2 - offset;
                cairo_move_to(cr, triangle_x, triangle_y);
                cairo_line_to(cr, triangle_x - indicator_size * 0.5, triangle_y - indicator_size);
                cairo_line_to(cr, triangle_x + indicator_size * 0.5, triangle_y - indicator_size);
                break;
        }
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

void draw_robot_chaser_robots(Visualizer *vis, cairo_t *cr) {
    for (int i = 0; i < vis->robot_chaser_robot_count; i++) {
        ChaserRobot *robot = &vis->robot_chaser_robots[i];
        if (!robot->visible) continue;
        
        double robot_x = vis->robot_chaser_offset_x + robot->x * vis->robot_chaser_cell_size + vis->robot_chaser_cell_size / 2;
        double robot_y = vis->robot_chaser_offset_y + robot->y * vis->robot_chaser_cell_size + vis->robot_chaser_cell_size / 2;
        
        double size = vis->robot_chaser_cell_size * 0.32 * robot->size_multiplier;
        
        // SECURITY DRONES - Different types based on color index
        double r, g, b;
        if (robot->scared) {
            // Disabled/hacked state - sparking and dim
            r = 0.3; g = 0.3; b = 0.3;
        } else {
            r = vis->robot_chaser_robot_colors[robot->color_index][0];
            g = vis->robot_chaser_robot_colors[robot->color_index][1];
            b = vis->robot_chaser_robot_colors[robot->color_index][2];
            
            double intensity = robot->audio_intensity;
            r = fmin(1.0, r + intensity * 0.4);
            g = fmin(1.0, g + intensity * 0.4);
            b = fmin(1.0, b + intensity * 0.4);
        }
        
        // Different drone designs based on color/type
        switch (robot->color_index % 4) {
            case 0: // Red - ASSAULT DRONE (angular, aggressive)
                {
                    // Main body - diamond shape
                    cairo_new_path(cr);
                    cairo_move_to(cr, robot_x, robot_y - size);
                    cairo_line_to(cr, robot_x + size * 0.8, robot_y);
                    cairo_line_to(cr, robot_x, robot_y + size);
                    cairo_line_to(cr, robot_x - size * 0.8, robot_y);
                    cairo_close_path(cr);
                    cairo_set_source_rgba(cr, r, g, b, 0.9);
                    cairo_fill(cr);
                    
                    // Weapon pods
                    cairo_set_source_rgba(cr, r * 0.7, g * 0.7, b * 0.7, 0.8);
                    cairo_rectangle(cr, robot_x - size * 0.9, robot_y - size * 0.2, size * 0.3, size * 0.4);
                    cairo_rectangle(cr, robot_x + size * 0.6, robot_y - size * 0.2, size * 0.3, size * 0.4);
                    cairo_fill(cr);
                    
                    // Red targeting laser
                    if (!robot->scared) {
                        cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.6);
                        cairo_set_line_width(cr, 2.0);
                        cairo_move_to(cr, robot_x, robot_y);
                        cairo_line_to(cr, robot_x, robot_y + size * 2);
                        cairo_stroke(cr);
                    }
                }
                break;
                
            case 1: // Green - SCANNER DRONE (disc-shaped)
                {
                    // Main disc body
                    cairo_set_source_rgba(cr, r, g, b, 0.8);
                    cairo_arc(cr, robot_x, robot_y, size * 0.7, 0, 2 * M_PI);
                    cairo_fill(cr);
                    
                    // Rotating scanner ring
                    cairo_save(cr);
                    cairo_translate(cr, robot_x, robot_y);
                    cairo_rotate(cr, vis->robot_chaser_beat_timer + i * 0.5);
                    
                    cairo_set_source_rgba(cr, r * 1.2, g * 1.2, b * 1.2, 0.6);
                    cairo_set_line_width(cr, 3.0);
                    cairo_arc(cr, 0, 0, size * 0.9, 0, M_PI);
                    cairo_stroke(cr);
                    
                    cairo_restore(cr);
                    
                    // Central sensor
                    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
                    cairo_arc(cr, robot_x, robot_y, size * 0.25, 0, 2 * M_PI);
                    cairo_fill(cr);
                    
                    // Scanning beams
                    if (!robot->scared) {
                        cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.4);
                        cairo_set_line_width(cr, 1.0);
                        for (int beam = 0; beam < 8; beam++) {
                            double beam_angle = (beam * M_PI / 4.0) + vis->robot_chaser_beat_timer;
                            double beam_end_x = robot_x + cos(beam_angle) * size * 1.5;
                            double beam_end_y = robot_y + sin(beam_angle) * size * 1.5;
                            cairo_move_to(cr, robot_x, robot_y);
                            cairo_line_to(cr, beam_end_x, beam_end_y);
                            cairo_stroke(cr);
                        }
                    }
                }
                break;
                
            case 2: // Blue - SHIELD DRONE (hexagonal)
                {
                    // Hexagonal shield body
                    cairo_new_path(cr);
                    for (int hex = 0; hex < 6; hex++) {
                        double angle = hex * M_PI / 3.0;
                        double hex_x = robot_x + size * 0.8 * cos(angle);
                        double hex_y = robot_y + size * 0.8 * sin(angle);
                        if (hex == 0) {
                            cairo_move_to(cr, hex_x, hex_y);
                        } else {
                            cairo_line_to(cr, hex_x, hex_y);
                        }
                    }
                    cairo_close_path(cr);
                    cairo_set_source_rgba(cr, r, g, b, 0.7);
                    cairo_fill_preserve(cr);
                    cairo_set_source_rgba(cr, r * 1.5, g * 1.5, b * 1.5, 0.9);
                    cairo_set_line_width(cr, 2.0);
                    cairo_stroke(cr);
                    
                    // Energy shield effect
                    if (!robot->scared && robot->audio_intensity > 0.3) {
                        cairo_set_source_rgba(cr, 0.0, 0.5, 1.0, 0.3);
                        cairo_arc(cr, robot_x, robot_y, size * 1.2, 0, 2 * M_PI);
                        cairo_fill(cr);
                    }
                    
                    // Central core
                    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
                    cairo_arc(cr, robot_x, robot_y, size * 0.3, 0, 2 * M_PI);
                    cairo_fill(cr);
                }
                break;
                
            case 3: // Purple - STEALTH DRONE (triangular)
                {
                    // Stealth triangle body
                    cairo_new_path(cr);
                    cairo_move_to(cr, robot_x, robot_y - size);
                    cairo_line_to(cr, robot_x - size * 0.8, robot_y + size * 0.5);
                    cairo_line_to(cr, robot_x + size * 0.8, robot_y + size * 0.5);
                    cairo_close_path(cr);
                    
                    // Cloaking effect
                    double cloak_alpha = robot->scared ? 0.3 : (0.6 + 0.3 * sin(vis->robot_chaser_beat_timer * 2 + i));
                    cairo_set_source_rgba(cr, r, g, b, cloak_alpha);
                    cairo_fill(cr);
                    
                    // Phase distortion lines
                    if (!robot->scared) {
                        cairo_set_source_rgba(cr, r * 1.3, g * 1.3, b * 1.3, 0.4);
                        cairo_set_line_width(cr, 1.5);
                        for (int line = 0; line < 3; line++) {
                            double line_y = robot_y - size * 0.6 + (line * size * 0.6);
                            cairo_move_to(cr, robot_x - size * 0.6, line_y);
                            cairo_line_to(cr, robot_x + size * 0.6, line_y);
                            cairo_stroke(cr);
                        }
                    }
                    
                    // Engine thrusters
                    cairo_set_source_rgba(cr, 1.0, 0.5, 1.0, 0.7);
                    cairo_arc(cr, robot_x - size * 0.4, robot_y + size * 0.3, size * 0.1, 0, 2 * M_PI);
                    cairo_arc(cr, robot_x + size * 0.4, robot_y + size * 0.3, size * 0.1, 0, 2 * M_PI);
                    cairo_fill(cr);
                }
                break;
        }
        
        // Disabled/hacked effects for scared state
        if (robot->scared) {
            // Sparking effect
            cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.8);
            for (int spark = 0; spark < 5; spark++) {
                double spark_x = robot_x + (rand() % 100 - 50) / 100.0 * size;
                double spark_y = robot_y + (rand() % 100 - 50) / 100.0 * size;
                cairo_arc(cr, spark_x, spark_y, 2.0, 0, 2 * M_PI);
                cairo_fill(cr);
            }
            
            // Error text overlay
            cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.9);
            cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, size * 0.3);
            cairo_move_to(cr, robot_x - size * 0.4, robot_y + size * 0.1);
            cairo_show_text(cr, "ERR");
        }
        
        // Audio reactive glow
        if (robot->audio_intensity > 0.2) {
            cairo_set_source_rgba(cr, r, g, b, robot->audio_intensity * 0.4);
            cairo_arc(cr, robot_x, robot_y, size * 1.4, 0, 2 * M_PI);
            cairo_fill(cr);
        }
    }
}

void draw_robot_chaser_visualization(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double bg_pulse = 0.05 + vis->volume_level * 0.1;
    cairo_set_source_rgba(cr, bg_pulse, bg_pulse, bg_pulse * 2, 1.0);
    cairo_paint(cr);
    
    draw_robot_chaser_maze(vis, cr);
    draw_robot_chaser_pellets(vis, cr);
    draw_robot_chaser_robots(vis, cr);
    draw_robot_chaser_player(vis, cr);
    
    if (vis->robot_chaser_power_mode) {
        double power_flash = 0.15 + 0.1 * sin(vis->robot_chaser_power_pellet_timer * 8.0);
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, power_flash);
        cairo_paint(cr);
        
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 24);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
        
        char timer_text[32];
        snprintf(timer_text, sizeof(timer_text), "INVINCIPLE: %.1f", vis->robot_chaser_power_pellet_timer);
        
        cairo_text_extents_t extents;
        cairo_text_extents(cr, timer_text, &extents);
        cairo_move_to(cr, (vis->width - extents.width) / 2, 35);
        cairo_show_text(cr, timer_text);
    }
    
    if (vis->robot_chaser_player.beat_pulse > 0) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, vis->robot_chaser_player.beat_pulse * 0.08);
        cairo_paint(cr);
    }
}

// Improved robot AI with better wall avoidance
ChaserDirection robot_chaser_choose_smart_direction_v2(Visualizer *vis, ChaserRobot *robot, int robot_index) {
    ChaserPlayer *player = &vis->robot_chaser_player;
    
    // Get all valid directions (no walls)
    ChaserDirection possible_directions[4];
    int possible_count = 0;
    
    for (int dir = 0; dir < 4; dir++) {
        int next_x = robot->grid_x;
        int next_y = robot->grid_y;
        
        switch (dir) {
            case CHASER_UP: next_y--; break;
            case CHASER_DOWN: next_y++; break;
            case CHASER_LEFT: next_x--; break;
            case CHASER_RIGHT: next_x++; break;
        }
        
        if (robot_chaser_can_move(vis, next_x, next_y)) {
            // Don't go backwards unless it's the only option
            if (dir != robot_chaser_get_opposite_direction(robot->direction) || possible_count == 0) {
                possible_directions[possible_count++] = (ChaserDirection)dir;
            }
        }
    }
    
    if (possible_count == 0) {
        // Forced to go backwards - this should prevent wall sticking
        return robot_chaser_get_opposite_direction(robot->direction);
    }
    
    // Remove backwards direction if we have other options
    if (possible_count > 1) {
        ChaserDirection backwards = robot_chaser_get_opposite_direction(robot->direction);
        for (int i = 0; i < possible_count; i++) {
            if (possible_directions[i] == backwards) {
                // Remove backwards direction
                for (int j = i; j < possible_count - 1; j++) {
                    possible_directions[j] = possible_directions[j + 1];
                }
                possible_count--;
                break;
            }
        }
    }
    
    double audio_intensity = robot->audio_intensity;
    
    if (robot->scared) {
        // Scared robots run away from player
        ChaserDirection best_direction = possible_directions[0];
        double best_distance = -1;
        
        for (int i = 0; i < possible_count; i++) {
            int test_x = robot->grid_x;
            int test_y = robot->grid_y;
            
            switch (possible_directions[i]) {
                case CHASER_UP: test_y--; break;
                case CHASER_DOWN: test_y++; break;
                case CHASER_LEFT: test_x--; break;
                case CHASER_RIGHT: test_x++; break;
            }
            
            double distance = sqrt(pow(test_x - player->grid_x, 2) + pow(test_y - player->grid_y, 2));
            if (distance > best_distance) {
                best_distance = distance;
                best_direction = possible_directions[i];
            }
        }
        
        // Add randomness based on audio
        if (audio_intensity > 0.5 && rand() % 100 < 25) {
            return possible_directions[rand() % possible_count];
        }
        
        return best_direction;
    } else {
        // Normal robot behavior
        switch (robot_index % 4) {
            case 0: // Red robot - aggressive chase
                {
                    if (audio_intensity > 0.3) {
                        ChaserDirection toward_player = robot_chaser_get_direction_to_target(
                            robot->grid_x, robot->grid_y, player->grid_x, player->grid_y);
                        
                        for (int i = 0; i < possible_count; i++) {
                            if (possible_directions[i] == toward_player) {
                                return toward_player;
                            }
                        }
                    }
                    break;
                }
                
            case 1: // Pink robot - ambush behavior
                {
                    if (audio_intensity > 0.2) {
                        int target_x = player->grid_x;
                        int target_y = player->grid_y;
                        
                        // Target ahead of player
                        switch (player->direction) {
                            case CHASER_UP: target_y -= 4; break;
                            case CHASER_DOWN: target_y += 4; break;
                            case CHASER_LEFT: target_x -= 4; break;
                            case CHASER_RIGHT: target_x += 4; break;
                        }
                        
                        ChaserDirection toward_target = robot_chaser_get_direction_to_target(
                            robot->grid_x, robot->grid_y, target_x, target_y);
                        
                        for (int i = 0; i < possible_count; i++) {
                            if (possible_directions[i] == toward_target) {
                                return toward_target;
                            }
                        }
                    }
                    break;
                }
                
            case 2: // Cyan robot - patrol corners
                {
                    // Prefer turns at intersections - evaluate all options
                    if (possible_count > 2) {
                        // Try to chase player when close
                        ChaserDirection best_direction = possible_directions[0];
                        double best_distance = -1;
                        
                        for (int i = 0; i < possible_count; i++) {
                            int test_x = robot->grid_x;
                            int test_y = robot->grid_y;
                            
                            switch (possible_directions[i]) {
                                case CHASER_UP: test_y--; break;
                                case CHASER_DOWN: test_y++; break;
                                case CHASER_LEFT: test_x--; break;
                                case CHASER_RIGHT: test_x++; break;
                            }
                            
                            double distance = sqrt(pow(test_x - player->grid_x, 2) + pow(test_y - player->grid_y, 2));
                            if (distance < best_distance || best_distance < 0) {
                                best_distance = distance;
                                best_direction = possible_directions[i];
                            }
                        }
                        return best_direction;
                    }
                    break;
                }
                
            case 3: // Orange robot - distance dependent
                {
                    double distance_to_player = robot_chaser_distance_to_player(robot, player);
                    
                    if (distance_to_player > 8.0 && audio_intensity > 0.2) {
                        // Chase when far
                        ChaserDirection toward_player = robot_chaser_get_direction_to_target(
                            robot->grid_x, robot->grid_y, player->grid_x, player->grid_y);
                        
                        for (int i = 0; i < possible_count; i++) {
                            if (possible_directions[i] == toward_player) {
                                return toward_player;
                            }
                        }
                    } else if (distance_to_player <= 8.0) {
                        // Run away when close
                        ChaserDirection away_from_player = robot_chaser_get_direction_to_target(
                            player->grid_x, player->grid_y, robot->grid_x, robot->grid_y);
                        
                        for (int i = 0; i < possible_count; i++) {
                            if (possible_directions[i] == away_from_player) {
                                return away_from_player;
                            }
                        }
                    }
                    break;
                }
        }
        
        // Default: continue straight if possible, otherwise random
        for (int i = 0; i < possible_count; i++) {
            if (possible_directions[i] == robot->direction) {
                return robot->direction;
            }
        }
        
        return possible_directions[rand() % possible_count];
    }
}

double robot_chaser_distance_to_player(ChaserRobot *robot, ChaserPlayer *player) {
    double dx = robot->x - player->x;
    double dy = robot->y - player->y;
    return sqrt(dx * dx + dy * dy);
}

ChaserDirection robot_chaser_get_opposite_direction(ChaserDirection dir) {
    switch (dir) {
        case CHASER_UP: return CHASER_DOWN;
        case CHASER_DOWN: return CHASER_UP;
        case CHASER_LEFT: return CHASER_RIGHT;
        case CHASER_RIGHT: return CHASER_LEFT;
    }
    return dir;
}

ChaserDirection robot_chaser_get_direction_to_target(int from_x, int from_y, int to_x, int to_y) {
    int dx = to_x - from_x;
    int dy = to_y - from_y;
    
    // Calculate which direction moves closer to target
    // Prefer the axis with larger difference
    if (abs(dx) > abs(dy)) {
        // Horizontal distance is larger
        return (dx > 0) ? CHASER_RIGHT : CHASER_LEFT;
    } else {
        // Vertical distance is larger
        return (dy > 0) ? CHASER_DOWN : CHASER_UP;
    }
}

void robot_chaser_find_nearest_pellet(Visualizer *vis, int from_x, int from_y, int *target_x, int *target_y) {
    double nearest_distance = 1000.0;
    *target_x = from_x;
    *target_y = from_y;
    
    for (int i = 0; i < vis->robot_chaser_pellet_count; i++) {
        ChaserPellet *pellet = &vis->robot_chaser_pellets[i];
        if (!pellet->active) continue;
        
        double dx = pellet->grid_x - from_x;
        double dy = pellet->grid_y - from_y;
        double distance = sqrt(dx * dx + dy * dy);
        
        if (distance < nearest_distance) {
            nearest_distance = distance;
            *target_x = pellet->grid_x;
            *target_y = pellet->grid_y;
        }
    }
}

gboolean robot_chaser_check_collision_with_robots(Visualizer *vis) {
    ChaserPlayer *player = &vis->robot_chaser_player;
    
    for (int i = 0; i < vis->robot_chaser_robot_count; i++) {
        ChaserRobot *robot = &vis->robot_chaser_robots[i];
        if (!robot->visible) continue;
        
        // Calculate distance between player and robot
        double dx = player->x - robot->x;
        double dy = player->y - robot->y;
        double distance = sqrt(dx * dx + dy * dy);
        
        // Collision threshold (adjust as needed)
        if (distance < 0.8) {
            if (robot->scared) {
                // Player eats scared robot
                robot->visible = FALSE;
                robot->scared = FALSE;
                robot->x = 12; // Reset to center
                robot->y = 7;
                robot->grid_x = 12;
                robot->grid_y = 7;
                vis->robot_chaser_score += 200;
                return FALSE; // No death
            } else {
                // robot kills player
                return TRUE;
            }
        }
    }
    return FALSE;
}

gboolean robot_chaser_is_level_complete(Visualizer *vis) {
    for (int i = 0; i < vis->robot_chaser_pellet_count; i++) {
        if (vis->robot_chaser_pellets[i].active) {
            return FALSE;
        }
    }
    return TRUE;
}

void robot_chaser_reset_level(Visualizer *vis) {
    // Reset player - use level-specific spawn points
    int spawn_positions[ROBOT_CHASER_NUM_LEVELS][2] = {
        {12, 11},  // Level 1 - original position
        {12, 7},   // Level 2 - center corridor (where 0 marker is)
        {12, 7},   // Level 3 - center
        {12, 7},   // Level 4 - center
        {12, 7}    // Level 5 - center
    };
    
    int level = vis->robot_chaser_current_level;
    if (level < 0) level = 0;
    if (level >= ROBOT_CHASER_NUM_LEVELS) level = ROBOT_CHASER_NUM_LEVELS - 1;
    
    vis->robot_chaser_player.grid_x = spawn_positions[level][0];
    vis->robot_chaser_player.grid_y = spawn_positions[level][1];
    vis->robot_chaser_player.x = vis->robot_chaser_player.grid_x;
    vis->robot_chaser_player.y = vis->robot_chaser_player.grid_y;
    vis->robot_chaser_player.direction = CHASER_RIGHT;
    vis->robot_chaser_player.next_direction = CHASER_RIGHT;
    vis->robot_chaser_player.moving = TRUE;
    vis->robot_chaser_player.beat_pulse = 0.0;
    
    // Reset mouse inactivity timer
    vis->robot_chaser_mouse_last_x = vis->mouse_x;
    vis->robot_chaser_mouse_last_y = vis->mouse_y;
    vis->robot_chaser_mouse_inactivity_timer = 0.0;
    
    // Reset robots to starting positions - level-specific safe positions
    // Level 1 start positions
    int robot_positions[ROBOT_CHASER_NUM_LEVELS][4][2] = {
        // Level 1
        {{11, 3}, {12, 3}, {13, 3}, {12, 5}},
        // Level 2 - corridors (must be in open areas)
        {{2, 1}, {4, 1}, {22, 1}, {2, 13}},
        // Level 3 - cross
        {{11, 3}, {12, 3}, {13, 3}, {12, 5}},
        // Level 4
        {{11, 3}, {12, 3}, {13, 3}, {12, 5}},
        // Level 5
        {{11, 3}, {12, 3}, {13, 3}, {12, 5}}
    };
    
    level = vis->robot_chaser_current_level;
    if (level < 0) level = 0;
    if (level >= ROBOT_CHASER_NUM_LEVELS) level = ROBOT_CHASER_NUM_LEVELS - 1;
    
    for (int i = 0; i < vis->robot_chaser_robot_count; i++) {
        ChaserRobot *robot = &vis->robot_chaser_robots[i];
        robot->grid_x = robot_positions[level][i][0];
        robot->grid_y = robot_positions[level][i][1];
        robot->x = robot->grid_x;
        robot->y = robot->grid_y;
        robot->direction = (ChaserDirection)(rand() % 4);
        robot->scared = FALSE;
        robot->scared_timer = 0.0;
        robot->visible = TRUE;
        robot->blink_timer = 0.0;
    }
    
    // Reset power mode
    vis->robot_chaser_power_mode = FALSE;
    vis->robot_chaser_power_pellet_timer = 0.0;
    
    // Reset game state
    vis->robot_chaser_game_state = GAME_PLAYING;
    vis->robot_chaser_death_timer = 0.0;
}

// Initialize game state (add this to init_robot_chaser_system)
void robot_chaser_init_game_state(Visualizer *vis) {
    vis->robot_chaser_game_state = GAME_PLAYING;
    vis->robot_chaser_death_timer = 0.0;
    vis->robot_chaser_lives = 30;  // Up UP Down Down Left Right Left Right B A Start
    vis->robot_chaser_score = 0;
}


gboolean robot_chaser_move_entity_safely(Visualizer *vis, double *x, double *y, int *grid_x, int *grid_y, ChaserDirection direction, double speed, double dt) {
    double new_x = *x;
    double new_y = *y;
    double move_distance = speed * dt;
    
    // Calculate target position
    switch (direction) {
        case CHASER_UP: new_y -= move_distance; break;
        case CHASER_DOWN: new_y += move_distance; break;
        case CHASER_LEFT: new_x -= move_distance; break;
        case CHASER_RIGHT: new_x += move_distance; break;
    }
    
    // Check if the new position is valid
    int new_grid_x = (int)round(new_x);
    int new_grid_y = (int)round(new_y);
    
    // Clamp to valid grid bounds
    new_grid_x = fmax(0, fmin(ROBOT_CHASER_MAZE_WIDTH - 1, new_grid_x));
    new_grid_y = fmax(0, fmin(ROBOT_CHASER_MAZE_HEIGHT - 1, new_grid_y));
    
    if (robot_chaser_can_move(vis, new_grid_x, new_grid_y)) {
        *x = new_x;
        *y = new_y;
        *grid_x = new_grid_x;
        *grid_y = new_grid_y;
        return TRUE;
    }
    
    // If can't move, snap back to grid center to prevent wall sticking
    *x = *grid_x;
    *y = *grid_y;
    return FALSE;
}

void robot_chaser_update_player(Visualizer *vis, double dt) {
    if (vis->robot_chaser_game_state != GAME_PLAYING) {
        return;
    }
    
    ChaserPlayer *player = &vis->robot_chaser_player;
    
    // Apply mouse aim direction if set
    if (player->next_direction != player->direction) {
        int test_x = player->grid_x;
        int test_y = player->grid_y;
        
        switch (player->next_direction) {
            case CHASER_UP: test_y--; break;
            case CHASER_DOWN: test_y++; break;
            case CHASER_LEFT: test_x--; break;
            case CHASER_RIGHT: test_x++; break;
        }
        
        if (robot_chaser_can_move(vis, test_x, test_y)) {
            player->direction = player->next_direction;
        }
    }
    
    if (player->beat_pulse > 0) {
        player->beat_pulse -= dt * 3.0;
        if (player->beat_pulse < 0) player->beat_pulse = 0;
    }
    
    player->size_multiplier = 1.0 + vis->volume_level * 0.3 + player->beat_pulse * 0.5;
    double speed_multiplier = 1.0 + vis->volume_level * 0.5;
    
    // Animate mouth
    double mouth_speed = 8.0 * speed_multiplier;
    player->mouth_angle += dt * mouth_speed;
    if (player->mouth_angle > 2.0 * M_PI) player->mouth_angle -= 2.0 * M_PI;
    
    if (player->moving) {
        // Check if we're aligned with grid
        double dx_to_grid = fabs(player->x - player->grid_x);
        double dy_to_grid = fabs(player->y - player->grid_y);
        gboolean near_grid_center = (dx_to_grid < 0.3 && dy_to_grid < 0.3);
        
        // At grid center - check if we SHOULD turn
        if (near_grid_center) {
            // Check what directions are available
            gboolean can_continue = FALSE;
            gboolean can_turn = FALSE;
            
            // Check if we can continue straight
            int straight_x = player->grid_x;
            int straight_y = player->grid_y;
            switch (player->direction) {
                case CHASER_UP: straight_y--; break;
                case CHASER_DOWN: straight_y++; break;
                case CHASER_LEFT: straight_x--; break;
                case CHASER_RIGHT: straight_x++; break;
            }
            can_continue = robot_chaser_can_move(vis, straight_x, straight_y);
            
            // Check if we can turn perpendicular
            if (player->direction == CHASER_UP || player->direction == CHASER_DOWN) {
                if (robot_chaser_can_move(vis, player->grid_x - 1, player->grid_y) ||
                    robot_chaser_can_move(vis, player->grid_x + 1, player->grid_y)) {
                    can_turn = TRUE;
                }
            } else {
                if (robot_chaser_can_move(vis, player->grid_x, player->grid_y - 1) ||
                    robot_chaser_can_move(vis, player->grid_x, player->grid_y + 1)) {
                    can_turn = TRUE;
                }
            }
            
            // If we can turn, evaluate if we SHOULD turn
            if (can_turn) {
                ChaserDirection best_direction = robot_chaser_choose_player_direction(vis);
                
                // Only turn if the AI suggests a different direction and it's valid
                if (best_direction != player->direction) {
                    int test_x = player->grid_x;
                    int test_y = player->grid_y;
                    
                    switch (best_direction) {
                        case CHASER_UP: test_y--; break;
                        case CHASER_DOWN: test_y++; break;
                        case CHASER_LEFT: test_x--; break;
                        case CHASER_RIGHT: test_x++; break;
                    }
                    
                    if (robot_chaser_can_move(vis, test_x, test_y)) {
                        player->direction = best_direction;
                        player->x = player->grid_x;
                        player->y = player->grid_y;
                    }
                }
            }
            
            // If we can't continue straight, MUST turn
            if (!can_continue && can_turn) {
                ChaserDirection new_direction = robot_chaser_choose_player_direction(vis);
                player->direction = new_direction;
                player->x = player->grid_x;
                player->y = player->grid_y;
            }
        }
        
        // Move in current direction
        int old_grid_x = player->grid_x;
        int old_grid_y = player->grid_y;
        
        gboolean moved = robot_chaser_move_entity_safely(vis, &player->x, &player->y, 
                                                         &player->grid_x, &player->grid_y,
                                                         player->direction, 
                                                         player->speed * speed_multiplier, dt);
        
        robot_chaser_handle_wraparound(vis, &player->x, &player->y, &player->grid_x, &player->grid_y);
        
        if (!moved) {
            // Hit a wall - must choose new direction
            ChaserDirection forced_direction = robot_chaser_choose_player_direction(vis);
            player->direction = forced_direction;
            player->x = player->grid_x;
            player->y = player->grid_y;
        } else {
            // Successfully moved - check for pellet consumption
            if (player->grid_x != old_grid_x || player->grid_y != old_grid_y) {
                robot_chaser_consume_pellet(vis, player->grid_x, player->grid_y);
            }
        }
    }
    
    // Handle beat detection
    if (robot_chaser_detect_beat(vis)) {
        player->beat_pulse = 1.0;
    }
    
    // Check collisions
    if (robot_chaser_check_collision_with_robots(vis)) {
        vis->robot_chaser_lives--;
        vis->robot_chaser_game_state = GAME_PLAYER_DIED;
        vis->robot_chaser_death_timer = 2.0;
    }
    
    // Check level complete
    if (robot_chaser_is_level_complete(vis)) {
        vis->robot_chaser_game_state = GAME_LEVEL_COMPLETE;
        vis->robot_chaser_death_timer = 3.0;
    }
}

void robot_chaser_update_robots(Visualizer *vis, double dt) {
    if (vis->robot_chaser_game_state != GAME_PLAYING) {
        return;
    }
    
    for (int i = 0; i < vis->robot_chaser_robot_count; i++) {
        ChaserRobot *robot = &vis->robot_chaser_robots[i];
        
        // Ensure robot is in a valid position
        if (!robot_chaser_can_move(vis, robot->grid_x, robot->grid_y)) {
            robot->x = 12;
            robot->y = 3;
            robot->grid_x = 12;
            robot->grid_y = 3;
        }
        
        robot_chaser_unstick_robot(vis, robot);
        
        robot->audio_intensity = vis->frequency_bands[robot->frequency_band];
        
        // Handle scared state
        if (robot->scared) {
            robot->scared_timer -= dt;
            if (robot->scared_timer <= 0) {
                robot->scared = FALSE;
            }
            if (robot->scared_timer < 2.0) {
                robot->blink_timer += dt;
                robot->visible = (fmod(robot->blink_timer, 0.3) < 0.15);
            } else {
                robot->visible = TRUE;
            }
        } else {
            robot->visible = TRUE;
        }
        
        robot->size_multiplier = 1.0 + robot->audio_intensity * 0.4;
        double speed_multiplier = 1.0 + robot->audio_intensity * 0.3;
        
        // Update color
        if (robot->scared) {
            robot->target_hue = 240.0;
        } else {
            robot->target_hue = robot->audio_intensity * 360.0;
        }
        robot->hue += (robot->target_hue - robot->hue) * dt * 5.0;
        
        // Check if robot is aligned enough with grid to turn
        double dx_to_grid = fabs(robot->x - robot->grid_x);
        double dy_to_grid = fabs(robot->y - robot->grid_y);
        gboolean near_grid_center = (dx_to_grid < 0.3 && dy_to_grid < 0.3);
        
        // At grid center or intersection - evaluate possible direction changes
        if (near_grid_center) {
            // Get AI's suggested direction
            ChaserDirection suggested_direction = robot_chaser_choose_smart_direction_v2(vis, robot, i);
            
            // Only change direction if:
            // 1. It's different from current direction
            // 2. The path in that direction is clear (no wall)
            // 3. It's considered safe by the AI
            if (suggested_direction != robot->direction) {
                int test_x = robot->grid_x;
                int test_y = robot->grid_y;
                
                // Calculate where this direction leads
                switch (suggested_direction) {
                    case CHASER_UP: test_y--; break;
                    case CHASER_DOWN: test_y++; break;
                    case CHASER_LEFT: test_x--; break;
                    case CHASER_RIGHT: test_x++; break;
                }
                
                // Verify the direction is valid and safe
                if (robot_chaser_can_move(vis, test_x, test_y)) {
                    robot->direction = suggested_direction;
                    // Snap to grid center for clean turning
                    robot->x = robot->grid_x;
                    robot->y = robot->grid_y;
                }
            }
        }
        
        // Move in current direction
        double old_x = robot->x, old_y = robot->y;
        int old_grid_x = robot->grid_x, old_grid_y = robot->grid_y;
        
        gboolean moved = robot_chaser_move_entity_safely(vis, &robot->x, &robot->y, 
                                                         &robot->grid_x, &robot->grid_y,
                                                         robot->direction, 
                                                         robot->speed * speed_multiplier, dt);
        
        if (!moved) {
            // Hit a wall - forced to choose new direction
            robot->direction = robot_chaser_choose_smart_direction_v2(vis, robot, i);
            robot->x = robot->grid_x;
            robot->y = robot->grid_y;
        }
    }
    
    // Handle power mode timer
    if (vis->robot_chaser_power_mode) {
        vis->robot_chaser_power_pellet_timer -= dt;
        if (vis->robot_chaser_power_pellet_timer <= 0) {
            vis->robot_chaser_power_mode = FALSE;
            for (int i = 0; i < vis->robot_chaser_robot_count; i++) {
                vis->robot_chaser_robots[i].scared = FALSE;
                vis->robot_chaser_robots[i].scared_timer = 0.0;
            }
        }
    }
}

// Updated main update function
void update_robot_chaser_visualization(Visualizer *vis, double dt) {
    robot_chaser_calculate_layout(vis);
    
    // Handle game state
    switch (vis->robot_chaser_game_state) {
        case GAME_PLAYING: {
            // ===== MOUSE CLICK DETECTION =====
            bool mouse_was_pressed = vis->robot_chaser_mouse_left_pressed_prev;
            bool mouse_is_pressed = vis->mouse_left_pressed;
            bool mouse_clicked = (mouse_was_pressed && !mouse_is_pressed);
            vis->robot_chaser_mouse_left_pressed_prev = mouse_is_pressed;
            // ==================================
            
            // ===== MOUSE INACTIVITY TIMER =====
            if (vis->mouse_x != vis->robot_chaser_mouse_last_x || 
                vis->mouse_y != vis->robot_chaser_mouse_last_y) {
                // Mouse moved - reset inactivity timer
                vis->robot_chaser_mouse_inactivity_timer = 0.0;
                vis->robot_chaser_mouse_last_x = vis->mouse_x;
                vis->robot_chaser_mouse_last_y = vis->mouse_y;
            } else {
                // Mouse hasn't moved - increment timer
                vis->robot_chaser_mouse_inactivity_timer += dt;
            }
            // ===================================
            
            // ===== MOUSE CONTROL =====
            if (vis->robot_chaser_mouse_enabled && vis->mouse_x > 0) {
                // If mouse inactive for 4+ seconds, use AI movement instead
                gboolean mouse_inactive = (vis->robot_chaser_mouse_inactivity_timer >= 4.0);
                
                if (mouse_clicked && (vis->robot_chaser_mouse_control_mode == 1 || 
                                      vis->robot_chaser_mouse_control_mode == 2)) {
                    robot_chaser_handle_click_to_move(vis, vis->mouse_x, vis->mouse_y);
                }
                
                if (mouse_inactive) {
                    // Fall back to AI movement
                    ChaserDirection ai_direction = robot_chaser_choose_player_direction(vis);
                    vis->robot_chaser_player.next_direction = ai_direction;
                } else if (vis->robot_chaser_mouse_control_mode == 0) {
                    robot_chaser_handle_mouse_aim(vis);
                } else if (vis->robot_chaser_mouse_control_mode == 1) {
                    robot_chaser_update_click_to_move(vis);
                } else {
                    if (vis->robot_chaser_has_mouse_target) {
                        robot_chaser_update_click_to_move(vis);
                    } else {
                        robot_chaser_handle_mouse_aim(vis);
                    }
                }
            }
            // ========================
            robot_chaser_update_player(vis, dt);
            robot_chaser_update_robots(vis, dt);
            robot_chaser_update_pellets(vis, dt);
            break;
        }
            
        case GAME_PLAYER_DIED:
            vis->robot_chaser_death_timer -= dt;
            if (vis->robot_chaser_death_timer <= 0) {
                if (vis->robot_chaser_lives > 0) {
                    robot_chaser_reset_level(vis);
                } else {
                    vis->robot_chaser_game_state = GAME_GAME_OVER;
                    vis->robot_chaser_death_timer = 5.0;
                }
            }
            break;
            
        case GAME_LEVEL_COMPLETE:
            vis->robot_chaser_death_timer -= dt;
            if (vis->robot_chaser_death_timer <= 0) {
                vis->robot_chaser_current_level++;
                
                if (vis->robot_chaser_current_level >= ROBOT_CHASER_NUM_LEVELS) {
                    vis->robot_chaser_current_level = 0;
                }
                
                vis->robot_chaser_score += 1000;
                
                robot_chaser_init_maze(vis);
                robot_chaser_reset_level(vis);
            }
            break;
            
        case GAME_GAME_OVER:
            vis->robot_chaser_death_timer -= dt;
            if (vis->robot_chaser_death_timer <= 0) {
                vis->robot_chaser_lives = 3;
                vis->robot_chaser_score = 0;
                vis->robot_chaser_current_level = 0;
                robot_chaser_init_maze(vis);
                robot_chaser_reset_level(vis);
            }
            break;
    }
    
    vis->robot_chaser_beat_timer += dt;
}

void draw_robot_chaser_visualization_enhanced(Visualizer *vis, cairo_t *cr) {
    if (vis->width <= 0 || vis->height <= 0) return;
    
    double bg_pulse = 0.05 + vis->volume_level * 0.1;
    cairo_set_source_rgba(cr, bg_pulse, bg_pulse, bg_pulse * 2, 1.0);
    cairo_paint(cr);
    
    draw_robot_chaser_maze(vis, cr);
    draw_robot_chaser_pellets(vis, cr);
    draw_robot_chaser_robots(vis, cr);
    
    // Only draw player if not dead
    if (vis->robot_chaser_game_state != GAME_PLAYER_DIED || 
        fmod(vis->robot_chaser_death_timer, 0.3) < 0.15) {
        draw_robot_chaser_player(vis, cr);
    }
    
    // Draw UI
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 20);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
    
    // Score, lives, and level
    char ui_text[64];
    snprintf(ui_text, sizeof(ui_text), "Level: %d  Score: %d  Lives: %d", 
             vis->robot_chaser_current_level + 1, // Display as 1-5 instead of 0-4
             vis->robot_chaser_score, 
             vis->robot_chaser_lives);
    cairo_move_to(cr, 20, 30);
    cairo_show_text(cr, ui_text);
    
    // Game state messages
    cairo_set_font_size(cr, 36);
    switch (vis->robot_chaser_game_state) {
        case GAME_PLAYER_DIED:
            cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.9);
            cairo_move_to(cr, vis->width / 2 - 100, vis->height / 2);
            cairo_show_text(cr, "Terminated");
            break;
            
        case GAME_LEVEL_COMPLETE: {
            // Add braces to create a scope for variable declarations
            cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.9);
            
            cairo_text_extents_t extents;
            char level_complete_text[64];
            snprintf(level_complete_text, sizeof(level_complete_text), "LEVEL %d COMPLETE!", 
                     vis->robot_chaser_current_level + 1);
            
            cairo_text_extents(cr, level_complete_text, &extents);
            cairo_move_to(cr, (vis->width - extents.width) / 2, vis->height / 2);
            cairo_show_text(cr, level_complete_text);
            
            // Next level preview
            cairo_set_font_size(cr, 24);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.8);
            
            int next_level = (vis->robot_chaser_current_level + 1) % ROBOT_CHASER_NUM_LEVELS;
            char next_level_text[64];
            snprintf(next_level_text, sizeof(next_level_text), "Next: Level %d", next_level + 1);
            
            cairo_text_extents(cr, next_level_text, &extents);
            cairo_move_to(cr, (vis->width - extents.width) / 2, vis->height / 2 + 50);
            cairo_show_text(cr, next_level_text);
            break;
        }
            
        case GAME_GAME_OVER:
            cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.9);
            cairo_move_to(cr, vis->width / 2 - 150, vis->height / 2);
            cairo_show_text(cr, "GAME OVER");
            cairo_set_font_size(cr, 20);
            cairo_move_to(cr, vis->width / 2 - 100, vis->height / 2 + 40);
            cairo_show_text(cr, "Restarting...");
            break;
            
        case GAME_PLAYING:
        default:
            // No message during normal play
            break;
    }
    
    // Power mode indicator
    if (vis->robot_chaser_power_mode) {
        double power_flash = 0.15 + 0.1 * sin(vis->robot_chaser_power_pellet_timer * 8.0);
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, power_flash);
        cairo_paint(cr);
        
        cairo_set_font_size(cr, 24);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.9);
        
        char timer_text[32];
        snprintf(timer_text, sizeof(timer_text), "POWER MODE: %.1f", vis->robot_chaser_power_pellet_timer);
        
        cairo_text_extents_t extents;
        cairo_text_extents(cr, timer_text, &extents);
        cairo_move_to(cr, (vis->width - extents.width) / 2, 65);
        cairo_show_text(cr, timer_text);
    }
    
    // Beat pulse effect
    if (vis->robot_chaser_player.beat_pulse > 0) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, vis->robot_chaser_player.beat_pulse * 0.08);
        cairo_paint(cr);
    }
    
    // Draw mouse UI elements
    robot_chaser_draw_mouse_ui(vis, cr);
}

const char* robot_chaser_get_level_name(int level) {
    switch (level) {
        case 0: return "Classic Maze";
        case 1: return "The Corridors";
        case 2: return "The Cross";
        case 3: return "The Spiral";
        case 4: return "The Arena";
        default: return "Unknown";
    }
}

void robot_chaser_unstick_robot(Visualizer *vis, ChaserRobot *robot) {
    static double stuck_timers[MAX_ROBOT_CHASER_ROBOTS] = {0};
    static double last_positions[MAX_ROBOT_CHASER_ROBOTS][2];
    
    int robot_index = robot - vis->robot_chaser_robots;
    
    // Check if robot is in the same position as last check
    if (robot->x == last_positions[robot_index][0] && 
        robot->y == last_positions[robot_index][1]) {
        stuck_timers[robot_index] += 0.033; // Assuming 30fps
        
        if (stuck_timers[robot_index] > 2.0) { // Stuck for 2 seconds
            // Teleport to center open area
            robot->x = 12;
            robot->y = 3;
            robot->grid_x = 12;
            robot->grid_y = 3;
            robot->direction = (ChaserDirection)(rand() % 4);
            stuck_timers[robot_index] = 0;
        }
    } else {
        stuck_timers[robot_index] = 0;
    }
    
    last_positions[robot_index][0] = robot->x;
    last_positions[robot_index][1] = robot->y;
}

gboolean robot_chaser_is_robot_nearby(Visualizer *vis, int x, int y, int look_ahead_distance) {
    for (int i = 0; i < vis->robot_chaser_robot_count; i++) {
        ChaserRobot *robot = &vis->robot_chaser_robots[i];
        if (!robot->visible || robot->scared) continue; // Ignore scared/invisible robots
        
        // Calculate distance to robot
        int dx = abs(robot->grid_x - x);
        int dy = abs(robot->grid_y - y);
        int distance = dx + dy; // Manhattan distance
        
        if (distance <= look_ahead_distance) {
            return TRUE;
        }
    }
    return FALSE;
}

// Add this helper to check if a direction leads toward a dangerous robot
gboolean robot_chaser_direction_leads_to_danger(Visualizer *vis, int from_x, int from_y, ChaserDirection direction, int look_ahead) {
    int test_x = from_x;
    int test_y = from_y;
    
    // Check multiple steps ahead in this direction
    for (int step = 1; step <= look_ahead; step++) {
        switch (direction) {
            case CHASER_UP: test_y = from_y - step; break;
            case CHASER_DOWN: test_y = from_y + step; break;
            case CHASER_LEFT: test_x = from_x - step; break;
            case CHASER_RIGHT: test_x = from_x + step; break;
        }
        
        // Stop if we hit a wall
        if (!robot_chaser_can_move(vis, test_x, test_y)) {
            break;
        }
        
        // Check if there's a dangerous robot at this position
        for (int i = 0; i < vis->robot_chaser_robot_count; i++) {
            ChaserRobot *robot = &vis->robot_chaser_robots[i];
            if (!robot->visible || robot->scared) continue;
            
            if (robot->grid_x == test_x && robot->grid_y == test_y) {
                return TRUE; // Dangerous robot found in this direction
            }
        }
    }
    return FALSE;
}

ChaserDirection robot_chaser_choose_player_direction(Visualizer *vis) {
    ChaserPlayer *player = &vis->robot_chaser_player;
    
    // Get all valid directions
    ChaserDirection possible_directions[4];
    int possible_count = 0;
    
    for (int dir = 0; dir < 4; dir++) {
        int next_x = player->grid_x;
        int next_y = player->grid_y;
        
        switch (dir) {
            case CHASER_UP: next_y--; break;
            case CHASER_DOWN: next_y++; break;
            case CHASER_LEFT: next_x--; break;
            case CHASER_RIGHT: next_x++; break;
        }
        
        if (robot_chaser_can_move(vis, next_x, next_y)) {
            possible_directions[possible_count++] = (ChaserDirection)dir;
        }
    }
    
    if (possible_count == 0) {
        return player->direction; // Keep current direction if stuck
    }
    
    // PHASE 1: SAFETY - Remove directions that lead to immediate danger
    ChaserDirection safe_directions[4];
    int safe_count = 0;
    
    for (int i = 0; i < possible_count; i++) {
        ChaserDirection dir = possible_directions[i];
        
        // Check if this direction leads to danger in the next 3-4 steps
        if (!robot_chaser_direction_leads_to_danger(vis, player->grid_x, player->grid_y, dir, 4)) {
            safe_directions[safe_count++] = dir;
        }
    }
    
    // If all directions are dangerous, pick the least bad one (furthest from robots)
    if (safe_count == 0) {
        ChaserDirection best_direction = possible_directions[0];
        double best_distance = -1;
        
        for (int i = 0; i < possible_count; i++) {
            int test_x = player->grid_x;
            int test_y = player->grid_y;
            
            switch (possible_directions[i]) {
                case CHASER_UP: test_y--; break;
                case CHASER_DOWN: test_y++; break;
                case CHASER_LEFT: test_x--; break;
                case CHASER_RIGHT: test_x++; break;
            }
            
            // Find distance to nearest dangerous robot
            double min_robot_distance = 1000.0;
            for (int j = 0; j < vis->robot_chaser_robot_count; j++) {
                ChaserRobot *robot = &vis->robot_chaser_robots[j];
                if (!robot->visible || robot->scared) continue;
                
                double dx = test_x - robot->grid_x;
                double dy = test_y - robot->grid_y;
                double distance = sqrt(dx * dx + dy * dy);
                
                if (distance < min_robot_distance) {
                    min_robot_distance = distance;
                }
            }
            
            if (min_robot_distance > best_distance) {
                best_distance = min_robot_distance;
                best_direction = possible_directions[i];
            }
        }
        
        return best_direction;
    }
    
    // PHASE 2: Remove backwards direction from safe directions (unless it's the only safe option)
    if (safe_count > 1) {
        ChaserDirection opposite = robot_chaser_get_opposite_direction(player->direction);
        ChaserDirection forward_safe_directions[4];
        int forward_safe_count = 0;
        
        for (int i = 0; i < safe_count; i++) {
            if (safe_directions[i] != opposite) {
                forward_safe_directions[forward_safe_count++] = safe_directions[i];
            }
        }
        
        // Use forward safe directions if available
        if (forward_safe_count > 0) {
            safe_count = forward_safe_count;
            for (int i = 0; i < safe_count; i++) {
                safe_directions[i] = forward_safe_directions[i];
            }
        }
    }
    
    // PHASE 3: Among safe directions, choose the one closest to the nearest pellet
    // (REMOVED the "prefer continuing straight" logic that was here)
    int target_x, target_y;
    robot_chaser_find_nearest_pellet(vis, player->grid_x, player->grid_y, &target_x, &target_y);
    
    ChaserDirection best_direction = safe_directions[0];
    double best_distance = 1000.0;
    
    for (int i = 0; i < safe_count; i++) {
        int test_x = player->grid_x;
        int test_y = player->grid_y;
        
        switch (safe_directions[i]) {
            case CHASER_UP: test_y--; break;
            case CHASER_DOWN: test_y++; break;
            case CHASER_LEFT: test_x--; break;
            case CHASER_RIGHT: test_x++; break;
        }
        
        double dx = test_x - target_x;
        double dy = test_y - target_y;
        double distance = sqrt(dx * dx + dy * dy);
        
        if (distance < best_distance) {
            best_distance = distance;
            best_direction = safe_directions[i];
        }
    }
     
    return best_direction;
}

void robot_chaser_handle_wraparound(Visualizer *vis, double *x, double *y, int *grid_x, int *grid_y) {
    if (*x < 0) {
        *x = ROBOT_CHASER_MAZE_WIDTH - 1;
        *grid_x = ROBOT_CHASER_MAZE_WIDTH - 1;
    }
    else if (*x >= ROBOT_CHASER_MAZE_WIDTH) {
        *x = 0;
        *grid_x = 0;
    }
    
    if (*y < 0) {
        *y = ROBOT_CHASER_MAZE_HEIGHT - 1;
        *grid_y = ROBOT_CHASER_MAZE_HEIGHT - 1;
    }
    else if (*y >= ROBOT_CHASER_MAZE_HEIGHT) {
        *y = 0;
        *grid_y = 0;
    }
}

// ============================================================================
// ROBOT CHASER MOUSE CONTROL FUNCTIONS
// ============================================================================

gboolean robot_chaser_screen_to_grid(Visualizer *vis, int screen_x, int screen_y,
                                      int *grid_x, int *grid_y) {
    double maze_left = vis->robot_chaser_offset_x;
    double maze_top = vis->robot_chaser_offset_y;
    double maze_right = maze_left + (ROBOT_CHASER_MAZE_WIDTH * vis->robot_chaser_cell_size);
    double maze_bottom = maze_top + (ROBOT_CHASER_MAZE_HEIGHT * vis->robot_chaser_cell_size);
    
    if (screen_x < maze_left || screen_x > maze_right ||
        screen_y < maze_top || screen_y > maze_bottom) {
        return FALSE;
    }
    
    *grid_x = (int)((screen_x - maze_left) / vis->robot_chaser_cell_size);
    *grid_y = (int)((screen_y - maze_top) / vis->robot_chaser_cell_size);
    
    if (*grid_x < 0) *grid_x = 0;
    if (*grid_y < 0) *grid_y = 0;
    if (*grid_x >= ROBOT_CHASER_MAZE_WIDTH) *grid_x = ROBOT_CHASER_MAZE_WIDTH - 1;
    if (*grid_y >= ROBOT_CHASER_MAZE_HEIGHT) *grid_y = ROBOT_CHASER_MAZE_HEIGHT - 1;
    
    return TRUE;
}

void robot_chaser_grid_to_screen(Visualizer *vis, int grid_x, int grid_y,
                                  double *screen_x, double *screen_y) {
    *screen_x = vis->robot_chaser_offset_x + (grid_x + 0.5) * vis->robot_chaser_cell_size;
    *screen_y = vis->robot_chaser_offset_y + (grid_y + 0.5) * vis->robot_chaser_cell_size;
}

ChaserDirection robot_chaser_angle_to_direction(double angle) {
    if (angle > -M_PI/4 && angle <= M_PI/4) {
        return CHASER_RIGHT;
    } else if (angle > M_PI/4 && angle <= 3*M_PI/4) {
        return CHASER_DOWN;
    } else if (angle > -3*M_PI/4 && angle <= -M_PI/4) {
        return CHASER_UP;
    } else {
        return CHASER_LEFT;
    }
}

void robot_chaser_handle_mouse_aim(Visualizer *vis) {
    if (!vis->robot_chaser_mouse_enabled) {
        return;
    }
    
    double player_screen_x, player_screen_y;
    robot_chaser_grid_to_screen(vis, 
                                 vis->robot_chaser_player.grid_x,
                                 vis->robot_chaser_player.grid_y,
                                 &player_screen_x, &player_screen_y);
    
    double dx = vis->mouse_x - player_screen_x;
    double dy = vis->mouse_y - player_screen_y;
    double distance = sqrt(dx*dx + dy*dy);
    
    if (distance < 3.0) {
        return;
    }
    
    double angle = atan2(dy, dx);
    vis->robot_chaser_player.next_direction = robot_chaser_angle_to_direction(angle);
}

void robot_chaser_handle_click_to_move(Visualizer *vis, int screen_x, int screen_y) {
    int grid_x, grid_y;
    
    if (!robot_chaser_screen_to_grid(vis, screen_x, screen_y, &grid_x, &grid_y)) {
        return;
    }
    
    if (vis->robot_chaser_maze[grid_y][grid_x] == CHASER_WALL) {
        return;
    }
    
    vis->robot_chaser_mouse_target_grid_x = grid_x;
    vis->robot_chaser_mouse_target_grid_y = grid_y;
    vis->robot_chaser_has_mouse_target = TRUE;
}

void robot_chaser_update_click_to_move(Visualizer *vis) {
    if (!vis->robot_chaser_has_mouse_target) {
        return;
    }
    
    int target_x = vis->robot_chaser_mouse_target_grid_x;
    int target_y = vis->robot_chaser_mouse_target_grid_y;
    
    int curr_x = (int)(vis->robot_chaser_player.x + 0.5);
    int curr_y = (int)(vis->robot_chaser_player.y + 0.5);
    
    if (curr_x == target_x && curr_y == target_y) {
        vis->robot_chaser_has_mouse_target = FALSE;
        return;
    }
    
    ChaserDirection best_dir = CHASER_RIGHT;
    int best_distance = abs(target_x - curr_x) + abs(target_y - curr_y);
    
    if (curr_x + 1 < ROBOT_CHASER_MAZE_WIDTH && 
        robot_chaser_can_move(vis, curr_x + 1, curr_y)) {
        int dist = abs(target_x - (curr_x + 1)) + abs(target_y - curr_y);
        if (dist < best_distance) {
            best_distance = dist;
            best_dir = CHASER_RIGHT;
        }
    }
    
    if (curr_x - 1 >= 0 && robot_chaser_can_move(vis, curr_x - 1, curr_y)) {
        int dist = abs(target_x - (curr_x - 1)) + abs(target_y - curr_y);
        if (dist < best_distance) {
            best_distance = dist;
            best_dir = CHASER_LEFT;
        }
    }
    
    if (curr_y + 1 < ROBOT_CHASER_MAZE_HEIGHT && 
        robot_chaser_can_move(vis, curr_x, curr_y + 1)) {
        int dist = abs(target_x - curr_x) + abs(target_y - (curr_y + 1));
        if (dist < best_distance) {
            best_distance = dist;
            best_dir = CHASER_DOWN;
        }
    }
    
    if (curr_y - 1 >= 0 && robot_chaser_can_move(vis, curr_x, curr_y - 1)) {
        int dist = abs(target_x - curr_x) + abs(target_y - (curr_y - 1));
        if (dist < best_distance) {
            best_distance = dist;
            best_dir = CHASER_UP;
        }
    }
    
    vis->robot_chaser_player.next_direction = best_dir;
}

void robot_chaser_draw_mouse_ui(Visualizer *vis, cairo_t *cr) {
    if (!vis->robot_chaser_mouse_enabled) {
        return;
    }
    
    double player_x, player_y;
    robot_chaser_grid_to_screen(vis,
                                 vis->robot_chaser_player.grid_x,
                                 vis->robot_chaser_player.grid_y,
                                 &player_x, &player_y);
    
    if (vis->robot_chaser_mouse_control_mode != 1) {
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.3);
        cairo_set_line_width(cr, 2.0);
        cairo_move_to(cr, player_x, player_y);
        cairo_line_to(cr, (double)vis->mouse_x, (double)vis->mouse_y);
        cairo_stroke(cr);
        
        double radius = vis->robot_chaser_cell_size * 0.3;
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.2);
        cairo_arc(cr, (double)vis->mouse_x, (double)vis->mouse_y, radius, 0, 2*M_PI);
        cairo_fill(cr);
    }
    
    if (vis->robot_chaser_has_mouse_target && vis->robot_chaser_mouse_control_mode != 0) {
        double target_x, target_y;
        robot_chaser_grid_to_screen(vis,
                                     vis->robot_chaser_mouse_target_grid_x,
                                     vis->robot_chaser_mouse_target_grid_y,
                                     &target_x, &target_y);
        
        double radius = vis->robot_chaser_cell_size * 0.4;
        double pulse = 0.5 + 0.5 * sin(vis->time_offset * 5.0);
        
        cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.3 * pulse);
        cairo_arc(cr, target_x, target_y, radius, 0, 2*M_PI);
        cairo_fill(cr);
        
        cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.6);
        cairo_set_line_width(cr, 1.5);
        cairo_arc(cr, target_x, target_y, radius, 0, 2*M_PI);
        cairo_stroke(cr);
    }
}
