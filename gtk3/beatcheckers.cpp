#include "beatcheckers.h"
#include "visualization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>

// ============================================================================
// CORE CHECKERS ENGINE
// ============================================================================

void checkers_init_board(CheckersGameState *game) {
    // Clear board
    for (int r = 0; r < CHECKERS_BOARD_SIZE; r++) {
        for (int c = 0; c < CHECKERS_BOARD_SIZE; c++) {
            game->board[r][c].color = CHECKERS_NONE;
            game->board[r][c].is_king = false;
        }
    }
    
    // Set up pieces - only on dark squares
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < CHECKERS_BOARD_SIZE; c++) {
            if ((r + c) % 2 == 1) {  // Dark squares
                game->board[r][c].color = CHECKERS_BLACK;
            }
        }
    }
    
    for (int r = 5; r < CHECKERS_BOARD_SIZE; r++) {
        for (int c = 0; c < CHECKERS_BOARD_SIZE; c++) {
            if ((r + c) % 2 == 1) {  // Dark squares
                game->board[r][c].color = CHECKERS_RED;
            }
        }
    }
    
    game->turn = CHECKERS_RED;
    game->red_pieces = 12;
    game->black_pieces = 12;
}

// Find all jumps from a position (recursive for multi-jumps)
static int find_jumps_from(CheckersGameState *game, int r, int c, 
                          CheckersMove *moves, int move_count,
                          CheckersMove current_move, bool jumped[8][8]) {
    CheckersPiece piece = game->board[r][c];
    
    // Possible jump directions
    int dirs[4][2];
    int num_dirs = 0;
    
    if (piece.is_king) {
        // Kings can jump in all 4 diagonal directions
        dirs[0][0] = -1; dirs[0][1] = -1;
        dirs[1][0] = -1; dirs[1][1] = 1;
        dirs[2][0] = 1;  dirs[2][1] = -1;
        dirs[3][0] = 1;  dirs[3][1] = 1;
        num_dirs = 4;
    } else if (piece.color == CHECKERS_RED) {
        // Red moves up (decreasing row)
        dirs[0][0] = -1; dirs[0][1] = -1;
        dirs[1][0] = -1; dirs[1][1] = 1;
        num_dirs = 2;
    } else {
        // Black moves down (increasing row)
        dirs[0][0] = 1; dirs[0][1] = -1;
        dirs[1][0] = 1; dirs[1][1] = 1;
        num_dirs = 2;
    }
    
    bool found_jump = false;
    
    for (int d = 0; d < num_dirs; d++) {
        int mid_r = r + dirs[d][0];
        int mid_c = c + dirs[d][1];
        int land_r = r + dirs[d][0] * 2;
        int land_c = c + dirs[d][1] * 2;
        
        // Check bounds
        if (land_r < 0 || land_r >= CHECKERS_BOARD_SIZE || 
            land_c < 0 || land_c >= CHECKERS_BOARD_SIZE) continue;
        
        // Check if there's an opponent piece to jump
        CheckersPiece mid = game->board[mid_r][mid_c];
        if (mid.color == CHECKERS_NONE || mid.color == piece.color) continue;
        if (jumped[mid_r][mid_c]) continue;  // Already jumped this piece
        
        // Check if landing square is empty
        if (game->board[land_r][land_c].color != CHECKERS_NONE) continue;
        
        // Valid jump found!
        found_jump = true;
        
        // Mark this piece as jumped
        jumped[mid_r][mid_c] = true;
        
        // Add to current move
        CheckersMove extended = current_move;
        extended.to_row = land_r;
        extended.to_col = land_c;
        extended.jumped_rows[extended.jump_count] = mid_r;
        extended.jumped_cols[extended.jump_count] = mid_c;
        extended.jump_count++;
        
        // Check for king promotion
        if (!piece.is_king) {
            if ((piece.color == CHECKERS_RED && land_r == 0) ||
                (piece.color == CHECKERS_BLACK && land_r == 7)) {
                extended.becomes_king = true;
            }
        }
        
        // Temporarily make the jump to check for more jumps
        CheckersGameState temp = *game;
        temp.board[land_r][land_c] = temp.board[r][c];
        temp.board[r][c].color = CHECKERS_NONE;
        temp.board[mid_r][mid_c].color = CHECKERS_NONE;
        if (extended.becomes_king) {
            temp.board[land_r][land_c].is_king = true;
        }
        
        // Recursively look for more jumps
        int new_count = find_jumps_from(&temp, land_r, land_c, moves, move_count, extended, jumped);
        
        if (new_count == move_count) {
            // No more jumps found, this is a complete move
            moves[move_count++] = extended;
        } else {
            move_count = new_count;
        }
        
        // Unmark for next iteration
        jumped[mid_r][mid_c] = false;
    }
    
    return move_count;
}

int checkers_get_all_moves(CheckersGameState *game, CheckersColor color, CheckersMove *moves) {
    int move_count = 0;
    
    // First, look for jumps (forced if available)
    for (int r = 0; r < CHECKERS_BOARD_SIZE; r++) {
        for (int c = 0; c < CHECKERS_BOARD_SIZE; c++) {
            if (game->board[r][c].color == color) {
                CheckersMove base_move = {r, c, r, c, 0, {0}, {0}, false};
                bool jumped[8][8] = {false};
                int new_count = find_jumps_from(game, r, c, moves, move_count, base_move, jumped);
                move_count = new_count;
            }
        }
    }
    
    // If jumps are available, they're forced
    if (move_count > 0) {
        return move_count;
    }
    
    // No jumps available, find regular moves
    for (int r = 0; r < CHECKERS_BOARD_SIZE; r++) {
        for (int c = 0; c < CHECKERS_BOARD_SIZE; c++) {
            CheckersPiece piece = game->board[r][c];
            if (piece.color != color) continue;
            
            // Possible move directions
            int dirs[4][2];
            int num_dirs = 0;
            
            if (piece.is_king) {
                dirs[0][0] = -1; dirs[0][1] = -1;
                dirs[1][0] = -1; dirs[1][1] = 1;
                dirs[2][0] = 1;  dirs[2][1] = -1;
                dirs[3][0] = 1;  dirs[3][1] = 1;
                num_dirs = 4;
            } else if (piece.color == CHECKERS_RED) {
                dirs[0][0] = -1; dirs[0][1] = -1;
                dirs[1][0] = -1; dirs[1][1] = 1;
                num_dirs = 2;
            } else {
                dirs[0][0] = 1; dirs[0][1] = -1;
                dirs[1][0] = 1; dirs[1][1] = 1;
                num_dirs = 2;
            }
            
            for (int d = 0; d < num_dirs; d++) {
                int new_r = r + dirs[d][0];
                int new_c = c + dirs[d][1];
                
                if (new_r < 0 || new_r >= CHECKERS_BOARD_SIZE || 
                    new_c < 0 || new_c >= CHECKERS_BOARD_SIZE) continue;
                
                if (game->board[new_r][new_c].color == CHECKERS_NONE) {
                    CheckersMove move = {r, c, new_r, new_c, 0, {0}, {0}, false};
                    
                    // Check for king promotion
                    if (!piece.is_king) {
                        if ((piece.color == CHECKERS_RED && new_r == 0) ||
                            (piece.color == CHECKERS_BLACK && new_r == 7)) {
                            move.becomes_king = true;
                        }
                    }
                    
                    moves[move_count++] = move;
                }
            }
        }
    }
    
    return move_count;
}

// Helper function to check if a move is valid
bool checkers_is_valid_move(CheckersGameState *game, CheckersMove *move) {
    if (move->from_row == move->to_row && move->from_col == move->to_col) {
        return false;  // No movement
    }
    
    CheckersPiece piece = game->board[move->from_row][move->from_col];
    if (piece.color == CHECKERS_NONE) {
        return false;  // No piece at source
    }
    
    CheckersMove moves[MAX_CHECKERS_MOVES];
    int count = checkers_get_all_moves(game, piece.color, moves);
    
    for (int i = 0; i < count; i++) {
        if (moves[i].from_row == move->from_row && 
            moves[i].from_col == move->from_col &&
            moves[i].to_row == move->to_row && 
            moves[i].to_col == move->to_col) {
            *move = moves[i];  // Update move with complete info (jumps, etc)
            return true;
        }
    }
    
    return false;
}

void checkers_make_move(CheckersGameState *game, CheckersMove *move) {
    CheckersPiece piece = game->board[move->from_row][move->from_col];
    
    // Move piece
    game->board[move->to_row][move->to_col] = piece;
    game->board[move->from_row][move->from_col].color = CHECKERS_NONE;
    game->board[move->from_row][move->from_col].is_king = false;
    
    // Remove jumped pieces
    for (int i = 0; i < move->jump_count; i++) {
        CheckersColor jumped_color = game->board[move->jumped_rows[i]][move->jumped_cols[i]].color;
        game->board[move->jumped_rows[i]][move->jumped_cols[i]].color = CHECKERS_NONE;
        game->board[move->jumped_rows[i]][move->jumped_cols[i]].is_king = false;
        
        if (jumped_color == CHECKERS_RED) game->red_pieces--;
        else if (jumped_color == CHECKERS_BLACK) game->black_pieces--;
    }
    
    // King promotion
    if (move->becomes_king) {
        game->board[move->to_row][move->to_col].is_king = true;
    }
    
    // Switch turn
    game->turn = (game->turn == CHECKERS_RED) ? CHECKERS_BLACK : CHECKERS_RED;
}

int checkers_evaluate_position(CheckersGameState *game) {
    int score = 0;
    
    // Piece-square table - favor center and advancing
    int position_value[8][8] = {
        {4, 4, 4, 4, 4, 4, 4, 4},
        {3, 4, 4, 4, 4, 4, 4, 3},
        {3, 3, 5, 5, 5, 5, 3, 3},
        {2, 3, 3, 6, 6, 3, 3, 2},
        {2, 3, 3, 6, 6, 3, 3, 2},
        {3, 3, 5, 5, 5, 5, 3, 3},
        {3, 4, 4, 4, 4, 4, 4, 3},
        {4, 4, 4, 4, 4, 4, 4, 4}
    };
    
    for (int r = 0; r < CHECKERS_BOARD_SIZE; r++) {
        for (int c = 0; c < CHECKERS_BOARD_SIZE; c++) {
            CheckersPiece piece = game->board[r][c];
            if (piece.color == CHECKERS_NONE) continue;
            
            int value = piece.is_king ? 300 : 100;
            int pos_value = position_value[r][c];
            
            // Bonus for advancement (for regular pieces)
            if (!piece.is_king) {
                if (piece.color == CHECKERS_RED) {
                    value += (7 - r) * 3;  // Red advances upward
                } else {
                    value += r * 3;  // Black advances downward
                }
            }
            
            // Back row bonus (defensive)
            if ((piece.color == CHECKERS_RED && r == 7) ||
                (piece.color == CHECKERS_BLACK && r == 0)) {
                value += 5;
            }
            
            int total = value + pos_value;
            score += (piece.color == CHECKERS_RED) ? total : -total;
        }
    }
    
    // Mobility bonus
    CheckersMove moves[MAX_CHECKERS_MOVES];
    int red_mobility = checkers_get_all_moves(game, CHECKERS_RED, moves);
    int black_mobility = checkers_get_all_moves(game, CHECKERS_BLACK, moves);
    score += (red_mobility - black_mobility) * 5;
    
    return score;
}

CheckersGameStatus checkers_check_game_status(CheckersGameState *game) {
    if (game->red_pieces == 0) return CHECKERS_BLACK_WINS;
    if (game->black_pieces == 0) return CHECKERS_RED_WINS;
    
    CheckersMove moves[MAX_CHECKERS_MOVES];
    int move_count = checkers_get_all_moves(game, game->turn, moves);
    
    if (move_count == 0) {
        return (game->turn == CHECKERS_RED) ? CHECKERS_BLACK_WINS : CHECKERS_RED_WINS;
    }
    
    return CHECKERS_PLAYING;
}

// ============================================================================
// INTERACTIVE FEATURES - MOVE HISTORY
// ============================================================================

void checkers_save_move_history(BeatCheckersVisualization *checkers, 
                                 CheckersMove move, double time_elapsed) {
    if (checkers->move_history_count >= MAX_CHECKERS_MOVES) {
        return;
    }
    
    CheckersMoveHistory *history_entry = 
        &checkers->move_history[checkers->move_history_count];
    
    history_entry->game = checkers->game;
    history_entry->move = move;
    history_entry->time_elapsed = time_elapsed;
    
    checkers->move_history_count++;
}

void checkers_clear_move_history(BeatCheckersVisualization *checkers) {
    checkers->move_history_count = 0;
    memset(checkers->move_history, 0, 
           sizeof(CheckersMoveHistory) * MAX_CHECKERS_MOVES);
}

bool checkers_can_undo(BeatCheckersVisualization *checkers) {
    return checkers->move_history_count >= 2 && checkers->player_vs_ai;
}

void checkers_undo_last_move(BeatCheckersVisualization *checkers) {
    if (!checkers_can_undo(checkers)) return;
    
    if (checkers->move_history_count >= 2) {
        CheckersMoveHistory *history_before_ai = 
            &checkers->move_history[checkers->move_history_count - 2];
        checkers->game = history_before_ai->game;
        checkers->move_history_count -= 2;
    }
    
    checkers->is_animating = false;
    checkers->animation_progress = 0;
    checkers->last_move_glow = 0;
    checkers->status = CHECKERS_PLAYING;
    strcpy(checkers->status_text, "Move undone - Red to move");
    checkers->status_flash_timer = 1.5;
    checkers->status_flash_color[0] = 1.0;
    checkers->status_flash_color[1] = 0.8;
    checkers->status_flash_color[2] = 0.0;
    
    checkers_start_thinking(&checkers->thinking_state, &checkers->game);
}

bool checkers_is_player_turn(BeatCheckersVisualization *checkers) {
    if (!checkers->player_vs_ai) return false;
    return checkers->game.turn == CHECKERS_RED;
}

// ============================================================================
// AI / THINKING
// ============================================================================

static int checkers_minimax(CheckersGameState *game, int depth, int alpha, int beta, bool maximizing) {
    CheckersMove moves[MAX_CHECKERS_MOVES];
    int move_count = checkers_get_all_moves(game, game->turn, moves);
    
    if (move_count == 0) {
        return maximizing ? -1000000 : 1000000;
    }
    
    if (depth == 0) {
        return checkers_evaluate_position(game);
    }
    
    if (maximizing) {
        int max_eval = INT_MIN;
        for (int i = 0; i < move_count; i++) {
            CheckersGameState temp = *game;
            checkers_make_move(&temp, &moves[i]);
            int eval = checkers_minimax(&temp, depth - 1, alpha, beta, false);
            max_eval = (eval > max_eval) ? eval : max_eval;
            alpha = (alpha > eval) ? alpha : eval;
            if (beta <= alpha) break;
        }
        return max_eval;
    } else {
        int min_eval = INT_MAX;
        for (int i = 0; i < move_count; i++) {
            CheckersGameState temp = *game;
            checkers_make_move(&temp, &moves[i]);
            int eval = checkers_minimax(&temp, depth - 1, alpha, beta, true);
            min_eval = (eval < min_eval) ? eval : min_eval;
            beta = (beta < eval) ? beta : eval;
            if (beta <= alpha) break;
        }
        return min_eval;
    }
}

void* checkers_think_continuously(void* arg) {
    CheckersThinkingState *ts = (CheckersThinkingState*)arg;
    
    while (true) {
        pthread_mutex_lock(&ts->lock);
        
        // Sleep if not thinking
        if (!ts->thinking) {
            pthread_mutex_unlock(&ts->lock);
            usleep(100000);  // Sleep 100ms when idle to prevent CPU spinning
            continue;
        }
        
        CheckersGameState game_copy = ts->game;
        pthread_mutex_unlock(&ts->lock);
        
        CheckersMove moves[MAX_CHECKERS_MOVES];
        int move_count = checkers_get_all_moves(&game_copy, game_copy.turn, moves);
        
        if (move_count == 0) {
            pthread_mutex_lock(&ts->lock);
            ts->has_move = false;
            ts->thinking = false;
            pthread_mutex_unlock(&ts->lock);
            usleep(100000);
            continue;
        }
        
        // Iterative deepening - up to depth 6 (checkers is simpler than chess)
        for (int depth = 1; depth <= 6; depth++) {
            CheckersMove best_moves[MAX_CHECKERS_MOVES];
            int best_move_count = 0;
            int best_score = (game_copy.turn == CHECKERS_RED) ? INT_MIN : INT_MAX;
            
            bool depth_completed = true;
            
            for (int i = 0; i < move_count; i++) {
                pthread_mutex_lock(&ts->lock);
                bool should_stop = !ts->thinking;
                pthread_mutex_unlock(&ts->lock);
                
                if (should_stop) {
                    depth_completed = false;
                    break;
                }
                
                CheckersGameState temp = game_copy;
                checkers_make_move(&temp, &moves[i]);
                int score = checkers_minimax(&temp, depth - 1, INT_MIN, INT_MAX, 
                                            game_copy.turn == CHECKERS_BLACK);
                
                if (game_copy.turn == CHECKERS_RED) {
                    if (score > best_score) {
                        best_score = score;
                        best_moves[0] = moves[i];
                        best_move_count = 1;
                    } else if (score == best_score) {
                        best_moves[best_move_count++] = moves[i];
                    }
                } else {
                    if (score < best_score) {
                        best_score = score;
                        best_moves[0] = moves[i];
                        best_move_count = 1;
                    } else if (score == best_score) {
                        best_moves[best_move_count++] = moves[i];
                    }
                }
            }
            
            pthread_mutex_lock(&ts->lock);
            if (depth_completed && ts->thinking && best_move_count > 0) {
                ts->best_move = best_moves[rand() % best_move_count];
                ts->best_score = best_score;
                ts->current_depth = depth;
                ts->has_move = true;
            }
            pthread_mutex_unlock(&ts->lock);
            
            // Allow main thread to check for cancellation
            usleep(1000);
        }
        
        usleep(100000);  // Sleep briefly between thinking sessions
    }
    
    return NULL;
}

void checkers_init_thinking_state(CheckersThinkingState *ts) {
    pthread_mutex_init(&ts->lock, NULL);
    ts->thinking = false;
    ts->has_move = false;
}

void checkers_start_thinking(CheckersThinkingState *ts, CheckersGameState *game) {
    pthread_mutex_lock(&ts->lock);
    ts->game = *game;
    ts->thinking = true;
    ts->has_move = false;
    pthread_mutex_unlock(&ts->lock);
}

CheckersMove checkers_get_best_move_now(CheckersThinkingState *ts) {
    CheckersMove move = {0};
    
    pthread_mutex_lock(&ts->lock);
    if (ts->has_move) {
        move = ts->best_move;
        ts->has_move = false;
    }
    pthread_mutex_unlock(&ts->lock);
    
    return move;
}

void checkers_stop_thinking(CheckersThinkingState *ts) {
    pthread_mutex_lock(&ts->lock);
    ts->thinking = false;
    pthread_mutex_unlock(&ts->lock);
}

// ============================================================================
// VISUALIZATION
// ============================================================================

void init_beat_checkers_system(void *vis_ptr) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    BeatCheckersVisualization *checkers = &vis->beat_checkers;
    
    checkers_init_board(&checkers->game);
    
    // Initialize thinking state only once (first time)
    static bool thinking_initialized = false;
    if (!thinking_initialized) {
        checkers_init_thinking_state(&checkers->thinking_state);
        pthread_create(&checkers->thinking_state.thread, NULL, checkers_think_continuously, 
                       &checkers->thinking_state);
        thinking_initialized = true;
    }
    
    checkers->status = CHECKERS_PLAYING;
    strcpy(checkers->status_text, "AI vs AI mode");
    checkers->status_flash_timer = 0;
    
    checkers->animating_from_row = -1;
    checkers->is_animating = false;
    checkers->last_from_row = -1;
    checkers->last_move_glow = 0;
    
    checkers->beat_history_index = 0;
    checkers->time_since_last_move = 0;
    checkers->beat_threshold = 0.3;
    checkers->move_count = 0;
    
    checkers->beats_since_game_over = 0;
    checkers->waiting_for_restart = false;
    
    checkers->time_thinking = 0;
    checkers->min_think_time = 0.5;
    checkers->good_move_threshold = 200;
    checkers->auto_play_enabled = true;
    
    checkers->king_promotion_active = false;
    checkers->king_promotion_glow = 0;
    checkers->captured_count = 0;
    
    // Initialize interactive features
    checkers->game_mode = CHECKERS_MODE_AI_VS_AI;
    checkers->player_vs_ai = false;
    
    checkers->selected_piece_row = -1;
    checkers->selected_piece_col = -1;
    checkers->has_selected_piece = false;
    checkers->selected_piece_was_pressed = 0;
    
    checkers->pvsa_button_x = 20;
    checkers->pvsa_button_y = 20;
    checkers->pvsa_button_width = 100;
    checkers->pvsa_button_height = 40;
    checkers->pvsa_button_hovered = false;
    checkers->pvsa_button_glow = 0;
    checkers->pvsa_button_was_pressed = false;
    
    checkers->undo_button_x = 20;
    checkers->undo_button_y = 70;
    checkers->undo_button_width = 100;
    checkers->undo_button_height = 40;
    checkers->undo_button_hovered = false;
    checkers->undo_button_glow = 0;
    checkers->undo_button_was_pressed = false;
    
    checkers->reset_button_x = 20;
    checkers->reset_button_y = 120;
    checkers->reset_button_width = 120;
    checkers->reset_button_height = 40;
    checkers->reset_button_hovered = false;
    checkers->reset_button_glow = 0;
    checkers->reset_button_was_pressed = false;
    
    checkers->white_total_time = 0.0;
    checkers->black_total_time = 0.0;
    checkers->current_move_start_time = 0.0;
    checkers->last_move_end_time = 0.0;
    checkers->move_history_count = 0;
    checkers->auto_reset_timer = 0.0;  // No auto-reset on initial start
    checkers_clear_move_history(checkers);
    
    // Start thinking with the already-created thread
    checkers_start_thinking(&checkers->thinking_state, &checkers->game);
}

void update_beat_checkers(void *vis_ptr, double dt) {
    Visualizer *vis = (Visualizer *)vis_ptr;
    BeatCheckersVisualization *checkers = &vis->beat_checkers;
    
    // Update time
    checkers->time_since_last_move += dt;
    checkers->time_thinking += dt;
    checkers->current_move_start_time += dt;
    
    // Update beat history
    checkers->beat_volume_history[checkers->beat_history_index] = vis->volume_level;
    checkers->beat_history_index = (checkers->beat_history_index + 1) % CHECKERS_BEAT_HISTORY;
    
    // Update glow effects
    if (checkers->status_flash_timer > 0) {
        checkers->status_flash_timer -= dt;
    }
    if (checkers->last_move_glow > 0) {
        checkers->last_move_glow -= dt * 2;
    }
    if (checkers->king_promotion_active) {
        checkers->king_promotion_glow -= dt;
        if (checkers->king_promotion_glow < 0) {
            checkers->king_promotion_active = false;
        }
    }
    
    // Handle auto-reset timer
    if (checkers->auto_reset_timer > 0) {
        checkers->auto_reset_timer -= dt;
        if (checkers->auto_reset_timer <= 0) {
            // Auto-reset: stop thinking, reinitialize game
            pthread_mutex_lock(&checkers->thinking_state.lock);
            checkers->thinking_state.thinking = false;
            pthread_mutex_unlock(&checkers->thinking_state.lock);
            
            usleep(50000);  // Give thread time to stop
            
            // Remember current game mode before reset
            bool was_player_vs_ai = checkers->player_vs_ai;
            
            init_beat_checkers_system(vis_ptr);
            
            // Restore game mode after reset
            checkers->player_vs_ai = was_player_vs_ai;
            if (was_player_vs_ai) {
                strcpy(checkers->status_text, "Red to move");
            }
            
            checkers->auto_reset_timer = 0;  // Reset timer
        }
    }
    
    // Update animation
    if (checkers->is_animating) {
        // Adjust animation speed based on number of jumps in this move
        // More jumps = slower animation so you can see each captured piece
        double animation_speed = 3.0;  // Default speed for regular moves (~0.33 seconds)
        
        if (checkers->current_move_jump_count > 0) {
            // For jump chains: approximately 0.4 seconds per jump segment
            animation_speed = 2.5 * (checkers->current_move_jump_count + 1);
        }
        
        checkers->animation_progress += dt * animation_speed;
        if (checkers->animation_progress >= 1.0) {
            checkers->animation_progress = 1.0;
            checkers->is_animating = false;
        }
    }
    
    // Update captured pieces fade
    for (int i = 0; i < checkers->captured_count; i++) {
        checkers->captured_fade[i] -= dt * 2;
        if (checkers->captured_fade[i] < 0) {
            checkers->captured_fade[i] = 0;
        }
    }
    
    // ===== PVSA BUTTON INTERACTION =====
    bool is_over_pvsa = (vis->mouse_x >= checkers->pvsa_button_x && 
                         vis->mouse_x <= checkers->pvsa_button_x + checkers->pvsa_button_width &&
                         vis->mouse_y >= checkers->pvsa_button_y && 
                         vis->mouse_y <= checkers->pvsa_button_y + checkers->pvsa_button_height);
    
    checkers->pvsa_button_hovered = is_over_pvsa;
    
    bool pvsa_was_pressed = checkers->pvsa_button_was_pressed;
    bool pvsa_is_pressed = vis->mouse_left_pressed;
    bool pvsa_clicked = (pvsa_was_pressed && !pvsa_is_pressed && is_over_pvsa);
    checkers->pvsa_button_was_pressed = pvsa_is_pressed;
    
    if (pvsa_clicked) {
        checkers->player_vs_ai = !checkers->player_vs_ai;
        
        // Reset game
        checkers_init_board(&checkers->game);
        checkers->status = CHECKERS_PLAYING;
        checkers->beats_since_game_over = 0;
        checkers->waiting_for_restart = false;
        checkers->move_count = 0;
        checkers->time_thinking = 0;
        checkers->last_move_glow = 0;
        checkers->animation_progress = 0;
        checkers->is_animating = false;
        checkers->last_from_row = -1;
        
        checkers->white_total_time = 0.0;
        checkers->black_total_time = 0.0;
        checkers->current_move_start_time = 0.0;
        checkers->last_move_end_time = 0.0;
        checkers_clear_move_history(checkers);
        
        checkers->has_selected_piece = false;
        checkers->selected_piece_row = -1;
        checkers->selected_piece_col = -1;
        
        if (checkers->player_vs_ai) {
            strcpy(checkers->status_text, "Player vs AI - Red to move");
            checkers->status_flash_color[0] = 1.0;
            checkers->status_flash_color[1] = 0.2;
            checkers->status_flash_color[2] = 0.2;
        } else {
            strcpy(checkers->status_text, "AI vs AI");
            checkers->status_flash_color[0] = 1.0;
            checkers->status_flash_color[1] = 0.65;
            checkers->status_flash_color[2] = 0.0;
        }
        checkers->status_flash_timer = 2.0;
        checkers->pvsa_button_glow = 1.0;
        
        checkers_start_thinking(&checkers->thinking_state, &checkers->game);
    }
    
    checkers->pvsa_button_glow *= 0.95;
    // =====================================
    
    // ===== UNDO BUTTON INTERACTION =====
    if (checkers->player_vs_ai) {
        bool is_over_undo = (vis->mouse_x >= checkers->undo_button_x && 
                             vis->mouse_x <= checkers->undo_button_x + checkers->undo_button_width &&
                             vis->mouse_y >= checkers->undo_button_y && 
                             vis->mouse_y <= checkers->undo_button_y + checkers->undo_button_height);
        
        checkers->undo_button_hovered = is_over_undo && checkers_can_undo(checkers);
        
        bool undo_was_pressed = checkers->undo_button_was_pressed;
        bool undo_is_pressed = vis->mouse_left_pressed;
        bool undo_clicked = (undo_was_pressed && !undo_is_pressed && is_over_undo && 
                            checkers_can_undo(checkers));
        
        checkers->undo_button_was_pressed = undo_is_pressed;
        
        if (undo_clicked) {
            checkers_undo_last_move(checkers);
            checkers->undo_button_glow = 1.0;
        }
    } else {
        checkers->undo_button_hovered = false;
        checkers->undo_button_was_pressed = false;
    }
    
    checkers->undo_button_glow *= 0.95;
    // =====================================
    
    // ===== PLAYER PIECE SELECTION (if player's turn) =====
    if (checkers->player_vs_ai && checkers_is_player_turn(checkers) && !checkers->is_animating) {
        double cell = checkers->cell_size;
        double ox = checkers->board_offset_x;
        double oy = checkers->board_offset_y;
        
        int mouse_row = -1, mouse_col = -1;
        if (vis->mouse_x >= ox && vis->mouse_x < ox + cell * CHECKERS_BOARD_SIZE &&
            vis->mouse_y >= oy && vis->mouse_y < oy + cell * CHECKERS_BOARD_SIZE) {
            mouse_row = (int)((vis->mouse_y - oy) / cell);
            mouse_col = (int)((vis->mouse_x - ox) / cell);
        }
        
        bool is_pressed = vis->mouse_left_pressed;
        bool was_pressed = checkers->selected_piece_was_pressed;
        bool just_clicked = (was_pressed && !is_pressed);
        
        checkers->selected_piece_was_pressed = is_pressed;
        
        if (just_clicked && mouse_row >= 0 && mouse_col >= 0) {
            if (!checkers->has_selected_piece) {
                CheckersPiece piece = checkers->game.board[mouse_row][mouse_col];
                if (piece.color == CHECKERS_RED) {
                    checkers->selected_piece_row = mouse_row;
                    checkers->selected_piece_col = mouse_col;
                    checkers->has_selected_piece = true;
                    strcpy(checkers->status_text, "Piece selected");
                }
            } else {
                int from_row = checkers->selected_piece_row;
                int from_col = checkers->selected_piece_col;
                int to_row = mouse_row;
                int to_col = mouse_col;
                
                if (from_row == to_row && from_col == to_col) {
                    checkers->has_selected_piece = false;
                    strcpy(checkers->status_text, "Deselected");
                } else {
                    CheckersMove move = {from_row, from_col, to_row, to_col, 0, {0}, {0}, false};
                    
                    if (checkers_is_valid_move(&checkers->game, &move)) {
                        checkers_make_move(&checkers->game, &move);
                        checkers_save_move_history(checkers, move, checkers->current_move_start_time);
                        
                        checkers->last_from_row = from_row;
                        checkers->last_from_col = from_col;
                        checkers->last_to_row = to_row;
                        checkers->last_to_col = to_col;
                        checkers->last_move_glow = 1.0;
                        
                        checkers->animating_from_row = from_row;
                        checkers->animating_from_col = from_col;
                        checkers->animating_to_row = to_row;
                        checkers->animating_to_col = to_col;
                        checkers->current_move_jump_count = move.jump_count;  // Store jump count for animation speed
                        checkers->animation_progress = 0;
                        checkers->is_animating = true;
                        
                        checkers->move_count++;
                        checkers->time_since_last_move = 0;
                        checkers->current_move_start_time = 0;
                        
                        checkers->status = checkers_check_game_status(&checkers->game);
                        if (checkers->status != CHECKERS_PLAYING) {
                            checkers->waiting_for_restart = true;
                        } else {
                            checkers_start_thinking(&checkers->thinking_state, &checkers->game);
                        }
                        
                        checkers->has_selected_piece = false;
                    } else {
                        strcpy(checkers->status_text, "Illegal move");
                        checkers->has_selected_piece = false;
                    }
                }
            }
        }
    }
    // ================================================
    
    // ===== RESET BUTTON INTERACTION =====
    bool is_over_reset = (vis->mouse_x >= checkers->reset_button_x && 
                          vis->mouse_x <= checkers->reset_button_x + checkers->reset_button_width &&
                          vis->mouse_y >= checkers->reset_button_y && 
                          vis->mouse_y <= checkers->reset_button_y + checkers->reset_button_height);
    
    checkers->reset_button_hovered = is_over_reset;
    
    bool reset_was_pressed = checkers->reset_button_was_pressed;
    bool reset_is_pressed = vis->mouse_left_pressed;
    bool reset_clicked = (reset_was_pressed && !reset_is_pressed && is_over_reset);
    
    checkers->reset_button_was_pressed = reset_is_pressed;
    
    if (reset_clicked) {
        // Stop current thinking
        pthread_mutex_lock(&checkers->thinking_state.lock);
        checkers->thinking_state.thinking = false;
        pthread_mutex_unlock(&checkers->thinking_state.lock);
        
        // Give thread time to stop
        usleep(50000);
        
        // Save current game mode before reinitializing
        bool current_player_vs_ai = checkers->player_vs_ai;
        
        // Now reinitialize the board and restart
        init_beat_checkers_system(vis_ptr);
        
        // Restore the game mode
        checkers->player_vs_ai = current_player_vs_ai;
        if (current_player_vs_ai) {
            strcpy(checkers->status_text, "Player vs AI - Red to move");
            checkers->status_flash_color[0] = 1.0;
            checkers->status_flash_color[1] = 0.2;
            checkers->status_flash_color[2] = 0.2;
        } else {
            strcpy(checkers->status_text, "AI vs AI mode");
            checkers->status_flash_color[0] = 1.0;
            checkers->status_flash_color[1] = 0.65;
            checkers->status_flash_color[2] = 0.0;
        }
        checkers->status_flash_timer = 1.5;
        checkers->reset_button_glow = 1.0;
        
        // Start AI thinking if needed
        checkers_start_thinking(&checkers->thinking_state, &checkers->game);
    }
    
    checkers->reset_button_glow *= 0.95;
    // =====================================
    
    // ===== AI MOVE LOGIC (AI vs AI mode) =====
    if (!checkers->player_vs_ai && checkers->status == CHECKERS_PLAYING && !checkers->is_animating) {
        double avg_volume = 0;
        for (int i = 0; i < CHECKERS_BEAT_HISTORY; i++) {
            avg_volume += checkers->beat_volume_history[i];
        }
        avg_volume /= CHECKERS_BEAT_HISTORY;
        
        if (avg_volume > checkers->beat_threshold) {
            CheckersMove best_move = checkers_get_best_move_now(&checkers->thinking_state);
            
            if (best_move.from_row >= 0) {
                checkers_make_move(&checkers->game, &best_move);
                checkers_save_move_history(checkers, best_move, checkers->time_since_last_move);
                
                checkers->last_from_row = best_move.from_row;
                checkers->last_from_col = best_move.from_col;
                checkers->last_to_row = best_move.to_row;
                checkers->last_to_col = best_move.to_col;
                checkers->last_move_glow = 1.0;
                
                checkers->animating_from_row = best_move.from_row;
                checkers->animating_from_col = best_move.from_col;
                checkers->animating_to_row = best_move.to_row;
                checkers->animating_to_col = best_move.to_col;
                checkers->current_move_jump_count = best_move.jump_count;  // Store jump count for animation speed
                checkers->animation_progress = 0;
                checkers->is_animating = true;
                
                checkers->move_count++;
                checkers->time_since_last_move = 0;
                
                // Check game status after move
                checkers->status = checkers_check_game_status(&checkers->game);
                if (checkers->status != CHECKERS_PLAYING) {
                    checkers->waiting_for_restart = true;
                    checkers->beats_since_game_over = 0;
                    
                    if (checkers->status == CHECKERS_RED_WINS) {
                        strcpy(checkers->status_text, "Red wins!");
                        checkers->status_flash_color[0] = 1.0;
                        checkers->status_flash_color[1] = 0.2;
                        checkers->status_flash_color[2] = 0.2;
                    } else if (checkers->status == CHECKERS_BLACK_WINS) {
                        strcpy(checkers->status_text, "Black wins!");
                        checkers->status_flash_color[0] = 0.2;
                        checkers->status_flash_color[1] = 0.2;
                        checkers->status_flash_color[2] = 0.2;
                    } else {
                        strcpy(checkers->status_text, "Draw!");
                        checkers->status_flash_color[0] = 0.7;
                        checkers->status_flash_color[1] = 0.7;
                        checkers->status_flash_color[2] = 0.7;
                    }
                    checkers->status_flash_timer = 2.0;
                    
                    // Auto-reset after 2 seconds
                    checkers->auto_reset_timer = 2.0;
                } else {
                    checkers_start_thinking(&checkers->thinking_state, &checkers->game);
                }
            }
        }
    }
    // ============================================
    
    // ===== AI MOVE IN PLAYER MODE =====
    if (checkers->player_vs_ai && checkers->status == CHECKERS_PLAYING &&
        !checkers_is_player_turn(checkers) && !checkers->is_animating) {
        
        CheckersMove best_move = checkers_get_best_move_now(&checkers->thinking_state);
        
        if (best_move.from_row >= 0) {
            checkers_make_move(&checkers->game, &best_move);
            checkers_save_move_history(checkers, best_move, checkers->time_since_last_move);
            
            checkers->last_from_row = best_move.from_row;
            checkers->last_from_col = best_move.from_col;
            checkers->last_to_row = best_move.to_row;
            checkers->last_to_col = best_move.to_col;
            checkers->last_move_glow = 1.0;
            
            checkers->animating_from_row = best_move.from_row;
            checkers->animating_from_col = best_move.from_col;
            checkers->animating_to_row = best_move.to_row;
            checkers->animating_to_col = best_move.to_col;
            checkers->current_move_jump_count = best_move.jump_count;  // Store jump count for animation speed
            checkers->animation_progress = 0;
            checkers->is_animating = true;
            
            checkers->move_count++;
            checkers->time_since_last_move = 0;
            
            // Check game status after AI moves
            checkers->status = checkers_check_game_status(&checkers->game);
            if (checkers->status != CHECKERS_PLAYING) {
                checkers->waiting_for_restart = true;
                checkers->beats_since_game_over = 0;
                
                if (checkers->status == CHECKERS_RED_WINS) {
                    strcpy(checkers->status_text, "Red (you) wins!");
                    checkers->status_flash_color[0] = 1.0;
                    checkers->status_flash_color[1] = 0.2;
                    checkers->status_flash_color[2] = 0.2;
                } else if (checkers->status == CHECKERS_BLACK_WINS) {
                    strcpy(checkers->status_text, "Black (AI) wins!");
                    checkers->status_flash_color[0] = 0.2;
                    checkers->status_flash_color[1] = 0.2;
                    checkers->status_flash_color[2] = 0.2;
                } else {
                    strcpy(checkers->status_text, "Draw!");
                    checkers->status_flash_color[0] = 0.7;
                    checkers->status_flash_color[1] = 0.7;
                    checkers->status_flash_color[2] = 0.7;
                }
                checkers->status_flash_timer = 2.0;
                
                // Auto-reset after 2 seconds
                checkers->auto_reset_timer = 2.0;
            } else {
                strcpy(checkers->status_text, "Red to move");
                checkers_start_thinking(&checkers->thinking_state, &checkers->game);
            }
        }
    }
    // ====================================
}

void draw_checkers_piece(cairo_t *cr, CheckersColor color, bool is_king, 
                         double x, double y, double size, double dance_offset) {
    double cx = x + size / 2;
    double cy = y + size / 2 + dance_offset;
    double radius = size * 0.35;
    
    // Shadow
    cairo_set_source_rgba(cr, 0, 0, 0, 0.3);
    cairo_arc(cr, cx + 3, cy + 3, radius, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // Piece body with gradient
    cairo_pattern_t *gradient = cairo_pattern_create_radial(
        cx - radius * 0.3, cy - radius * 0.3, radius * 0.1,
        cx, cy, radius);
    
    if (color == CHECKERS_RED) {
        cairo_pattern_add_color_stop_rgb(gradient, 0, 0.95, 0.3, 0.2);
        cairo_pattern_add_color_stop_rgb(gradient, 1, 0.7, 0.1, 0.05);
    } else {
        cairo_pattern_add_color_stop_rgb(gradient, 0, 0.3, 0.3, 0.3);
        cairo_pattern_add_color_stop_rgb(gradient, 1, 0.1, 0.1, 0.1);
    }
    
    cairo_set_source(cr, gradient);
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(gradient);
    
    // Outline
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_set_line_width(cr, 2.0);
    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    cairo_stroke(cr);
    
    // Highlight
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);
    cairo_arc(cr, cx - radius * 0.25, cy - radius * 0.25, radius * 0.2, 0, 2 * M_PI);
    cairo_fill(cr);
    
    // King crown
    if (is_king) {
        double crown_size = radius * 0.5;
        
        if (color == CHECKERS_RED) {
            cairo_set_source_rgb(cr, 1.0, 0.8, 0.0);
        } else {
            cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
        }
        
        // Simple crown shape
        cairo_move_to(cr, cx - crown_size, cy + crown_size * 0.2);
        cairo_line_to(cr, cx - crown_size * 0.6, cy - crown_size * 0.5);
        cairo_line_to(cr, cx - crown_size * 0.3, cy - crown_size * 0.2);
        cairo_line_to(cr, cx, cy - crown_size * 0.7);
        cairo_line_to(cr, cx + crown_size * 0.3, cy - crown_size * 0.2);
        cairo_line_to(cr, cx + crown_size * 0.6, cy - crown_size * 0.5);
        cairo_line_to(cr, cx + crown_size, cy + crown_size * 0.2);
        cairo_close_path(cr);
        cairo_fill(cr);
        
        // Crown outline
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_set_line_width(cr, 1.5);
        cairo_move_to(cr, cx - crown_size, cy + crown_size * 0.2);
        cairo_line_to(cr, cx - crown_size * 0.6, cy - crown_size * 0.5);
        cairo_line_to(cr, cx - crown_size * 0.3, cy - crown_size * 0.2);
        cairo_line_to(cr, cx, cy - crown_size * 0.7);
        cairo_line_to(cr, cx + crown_size * 0.3, cy - crown_size * 0.2);
        cairo_line_to(cr, cx + crown_size * 0.6, cy - crown_size * 0.5);
        cairo_line_to(cr, cx + crown_size, cy + crown_size * 0.2);
        cairo_close_path(cr);
        cairo_stroke(cr);
    }
}

void draw_checkers_reset_button(BeatCheckersVisualization *checkers, cairo_t *cr, int width, int height) {
    (void)width;   // Unused parameter
    (void)height;  // Unused parameter
    
    double button_x = checkers->reset_button_x;
    double button_y = checkers->reset_button_y;
    double button_width = checkers->reset_button_width;
    double button_height = checkers->reset_button_height;
    
    // Background
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_rectangle(cr, button_x, button_y, button_width, button_height);
    cairo_fill(cr);
    
    // Glow effect if hovered
    if (checkers->reset_button_hovered || checkers->reset_button_glow > 0) {
        double glow_alpha = checkers->reset_button_glow * 0.5;
        if (checkers->reset_button_hovered) glow_alpha = 0.4;
        
        cairo_set_source_rgba(cr, 1.0, 0.7, 0.2, glow_alpha);
        cairo_rectangle(cr, button_x - 3, button_y - 3, button_width + 6, button_height + 6);
        cairo_stroke(cr);
    }
    
    // Border
    cairo_set_source_rgb(cr, checkers->reset_button_hovered ? 1.0 : 0.7, 
                         checkers->reset_button_hovered ? 0.7 : 0.5, 
                         checkers->reset_button_hovered ? 0.2 : 0.3);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, button_x, button_y, button_width, button_height);
    cairo_stroke(cr);
    
    // Text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, "RESET", &extents);
    
    double text_x = button_x + (button_width - extents.width) / 2;
    double text_y = button_y + (button_height + extents.height) / 2;
    
    cairo_set_source_rgb(cr, checkers->reset_button_hovered ? 1.0 : 0.9, 
                         checkers->reset_button_hovered ? 0.8 : 0.7, 
                         checkers->reset_button_hovered ? 0.3 : 0.4);
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, "RESET");
}

void draw_checkers_pvsa_button(BeatCheckersVisualization *checkers, cairo_t *cr) {
    double x = checkers->pvsa_button_x;
    double y = checkers->pvsa_button_y;
    double w = checkers->pvsa_button_width;
    double h = checkers->pvsa_button_height;
    
    if (checkers->pvsa_button_glow > 0) {
        cairo_set_source_rgba(cr, 1.0, 0.8, 0.0, checkers->pvsa_button_glow * 0.3);
        cairo_rectangle(cr, x - 3, y - 3, w + 6, h + 6);
        cairo_stroke(cr);
    }
    
    if (checkers->pvsa_button_hovered) {
        cairo_set_source_rgb(cr, 0.3, 0.5, 0.9);
    } else {
        cairo_set_source_rgb(cr, 0.2, 0.4, 0.8);
    }
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
    
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, x, y, w, h);
    cairo_stroke(cr);
    
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    
    const char *text = checkers->player_vs_ai ? "PvsA" : "AvsA";
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text, &extents);
    
    double text_x = x + (w - extents.width) / 2;
    double text_y = y + (h + extents.height) / 2;
    
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, text);
}

void draw_checkers_undo_button(BeatCheckersVisualization *checkers, cairo_t *cr) {
    if (!checkers->player_vs_ai) return;
    
    double x = checkers->undo_button_x;
    double y = checkers->undo_button_y;
    double w = checkers->undo_button_width;
    double h = checkers->undo_button_height;
    bool can_undo = checkers_can_undo(checkers);
    
    if (checkers->undo_button_glow > 0 && can_undo) {
        cairo_set_source_rgba(cr, 1.0, 0.8, 0.0, checkers->undo_button_glow * 0.3);
        cairo_rectangle(cr, x - 3, y - 3, w + 6, h + 6);
        cairo_stroke(cr);
    }
    
    if (!can_undo) {
        cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
    } else if (checkers->undo_button_hovered) {
        cairo_set_source_rgb(cr, 0.8, 0.3, 0.3);
    } else {
        cairo_set_source_rgb(cr, 0.7, 0.2, 0.2);
    }
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
    
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, x, y, w, h);
    cairo_stroke(cr);
    
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, "UNDO", &extents);
    
    double text_x = x + (w - extents.width) / 2;
    double text_y = y + (h + extents.height) / 2;
    
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, "UNDO");
}

void draw_beat_checkers(void *vis_ptr, cairo_t *cr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    BeatCheckersVisualization *checkers = &vis->beat_checkers;
    
    int width = vis->width;
    int height = vis->height;
    
    // Calculate board layout
    double available = fmin(width * 0.85, height * 0.85);
    checkers->cell_size = available / 8;
    checkers->board_offset_x = (width - checkers->cell_size * 8) / 2;
    checkers->board_offset_y = (height - checkers->cell_size * 8) / 2 + 20;
    
    double cell = checkers->cell_size;
    double ox = checkers->board_offset_x;
    double oy = checkers->board_offset_y;
    
    // Background gradient
    cairo_pattern_t *bg = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgb(bg, 0, 0.15, 0.15, 0.18);
    cairo_pattern_add_color_stop_rgb(bg, 1, 0.08, 0.08, 0.1);
    cairo_set_source(cr, bg);
    cairo_paint(cr);
    cairo_pattern_destroy(bg);
    
    // Draw board squares
    for (int r = 0; r < CHECKERS_BOARD_SIZE; r++) {
        for (int c = 0; c < CHECKERS_BOARD_SIZE; c++) {
            bool is_light = (r + c) % 2 == 0;
            
            if (is_light) {
                cairo_set_source_rgb(cr, 0.85, 0.8, 0.7);
            } else {
                cairo_set_source_rgb(cr, 0.3, 0.25, 0.2);
            }
            
            cairo_rectangle(cr, ox + c * cell, oy + r * cell, cell, cell);
            cairo_fill(cr);
        }
    }
    
    // Last move highlight
    if (checkers->last_from_row >= 0 && checkers->last_move_glow > 0) {
        double alpha = checkers->last_move_glow * 0.4;
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.3, alpha);
        cairo_rectangle(cr, ox + checkers->last_from_col * cell, 
                       oy + checkers->last_from_row * cell, cell, cell);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.3, alpha);
        cairo_rectangle(cr, ox + checkers->last_to_col * cell, 
                       oy + checkers->last_to_row * cell, cell, cell);
        cairo_fill(cr);
    }
    
    // Board border
    cairo_set_source_rgb(cr, 0.5, 0.4, 0.3);
    cairo_set_line_width(cr, 4);
    cairo_rectangle(cr, ox, oy, cell * 8, cell * 8);
    cairo_stroke(cr);
    
    // Coordinates
    cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, cell * 0.18);
    
    for (int i = 0; i < 8; i++) {
        char label[2];
        label[0] = 'a' + i;
        label[1] = '\0';
        cairo_move_to(cr, ox + i * cell + cell * 0.45, oy + 8 * cell + cell * 0.3);
        cairo_show_text(cr, label);
        
        label[0] = '8' - i;
        cairo_move_to(cr, ox - cell * 0.3, oy + i * cell + cell * 0.55);
        cairo_show_text(cr, label);
    }
    
    // Draw fading captured pieces
    for (int i = 0; i < checkers->captured_count; i++) {
        if (checkers->captured_fade[i] > 0) {
            int idx = checkers->captured_pieces[i];
            int r = idx / CHECKERS_BOARD_SIZE;
            int c = idx % CHECKERS_BOARD_SIZE;
            
            double x = ox + c * cell;
            double y = oy + r * cell;
            
            cairo_save(cr);
            cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, checkers->captured_fade[i]);
            cairo_arc(cr, x + cell/2, y + cell/2, cell * 0.35, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_restore(cr);
        }
    }
    
    // Draw pieces with music-reactive dance
    double volume = vis->volume_level;
    
    for (int r = 0; r < CHECKERS_BOARD_SIZE; r++) {
        for (int c = 0; c < CHECKERS_BOARD_SIZE; c++) {
            CheckersPiece piece = checkers->game.board[r][c];
            
            // Skip animating piece
            if (checkers->is_animating && 
                r == checkers->animating_from_row && 
                c == checkers->animating_from_col) {
                continue;
            }
            
            if (piece.color != CHECKERS_NONE) {
                double x = ox + c * cell;
                double y = oy + r * cell;
                
                // Dance to the music
                double phase = (r * 0.7 + c * 0.5) * 3.14159;
                double time_wave = sin(checkers->time_since_last_move * 12.0 + phase);
                double dance_amount = time_wave * volume * cell * 0.15;
                
                // Highlight selected piece
                if (checkers->has_selected_piece && 
                    r == checkers->selected_piece_row && 
                    c == checkers->selected_piece_col) {
                    cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.3);
                    cairo_arc(cr, x + cell/2, y + cell/2, cell * 0.42, 0, 2 * M_PI);
                    cairo_fill(cr);
                    
                    cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 0.8);
                    cairo_set_line_width(cr, 3);
                    cairo_arc(cr, x + cell/2, y + cell/2, cell * 0.42, 0, 2 * M_PI);
                    cairo_stroke(cr);
                }
                
                // King promotion glow
                if (checkers->king_promotion_active && 
                    r == checkers->king_promotion_row && 
                    c == checkers->king_promotion_col) {
                    cairo_set_source_rgba(cr, 1.0, 0.8, 0.0, checkers->king_promotion_glow * 0.5);
                    cairo_arc(cr, x + cell/2, y + cell/2, cell * 0.5, 0, 2 * M_PI);
                    cairo_fill(cr);
                }
                
                draw_checkers_piece(cr, piece.color, piece.is_king, x, y, cell, dance_amount);
            }
        }
    }
    
    // Draw animating piece
    if (checkers->is_animating) {
        int fr = checkers->animating_from_row;
        int fc = checkers->animating_from_col;
        int tr = checkers->animating_to_row;
        int tc = checkers->animating_to_col;
        
        CheckersPiece piece = checkers->game.board[tr][tc];
        
        double t = checkers->animation_progress;
        t = t * t * (3.0 - 2.0 * t);  // Smoothstep
        
        double x = ox + (fc + t * (tc - fc)) * cell;
        double y = oy + (fr + t * (tr - fr)) * cell;
        
        double dance = sin(checkers->time_since_last_move * 18.0) * volume * cell * 0.25;
        
        // Glow for moving piece
        cairo_set_source_rgba(cr, 1.0, 1.0, 0.5, 0.6);
        cairo_arc(cr, x + cell/2, y + cell/2 + dance, cell * 0.45, 0, 2 * M_PI);
        cairo_fill(cr);
        
        draw_checkers_piece(cr, piece.color, piece.is_king, x, y, cell, dance);
    }
    
    // Status text at top
    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, checkers->status_text, &extents);
    
    double text_x = (width - extents.width) / 2;
    double text_y = oy - 25;
    
    if (checkers->status_flash_timer > 0) {
        double alpha = checkers->status_flash_timer * 0.3;
        cairo_set_source_rgba(cr, 
                            checkers->status_flash_color[0],
                            checkers->status_flash_color[1],
                            checkers->status_flash_color[2],
                            alpha);
        cairo_rectangle(cr, text_x - 10, text_y - extents.height - 5, 
                       extents.width + 20, extents.height + 10);
        cairo_fill(cr);
    }
    
    if (checkers->status_flash_timer > 0) {
        cairo_set_source_rgb(cr,
                           checkers->status_flash_color[0],
                           checkers->status_flash_color[1],
                           checkers->status_flash_color[2]);
    } else {
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    }
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, checkers->status_text);
    
    // Piece count
    char count_text[64];
    snprintf(count_text, sizeof(count_text), "Red: %d | Black: %d | Move: %d", 
             checkers->game.red_pieces, checkers->game.black_pieces, checkers->move_count);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_set_font_size(cr, 14);
    cairo_text_extents(cr, count_text, &extents);
    cairo_move_to(cr, (width - extents.width) / 2, oy + cell * 8 + 35);
    cairo_show_text(cr, count_text);
    
    // Draw buttons
    draw_checkers_reset_button(checkers, cr, width, height);
    draw_checkers_pvsa_button(checkers, cr);
    draw_checkers_undo_button(checkers, cr);
}

void checkers_cleanup_thinking_state(CheckersThinkingState *ts) {
    pthread_mutex_lock(&ts->lock);
    ts->thinking = false;
    pthread_mutex_unlock(&ts->lock);
    
    // Wait for thread to finish (with timeout)
    pthread_join(ts->thread, NULL);
    pthread_mutex_destroy(&ts->lock);
}
