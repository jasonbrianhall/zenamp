#ifndef BEATCHECKERS_H
#define BEATCHECKERS_H

#include <gtk/gtk.h>
#include <cairo.h>
#include <pthread.h>
#include <stdbool.h>

#define CHECKERS_BOARD_SIZE 8
#define MAX_CHECKERS_MOVES 64
#define MAX_JUMP_CHAIN 12
#define CHECKERS_BEAT_HISTORY 10

typedef enum { CHECKERS_NONE, CHECKERS_RED, CHECKERS_BLACK } CheckersColor;

typedef enum {
    CHECKERS_MODE_PLAYER_VS_AI,
    CHECKERS_MODE_AI_VS_AI
} CheckersGameMode;

typedef struct {
    CheckersColor color;
    bool is_king;
} CheckersPiece;

typedef struct {
    int from_row, from_col;
    int to_row, to_col;
    int jump_count;  // Number of pieces jumped in this move
    int jumped_rows[MAX_JUMP_CHAIN];
    int jumped_cols[MAX_JUMP_CHAIN];
    bool becomes_king;
} CheckersMove;

typedef struct {
    CheckersPiece board[CHECKERS_BOARD_SIZE][CHECKERS_BOARD_SIZE];
    CheckersColor turn;
    int red_pieces;
    int black_pieces;
} CheckersGameState;

typedef struct {
    CheckersGameState game;
    CheckersMove best_move;
    int best_score;
    int current_depth;
    bool has_move;
    bool thinking;
    pthread_mutex_t lock;
    pthread_t thread;
} CheckersThinkingState;

typedef enum {
    CHECKERS_PLAYING,
    CHECKERS_RED_WINS,
    CHECKERS_BLACK_WINS,
    CHECKERS_DRAW
} CheckersGameStatus;

// Move history structure for undo functionality
typedef struct {
    CheckersGameState game;
    CheckersMove move;
    double time_elapsed;
} CheckersMoveHistory;

typedef struct {
    // Game state
    CheckersGameState game;
    CheckersThinkingState thinking_state;
    CheckersGameStatus status;
    
    // Animation
    int animating_from_row, animating_from_col;
    int animating_to_row, animating_to_col;
    double animation_progress;
    bool is_animating;
    
    // Jump animation - for multi-jump sequences
    CheckersMove current_jump_chain;
    int jump_animation_index;  // Which jump in the chain we're animating
    int current_move_jump_count;  // Number of jumps in current move for animation speed
    
    // Last move highlight
    int last_from_row, last_from_col;
    int last_to_row, last_to_col;
    double last_move_glow;
    
    // Captured pieces fade out
    int captured_pieces[MAX_JUMP_CHAIN * 2];
    double captured_fade[MAX_JUMP_CHAIN * 2];
    CheckersColor captured_colors[MAX_JUMP_CHAIN * 2];  // Store the color of each captured piece
    int captured_count;
    
    // Status
    char status_text[256];
    double status_flash_timer;
    double status_flash_color[3];
    double auto_reset_timer;
    
    // Beat detection
    double beat_volume_history[CHECKERS_BEAT_HISTORY];
    int beat_history_index;
    double time_since_last_move;
    double beat_threshold;
    
    // Visual
    double board_offset_x, board_offset_y;
    double cell_size;
    int move_count;
    
    // Game over
    int beats_since_game_over;
    bool waiting_for_restart;
    
    // Auto-play
    double time_thinking;
    double min_think_time;
    int good_move_threshold;
    bool auto_play_enabled;
    
    // King promotion celebration
    bool king_promotion_active;
    double king_promotion_glow;
    int king_promotion_row, king_promotion_col;
    
    // Reset button
    double reset_button_x, reset_button_y;
    double reset_button_width, reset_button_height;
    bool reset_button_hovered;
    double reset_button_glow;
    bool reset_button_was_pressed;  // Track previous frame state for click detection
    
    // ===== INTERACTIVE FEATURES =====
    // Game mode
    CheckersGameMode game_mode;
    bool player_vs_ai;
    
    // Player piece selection
    int selected_piece_row, selected_piece_col;
    bool has_selected_piece;
    int selected_piece_was_pressed;
    
    // Player vs AI toggle button
    double pvsa_button_x, pvsa_button_y;
    double pvsa_button_width, pvsa_button_height;
    bool pvsa_button_hovered;
    double pvsa_button_glow;
    bool pvsa_button_was_pressed;
    
    // Undo button (Player vs AI only)
    double undo_button_x, undo_button_y;
    double undo_button_width, undo_button_height;
    bool undo_button_hovered;
    double undo_button_glow;
    bool undo_button_was_pressed;
    
    // Move history
    CheckersMoveHistory move_history[MAX_CHECKERS_MOVES];
    int move_history_count;
    
    // Time tracking
    double white_total_time;  // Red
    double black_total_time;  // Black
    double current_move_start_time;
    double last_move_end_time;
    // ===== END INTERACTIVE FEATURES =====
    
} BeatCheckersVisualization;

// Core game functions
void checkers_init_board(CheckersGameState *game);
bool checkers_is_valid_move(CheckersGameState *game, CheckersMove *move);
void checkers_make_move(CheckersGameState *game, CheckersMove *move);
int checkers_get_all_moves(CheckersGameState *game, CheckersColor color, CheckersMove *moves);
int checkers_evaluate_position(CheckersGameState *game);
CheckersGameStatus checkers_check_game_status(CheckersGameState *game);

// AI functions
void checkers_init_thinking_state(CheckersThinkingState *ts);
void checkers_start_thinking(CheckersThinkingState *ts, CheckersGameState *game);
CheckersMove checkers_get_best_move_now(CheckersThinkingState *ts);
void checkers_stop_thinking(CheckersThinkingState *ts);

// Visualization functions
void init_beat_checkers_system(void *vis_ptr);
void update_beat_checkers(void *vis_ptr, double dt);
void draw_beat_checkers(void *vis_ptr, cairo_t *cr);
void draw_checkers_reset_button(BeatCheckersVisualization *checkers, cairo_t *cr, int width, int height);

// ===== INTERACTIVE FEATURE DECLARATIONS =====
// Move history functions
void checkers_save_move_history(BeatCheckersVisualization *checkers, 
                                 CheckersMove move, double time_elapsed);
bool checkers_can_undo(BeatCheckersVisualization *checkers);
void checkers_undo_last_move(BeatCheckersVisualization *checkers);
void checkers_clear_move_history(BeatCheckersVisualization *checkers);

// Helper functions
bool checkers_is_player_turn(BeatCheckersVisualization *checkers);

// Button drawing functions
void checkers_draw_pvsa_button(BeatCheckersVisualization *checkers, cairo_t *cr, 
                               int width, int height);
void checkers_draw_undo_button(BeatCheckersVisualization *checkers, cairo_t *cr, 
                               int width, int height);
// ===== END INTERACTIVE DECLARATIONS =====

#endif // BEATCHECKERS_H
