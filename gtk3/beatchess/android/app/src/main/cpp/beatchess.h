#ifndef BEATCHESS_H
#define BEATCHESS_H

#include <pthread.h>
#include <stdbool.h>

#define BOARD_SIZE 8
#define MAX_CHESS_DEPTH 4
#define BEAT_HISTORY_SIZE 10
#define MAX_MOVE_HISTORY 256

typedef enum { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING } PieceType;
typedef enum { NONE, WHITE, BLACK } ChessColor;

typedef struct {
    PieceType type;
    ChessColor color;
} ChessPiece;

typedef struct {
    int from_row, from_col;
    int to_row, to_col;
    int score;
} ChessMove;


typedef struct {
    ChessPiece board[BOARD_SIZE][BOARD_SIZE];
    ChessColor turn;
    bool white_king_moved, black_king_moved;
    bool white_rook_a_moved, white_rook_h_moved;
    bool black_rook_a_moved, black_rook_h_moved;
    int en_passant_col; // -1 if no en passant available, otherwise the column
    int en_passant_row; // The row where en passant capture would land
} ChessGameState;

typedef struct {
    ChessGameState game;
    ChessMove best_move;
    int best_score;
    int current_depth;
    bool has_move;
    bool thinking;
    pthread_mutex_t lock;
    pthread_t thread;
} ChessThinkingState;

typedef enum {
    CHESS_PLAYING,
    CHESS_CHECKMATE_WHITE,
    CHESS_CHECKMATE_BLACK,
    CHESS_STALEMATE
} ChessGameStatus;

typedef struct {
    ChessGameState game_state;
    ChessMove move;
    double time_elapsed;  // Time spent on this move
} MoveHistory;



typedef struct {
    // Game state
    ChessGameState game;
    ChessThinkingState thinking_state;
    ChessGameStatus status;
    
    // Animation state
    double piece_x[BOARD_SIZE][BOARD_SIZE];
    double piece_y[BOARD_SIZE][BOARD_SIZE];
    double target_x[BOARD_SIZE][BOARD_SIZE];
    double target_y[BOARD_SIZE][BOARD_SIZE];
    
    // Last move highlight
    int last_from_row, last_from_col;
    int last_to_row, last_to_col;
    double last_move_glow;
    
    // Move being animated
    int animating_from_row, animating_from_col;
    int animating_to_row, animating_to_col;
    double animation_progress;
    bool is_animating;
    
    // Status display
    char status_text[256];
    double status_flash_timer;
    double status_flash_color[3]; // RGB
    int last_eval_change;
    
    // Beat detection
    double beat_volume_history[BEAT_HISTORY_SIZE];
    int beat_history_index;
    double time_since_last_move;
    double beat_threshold;
    
    // Visual elements
    double board_offset_x, board_offset_y;
    double cell_size;
    int move_count;
    
    // Evaluation bar
    double eval_bar_position; // -1 to 1, smoothed
    double eval_bar_target;

    int beats_since_game_over;
    bool waiting_for_restart;

    double time_thinking;          // How long AI has been thinking
    double min_think_time;         // Minimum time before auto-play (e.g., 0.5s)
    int good_move_threshold;       // Score threshold to auto-play (e.g., 200)
    bool auto_play_enabled;        // Whether to auto-play good moves
    
    // Reset button
    double reset_button_x, reset_button_y;
    double reset_button_width, reset_button_height;
    bool reset_button_hovered;
    double reset_button_glow;
    bool reset_button_was_pressed;  // Track previous frame state for click detection
    
    // PvsA toggle button
    double pvsa_button_x, pvsa_button_y;
    double pvsa_button_width, pvsa_button_height;
    bool pvsa_button_hovered;
    double pvsa_button_glow;
    bool pvsa_button_was_pressed;
    bool player_vs_ai;  // true = Player vs AI, false = AI vs AI
    
    // Player move tracking (for Player vs AI mode)
    int selected_piece_row, selected_piece_col;  // Currently selected piece
    bool has_selected_piece;
    int selected_piece_was_pressed;  // Mouse state for selection
    
    // Undo button and move history (for Player vs AI mode)
    double undo_button_x, undo_button_y;
    double undo_button_width, undo_button_height;
    bool undo_button_hovered;
    double undo_button_glow;
    bool undo_button_was_pressed;
    
    MoveHistory move_history[MAX_MOVE_HISTORY];
    int move_history_count;
    
    // Time tracking
    double white_total_time;  // Cumulative time for White
    double black_total_time;  // Cumulative time for Black
    double current_move_start_time;  // When current move phase started (for display)
    double last_move_end_time;  // When the last move was completed (for accurate timing)
    
    // Flip board button (for playing as Black)
    double flip_button_x, flip_button_y;
    double flip_button_width, flip_button_height;
    bool flip_button_hovered;
    double flip_button_glow;
    bool flip_button_was_pressed;
    bool board_flipped;  // true = board flipped (player plays Black), false = normal (player plays White)
    
} BeatChessVisualization;

#endif // BEATCHESS_H
