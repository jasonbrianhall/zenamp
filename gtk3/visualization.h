#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include "sudoku.h"
#include "generatepuzzle.h"
#include "bouncyball.h"
#include "kaleidoscope.h"
#include "ripples.h"
#include "fourier.h"
#include "dna.h"
#include "fireworks.h"
#include "matrix.h"
#include "bubble.h"
#include "clock.h"
#include "robotchaser.h"
#include "radialwave.h"
#include "blockstack.h"
#include "cdg.h"
#include "zip_support.h"
#include "parrot.h"
#include "sauron.h"
#include "hanoi.h"
#include "beatchess.h"
#include "beatcheckers.h"
#include "maze3d.h"
#include "drawradialbars.h"
#include "bouncingcircle.h"
#include "mandelbrot.h"
#include "pong.h"
#include "minesweeper.h"
#include "cometbuster.h"

#define VIS_SAMPLES 512
#define VIS_FREQUENCY_BARS 32
#define VIS_HISTORY_SIZE 64

typedef enum {
    VIS_MAZE_3D,
    VIS_ANALOG_CLOCK,
    VIS_BARS,
    VIS_BEAT_CHECKERS,
    VIS_BEAT_CHESS,
    VIS_BIRTHDAY,
    VIS_BLOCK_STACK,
    VIS_BOUNCING_CIRCLE,
    VIS_BOUNCY_BALLS,
    VIS_BUBBLES,
    VIS_CIRCLE,
    VIS_COMET_BUSTER,
    VIS_DNA_HELIX,
    VIS_DNA2_HELIX,
    VIS_PARROT,
    VIS_DIGITAL_CLOCK,
    VIS_FIREWORKS,
    VIS_FOURIER_TRANSFORM,
    VIS_FRACTAL_BLOOM,
    VIS_RABBITHARE,
    VIS_KALEIDOSCOPE,
    VIS_MANDELBROT,
    VIS_MATRIX,
    VIS_MINESWEEPER,
    VIS_OSCILLOSCOPE,
    VIS_PONG,
    VIS_RADIAL_BARS,
    VIS_RADIAL_WAVE,
    VIS_RIPPLES,
    VIS_ROBOT_CHASER,
    VIS_SUDOKU_SOLVER,
    VIS_SYMMETRY_CASCADE,
    VIS_EYE_OF_SAURON,
    VIS_TOWER_OF_HANOI,
    VIS_TRIPPY_BARS,
    VIS_VOLUME_METER,
    VIS_WAVEFORM,
    VIS_DRAW_WORMHOLE,
    VIS_KARAOKE,
    VIS_KARAOKE_EXCITING
} VisualizationType;

typedef struct {
    GtkWidget *drawing_area;
    cairo_surface_t *surface;
    
    // Error messages
    char error_message[4096];
    double error_display_time;
    bool showing_error;

    // Audio analysis data
    double *audio_samples;
    double *frequency_bands;
    double *peak_data;
    double *history[VIS_HISTORY_SIZE];
    int history_index;
    
    // Mouse interaction
    int mouse_x, mouse_y;
    int mouse_last_x, mouse_last_y;
    double mouse_velocity_x, mouse_velocity_y;
    gboolean mouse_over;
    gboolean mouse_left_pressed;
    gboolean mouse_right_pressed;
    gboolean mouse_middle_pressed;
    double mouse_distance_from_center;
    double mouse_influence_strength;
    gboolean mouse_interactions_enabled;
    double mouse_press_time;
    
    // CDG
    CDGDisplay *cdg_display;
    cairo_surface_t *cdg_surface;
    bool cdg_needs_update;
    int cdg_last_packet;
    
    // Simple frequency analysis without FFT
    double *band_filters[VIS_FREQUENCY_BARS];  // Use renamed constant
    double *band_values;
    
    // Visualization settings
    VisualizationType type;
    double sensitivity;
    double decay_rate;
    gboolean enabled;
    
    // Color scheme
    double bg_r, bg_g, bg_b;
    double fg_r, fg_g, fg_b;
    double accent_r, accent_g, accent_b;

    // Track info overlay
    double track_info_display_time;    // Seconds remaining to show
    char track_info_title[256];        // Song title
    char track_info_artist[256];       // Artist name
    char track_info_album[256];        // Album name
    int track_info_duration;           // Duration in seconds
    double track_info_fade_alpha;      // Fade opacity (0.0 to 1.0)
    
    // Animation
    guint timer_id;
    double rotation;
    double time_offset;
    double volume_level;
    
    // Size
    int width, height;
    
    // Bubble system data
    Bubble bubbles[MAX_BUBBLES];
    PopEffect pop_effects[MAX_POP_EFFECTS];
    int bubble_count;
    int pop_effect_count;
    double bubble_spawn_timer;
    double last_peak_level;
    
    // Matrix
    MatrixColumn matrix_columns[MAX_MATRIX_COLUMNS];
    int matrix_column_count;
    double matrix_spawn_timer;
    int matrix_char_size;

    // Fireworks
    Firework fireworks[MAX_FIREWORKS];
    FireworkParticle particles[MAX_TOTAL_PARTICLES];
    int firework_count;
    int particle_count;
    double firework_spawn_timer;
    double last_beat_time;
    double beat_threshold;
    double gravity;

    // DNA 1
    DNAHelix dna_helix;
    double dna_time_offset;
    double dna_amplitude_multiplier;
    double dna_twist_rate;

    // DNA 2
    DNASegment dna_segments[MAX_DNA_SEGMENTS];
    int dna_segment_count;
    double dna_rotation;
    double dna_twist_speed;
    double dna_spine_offset;
    double dna_base_pulse[DNA_BASE_PAIRS]; // Pulse intensity for each base type    

    // Sudoku visualization
    Sudoku* sudoku_solver;
    PuzzleGenerator* puzzle_generator;
    double sudoku_solve_timer;
    double sudoku_beat_threshold;
    int sudoku_solving_speed;  // milliseconds per move
    bool sudoku_is_solving;
    bool sudoku_puzzle_complete;
    int sudoku_current_step;
    double sudoku_last_beat;
    char sudoku_difficulty[32];
    
    // Visual state
    int sudoku_highlight_x, sudoku_highlight_y;
    double sudoku_highlight_intensity;
    int sudoku_last_changed_x, sudoku_last_changed_y;
    double sudoku_change_glow;
    
    // Beat detection for solving steps
    double sudoku_volume_history[10];
    int sudoku_volume_index;
    int sudoku_last_placed_value;     // The number that was just placed
    double sudoku_completion_glow;    // Celebration effect when puzzle completes    

    // Sudoku pre-solved data
    int sudoku_original_puzzle[9][9];    // The starting puzzle  
    int sudoku_complete_solution[9][9];  // The complete solution
    int sudoku_reveal_order[81][2];      // Order to reveal cells (x,y pairs)
    int sudoku_reveal_index;             // Current position in reveal order
    int sudoku_total_empty_cells;        // How many cells need to be filled
    
    // Background puzzle generation
    Sudoku* background_solver;           // Secondary solver for background generation
    PuzzleGenerator* background_generator; // Background puzzle generator
    bool background_puzzle_ready;        // Is background puzzle ready?
    int background_original_puzzle[9][9]; // Background puzzle state
    int background_complete_solution[9][9]; // Background complete solution
    int background_reveal_order[81][2];   // Background reveal order
    int background_total_empty_cells;    // Background empty cell count
    char background_difficulty[32];      // Background puzzle difficulty
    bool generating_background_puzzle;   // Currently generating?
    
    // Beat synchronization
    double last_real_beat;               // Last detected beat timestamp
    double beat_interval;                // Average time between beats
    double beat_sync_timer;              // Timer for beat synchronization
    bool waiting_for_beat_sync;          // Should wait for next beat?

    // Fourier
    FourierBin fourier_bins[FOURIER_POINTS];
    double fourier_time;
    double fourier_rotation_speed;
    double fourier_zoom;
    int fourier_display_mode; // 0=circular, 1=linear, 2=3D perspective
    double fourier_trail_fade;
    gboolean show_fourier_math; // Show frequency labels and values    

    // Ripple
    Ripple ripples[MAX_RIPPLES];
    int ripple_count;
    double ripple_spawn_timer;
    double last_ripple_volume;
    double ripple_beat_threshold;

    // Kaleidoscope system
    KaleidoscopeShape kaleidoscope_shapes[MAX_KALEIDOSCOPE_SHAPES];
    int kaleidoscope_shape_count;
    double kaleidoscope_rotation;          // Global rotation
    double kaleidoscope_rotation_speed;    // Rotation speed based on tempo
    double kaleidoscope_zoom;              // Zoom level
    double kaleidoscope_zoom_target;       // Target zoom level
    double kaleidoscope_spawn_timer;       // Timer for spawning new shapes
    double kaleidoscope_mirror_offset;     // Offset for mirror positioning
    int kaleidoscope_mirror_count;         // Number of mirrors (3-12)
    double kaleidoscope_color_shift;       // Global color shift
    gboolean kaleidoscope_auto_shapes;     // Automatically spawn shapes on beats

    // Bouncy Ball
    BouncyBall bouncy_balls[MAX_BOUNCY_BALLS];
    int bouncy_ball_count;
    double bouncy_spawn_timer;
    double bouncy_beat_threshold;
    double bouncy_gravity_strength;
    double bouncy_size_multiplier;
    gboolean bouncy_physics_enabled;

    // Digital Clock
    SwirlParticle swirl_particles[MAX_SWIRL_PARTICLES];
    int swirl_particle_count;
    double swirl_spawn_timer;
    double swirl_beat_threshold;
    double clock_center_x, clock_center_y;
    double clock_dot_size;
    double clock_digit_spacing;
    double clock_colon_blink_timer;
    double clock_beat_pulse;
    gboolean clock_show_seconds;

    // Analog Clock
    ClockParticle clock_particles[MAX_CLOCK_PARTICLES];
    int clock_particle_count;
    double analog_clock_radius;
    double analog_clock_center_x, analog_clock_center_y;
    double clock_particle_spawn_timer;
    double clock_beat_pulse_outer;
    double clock_beat_pulse_inner;
    double clock_hand_glow_intensity;
    double clock_face_glow;
    double clock_tick_volume_history[10];
    int clock_tick_volume_index;
    gboolean clock_show_numbers;
    gboolean clock_particles_enabled;

    // robot Chaser (pacman)
    ChaserPlayer robot_chaser_player;
    ChaserRobot robot_chaser_robots[MAX_ROBOT_CHASER_ROBOTS];
    ChaserPellet robot_chaser_pellets[MAX_ROBOT_CHASER_PELLETS];
    int robot_chaser_pellet_count;
    int robot_chaser_robot_count;
    double robot_chaser_cell_size;
    double robot_chaser_offset_x, robot_chaser_offset_y;
    double robot_chaser_beat_timer;
    double robot_chaser_power_pellet_timer;
    gboolean robot_chaser_power_mode;
    int robot_chaser_maze[ROBOT_CHASER_MAZE_HEIGHT][ROBOT_CHASER_MAZE_WIDTH];
    double robot_chaser_move_timer;
    double robot_chaser_robot_colors[ROBOT_CHASER_ROBOT_COLORS][3]; // RGB for each robot color
    int robot_chaser_score;
    
    GameState robot_chaser_game_state;
    double robot_chaser_death_timer;
    int robot_chaser_lives;
    int robot_chaser_current_level;

    // Radial Wave
    RadialWaveSystem radial_wave;
    double radial_beat_volume_history[10];
    int radial_beat_history_index;

    // Blockstack
    BlockStackSystem blockstack;

    // Parrot
    ParrotState parrot_state;

    // EOS
    EyeOfSauronState eye_of_sauron;

    // Tower of Hanoi
    HanoiSystem hanoi;

    // Beat Chess
    BeatChessVisualization beat_chess;
    
    // Checkers
    BeatCheckersVisualization beat_checkers;

    // Maze 3D
    Maze3D maze3d;
 
    // Radial Bar
    RadialBarsSystem  radial_bars_system;

    // Bouncing Circle
    BouncingCircleState bouncing_circle_state;

    // Fractal
    MandelbrotState mandelbrot;

    // Pong
    PongGame pong_game;

    // Minesweeper
    MinesweeperGame minesweeper_game; 
    
    CometBusterGame comet_buster;

} Visualizer;

// Function declarations
Visualizer* visualizer_new(void);
void visualizer_free(Visualizer *vis);
void visualizer_set_type(Visualizer *vis, VisualizationType type);
void visualizer_update_audio_data(Visualizer *vis, int16_t *samples, size_t sample_count, int channels);
void visualizer_set_enabled(Visualizer *vis, gboolean enabled);
GtkWidget* create_visualization_controls(Visualizer *vis);

// Internal functions
gboolean on_visualizer_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data);
gboolean on_visualizer_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data);
gboolean visualizer_timer_callback(gpointer user_data);
void draw_waveform(Visualizer *vis, cairo_t *cr);
void draw_oscilloscope(Visualizer *vis, cairo_t *cr);
void draw_bars(Visualizer *vis, cairo_t *cr);
void draw_circle(Visualizer *vis, cairo_t *cr);
void draw_volume_meter(Visualizer *vis, cairo_t *cr);
void draw_bubbles(Visualizer *vis, cairo_t *cr);
void process_audio_simple(Visualizer *vis);
void init_frequency_bands(Visualizer *vis);
bool save_last_visualization(VisualizationType vis_type);
bool load_last_visualization(VisualizationType *vis_type);
static gboolean on_queue_focus_in(GtkWidget *widget, GdkEventFocus *event, gpointer user_data);
void visualizer_next_mode(Visualizer *vis);
void visualizer_prev_mode(Visualizer *vis);
void on_vis_type_changed(GtkComboBox *combo, gpointer user_data);
bool ends_with_zip(const char *filename);

// Bubble system function declarations
void init_bubble_system(Visualizer *vis);
void spawn_bubble(Visualizer *vis, double intensity);
void create_pop_effect(Visualizer *vis, Bubble *bubble);
void update_bubbles(Visualizer *vis, double dt);
void spawn_bubble_at(Visualizer *vis, double intensity, double x, double y);

// Matrix
void init_matrix_system(Visualizer *vis);
void spawn_matrix_column(Visualizer *vis, int frequency_band);
void update_matrix(Visualizer *vis, double dt);
void draw_matrix(Visualizer *vis, cairo_t *cr);
const char* get_random_matrix_char(void);

// Fireworks
void init_fireworks_system(Visualizer *vis);
void spawn_firework(Visualizer *vis, double intensity, int frequency_band);
void explode_firework(Visualizer *vis, Firework *firework);
void update_fireworks(Visualizer *vis, double dt);
void draw_fireworks(Visualizer *vis, cairo_t *cr);
void spawn_particle(Visualizer *vis, double x, double y, double vx, double vy, 
                          double r, double g, double b, double life);
double get_hue_for_frequency(int frequency_band);

// DNA
void init_dna_system(Visualizer *vis);
void update_dna_helix(Visualizer *vis, double dt);
void draw_dna_helix(Visualizer *vis, cairo_t *cr);
void init_dna2_system(Visualizer *vis);
void update_dna2_helix(Visualizer *vis, double dt);
void draw_dna2_helix(Visualizer *vis, cairo_t *cr);
void get_base_color(int base_type, double intensity, double *r, double *g, double *b);
void on_visualizer_realize(GtkWidget *widget, gpointer user_data);

// Function declarations (Sudoku)
void init_sudoku_system(Visualizer *vis);
void update_sudoku_solver(Visualizer *vis, double dt);
void draw_sudoku_solver(Visualizer *vis, cairo_t *cr);
void sudoku_generate_new_puzzle(Visualizer *vis);
bool sudoku_detect_beat(Visualizer *vis);
void sudoku_solve_step(Visualizer *vis);
void sudoku_draw_grid(Visualizer *vis, cairo_t *cr);
void sudoku_draw_numbers(Visualizer *vis, cairo_t *cr);
void sudoku_draw_effects(Visualizer *vis, cairo_t *cr);
int sudoku_find_naked_single(Visualizer *vis);
int sudoku_place_single_from_technique(Visualizer *vis, const char* technique);
void sudoku_start_background_generation(Visualizer *vis);
void sudoku_update_background_generation(Visualizer *vis);
bool sudoku_detect_beat_with_tempo(Visualizer *vis);
void sudoku_generate_new_puzzle_from_background(Visualizer *vis);

// Fourier
void update_fourier_mode(Visualizer *vis, double dt);
void draw_fourier_math_overlay(Visualizer *vis, cairo_t *cr);
void draw_fourier_points(Visualizer *vis, cairo_t *cr);
void draw_fourier_trails(Visualizer *vis, cairo_t *cr);
void draw_fourier_background(Visualizer *vis, cairo_t *cr, double center_x, double center_y);
void draw_fourier_transform(Visualizer *vis, cairo_t *cr);
void update_fourier_transform(Visualizer *vis, double dt);
void init_fourier_system(Visualizer *vis);

void hsv_to_rgb(double h, double s, double v, double *r, double *g, double *b);

// Ripples
void init_ripple_system(Visualizer *vis);
void spawn_ripple(Visualizer *vis, double x, double y, double intensity, int frequency_band);
void update_ripples(Visualizer *vis, double dt);
void draw_ripples(Visualizer *vis, cairo_t *cr);
void spawn_ripple_at_mouse(Visualizer *vis, double mouse_x, double mouse_y, double intensity, int frequency_band);

// Kaleidoscope
void init_kaleidoscope_system(Visualizer *vis);
void spawn_kaleidoscope_shape(Visualizer *vis, double intensity, int frequency_band);
void update_kaleidoscope(Visualizer *vis, double dt);
void draw_kaleidoscope_shape(cairo_t *cr, KaleidoscopeShape *shape, double scale_factor);
void draw_kaleidoscope(Visualizer *vis, cairo_t *cr);

// Bouncy Ball
void init_bouncy_ball_system(Visualizer *vis);
void spawn_bouncy_ball(Visualizer *vis, double intensity, int frequency_band);
void update_bouncy_balls(Visualizer *vis, double dt);
void draw_bouncy_balls(Visualizer *vis, cairo_t *cr);
void bouncy_ball_wall_collision(BouncyBall *ball, double width, double height);
void bouncy_ball_update_trail(BouncyBall *ball);
void spawn_bouncy_ball_beat(Visualizer *vis, double x, double y);
void spawn_bouncy_ball_at_position(Visualizer *vis, double x, double y, int click_type);


// Digital Clock
void init_clock_system(Visualizer *vis);
void spawn_swirl_particle(Visualizer *vis, double intensity, int frequency_band);
void update_clock_swirls(Visualizer *vis, double dt);
void draw_clock_visualization(Visualizer *vis, cairo_t *cr);
void draw_digit_matrix(cairo_t *cr, int digit, double x, double y, double dot_size, double r, double g, double b, double intensity);
void draw_clock_swirls(Visualizer *vis, cairo_t *cr);
gboolean clock_detect_beat(Visualizer *vis);

// Analog Clock
void init_analog_clock_system(Visualizer *vis);
void spawn_clock_particle(Visualizer *vis, double angle, double intensity, int type);
void update_analog_clock(Visualizer *vis, double dt);
void draw_analog_clock(Visualizer *vis, cairo_t *cr);
void draw_clock_face(Visualizer *vis, cairo_t *cr);
void draw_clock_hands(Visualizer *vis, cairo_t *cr);
void draw_clock_particles(Visualizer *vis, cairo_t *cr);
void draw_clock_hour_marks(Visualizer *vis, cairo_t *cr);
gboolean analog_clock_detect_beat(Visualizer *vis);

// Robot Chaser
void init_robot_chaser_system(Visualizer *vis);
void update_robot_chaser_visualization(Visualizer *vis, double dt);
void draw_robot_chaser_visualization(Visualizer *vis, cairo_t *cr);
void robot_chaser_calculate_layout(Visualizer *vis);
void robot_chaser_init_maze(Visualizer *vis);
void robot_chaser_update_player(Visualizer *vis, double dt);
void robot_chaser_update_robots(Visualizer *vis, double dt);
void robot_chaser_update_pellets(Visualizer *vis, double dt);
void draw_robot_chaser_maze(Visualizer *vis, cairo_t *cr);
void draw_robot_chaser_player(Visualizer *vis, cairo_t *cr);
void draw_robot_chaser_robots(Visualizer *vis, cairo_t *cr);
void draw_robot_chaser_pellets(Visualizer *vis, cairo_t *cr);
gboolean robot_chaser_can_move(Visualizer *vis, int grid_x, int grid_y);
void robot_chaser_consume_pellet(Visualizer *vis, int grid_x, int grid_y);
gboolean robot_chaser_detect_beat(Visualizer *vis);
ChaserDirection robot_chaser_get_opposite_direction(ChaserDirection dir);
ChaserDirection robot_chaser_get_direction_to_target(int from_x, int from_y, int to_x, int to_y);
double robot_chaser_distance_to_player(ChaserRobot *robot, ChaserPlayer *player);
void robot_chaser_find_nearest_pellet(Visualizer *vis, int from_x, int from_y, int *target_x, int *target_y);
gboolean robot_chaser_check_collision_with_robots(Visualizer *vis);
gboolean robot_chaser_is_level_complete(Visualizer *vis);
gboolean robot_chaser_move_entity_safely(Visualizer *vis, double *x, double *y, int *grid_x, int *grid_y, ChaserDirection direction, double speed, double dt);
void robot_chaser_unstick_robot(Visualizer *vis, ChaserRobot *robot);
ChaserDirection robot_chaser_choose_player_direction(Visualizer *vis);
void robot_chaser_init_game_state(Visualizer *vis);
void draw_robot_chaser_visualization_enhanced(Visualizer *vis, cairo_t *cr);
void robot_chaser_handle_wraparound(Visualizer *vis, double *x, double *y, int *grid_x, int *grid_y);

// Radial Wave
void init_radial_wave_system(void *vis);
void update_radial_wave(void *vis, double dt);
void draw_radial_wave(void *vis, cairo_t *cr);
void draw_radial_ring(void *vis, cairo_t *cr, double center_x, double center_y);
void draw_pulsar_core(void *vis, cairo_t *cr, double center_x, double center_y);
void update_pulsar_beat(void *vis);
void spawn_star_particle(void *vis, double intensity);
gboolean radial_wave_detect_beat(void *vis);
void draw_background_waveform(void *vis_ptr, cairo_t *cr);

// Block Stack
void init_blockstack_system(void *vis);
void update_blockstack(void *vis, double dt);
void draw_blockstack(void *vis, cairo_t *cr);
void spawn_block(void *vis, int column, double intensity, int frequency_band);
void update_block_physics(Block *block, double dt, double ground_level);
gboolean blockstack_detect_beat(void *vis);
void get_block_color(int frequency_band, double intensity, double *r, double *g, double *b);

// Karaoke
void draw_karaoke_exciting(Visualizer *vis, cairo_t *cr);
void draw_karaoke_boring(Visualizer *vis, cairo_t *cr);

// Parrot
void draw_audio_bars_around_parrot(Visualizer *vis, cairo_t *cr, double cx, double cy, double scale);
void draw_parrot(Visualizer *vis, cairo_t *cr);
void update_parrot(Visualizer *vis, double dt);

// EOS
void update_eye_of_sauron(Visualizer *vis, double dt);
void draw_eye_of_sauron(Visualizer *vis, cairo_t *cr);

// TOH
void init_hanoi_system(Visualizer *vis);
void update_hanoi(Visualizer *vis, double dt);
void draw_hanoi(Visualizer *vis, cairo_t *cr);

// Beatchess (stupid chess engine)
// Core chess engine functions
void chess_init_board(ChessGameState *game);
bool chess_is_valid_move(ChessGameState *game, int fr, int fc, int tr, int tc);
bool chess_is_in_check(ChessGameState *game, ChessColor color);
void chess_make_move(ChessGameState *game, ChessMove move);
int chess_evaluate_position(ChessGameState *game);
int chess_get_all_moves(ChessGameState *game, ChessColor color, ChessMove *moves);
int chess_minimax(ChessGameState *game, int depth, int alpha, int beta, bool maximizing);

// Thinking state management
void chess_init_thinking_state(ChessThinkingState *ts);
void chess_start_thinking(ChessThinkingState *ts, ChessGameState *game);
ChessMove chess_get_best_move_now(ChessThinkingState *ts);
void chess_stop_thinking(ChessThinkingState *ts);
void* chess_think_continuously(void* arg);

// Chess Visualization functions
void init_beat_chess_system(void *vis);
void update_beat_chess(void *vis, double dt);
void draw_beat_chess(void *vis, cairo_t *cr);
bool beat_chess_detect_beat(void *vis);
void chess_cleanup_thinking_state(ChessThinkingState *ts);

// Helper drawing functions
void draw_chess_board(BeatChessVisualization *chess, cairo_t *cr);
void draw_chess_pieces(BeatChessVisualization *chess, cairo_t *cr);
void draw_chess_last_move_highlight(BeatChessVisualization *chess, cairo_t *cr);
void draw_chess_status(BeatChessVisualization *chess, cairo_t *cr, int width, int height);
void draw_chess_eval_bar(BeatChessVisualization *chess, cairo_t *cr, int width, int height);
void draw_chess_reset_button(BeatChessVisualization *chess, cairo_t *cr, int width, int height);
void draw_chess_pvsa_button(BeatChessVisualization *chess, cairo_t *cr, int width, int height);
void get_piece_symbol(PieceType type, ChessColor color, char *buffer);

// Chess functions
bool chess_is_in_bounds(int r, int c);
bool chess_is_path_clear(ChessGameState *game, int fr, int fc, int tr, int tc);
ChessGameStatus chess_check_game_status(ChessGameState *game);
void update_beat_chess(void *vis_ptr, double dt);

// Beat Checkers (checkers engine)
// Core checkers engine functions
void checkers_init_board(CheckersGameState *game);
bool checkers_is_valid_move(CheckersGameState *game, CheckersMove *move);
void checkers_make_move(CheckersGameState *game, CheckersMove *move);
int checkers_evaluate_position(CheckersGameState *game);
int checkers_get_all_moves(CheckersGameState *game, CheckersColor color, CheckersMove *moves);
CheckersGameStatus checkers_check_game_status(CheckersGameState *game);
void checkers_cleanup_thinking_state(CheckersThinkingState *ts);

// Thinking state management
void checkers_init_thinking_state(CheckersThinkingState *ts);
void checkers_start_thinking(CheckersThinkingState *ts, CheckersGameState *game);
CheckersMove checkers_get_best_move_now(CheckersThinkingState *ts);
void checkers_stop_thinking(CheckersThinkingState *ts);
void* checkers_think_continuously(void* arg);

// Checkers Visualization functions
void init_beat_checkers_system(void *vis);
void update_beat_checkers(void *vis, double dt);
void draw_beat_checkers(void *vis, cairo_t *cr);
bool beat_checkers_detect_beat(void *vis);

// Fractal Bloom
void draw_waveform_fractal_bloom(Visualizer *vis, cairo_t *cr);

// Cascade
void draw_waveform_symmetry_cascade(Visualizer *vis, cairo_t *cr);

// Trippy Bars
void draw_trippy(Visualizer *vis, cairo_t *cr);
void update_trippy(Visualizer *vis, double dt);

// wormhole 
void draw_stargate(Visualizer *vis, cairo_t *cr);
void update_stargate(Visualizer *vis, double dt);

void draw_birthday_candles(Visualizer *vis, cairo_t *cr);

void draw_rabbithare(Visualizer *vis, cairo_t *cr);
void update_rabbithare(Visualizer *vis, double dt);

// Function declarations
void init_maze3d_system(Visualizer *vis);
void generate_maze(Maze3D *maze);
void solve_maze(Maze3D *maze);
void update_maze3d(Visualizer *vis, double dt);
void draw_maze3d(Visualizer *vis, cairo_t *cr);
void draw_penguin_3d(cairo_t *cr, double screen_x, double distance, double scale, 
                     double rotation, double bob_offset, gboolean found, double pulse);

// Radial Bars
void draw_radial_bars_bouncing(Visualizer *vis, cairo_t *cr);
void update_radial_bars_bouncing(Visualizer *vis, double dt);

// Track Overlay
void show_track_info_overlay(Visualizer *vis, const char *title, const char *artist, const char *album,  int duration_seconds);
void update_track_info_overlay(Visualizer *vis, double dt);
void draw_track_info_overlay(Visualizer *vis, cairo_t *cr);

// Bouncing Circle
void init_bouncing_circle_system(void *vis);
void update_bouncing_circle(void *vis, BouncingCircleState *state, double width, double height, double dt);
void draw_bouncing_circle(void *vis, cairo_t *cr, double width, double height, BouncingCircleState *state);

// Initialization and cleanup
void init_mandelbrot_system(void *vis);

// Update and render
void update_mandelbrot(void *vis, double dt);
void draw_mandelbrot(void *vis, cairo_t *cr);

// Beat detection
gboolean mandelbrot_detect_beat(void *vis);

// Helper functions
int mandelbrot_calculate_iterations(double real, double imag, double center_x, double center_y, 
                                   double zoom, int max_iterations, int width, int height);
void mandelbrot_get_color(int iterations, int max_iterations, double hue_offset, 
                         double *r, double *g, double *b);
void mandelbrot_recalculate_region(void *vis, int width, int height);

gboolean on_visualizer_leave(GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);
gboolean on_visualizer_enter(GtkWidget *widget, GdkEventCrossing *event, gpointer user_data);
gboolean on_visualizer_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
gboolean on_visualizer_motion(GtkWidget *widget, GdkEventMotion *event, gpointer user_data);

// Pong
void pong_init(void *vis);
void pong_update(void *vis, double dt);
void pong_draw(void *vis, cairo_t *cr);

// Minesweeper

void init_minesweeper(Visualizer *vis);
void minesweeper_draw(Visualizer *vis, cairo_t *cr);
void minesweeper_update(Visualizer *vis, double dt);

// Comet Buster
void init_comet_buster_system(Visualizer *vis_ptr);
void update_comet_buster(Visualizer *vis_ptr, double dt);
void draw_comet_buster(Visualizer *vis_ptr, cairo_t *cr);
void comet_buster_cleanup(CometBusterGame *game);
void comet_buster_on_ship_hit(CometBusterGame *game, Visualizer *visualizer);
bool comet_buster_splash_screen_input_detected(Visualizer *visualizer);
void comet_buster_update_splash_screen(CometBusterGame *game, double dt, int width, int height, Visualizer *visualizer);
void comet_buster_update_enemy_ships(CometBusterGame *game, double dt, int width, int height, Visualizer *visualizer);
void comet_buster_brown_coat_standard_fire(CometBusterGame *game, int ship_index, Visualizer *visualizer);
void comet_buster_update_brown_coat_ship(CometBusterGame *game, int ship_index, double dt, Visualizer *visualizer);

#endif
