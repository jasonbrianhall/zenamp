#include "beatchess.h"
#include "visualization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>

// ============================================================================
// CORE CHESS ENGINE
// ============================================================================

#define MAX_MOVES_BEFORE_DRAW 300

bool chess_is_in_bounds(int r, int c) {
    return r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE;
}

bool chess_is_path_clear(ChessGameState *game, int fr, int fc, int tr, int tc) {
    int dr = (tr > fr) ? 1 : (tr < fr) ? -1 : 0;
    int dc = (tc > fc) ? 1 : (tc < fc) ? -1 : 0;
    
    int r = fr + dr;
    int c = fc + dc;
    
    while (r != tr || c != tc) {
        if (game->board[r][c].type != EMPTY) return false;
        r += dr;
        c += dc;
    }
    return true;
}

void chess_init_board(ChessGameState *game) {
    // Clear board
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            game->board[r][c].type = EMPTY;
            game->board[r][c].color = NONE;
        }
    }
    
    // Set up pieces
    PieceType back_row[] = {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK};
    
    for (int c = 0; c < BOARD_SIZE; c++) {
        game->board[0][c].type = back_row[c];
        game->board[0][c].color = BLACK;
        game->board[1][c].type = PAWN;
        game->board[1][c].color = BLACK;
        
        game->board[6][c].type = PAWN;
        game->board[6][c].color = WHITE;
        game->board[7][c].type = back_row[c];
        game->board[7][c].color = WHITE;
    }
    
    game->turn = WHITE;
    game->white_king_moved = false;
    game->black_king_moved = false;
    game->white_rook_a_moved = false;
    game->white_rook_h_moved = false;
    game->black_rook_a_moved = false;
    game->black_rook_h_moved = false;
    game->en_passant_col = -1;
    game->en_passant_row = -1;
}

bool chess_is_valid_move(ChessGameState *game, int fr, int fc, int tr, int tc) {
    if (!chess_is_in_bounds(fr, fc) || !chess_is_in_bounds(tr, tc)) return false;
    if (fr == tr && fc == tc) return false;
    
    ChessPiece piece = game->board[fr][fc];
    ChessPiece target = game->board[tr][tc];
    
    if (piece.type == EMPTY || piece.color != game->turn) return false;
    if (target.color == piece.color) return false;
    
    int dr = tr - fr;
    int dc = tc - fc;
    
    switch (piece.type) {
        case PAWN: {
            int direction = (piece.color == WHITE) ? -1 : 1;
            int start_row = (piece.color == WHITE) ? 6 : 1;
            
            // Normal pawn moves
            if (dc == 0 && target.type == EMPTY) {
                if (dr == direction) return true;
                if (fr == start_row && dr == 2 * direction && 
                    game->board[fr + direction][fc].type == EMPTY) return true;
            }
            // Regular captures
            if (abs(dc) == 1 && dr == direction && target.type != EMPTY) return true;
            
            // En passant capture
            if (abs(dc) == 1 && dr == direction && target.type == EMPTY) {
                if (game->en_passant_col == tc && game->en_passant_row == tr) {
                    return true;
                }
            }
            return false;
        }
        
        case KNIGHT:
            return (abs(dr) == 2 && abs(dc) == 1) || (abs(dr) == 1 && abs(dc) == 2);
        
        case BISHOP:
            return abs(dr) == abs(dc) && abs(dr) > 0 && chess_is_path_clear(game, fr, fc, tr, tc);
        
        case ROOK:
            return (dr == 0 || dc == 0) && (dr != 0 || dc != 0) && chess_is_path_clear(game, fr, fc, tr, tc);
        
        case QUEEN:
            return ((dr == 0 || dc == 0) || (abs(dr) == abs(dc))) && 
                   (dr != 0 || dc != 0) &&
                   chess_is_path_clear(game, fr, fc, tr, tc);
        
        case KING: {
            // Normal king move
            if (abs(dr) <= 1 && abs(dc) <= 1 && (dr != 0 || dc != 0)) {
                return true;
            }
            
            // Castling
            if (dr == 0 && abs(dc) == 2) {
                // Must not have moved king
                if (piece.color == WHITE && game->white_king_moved) return false;
                if (piece.color == BLACK && game->black_king_moved) return false;
                
                // Kingside castling (king moves to g file)
                if (dc == 2) {
                    // Rook must not have moved
                    if (piece.color == WHITE && game->white_rook_h_moved) return false;
                    if (piece.color == BLACK && game->black_rook_h_moved) return false;
                    
                    // Rook must still be on starting square
                    ChessPiece rook = game->board[fr][7];
                    if (rook.type != ROOK || rook.color != piece.color) return false;
                    
                    // Check path is clear (f and g files)
                    if (game->board[fr][5].type != EMPTY) return false;
                    if (game->board[fr][6].type != EMPTY) return false;
                    
                    // Check king is not currently in check
                    if (chess_is_in_check(game, piece.color)) return false;
                    
                    // Check king doesn't pass through check (f file - intermediate square)
                    ChessGameState temp = *game;
                    temp.board[fr][5] = piece;
                    temp.board[fr][4].type = EMPTY;
                    if (chess_is_in_check(&temp, piece.color)) return false;
                    
                    // Check king doesn't end in check (g file - final square)
                    temp.board[fr][6] = piece;
                    temp.board[fr][5].type = EMPTY;
                    if (chess_is_in_check(&temp, piece.color)) return false;
                    
                    return true;
                }
                
                // Queenside castling (king moves to c file)
                if (dc == -2) {
                    // Rook must not have moved
                    if (piece.color == WHITE && game->white_rook_a_moved) return false;
                    if (piece.color == BLACK && game->black_rook_a_moved) return false;
                    
                    // Rook must still be on starting square
                    ChessPiece rook = game->board[fr][0];
                    if (rook.type != ROOK || rook.color != piece.color) return false;
                    
                    // Check path is clear (a, b, c, d files)
                    if (game->board[fr][1].type != EMPTY) return false;
                    if (game->board[fr][2].type != EMPTY) return false;
                    if (game->board[fr][3].type != EMPTY) return false;
                    
                    // Check king is not currently in check
                    if (chess_is_in_check(game, piece.color)) return false;
                    
                    // Check king doesn't pass through check (d file - intermediate square)
                    ChessGameState temp = *game;
                    temp.board[fr][3] = piece;
                    temp.board[fr][4].type = EMPTY;
                    if (chess_is_in_check(&temp, piece.color)) return false;
                    
                    // Check king doesn't end in check (c file - final square)
                    temp.board[fr][2] = piece;
                    temp.board[fr][3].type = EMPTY;
                    if (chess_is_in_check(&temp, piece.color)) return false;
                    
                    return true;
                }
            }
            return false;
        }
        
        default:
            return false;
    }
}

bool chess_is_in_check(ChessGameState *game, ChessColor color) {
    int king_r = -1, king_c = -1;
    
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->board[r][c].type == KING && game->board[r][c].color == color) {
                king_r = r;
                king_c = c;
                break;
            }
        }
        if (king_r != -1) break;
    }
    
    if (king_r == -1) return false;
    
    ChessColor opponent = (color == WHITE) ? BLACK : WHITE;
    
    // Check each opponent piece to see if it can attack the king
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            ChessPiece piece = game->board[r][c];
            if (piece.color != opponent) continue;
            
            // Temporarily set turn to opponent to check if move is valid
            ChessColor saved_turn = game->turn;
            game->turn = opponent;
            bool can_attack = chess_is_valid_move(game, r, c, king_r, king_c);
            game->turn = saved_turn;
            
            if (can_attack) {
                return true;
            }
        }
    }
    
    return false;
}

void chess_make_move(ChessGameState *game, ChessMove move) {
    ChessPiece piece = game->board[move.from_row][move.from_col];
    
    // Clear en passant state before making move
    game->en_passant_col = -1;
    game->en_passant_row = -1;
    
    // Handle en passant capture
    if (piece.type == PAWN && move.to_col != move.from_col && 
        game->board[move.to_row][move.to_col].type == EMPTY) {
        // This is an en passant capture - remove the captured pawn
        int captured_pawn_row = (piece.color == WHITE) ? move.to_row + 1 : move.to_row - 1;
        game->board[captured_pawn_row][move.to_col].type = EMPTY;
        game->board[captured_pawn_row][move.to_col].color = NONE;
    }
    
    // Move the piece
    game->board[move.to_row][move.to_col] = piece;
    game->board[move.from_row][move.from_col].type = EMPTY;
    game->board[move.from_row][move.from_col].color = NONE;
    
    // Set en passant state if pawn moved two squares
    if (piece.type == PAWN && abs(move.to_row - move.from_row) == 2) {
        game->en_passant_col = move.to_col;
        game->en_passant_row = (move.from_row + move.to_row) / 2;
    }
    
    // Handle castling - move the rook too
    if (piece.type == KING && abs(move.to_col - move.from_col) == 2) {
        if (move.to_col > move.from_col) {
            // Kingside castle - move rook from h to f
            ChessPiece rook = game->board[move.from_row][7];
            game->board[move.from_row][5] = rook;
            game->board[move.from_row][7].type = EMPTY;
            game->board[move.from_row][7].color = NONE;
        } else {
            // Queenside castle - move rook from a to d
            ChessPiece rook = game->board[move.from_row][0];
            game->board[move.from_row][3] = rook;
            game->board[move.from_row][0].type = EMPTY;
            game->board[move.from_row][0].color = NONE;
        }
    }
    
    // Pawn promotion
    if (piece.type == PAWN) {
        if ((piece.color == WHITE && move.to_row == 0) || 
            (piece.color == BLACK && move.to_row == 7)) {
            // Promote to queen (90% of the time) or knight (10% for variety)
            game->board[move.to_row][move.to_col].type = (rand() % 10 == 0) ? KNIGHT : QUEEN;
        }
    }
    
    if (piece.type == KING) {
        if (piece.color == WHITE) game->white_king_moved = true;
        else game->black_king_moved = true;
    }
    if (piece.type == ROOK) {
        if (piece.color == WHITE) {
            if (move.from_col == 0) game->white_rook_a_moved = true;
            if (move.from_col == 7) game->white_rook_h_moved = true;
        } else {
            if (move.from_col == 0) game->black_rook_a_moved = true;
            if (move.from_col == 7) game->black_rook_h_moved = true;
        }
    }
    
    // Also mark rook as moved if it's being captured
    ChessPiece target = game->board[move.to_row][move.to_col];
    if (target.type == ROOK) {
        if (target.color == WHITE) {
            if (move.to_col == 0) game->white_rook_a_moved = true;
            if (move.to_col == 7) game->white_rook_h_moved = true;
        } else {
            if (move.to_col == 0) game->black_rook_a_moved = true;
            if (move.to_col == 7) game->black_rook_h_moved = true;
        }
    }
    
    game->turn = (game->turn == WHITE) ? BLACK : WHITE;
}

int chess_evaluate_position(ChessGameState *game) {
    int piece_values[] = {0, 100, 320, 330, 500, 900, 20000};
    int score = 0;
    
    // Piece-square tables for positional bonuses
    int pawn_table[8][8] = {
        {0,  0,  0,  0,  0,  0,  0,  0},
        {50, 50, 50, 50, 50, 50, 50, 50},
        {10, 10, 20, 30, 30, 20, 10, 10},
        {5,  5, 10, 25, 25, 10,  5,  5},
        {0,  0,  0, 20, 20,  0,  0,  0},
        {5, -5,-10,  0,  0,-10, -5,  5},
        {5, 10, 10,-20,-20, 10, 10,  5},
        {0,  0,  0,  0,  0,  0,  0,  0}
    };
    
    int knight_table[8][8] = {
        {-50,-40,-30,-30,-30,-30,-40,-50},
        {-40,-20,  0,  0,  0,  0,-20,-40},
        {-30,  0, 10, 15, 15, 10,  0,-30},
        {-30,  5, 15, 20, 20, 15,  5,-30},
        {-30,  0, 15, 20, 20, 15,  0,-30},
        {-30,  5, 10, 15, 15, 10,  5,-30},
        {-40,-20,  0,  5,  5,  0,-20,-40},
        {-50,-40,-30,-30,-30,-30,-40,-50}
    };
    
    int bishop_table[8][8] = {
        {-20,-10,-10,-10,-10,-10,-10,-20},
        {-10,  0,  0,  0,  0,  0,  0,-10},
        {-10,  0,  5, 10, 10,  5,  0,-10},
        {-10,  5,  5, 10, 10,  5,  5,-10},
        {-10,  0, 10, 10, 10, 10,  0,-10},
        {-10, 10, 10, 10, 10, 10, 10,-10},
        {-10,  5,  0,  0,  0,  0,  5,-10},
        {-20,-10,-10,-10,-10,-10,-10,-20}
    };
    
    int king_middle_game[8][8] = {
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-30,-40,-40,-50,-50,-40,-40,-30},
        {-20,-30,-30,-40,-40,-30,-30,-20},
        {-10,-20,-20,-20,-20,-20,-20,-10},
        { 20, 20,  0,  0,  0,  0, 20, 20},
        { 20, 30, 10,  0,  0, 10, 30, 20}
    };
    
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            ChessPiece p = game->board[r][c];
            if (p.type != EMPTY) {
                int value = piece_values[p.type];
                int positional_bonus = 0;
                
                // Add positional bonuses
                if (p.type == PAWN) {
                    if (p.color == WHITE) {
                        positional_bonus = pawn_table[r][c];
                        // Bonus for passed pawns
                        bool passed = true;
                        for (int check_r = r - 1; check_r >= 0; check_r--) {
                            for (int check_c = c - 1; check_c <= c + 1; check_c++) {
                                if (chess_is_in_bounds(check_r, check_c)) {
                                    if (game->board[check_r][check_c].type == PAWN && 
                                        game->board[check_r][check_c].color == BLACK) {
                                        passed = false;
                                    }
                                }
                            }
                        }
                        if (passed && r < 4) positional_bonus += 20;
                    } else {
                        positional_bonus = pawn_table[7-r][c];
                        // Bonus for passed pawns
                        bool passed = true;
                        for (int check_r = r + 1; check_r < 8; check_r++) {
                            for (int check_c = c - 1; check_c <= c + 1; check_c++) {
                                if (chess_is_in_bounds(check_r, check_c)) {
                                    if (game->board[check_r][check_c].type == PAWN && 
                                        game->board[check_r][check_c].color == WHITE) {
                                        passed = false;
                                    }
                                }
                            }
                        }
                        if (passed && r > 3) positional_bonus += 20;
                    }
                } else if (p.type == KNIGHT) {
                    if (p.color == WHITE) {
                        positional_bonus = knight_table[r][c];
                    } else {
                        positional_bonus = knight_table[7-r][c];
                    }
                } else if (p.type == BISHOP) {
                    if (p.color == WHITE) {
                        positional_bonus = bishop_table[r][c];
                    } else {
                        positional_bonus = bishop_table[7-r][c];
                    }
                } else if (p.type == KING) {
                    if (p.color == WHITE) {
                        positional_bonus = king_middle_game[r][c];
                    } else {
                        positional_bonus = king_middle_game[7-r][c];
                    }
                }
                
                // Bonus for center control (e4, d4, e5, d5)
                if ((r == 3 || r == 4) && (c == 3 || c == 4)) {
                    if (p.type == PAWN || p.type == KNIGHT) {
                        positional_bonus += 15;
                    }
                }
                
                int total_value = value + positional_bonus;
                score += (p.color == WHITE) ? total_value : -total_value;
            }
        }
    }
    
    // King safety - penalize if king is exposed
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->board[r][c].type == KING) {
                int safety_score = 0;
                ChessColor king_color = game->board[r][c].color;
                int direction = (king_color == WHITE) ? -1 : 1;
                
                // Check for pawns in front of king
                for (int dc = -1; dc <= 1; dc++) {
                    int check_r = r + direction;
                    int check_c = c + dc;
                    if (chess_is_in_bounds(check_r, check_c)) {
                        if (game->board[check_r][check_c].type == PAWN && 
                            game->board[check_r][check_c].color == king_color) {
                            safety_score += 20;
                        }
                    }
                }
                
                // Bonus for castling (king on g1/g8 or c1/c8)
                if ((king_color == WHITE && r == 7 && (c == 6 || c == 2)) ||
                    (king_color == BLACK && r == 0 && (c == 6 || c == 2))) {
                    safety_score += 30;
                }
                
                score += (king_color == WHITE) ? safety_score : -safety_score;
            }
        }
    }
    
    // Bishop pair bonus
    int white_bishops = 0, black_bishops = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->board[r][c].type == BISHOP) {
                if (game->board[r][c].color == WHITE) white_bishops++;
                else black_bishops++;
            }
        }
    }
    if (white_bishops >= 2) score += 30;
    if (black_bishops >= 2) score -= 30;
    
    // Small random factor for variety
    score += (rand() % 10) - 5;
    
    return score;
}

int chess_get_all_moves(ChessGameState *game, ChessColor color, ChessMove *moves) {
    int count = 0;
    
    for (int fr = 0; fr < BOARD_SIZE; fr++) {
        for (int fc = 0; fc < BOARD_SIZE; fc++) {
            if (game->board[fr][fc].color == color) {
                for (int tr = 0; tr < BOARD_SIZE; tr++) {
                    for (int tc = 0; tc < BOARD_SIZE; tc++) {
                        if (chess_is_valid_move(game, fr, fc, tr, tc)) {
                            ChessGameState temp = *game;
                            ChessMove m = {fr, fc, tr, tc, 0};
                            chess_make_move(&temp, m);
                            
                            if (!chess_is_in_check(&temp, color)) {
                                moves[count++] = m;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return count;
}

int chess_minimax(ChessGameState *game, int depth, int alpha, int beta, bool maximizing) {
    ChessMove moves[256];
    int move_count = chess_get_all_moves(game, game->turn, moves);
    
    if (move_count == 0) {
        if (chess_is_in_check(game, game->turn)) {
            return maximizing ? (-1000000 + depth) : (1000000 - depth);
        }
        return 0; // Stalemate
    }
    
    if (depth == 0) {
        return chess_evaluate_position(game);
    }
    
    if (maximizing) {
        int max_eval = INT_MIN;
        for (int i = 0; i < move_count; i++) {
            ChessGameState temp = *game;
            chess_make_move(&temp, moves[i]);
            int eval = chess_minimax(&temp, depth - 1, alpha, beta, false);
            max_eval = (eval > max_eval) ? eval : max_eval;
            alpha = (alpha > eval) ? alpha : eval;
            if (beta <= alpha) break;
        }
        return max_eval;
    } else {
        int min_eval = INT_MAX;
        for (int i = 0; i < move_count; i++) {
            ChessGameState temp = *game;
            chess_make_move(&temp, moves[i]);
            int eval = chess_minimax(&temp, depth - 1, alpha, beta, true);
            min_eval = (eval < min_eval) ? eval : min_eval;
            beta = (beta < eval) ? beta : eval;
            if (beta <= alpha) break;
        }
        return min_eval;
    }
}

// ============================================================================
// THINKING STATE MANAGEMENT
// ============================================================================

void* chess_think_continuously(void* arg) {
    ChessThinkingState *ts = (ChessThinkingState*)arg;
    
    while (true) {
        pthread_mutex_lock(&ts->lock);
        if (!ts->thinking) {
            pthread_mutex_unlock(&ts->lock);
            usleep(10000);
            continue;
        }
        
        ChessGameState game_copy = ts->game;
        pthread_mutex_unlock(&ts->lock);
        
        ChessMove moves[256];
        int move_count = chess_get_all_moves(&game_copy, game_copy.turn, moves);
        
        if (move_count == 0) {
            pthread_mutex_lock(&ts->lock);
            ts->has_move = false;
            ts->thinking = false;
            pthread_mutex_unlock(&ts->lock);
            //printf("THINK: No legal moves found!\n");
            continue;
        }
        
        //printf("THINK: Starting search for %s, %d legal moves\n",  game_copy.turn == WHITE ? "WHITE" : "BLACK", move_count);
        
        // Iterative deepening - LIMITED TO DEPTH 4
        for (int depth = 1; depth <= 4; depth++) {
            ChessMove best_moves[256];
            int best_move_count = 0;
            int best_score = (game_copy.turn == WHITE) ? INT_MIN : INT_MAX;
            
            bool depth_completed = true;
            
            for (int i = 0; i < move_count; i++) {
                pthread_mutex_lock(&ts->lock);
                bool should_stop = !ts->thinking;
                pthread_mutex_unlock(&ts->lock);
                
                if (should_stop) {
                    depth_completed = false;
                    break;
                }
                
                ChessGameState temp = game_copy;
                chess_make_move(&temp, moves[i]);
                int score = chess_minimax(&temp, depth - 1, INT_MIN, INT_MAX, 
                                         game_copy.turn == BLACK);
                
                if (game_copy.turn == WHITE) {
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
            
            // Update if we completed this depth
            pthread_mutex_lock(&ts->lock);
            if (depth_completed && ts->thinking && best_move_count > 0) {
                ts->best_move = best_moves[rand() % best_move_count];
                ts->best_score = best_score;
                ts->current_depth = depth;
                ts->has_move = true;
                
                // Debug output - print all best moves
                //printf("THINK: Depth %d complete (score=%d, %s), found %d best move(s):\n", depth, best_score, game_copy.turn == WHITE ? "WHITE" : "BLACK", best_move_count);
                for (int i = 0; i < best_move_count; i++) {
                    //printf("       %c%d->%c%d", 'a' + best_moves[i].from_col, 8 - best_moves[i].from_row, 'a' + best_moves[i].to_col, 8 - best_moves[i].to_row);
                          }
                //printf("       Selected: %c%d->%c%d\n", 'a' + ts->best_move.from_col, 8 - ts->best_move.from_row, 'a' + ts->best_move.to_col, 8 - ts->best_move.to_row);
            }
            pthread_mutex_unlock(&ts->lock);
            

        }
        
        pthread_mutex_lock(&ts->lock);
        ts->thinking = false;
        //printf("THINK: Finished thinking, final depth=%d, has_move=%d\n",  ts->current_depth, ts->has_move);
        pthread_mutex_unlock(&ts->lock);
    }
    
    return NULL;
}

void chess_init_thinking_state(ChessThinkingState *ts) {
    ts->thinking = false;
    ts->has_move = false;
    ts->current_depth = 0;
    ts->best_score = 0;
    pthread_mutex_init(&ts->lock, NULL);
    pthread_create(&ts->thread, NULL, chess_think_continuously, ts);
}

void chess_start_thinking(ChessThinkingState *ts, ChessGameState *game) {
    pthread_mutex_lock(&ts->lock);
    ts->game = *game;
    ts->thinking = true;
    ts->has_move = false;
    ts->current_depth = 0;
    pthread_mutex_unlock(&ts->lock);
}

ChessMove chess_get_best_move_now(ChessThinkingState *ts) {
    pthread_mutex_lock(&ts->lock);
    ChessMove move = ts->best_move;
    bool has_move = ts->has_move;
    ts->thinking = false; // Stop thinking
    pthread_mutex_unlock(&ts->lock);
    
    if (!has_move) {
        // No move found yet - pick random legal move as fallback
        ChessMove moves[256];
        int count = chess_get_all_moves(&ts->game, ts->game.turn, moves);
        if (count > 0) {
            move = moves[rand() % count];
        }
    }
    
    // Return the best move found so far (stored in ts->best_move)
    return move;
}

void chess_stop_thinking(ChessThinkingState *ts) {
    pthread_mutex_lock(&ts->lock);
    ts->thinking = false;
    pthread_mutex_unlock(&ts->lock);
}

// ============================================================================
// GAME STATUS
// ============================================================================

ChessGameStatus chess_check_game_status(ChessGameState *game) {
    ChessMove moves[256];
    int move_count = chess_get_all_moves(game, game->turn, moves);
    
    if (move_count == 0) {
        if (chess_is_in_check(game, game->turn)) {
            return (game->turn == WHITE) ? CHESS_CHECKMATE_BLACK : CHESS_CHECKMATE_WHITE;
        }
        return CHESS_STALEMATE;
    }
    
    return CHESS_PLAYING;
}

// ============================================================================
// UNDO FUNCTIONALITY
// ============================================================================

void chess_save_move_history(BeatChessVisualization *chess, ChessMove move, double time_spent) {
    if (chess->move_history_count < MAX_MOVE_HISTORY) {
        chess->move_history[chess->move_history_count].game_state = chess->game;
        chess->move_history[chess->move_history_count].move = move;
        chess->move_history[chess->move_history_count].time_elapsed = time_spent;
        chess->move_history_count++;
    }
}

bool chess_can_undo(BeatChessVisualization *chess) {
    // Can undo if:
    // 1. In Player vs AI mode
    // 2. It's the player's turn (WHITE in normal mode, BLACK in flipped mode)
    // 3. There's at least one move in history (a move was just made by AI)
    
    ChessColor player_color = chess->board_flipped ? BLACK : WHITE;
    
    return chess->player_vs_ai && 
           chess->game.turn == player_color && 
           chess->move_history_count > 0;
}

void chess_undo_last_move(BeatChessVisualization *chess) {
    if (!chess_can_undo(chess)) return;
    
    // Determine actual player and AI colors based on board state
    ChessColor player_color = chess->board_flipped ? BLACK : WHITE;
    ChessColor ai_color = chess->board_flipped ? WHITE : BLACK;
    
    // When player clicks undo, it's currently the player's turn
    // This means: Player move -> AI move -> [NOW]
    // We want to undo the AI's move AND the player's move before it
    // So we go back 2 moves in history
    
    if (chess->move_history_count >= 2) {
        // Get the AI's move (most recent, at index count-1)
        // Get the player's move before it (at index count-2)
        MoveHistory *player_move = &chess->move_history[chess->move_history_count - 2];
        MoveHistory *ai_move = &chess->move_history[chess->move_history_count - 1];
        
        // Restore to the state BEFORE the player's move
        // We need to go back 3 moves worth: to before the previous AI move
        if (chess->move_history_count >= 3) {
            chess->game = chess->move_history[chess->move_history_count - 3].game_state;
        } else {
            // Only 2 moves in history - this was the first exchange
            // Go back to starting position
            chess_init_board(&chess->game);
        }
        
        // Subtract times from totals (using correct colors)
        if (player_color == WHITE) {
            chess->white_total_time -= player_move->time_elapsed;
        } else {
            chess->black_total_time -= player_move->time_elapsed;
        }
        
        if (ai_color == WHITE) {
            chess->white_total_time -= ai_move->time_elapsed;
        } else {
            chess->black_total_time -= ai_move->time_elapsed;
        }
        
        if (chess->white_total_time < 0) chess->white_total_time = 0;
        if (chess->black_total_time < 0) chess->black_total_time = 0;
        
        // Remove the 2 moves from history
        chess->move_history_count -= 2;
        
        strcpy(chess->status_text, "Moves undone - your turn to play again");
        chess->status_flash_color[0] = 0.2;
        chess->status_flash_color[1] = 0.8;
        chess->status_flash_color[2] = 1.0;
    } else if (chess->move_history_count == 1) {
        // Only player's opening move - undo it
        chess_init_board(&chess->game);
        
        // Subtract time for player's move (using correct color)
        if (player_color == WHITE) {
            chess->white_total_time -= chess->move_history[0].time_elapsed;
        } else {
            chess->black_total_time -= chess->move_history[0].time_elapsed;
        }
        
        if (chess->white_total_time < 0) chess->white_total_time = 0;
        if (chess->black_total_time < 0) chess->black_total_time = 0;
        
        chess->move_history_count = 0;
        
        strcpy(chess->status_text, "Opening move undone - try again");
        chess->status_flash_color[0] = 0.2;
        chess->status_flash_color[1] = 0.8;
        chess->status_flash_color[2] = 1.0;
    }
    
    chess->move_count = chess->move_history_count;
    chess->last_move_glow = 0;
    chess->animation_progress = 0;
    chess->is_animating = false;
    chess->undo_button_glow = 1.0;
    chess->status_flash_timer = 1.5;
}

// ============================================================================
// VISUALIZATION SYSTEM
// ============================================================================

void init_beat_chess_system(void *vis_ptr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    BeatChessVisualization *chess = &vis->beat_chess;
    
    // Initialize game
    chess_init_board(&chess->game);
    chess_init_thinking_state(&chess->thinking_state);
    chess->status = CHESS_PLAYING;
    
    // Start first player thinking
    chess_start_thinking(&chess->thinking_state, &chess->game);
    
    // Initialize animation positions
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            chess->piece_x[r][c] = 0;
            chess->piece_y[r][c] = 0;
            chess->target_x[r][c] = 0;
            chess->target_y[r][c] = 0;
        }
    }
    
    // Initialize state
    chess->last_from_row = -1;
    chess->last_from_col = -1;
    chess->last_to_row = -1;
    chess->last_to_col = -1;
    chess->last_move_glow = 0;
    
    chess->is_animating = false;
    chess->animation_progress = 0;
    
    strcpy(chess->status_text, "Game started - White to move");
    chess->status_flash_timer = 0;
    chess->status_flash_color[0] = 1.0;
    chess->status_flash_color[1] = 1.0;
    chess->status_flash_color[2] = 1.0;
    chess->last_eval_change = 0;
    
    // Beat detection
    for (int i = 0; i < BEAT_HISTORY_SIZE; i++) {
        chess->beat_volume_history[i] = 0;
    }
    chess->beat_history_index = 0;
    chess->time_since_last_move = 0;
    chess->beat_threshold = 1.3;
    
    chess->move_count = 0;
    chess->eval_bar_position = 0;
    chess->eval_bar_target = 0;
    
    // Game over handling
    chess->beats_since_game_over = 0;
    chess->waiting_for_restart = false;

    chess->time_thinking = 0;
    chess->min_think_time = 0.5;        // Wait at least 0.5s before auto-playing
    chess->good_move_threshold = 150;   // Auto-play if advantage > 150 centipawns
    chess->auto_play_enabled = true;    // Enable auto-play
    
    // Reset button
    chess->reset_button_hovered = false;
    chess->reset_button_glow = 0;
    chess->reset_button_was_pressed = false;
    
    // PvsA toggle button
    chess->pvsa_button_hovered = false;
    chess->pvsa_button_glow = 0;
    chess->pvsa_button_was_pressed = false;
    chess->player_vs_ai = false;  // Start with AI vs AI
    
    // Undo button
    chess->undo_button_hovered = false;
    chess->undo_button_glow = 0;
    chess->undo_button_was_pressed = false;
    chess->undo_button_x = 20;
    chess->undo_button_y = 170;
    chess->undo_button_width = 120;
    chess->undo_button_height = 40;
    
    // Flip board button
    chess->flip_button_hovered = false;
    chess->flip_button_glow = 0;
    chess->flip_button_was_pressed = false;
    chess->board_flipped = false;  // Normal orientation by default
    
    // Move history
    chess->move_history_count = 0;
    
    // Time tracking
    chess->white_total_time = 0.0;
    chess->black_total_time = 0.0;
    chess->current_move_start_time = 0.0;
    chess->last_move_end_time = 0.0;
    
    // Player move tracking
    chess->selected_piece_row = -1;
    chess->selected_piece_col = -1;
    chess->has_selected_piece = false;
    chess->selected_piece_was_pressed = false;
    
    printf("Beat chess system initialized\n");
}

bool beat_chess_detect_beat(void *vis_ptr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    BeatChessVisualization *chess = &vis->beat_chess;
    
    // Update history
    chess->beat_volume_history[chess->beat_history_index] = vis->volume_level;
    chess->beat_history_index = (chess->beat_history_index + 1) % BEAT_HISTORY_SIZE;
    
    // Calculate average
    double avg = 0;
    for (int i = 0; i < BEAT_HISTORY_SIZE; i++) {
        avg += chess->beat_volume_history[i];
    }
    avg /= BEAT_HISTORY_SIZE;
    
    // Detect beat with minimum time between moves
    if (vis->volume_level > avg * chess->beat_threshold && 
        vis->volume_level > 0.05 &&
        chess->time_since_last_move > 0.2) { // Minimum 200ms between moves
        return true;
    }
    
    return false;
}

void update_beat_chess(void *vis_ptr, double dt) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    BeatChessVisualization *chess = &vis->beat_chess;
    
    chess->time_since_last_move += dt;
    chess->time_thinking += dt;
    
    // Track current move time (in Player vs AI mode when it's player's or AI's turn)
    if (chess->player_vs_ai && chess->status == CHESS_PLAYING) {
        chess->current_move_start_time += dt;
    }
    
    // Calculate board layout early so player moves can use it
    double available_width = vis->width * 0.8;
    double available_height = vis->height * 0.8;
    chess->cell_size = fmin(available_width / 8, available_height / 8);
    chess->board_offset_x = (vis->width - chess->cell_size * 8) / 2;
    chess->board_offset_y = (vis->height - chess->cell_size * 8) / 2;
    
    // ===== CHECK RESET BUTTON INTERACTION =====
    // Detect if mouse is over button (for hover effects)
    bool is_over_reset = (vis->mouse_x >= chess->reset_button_x && 
                          vis->mouse_x <= chess->reset_button_x + chess->reset_button_width &&
                          vis->mouse_y >= chess->reset_button_y && 
                          vis->mouse_y <= chess->reset_button_y + chess->reset_button_height);
    
    chess->reset_button_hovered = is_over_reset;
    
    // Detect click: button was pressed last frame AND released this frame
    bool reset_was_pressed = chess->reset_button_was_pressed;
    bool reset_is_pressed = vis->mouse_left_pressed;
    bool reset_clicked = (reset_was_pressed && !reset_is_pressed && is_over_reset);
    
    // Update for next frame
    chess->reset_button_was_pressed = reset_is_pressed;
    
    // Handle the click if it happened
    if (reset_clicked) {
        // Reset the game
        chess_init_board(&chess->game);
        chess->status = CHESS_PLAYING;
        chess->beats_since_game_over = 0;
        chess->waiting_for_restart = false;
        chess->move_count = 0;
        chess->eval_bar_position = 0;
        chess->eval_bar_target = 0;
        chess->time_thinking = 0;
        chess->last_move_glow = 0;
        chess->animation_progress = 0;
        chess->is_animating = false;
        chess->last_from_row = -1;
        
        strcpy(chess->status_text, "Game Reset! White to move");
        chess->status_flash_color[0] = 0.2;
        chess->status_flash_color[1] = 0.8;
        chess->status_flash_color[2] = 1.0;
        chess->status_flash_timer = 1.5;
        
        chess->reset_button_glow = 1.0;
        
        // Start thinking for new game
        chess_start_thinking(&chess->thinking_state, &chess->game);
    }
    // ========================================
    
    // ===== CHECK PVSA TOGGLE BUTTON INTERACTION =====
    // Detect if mouse is over button (for hover effects)
    bool is_over_pvsa = (vis->mouse_x >= chess->pvsa_button_x && 
                         vis->mouse_x <= chess->pvsa_button_x + chess->pvsa_button_width &&
                         vis->mouse_y >= chess->pvsa_button_y && 
                         vis->mouse_y <= chess->pvsa_button_y + chess->pvsa_button_height);
    
    chess->pvsa_button_hovered = is_over_pvsa;
    
    // Detect click: button was pressed last frame AND released this frame
    bool pvsa_was_pressed = chess->pvsa_button_was_pressed;
    bool pvsa_is_pressed = vis->mouse_left_pressed;
    bool pvsa_clicked = (pvsa_was_pressed && !pvsa_is_pressed && is_over_pvsa);
    
    // Update for next frame
    chess->pvsa_button_was_pressed = pvsa_is_pressed;
    
    // Handle the click if it happened
    if (pvsa_clicked) {
        // Toggle between Player vs AI and AI vs AI
        chess->player_vs_ai = !chess->player_vs_ai;
        
        // Reset game when toggling
        chess_init_board(&chess->game);
        chess->status = CHESS_PLAYING;
        chess->beats_since_game_over = 0;
        chess->waiting_for_restart = false;
        chess->move_count = 0;
        chess->eval_bar_position = 0;
        chess->eval_bar_target = 0;
        chess->time_thinking = 0;
        chess->last_move_glow = 0;
        chess->animation_progress = 0;
        chess->is_animating = false;
        chess->last_from_row = -1;
        
        if (chess->player_vs_ai) {
            strcpy(chess->status_text, "Player vs AI - White (player) to move");
            chess->status_flash_color[0] = 0.2;
            chess->status_flash_color[1] = 0.8;
            chess->status_flash_color[2] = 1.0;
        } else {
            strcpy(chess->status_text, "AI vs AI - Game started!");
            chess->status_flash_color[0] = 1.0;
            chess->status_flash_color[1] = 0.65;
            chess->status_flash_color[2] = 0.0;
        }
        chess->status_flash_timer = 2.0;
        chess->pvsa_button_glow = 1.0;
        
        // Start thinking for new game
        chess_start_thinking(&chess->thinking_state, &chess->game);
    }
    // ===============================================
    
    // ===== CHECK UNDO BUTTON INTERACTION =====
    // Only check undo in Player vs AI mode
    if (chess->player_vs_ai) {
        // Detect if mouse is over button (for hover effects)
        bool is_over_undo = (vis->mouse_x >= chess->undo_button_x && 
                             vis->mouse_x <= chess->undo_button_x + chess->undo_button_width &&
                             vis->mouse_y >= chess->undo_button_y && 
                             vis->mouse_y <= chess->undo_button_y + chess->undo_button_height);
        
        chess->undo_button_hovered = is_over_undo && chess_can_undo(chess);
        
        // Detect click: button was pressed last frame AND released this frame
        bool undo_was_pressed = chess->undo_button_was_pressed;
        bool undo_is_pressed = vis->mouse_left_pressed;
        bool undo_clicked = (undo_was_pressed && !undo_is_pressed && is_over_undo && chess_can_undo(chess));
        
        // Update for next frame
        chess->undo_button_was_pressed = undo_is_pressed;
        
        // Handle the click if it happened
        if (undo_clicked) {
            chess_undo_last_move(chess);
        }
    } else {
        chess->undo_button_hovered = false;
        chess->undo_button_was_pressed = false;
    }
    
    // ===== CHECK FLIP BOARD BUTTON INTERACTION =====
    // Only check if in Player vs AI mode
    if (chess->player_vs_ai) {
        bool is_over_flip = (vis->mouse_x >= chess->flip_button_x && 
                             vis->mouse_x <= chess->flip_button_x + chess->flip_button_width &&
                             vis->mouse_y >= chess->flip_button_y && 
                             vis->mouse_y <= chess->flip_button_y + chess->flip_button_height);
        
        chess->flip_button_hovered = is_over_flip;
        
        // Detect click: button was pressed last frame AND released this frame
        bool flip_was_pressed = chess->flip_button_was_pressed;
        bool flip_is_pressed = vis->mouse_left_pressed;
        bool flip_clicked = (flip_was_pressed && !flip_is_pressed && is_over_flip);
        
        // Update for next frame
        chess->flip_button_was_pressed = flip_is_pressed;
        
        // Handle the click if it happened
        if (flip_clicked) {
            chess->board_flipped = !chess->board_flipped;
            chess->flip_button_glow = 1.0;
            
            // Reset the game when flipping
            chess_init_board(&chess->game);
            chess->status = CHESS_PLAYING;
            chess->beats_since_game_over = 0;
            chess->waiting_for_restart = false;
            chess->move_count = 0;
            chess->eval_bar_position = 0;
            chess->eval_bar_target = 0;
            chess->time_thinking = 0;
            chess->last_move_glow = 0;
            chess->animation_progress = 0;
            chess->is_animating = false;
            chess->last_from_row = -1;
            chess->last_from_col = -1;
            chess->last_to_row = -1;
            chess->last_to_col = -1;
            
            // Reset timers
            chess->white_total_time = 0.0;
            chess->black_total_time = 0.0;
            chess->current_move_start_time = 0.0;
            chess->last_move_end_time = 0.0;
            chess->time_since_last_move = 0.0;
            
            // Clear move history
            chess->move_history_count = 0;
            
            // Clear selection
            chess->has_selected_piece = false;
            chess->selected_piece_row = -1;
            chess->selected_piece_col = -1;
            
            // Update status text
            if (chess->board_flipped) {
                strcpy(chess->status_text, "Playing as BLACK - AI plays WHITE");
                chess->status_flash_color[0] = 0.9;
                chess->status_flash_color[1] = 0.9;
                chess->status_flash_color[2] = 0.2;
            } else {
                strcpy(chess->status_text, "Playing as WHITE - AI plays BLACK");
                chess->status_flash_color[0] = 0.2;
                chess->status_flash_color[1] = 0.8;
                chess->status_flash_color[2] = 1.0;
            }
            chess->status_flash_timer = 2.0;
            
            // Start thinking for new game
            chess_start_thinking(&chess->thinking_state, &chess->game);
        }
        
        // Decay glow effect
        chess->flip_button_glow *= 0.95;
    }
    // =============================================
    
    // Decay glow effects
    chess->reset_button_glow *= 0.95;
    chess->pvsa_button_glow *= 0.95;
    chess->undo_button_glow *= 0.95;
    // =========================================
    if (chess->player_vs_ai) {
        // Determine which color the player is controlling based on board flip
        ChessColor player_color = chess->board_flipped ? BLACK : WHITE;
        
        if (chess->game.turn == player_color) {
        // Get which square the mouse is over
        double cell = chess->cell_size;
        double ox = chess->board_offset_x;
        double oy = chess->board_offset_y;
        
        // Calculate which square (if any) mouse is over
        int mouse_row = -1, mouse_col = -1;
        if (vis->mouse_x >= ox && vis->mouse_x < ox + cell * 8 &&
            vis->mouse_y >= oy && vis->mouse_y < oy + cell * 8) {
            int visual_row = (int)((vis->mouse_y - oy) / cell);
            int visual_col = (int)((vis->mouse_x - ox) / cell);
            
            // Transform visual coordinates back to logical board coordinates if board is flipped
            if (chess->board_flipped) {
                mouse_row = BOARD_SIZE - 1 - visual_row;
                mouse_col = BOARD_SIZE - 1 - visual_col;
            } else {
                mouse_row = visual_row;
                mouse_col = visual_col;
            }
        }
        
        // Detect single click (press then release)
        bool is_pressed = vis->mouse_left_pressed;
        bool was_pressed = chess->selected_piece_was_pressed;
        bool just_clicked = (was_pressed && !is_pressed);  // Button released this frame
        
        // Update for next frame
        chess->selected_piece_was_pressed = is_pressed;
        
        if (just_clicked && mouse_row >= 0 && mouse_col >= 0) {
            // Handle click based on whether we have a piece selected
            if (!chess->has_selected_piece) {
                // First click: select a piece if it's the player's color
                ChessPiece piece = chess->game.board[mouse_row][mouse_col];
                if (piece.type != EMPTY && piece.color == player_color) {
                    chess->selected_piece_row = mouse_row;
                    chess->selected_piece_col = mouse_col;
                    chess->has_selected_piece = true;
                    strcpy(chess->status_text, "Piece selected - click destination");
                }
            } else {
                // Second click: try to move to destination
                int from_row = chess->selected_piece_row;
                int from_col = chess->selected_piece_col;
                int to_row = mouse_row;
                int to_col = mouse_col;
                
                // Don't allow moving to same square
                if (from_row == to_row && from_col == to_col) {
                    // Same square - deselect piece
                    chess->has_selected_piece = false;
                    strcpy(chess->status_text, "Piece deselected");
                } else {
                    // Try to move
                    if (chess_is_valid_move(&chess->game, from_row, from_col, to_row, to_col)) {
                        // Check if move leaves king in check
                        ChessGameState temp_game = chess->game;
                        ChessMove test_move = {from_row, from_col, to_row, to_col, 0};
                        chess_make_move(&temp_game, test_move);
                        
                        if (!chess_is_in_check(&temp_game, player_color)) {
                            // Valid move! Make it
                            chess_make_move(&chess->game, test_move);
                            
                            // Track time for player's color and save move history
                            double time_on_move = chess->current_move_start_time;
                            if (player_color == WHITE) {
                                chess->white_total_time += time_on_move;
                            } else {
                                chess->black_total_time += time_on_move;
                            }
                            chess->last_move_end_time = 0;  // Reset for next turn
                            
                            chess_save_move_history(chess, test_move, time_on_move);
                            
                            // Update display
                            chess->last_from_row = from_row;
                            chess->last_from_col = from_col;
                            chess->last_to_row = to_row;
                            chess->last_to_col = to_col;
                            chess->last_move_glow = 1.0;
                            
                            chess->animating_from_row = from_row;
                            chess->animating_from_col = from_col;
                            chess->animating_to_row = to_row;
                            chess->animating_to_col = to_col;
                            chess->animation_progress = 0;
                            chess->is_animating = true;
                            
                            // Show whose turn it is now (AI's color)
                            ChessColor ai_color = chess->board_flipped ? WHITE : BLACK;
                            if (ai_color == WHITE) {
                                strcpy(chess->status_text, "White (AI) thinking...");
                            } else {
                                strcpy(chess->status_text, "Black (AI) thinking...");
                            }
                            chess->move_count++;
                            chess->time_since_last_move = 0;
                            chess->current_move_start_time = 0;  // Reset timer for AI's thinking
                            
                            // Check game status
                            chess->status = chess_check_game_status(&chess->game);
                            if (chess->status != CHESS_PLAYING) {
                                chess->waiting_for_restart = true;
                                chess->beats_since_game_over = 0;
                                chess->white_total_time = 0.0;
                                chess->black_total_time = 0.0;
                                chess->current_move_start_time = 0.0;
                                chess->last_move_end_time = 0.0;

                                if (chess->status == CHESS_CHECKMATE_WHITE) {
                                    strcpy(chess->status_text, "Checkmate! Black wins!");
                                    chess->status_flash_color[0] = 0.85;
                                    chess->status_flash_color[1] = 0.65;
                                    chess->status_flash_color[2] = 0.13;
                                } else if (chess->status == CHESS_CHECKMATE_BLACK) {
                                    strcpy(chess->status_text, "Checkmate! White wins!");
                                    chess->status_flash_color[0] = 1.0;
                                    chess->status_flash_color[1] = 1.0;
                                    chess->status_flash_color[2] = 1.0;
                                } else {
                                    strcpy(chess->status_text, "Stalemate!");
                                    chess->status_flash_color[0] = 0.7;
                                    chess->status_flash_color[1] = 0.7;
                                    chess->status_flash_color[2] = 0.7;
                                }
                                chess->status_flash_timer = 2.0;
                            } else {
                                // Start AI thinking for Black's move
                                chess->time_thinking = 0;  // Reset timer for new thinking phase
                                chess_start_thinking(&chess->thinking_state, &chess->game);
                            }
                            
                            // Deselect piece after successful move
                            chess->has_selected_piece = false;
                            chess->selected_piece_row = -1;
                            chess->selected_piece_col = -1;
                        } else {
                            // Move would leave king in check - invalid
                            strcpy(chess->status_text, "Illegal move - king in check");
                            chess->has_selected_piece = false;
                        }
                    } else {
                        // Invalid move
                        strcpy(chess->status_text, "Illegal move");
                        chess->has_selected_piece = false;
                    }
                }
            }
        }
        }  // Close the if (chess->game.turn == player_color) block
    }
    // ===================================================
    
    // Update glow effects
    if (chess->last_move_glow > 0) {
        chess->last_move_glow -= dt * 2.0;
        if (chess->last_move_glow < 0) chess->last_move_glow = 0;
    }
    
    // Update reset button hover glow
    if (chess->reset_button_glow > 0) {
        chess->reset_button_glow -= dt * 2.0;
        if (chess->reset_button_glow < 0) chess->reset_button_glow = 0;
    }
    
    // Update PvsA button hover glow
    if (chess->pvsa_button_glow > 0) {
        chess->pvsa_button_glow -= dt * 2.0;
        if (chess->pvsa_button_glow < 0) chess->pvsa_button_glow = 0;
    }
    
    if (chess->status_flash_timer > 0) {
        chess->status_flash_timer -= dt * 2.0;
        if (chess->status_flash_timer < 0) chess->status_flash_timer = 0;
    }
    
    // Animate piece movement
    if (chess->is_animating) {
        chess->animation_progress += dt * 3.0;
        if (chess->animation_progress >= 1.0) {
            chess->animation_progress = 1.0;
            chess->is_animating = false;
        }
    }
    
    // Smooth eval bar
    double diff = chess->eval_bar_target - chess->eval_bar_position;
    chess->eval_bar_position += diff * dt * 3.0;
    
    // Handle game over
    if (chess->status != CHESS_PLAYING) {
        if (chess->waiting_for_restart) {
            if (beat_chess_detect_beat(vis)) {
                chess->beats_since_game_over++;
                chess->time_since_last_move = 0;
                
                if (chess->beats_since_game_over >= 2) {
                    // Restart game
                    chess_init_board(&chess->game);
                    chess->status = CHESS_PLAYING;
                    chess->beats_since_game_over = 0;
                    chess->waiting_for_restart = false;
                    chess->move_count = 0;
                    chess->eval_bar_position = 0;
                    chess->eval_bar_target = 0;
                    chess->time_thinking = 0;
                    strcpy(chess->status_text, "New game! White to move");
                    chess->status_flash_color[0] = 0.0;
                    chess->status_flash_color[1] = 1.0;
                    chess->status_flash_color[2] = 1.0;
                    chess->status_flash_timer = 1.0;
                    
                    chess_start_thinking(&chess->thinking_state, &chess->game);
                }
            }
        }
        return;
    }
    
    // Track thinking time
    pthread_mutex_lock(&chess->thinking_state.lock);
    bool is_thinking = chess->thinking_state.thinking;
    bool has_move = chess->thinking_state.has_move;
    int current_depth = chess->thinking_state.current_depth;
    int best_score = chess->thinking_state.best_score;
    pthread_mutex_unlock(&chess->thinking_state.lock);
    
    // Keep incrementing time as long as we haven't made a move yet
    if (is_thinking || has_move) {
        chess->time_thinking += dt;
    }
    
    // AUTO-PLAY: Check if we should play immediately
    bool should_auto_play = false;
    if (chess->auto_play_enabled && has_move && 
        chess->time_thinking >= chess->min_think_time) {
        
        // Determine which color the player is controlling
        ChessColor player_color_local = chess->board_flipped ? BLACK : WHITE;
        
        // In Player vs AI mode: don't autoplay if it's the player's turn
        if (chess->player_vs_ai && chess->game.turn == player_color_local) {
            should_auto_play = false;
        }
        // Force move after 4 seconds regardless of depth/evaluation
        else if (chess->time_thinking >= 4.0) {
            should_auto_play = true;
        }
        // Play if we've reached depth 3 or 4
        else if (current_depth >= 3) {
            should_auto_play = true;
        }
        // Or if we found a really good move (even at depth 2)
        else {
            int eval_before = chess_evaluate_position(&chess->game);
            int advantage = (chess->game.turn == WHITE) ? 
                           (best_score - eval_before) : (eval_before - best_score);
            
            if (advantage > chess->good_move_threshold && current_depth >= 2) {
                should_auto_play = true;
            }
        }
    }
    
    // Detect beat OR auto-play trigger
    bool beat_detected = beat_chess_detect_beat(vis);
    
    // Determine which color the player is controlling
    ChessColor player_color = chess->board_flipped ? BLACK : WHITE;
    ChessColor ai_color = chess->board_flipped ? WHITE : BLACK;
    
    // In Player vs AI mode: only AI makes moves when it's AI's turn, player controls their color
    // In AI vs AI mode: both sides make moves
    bool should_make_move = beat_detected || should_auto_play;
    if (chess->player_vs_ai && chess->game.turn == player_color) {
        // Player's turn - don't auto-make move
        should_make_move = false;
    }
    
    if (should_make_move) {
        // Get current evaluation
        int eval_before = chess_evaluate_position(&chess->game);
        
        // Force move
        ChessMove forced_move = chess_get_best_move_now(&chess->thinking_state);
        
        // Validate move
        if (!chess_is_valid_move(&chess->game, 
                                 forced_move.from_row, forced_move.from_col,
                                 forced_move.to_row, forced_move.to_col)) {
            chess_start_thinking(&chess->thinking_state, &chess->game);
            chess->time_thinking = 0;
            return;
        }
        
        // Check if move leaves king in check
        ChessGameState temp_game = chess->game;
        chess_make_move(&temp_game, forced_move);
        if (chess_is_in_check(&temp_game, chess->game.turn)) {
            chess_start_thinking(&chess->thinking_state, &chess->game);
            chess->time_thinking = 0;
            return;
        }
        
        // Get depth reached
        pthread_mutex_lock(&chess->thinking_state.lock);
        int depth_reached = chess->thinking_state.current_depth;
        pthread_mutex_unlock(&chess->thinking_state.lock);
        
        // Make the move
        ChessColor moving_color = chess->game.turn;
        chess_make_move(&chess->game, forced_move);
        
        // Track time and save move history (only in Player vs AI mode)
        if (chess->player_vs_ai) {
            double ai_time = chess->time_thinking;
            // Track time for whichever color the AI is playing
            if (ai_color == WHITE) {
                chess->white_total_time += ai_time;
            } else {
                chess->black_total_time += ai_time;
            }
            chess_save_move_history(chess, forced_move, ai_time);
            chess->time_thinking = 0;  // Reset for next AI turn
        }
        
        // Evaluate
        int eval_after = chess_evaluate_position(&chess->game);
        int eval_change = (moving_color == WHITE) ? 
                         (eval_after - eval_before) : (eval_before - eval_after);
        chess->last_eval_change = eval_change;
        
        // Update eval bar
        chess->eval_bar_target = fmax(-1.0, fmin(1.0, eval_after / 1000.0));
        
        // Update display
        chess->last_from_row = forced_move.from_row;
        chess->last_from_col = forced_move.from_col;
        chess->last_to_row = forced_move.to_row;
        chess->last_to_col = forced_move.to_col;
        chess->last_move_glow = 1.0;
        
        // Animation
        chess->animating_from_row = forced_move.from_row;
        chess->animating_from_col = forced_move.from_col;
        chess->animating_to_row = forced_move.to_row;
        chess->animating_to_col = forced_move.to_col;
        chess->animation_progress = 0;
        chess->is_animating = true;
        
        // Status text
        const char *piece_names[] = {"", "Pawn", "Knight", "Bishop", "Rook", "Queen", "King"};
        ChessPiece moved_piece = chess->game.board[forced_move.to_row][forced_move.to_col];
        
        const char *trigger = should_auto_play ? "AUTO" : "BEAT";
        
        if (eval_change < -500) {
            snprintf(chess->status_text, sizeof(chess->status_text),
                    "[%s] BLUNDER! %s %c%d->%c%d (depth %d, -%d)",
                    trigger, piece_names[moved_piece.type],
                    'a' + forced_move.from_col, 8 - forced_move.from_row,
                    'a' + forced_move.to_col, 8 - forced_move.to_row,
                    depth_reached, -eval_change);
            chess->status_flash_color[0] = 1.0;
            chess->status_flash_color[1] = 0.0;
            chess->status_flash_color[2] = 0.0;
            chess->status_flash_timer = 1.0;
        } else if (eval_change > 200) {
            snprintf(chess->status_text, sizeof(chess->status_text),
                    "[%s] Brilliant! %s %c%d->%c%d (depth %d, +%d)",
                    trigger, piece_names[moved_piece.type],
                    'a' + forced_move.from_col, 8 - forced_move.from_row,
                    'a' + forced_move.to_col, 8 - forced_move.to_row,
                    depth_reached, eval_change);
            chess->status_flash_color[0] = 0.0;
            chess->status_flash_color[1] = 1.0;
            chess->status_flash_color[2] = 0.0;
            chess->status_flash_timer = 1.0;
        } else {
            snprintf(chess->status_text, sizeof(chess->status_text),
                    "[%s] %s: %s %c%d->%c%d (depth %d)",
                    trigger, moving_color == WHITE ? "White" : "Black",
                    piece_names[moved_piece.type],
                    'a' + forced_move.from_col, 8 - forced_move.from_row,
                    'a' + forced_move.to_col, 8 - forced_move.to_row,
                    depth_reached);
        }
        
        chess->move_count++;
        chess->time_since_last_move = 0;
        chess->time_thinking = 0;
        
        // Check move limit
        if (chess->move_count >= MAX_MOVES_BEFORE_DRAW) {
            strcpy(chess->status_text, "Draw by move limit! New game in 2 beats...");
            chess->status = CHESS_STALEMATE;
            chess->waiting_for_restart = true;
            chess->beats_since_game_over = 0;
            return;
        }
        
        // Check game status
        chess->status = chess_check_game_status(&chess->game);
        
        if (chess->status != CHESS_PLAYING) {
            chess->waiting_for_restart = true;
            chess->beats_since_game_over = 0;
            
            if (chess->status == CHESS_CHECKMATE_WHITE) {
                strcpy(chess->status_text, "Checkmate! White wins! New game in 2 beats...");
                chess->status_flash_color[0] = 1.0;
                chess->status_flash_color[1] = 1.0;
                chess->status_flash_color[2] = 1.0;
                chess->status_flash_timer = 2.0;
            } else if (chess->status == CHESS_CHECKMATE_BLACK) {
                strcpy(chess->status_text, "Checkmate! Black wins! New game in 2 beats...");
                chess->status_flash_color[0] = 0.85;
                chess->status_flash_color[1] = 0.65;
                chess->status_flash_color[2] = 0.13;
                chess->status_flash_timer = 2.0;
            } else {
                strcpy(chess->status_text, "Stalemate! New game in 2 beats...");
                chess->status_flash_color[0] = 0.7;
                chess->status_flash_color[1] = 0.7;
                chess->status_flash_color[2] = 0.7;
                chess->status_flash_timer = 2.0;
            }
        } else {
            // Start thinking for next move
            chess_start_thinking(&chess->thinking_state, &chess->game);
        }
    }
}
// ============================================================================
// DRAWING FUNCTIONS
// ============================================================================

void draw_piece(cairo_t *cr, PieceType type, ChessColor color, double x, double y, double size, double dance_offset) {
    double cx = x + size / 2;
    double cy = y + size / 2;
    double s = size * 0.4;  // Scale factor
    
    // Apply dance offset (vertical bounce)
    cy += dance_offset;
    
    // Set colors
    if (color == WHITE) {
        cairo_set_source_rgb(cr, 0.95, 0.95, 0.95);
    } else {
        // Gold color for black pieces
        cairo_set_source_rgb(cr, 0.85, 0.65, 0.13);
    }
    
    switch (type) {
        case PAWN:
            // Circle on small rectangle
            cairo_arc(cr, cx, cy - s * 0.15, s * 0.25, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_rectangle(cr, cx - s * 0.2, cy + s * 0.1, s * 0.4, s * 0.3);
            cairo_fill(cr);
            break;
            
        case KNIGHT:
            // Crude horse head - blocky and angular
            // Base/neck
            cairo_rectangle(cr, cx - s * 0.15, cy, s * 0.3, s * 0.4);
            cairo_fill(cr);
            // Head (off-center rectangle)
            cairo_rectangle(cr, cx - s * 0.1, cy - s * 0.4, s * 0.35, s * 0.4);
            cairo_fill(cr);
            // Snout (small rectangle sticking out)
            cairo_rectangle(cr, cx + s * 0.15, cy - s * 0.25, s * 0.2, s * 0.15);
            cairo_fill(cr);
            // Ear (triangle on top)
            cairo_move_to(cr, cx + s * 0.05, cy - s * 0.4);
            cairo_line_to(cr, cx + s * 0.15, cy - s * 0.55);
            cairo_line_to(cr, cx + s * 0.2, cy - s * 0.35);
            cairo_fill(cr);
            break;
            
        case BISHOP:
            // Triangle with circle on top
            cairo_move_to(cr, cx, cy - s * 0.5);
            cairo_line_to(cr, cx - s * 0.25, cy + s * 0.4);
            cairo_line_to(cr, cx + s * 0.25, cy + s * 0.4);
            cairo_close_path(cr);
            cairo_fill(cr);
            cairo_arc(cr, cx, cy - s * 0.5, s * 0.12, 0, 2 * M_PI);
            cairo_fill(cr);
            break;
            
        case ROOK:
            // Castle tower
            cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.1, s * 0.6, s * 0.5);
            cairo_fill(cr);
            // Crenellations
            cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.5, s * 0.15, s * 0.35);
            cairo_fill(cr);
            cairo_rectangle(cr, cx - s * 0.05, cy - s * 0.5, s * 0.1, s * 0.35);
            cairo_fill(cr);
            cairo_rectangle(cr, cx + s * 0.15, cy - s * 0.5, s * 0.15, s * 0.35);
            cairo_fill(cr);
            break;
            
        case QUEEN:
            // Crown with multiple points
            cairo_move_to(cr, cx, cy - s * 0.5);
            cairo_line_to(cr, cx - s * 0.15, cy - s * 0.2);
            cairo_line_to(cr, cx - s * 0.3, cy - s * 0.4);
            cairo_line_to(cr, cx - s * 0.3, cy + s * 0.4);
            cairo_line_to(cr, cx + s * 0.3, cy + s * 0.4);
            cairo_line_to(cr, cx + s * 0.3, cy - s * 0.4);
            cairo_line_to(cr, cx + s * 0.15, cy - s * 0.2);
            cairo_close_path(cr);
            cairo_fill(cr);
            // Center ball
            cairo_arc(cr, cx, cy - s * 0.5, s * 0.1, 0, 2 * M_PI);
            cairo_fill(cr);
            break;
            
        case KING:
            // Crown with cross
            cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.1, s * 0.6, s * 0.5);
            cairo_fill(cr);
            // Cross on top
            cairo_rectangle(cr, cx - s * 0.05, cy - s * 0.6, s * 0.1, s * 0.5);
            cairo_fill(cr);
            cairo_rectangle(cr, cx - s * 0.25, cy - s * 0.45, s * 0.5, s * 0.1);
            cairo_fill(cr);
            break;
            
        default:
            break;
    }
    
    // Outline for all pieces
    if (type != EMPTY) {
        if (color == WHITE) {
            cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        } else {
            // Darker gold outline for gold pieces
            cairo_set_source_rgb(cr, 0.5, 0.35, 0.05);
        }
        cairo_set_line_width(cr, 1.5);
        
        switch (type) {
            case PAWN:
                cairo_arc(cr, cx, cy - s * 0.15, s * 0.25, 0, 2 * M_PI);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.2, cy + s * 0.1, s * 0.4, s * 0.3);
                cairo_stroke(cr);
                break;
            case KNIGHT:
                cairo_rectangle(cr, cx - s * 0.15, cy, s * 0.3, s * 0.4);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.1, cy - s * 0.4, s * 0.35, s * 0.4);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx + s * 0.15, cy - s * 0.25, s * 0.2, s * 0.15);
                cairo_stroke(cr);
                cairo_move_to(cr, cx + s * 0.05, cy - s * 0.4);
                cairo_line_to(cr, cx + s * 0.15, cy - s * 0.55);
                cairo_line_to(cr, cx + s * 0.2, cy - s * 0.35);
                cairo_stroke(cr);
                break;
            case BISHOP:
                cairo_move_to(cr, cx, cy - s * 0.5);
                cairo_line_to(cr, cx - s * 0.25, cy + s * 0.4);
                cairo_line_to(cr, cx + s * 0.25, cy + s * 0.4);
                cairo_close_path(cr);
                cairo_stroke(cr);
                cairo_arc(cr, cx, cy - s * 0.5, s * 0.12, 0, 2 * M_PI);
                cairo_stroke(cr);
                break;
            case ROOK:
                cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.1, s * 0.6, s * 0.5);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.5, s * 0.15, s * 0.35);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.05, cy - s * 0.5, s * 0.1, s * 0.35);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx + s * 0.15, cy - s * 0.5, s * 0.15, s * 0.35);
                cairo_stroke(cr);
                break;
            case QUEEN:
                cairo_move_to(cr, cx, cy - s * 0.5);
                cairo_line_to(cr, cx - s * 0.15, cy - s * 0.2);
                cairo_line_to(cr, cx - s * 0.3, cy - s * 0.4);
                cairo_line_to(cr, cx - s * 0.3, cy + s * 0.4);
                cairo_line_to(cr, cx + s * 0.3, cy + s * 0.4);
                cairo_line_to(cr, cx + s * 0.3, cy - s * 0.4);
                cairo_line_to(cr, cx + s * 0.15, cy - s * 0.2);
                cairo_close_path(cr);
                cairo_stroke(cr);
                cairo_arc(cr, cx, cy - s * 0.5, s * 0.1, 0, 2 * M_PI);
                cairo_stroke(cr);
                break;
            case KING:
                cairo_rectangle(cr, cx - s * 0.3, cy - s * 0.1, s * 0.6, s * 0.5);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.05, cy - s * 0.6, s * 0.1, s * 0.5);
                cairo_stroke(cr);
                cairo_rectangle(cr, cx - s * 0.25, cy - s * 0.45, s * 0.5, s * 0.1);
                cairo_stroke(cr);
                break;
            default:
                break;
        }
    }
}
void draw_chess_board(BeatChessVisualization *chess, cairo_t *cr) {
    double cell = chess->cell_size;
    double ox = chess->board_offset_x;
    double oy = chess->board_offset_y;
    
    // Draw board squares
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            // Apply board flip transformation if enabled
            int draw_r = chess->board_flipped ? (BOARD_SIZE - 1 - r) : r;
            int draw_c = chess->board_flipped ? (BOARD_SIZE - 1 - c) : c;
            
            bool is_light = (r + c) % 2 == 0;
            
            if (is_light) {
                cairo_set_source_rgb(cr, 0.9, 0.9, 0.85);
            } else {
                cairo_set_source_rgb(cr, 0.4, 0.5, 0.4);
            }
            
            cairo_rectangle(cr, ox + draw_c * cell, oy + draw_r * cell, cell, cell);
            cairo_fill(cr);
        }
    }
    
    // Draw coordinates (flipped if needed)
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, cell * 0.2);
    
    for (int i = 0; i < 8; i++) {
        char label[2];
        
        if (chess->board_flipped) {
            // Flipped coordinates
            // Files (h-a instead of a-h)
            label[0] = 'h' - i;
            label[1] = '\0';
            cairo_move_to(cr, ox + i * cell + cell * 0.05, oy + 8 * cell - cell * 0.05);
            cairo_show_text(cr, label);
            
            // Ranks (1-8 instead of 8-1)
            label[0] = '1' + i;
            cairo_move_to(cr, ox + cell * 0.05, oy + i * cell + cell * 0.25);
            cairo_show_text(cr, label);
        } else {
            // Normal coordinates
            // Files (a-h)
            label[0] = 'a' + i;
            label[1] = '\0';
            cairo_move_to(cr, ox + i * cell + cell * 0.05, oy + 8 * cell - cell * 0.05);
            cairo_show_text(cr, label);
            
            // Ranks (8-1)
            label[0] = '8' - i;
            cairo_move_to(cr, ox + cell * 0.05, oy + i * cell + cell * 0.25);
            cairo_show_text(cr, label);
        }
    }
}

void draw_chess_last_move_highlight(BeatChessVisualization *chess, cairo_t *cr) {
    if (chess->last_from_row < 0 || chess->last_move_glow <= 0) return;
    
    double cell = chess->cell_size;
    double ox = chess->board_offset_x;
    double oy = chess->board_offset_y;
    
    double alpha = chess->last_move_glow * 0.5;
    
    // Transform coordinates if board is flipped
    int from_row = chess->board_flipped ? (BOARD_SIZE - 1 - chess->last_from_row) : chess->last_from_row;
    int from_col = chess->board_flipped ? (BOARD_SIZE - 1 - chess->last_from_col) : chess->last_from_col;
    int to_row = chess->board_flipped ? (BOARD_SIZE - 1 - chess->last_to_row) : chess->last_to_row;
    int to_col = chess->board_flipped ? (BOARD_SIZE - 1 - chess->last_to_col) : chess->last_to_col;
    
    // Highlight from square
    cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, alpha);
    cairo_rectangle(cr, 
                    ox + from_col * cell, 
                    oy + from_row * cell, 
                    cell, cell);
    cairo_fill(cr);
    
    // Highlight to square
    cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, alpha);
    cairo_rectangle(cr, 
                    ox + to_col * cell, 
                    oy + to_row * cell, 
                    cell, cell);
    cairo_fill(cr);
}

void draw_chess_pieces(BeatChessVisualization *chess, cairo_t *cr) {
    double cell = chess->cell_size;
    double ox = chess->board_offset_x;
    double oy = chess->board_offset_y;
    
    // Draw selection highlight if a piece is selected
    if (chess->has_selected_piece && chess->selected_piece_row >= 0) {
        int sel_r = chess->board_flipped ? (BOARD_SIZE - 1 - chess->selected_piece_row) : chess->selected_piece_row;
        int sel_c = chess->board_flipped ? (BOARD_SIZE - 1 - chess->selected_piece_col) : chess->selected_piece_col;
        
        cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 0.3);  // Cyan highlight
        cairo_rectangle(cr, 
                       ox + sel_c * cell, 
                       oy + sel_r * cell, 
                       cell, cell);
        cairo_fill(cr);
        
        // Border around selected piece
        cairo_set_source_rgb(cr, 0.0, 1.0, 1.0);
        cairo_set_line_width(cr, 3);
        cairo_rectangle(cr, 
                       ox + sel_c * cell, 
                       oy + sel_r * cell, 
                       cell, cell);
        cairo_stroke(cr);
    }
    
    // Get volume level from the parent visualizer structure
    // We need to pass this through from draw_beat_chess
    Visualizer *vis = (Visualizer*)((char*)chess - offsetof(Visualizer, beat_chess));
    double volume = vis->volume_level;
    
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            ChessPiece piece = chess->game.board[r][c];
            
            // Skip if animating this piece
            if (chess->is_animating && 
                r == chess->animating_from_row && 
                c == chess->animating_from_col) {
                continue;
            }
            
            if (piece.type != EMPTY) {
                // Apply board flip transformation
                int draw_r = chess->board_flipped ? (BOARD_SIZE - 1 - r) : r;
                int draw_c = chess->board_flipped ? (BOARD_SIZE - 1 - c) : c;
                
                double x = ox + draw_c * cell;
                double y = oy + draw_r * cell;
                
                // Calculate dance offset based on music volume and position
                double phase = (r * 0.5 + c * 0.3) * 3.14159;  // Different phase for each square
                double time_wave = sin(chess->time_since_last_move * 10.0 + phase);
                // Scale dance by volume - pieces bounce more with louder music
                double dance_amount = time_wave * volume * cell * 0.2;
                
                // Draw shadow - dark for all pieces
                cairo_save(cr);
                cairo_translate(cr, 3, 3);
                cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
                draw_piece(cr, piece.type, piece.color, x, y, cell, dance_amount);
                cairo_restore(cr);
                
                // Draw piece
                draw_piece(cr, piece.type, piece.color, x, y, cell, dance_amount);
            }
        }
    }
    
    // Draw animating piece
    if (chess->is_animating) {
        int fr = chess->animating_from_row;
        int fc = chess->animating_from_col;
        int tr = chess->animating_to_row;
        int tc = chess->animating_to_col;
        
        // Apply board flip transformation
        int draw_fr = chess->board_flipped ? (BOARD_SIZE - 1 - fr) : fr;
        int draw_fc = chess->board_flipped ? (BOARD_SIZE - 1 - fc) : fc;
        int draw_tr = chess->board_flipped ? (BOARD_SIZE - 1 - tr) : tr;
        int draw_tc = chess->board_flipped ? (BOARD_SIZE - 1 - tc) : tc;
        
        ChessPiece piece = chess->game.board[tr][tc];
        
        // Smooth interpolation
        double t = chess->animation_progress;
        t = t * t * (3.0 - 2.0 * t); // Smoothstep
        
        double x = ox + (draw_fc + t * (draw_tc - draw_fc)) * cell;
        double y = oy + (draw_fr + t * (draw_tr - draw_fr)) * cell;
        
        // Animating piece dances even more to the music
        double dance_amount = sin(chess->time_since_last_move * 15.0) * volume * cell * 0.3;
        
        // Draw shadow - dark for all pieces
        cairo_save(cr);
        cairo_translate(cr, 3, 3);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
        draw_piece(cr, piece.type, piece.color, x, y, cell, dance_amount);
        cairo_restore(cr);
        
        // Draw piece with slight glow
        cairo_save(cr);
        if (piece.color == WHITE) {
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.9);
        } else {
            // Brighter gold glow for animating gold pieces
            cairo_set_source_rgb(cr, 0.95, 0.75, 0.2);
        }
        draw_piece(cr, piece.type, piece.color, x, y, cell, dance_amount);
        cairo_restore(cr);
    }
}

void draw_chess_eval_bar(BeatChessVisualization *chess, cairo_t *cr, int width, int height) {
    double bar_width = 30;
    double bar_height = chess->cell_size * 8;
    double bar_x = chess->board_offset_x + chess->cell_size * 8 + 20;
    double bar_y = chess->board_offset_y;
    
    // Background
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_rectangle(cr, bar_x, bar_y, bar_width, bar_height);
    cairo_fill(cr);
    
    // Center line
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, bar_x, bar_y + bar_height / 2);
    cairo_line_to(cr, bar_x + bar_width, bar_y + bar_height / 2);
    cairo_stroke(cr);
    
    // Evaluation position (-1 to 1)
    double eval_pos = chess->eval_bar_position;
    double fill_y = bar_y + bar_height / 2 - (eval_pos * bar_height / 2);
    double fill_height = eval_pos * bar_height / 2;
    
    if (eval_pos > 0) {
        // White advantage
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
        cairo_rectangle(cr, bar_x, fill_y, bar_width, fill_height);
    } else {
        // Black advantage
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_rectangle(cr, bar_x, bar_y + bar_height / 2, bar_width, -fill_height);
    }
    cairo_fill(cr);
    
    // Border
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, bar_x, bar_y, bar_width, bar_height);
    cairo_stroke(cr);
}

void draw_chess_status(BeatChessVisualization *chess, cairo_t *cr, int width, int height) {
    // Status text
    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, chess->status_text, &extents);
    
    double text_x = (width - extents.width) / 2;
    double text_y = chess->board_offset_y - 20;
    
    // Flash background if timer active
    if (chess->status_flash_timer > 0) {
        double alpha = chess->status_flash_timer * 0.3;
        cairo_set_source_rgba(cr, 
                            chess->status_flash_color[0],
                            chess->status_flash_color[1],
                            chess->status_flash_color[2],
                            alpha);
        cairo_rectangle(cr, text_x - 10, text_y - extents.height - 5, 
                       extents.width + 20, extents.height + 10);
        cairo_fill(cr);
    }
    
    // Text
    if (chess->status_flash_timer > 0) {
        cairo_set_source_rgb(cr,
                           chess->status_flash_color[0],
                           chess->status_flash_color[1],
                           chess->status_flash_color[2]);
    } else {
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    }
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, chess->status_text);
    
    // Move counter
    char move_text[64];
    snprintf(move_text, sizeof(move_text), "Move: %d", chess->move_count);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_set_font_size(cr, 14);
    cairo_text_extents(cr, move_text, &extents);
    cairo_move_to(cr, (width - extents.width) / 2, 
                  chess->board_offset_y + chess->cell_size * 8 + 30);
    cairo_show_text(cr, move_text);
    
    // Time display (only in Player vs AI mode)
    if (chess->player_vs_ai) {
        char time_text[256];
        
        // Show current turn's elapsed time prominently
        double current_time = (chess->game.turn == WHITE) ? chess->current_move_start_time : chess->time_thinking;
        const char *current_player = (chess->game.turn == WHITE) ? "Your" : "AI";
        
        snprintf(time_text, sizeof(time_text), "%s turn: %.1fs | Total - White: %.1fs | Black: %.1fs",
                current_player, current_time,
                chess->white_total_time, chess->black_total_time);
        
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);  // Bright yellow for visibility
        cairo_set_font_size(cr, 14);  // Larger font
        cairo_text_extents(cr, time_text, &extents);
        cairo_move_to(cr, (width - extents.width) / 2, 
                      chess->board_offset_y + chess->cell_size * 8 + 55);
        cairo_show_text(cr, time_text);
    }
}

void draw_chess_reset_button(BeatChessVisualization *chess, cairo_t *cr, int width, int height) {
    // Button position and size - LEFT SIDE
    double button_width = 120;
    double button_height = 40;
    double button_x = 20;  // LEFT side, 20px from edge
    double button_y = 20;
    
    // Store button position for hit detection
    chess->reset_button_x = button_x;
    chess->reset_button_y = button_y;
    chess->reset_button_width = button_width;
    chess->reset_button_height = button_height;
    
    // Background
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_rectangle(cr, button_x, button_y, button_width, button_height);
    cairo_fill(cr);
    
    // Glow effect if hovered
    if (chess->reset_button_hovered || chess->reset_button_glow > 0) {
        double glow_alpha = chess->reset_button_glow * 0.5;
        if (chess->reset_button_hovered) glow_alpha = 0.4;
        
        cairo_set_source_rgba(cr, 1.0, 0.7, 0.2, glow_alpha);
        cairo_rectangle(cr, button_x - 3, button_y - 3, button_width + 6, button_height + 6);
        cairo_stroke(cr);
    }
    
    // Border
    cairo_set_source_rgb(cr, chess->reset_button_hovered ? 1.0 : 0.7, 
                         chess->reset_button_hovered ? 0.7 : 0.5, 
                         chess->reset_button_hovered ? 0.2 : 0.3);
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
    
    cairo_set_source_rgb(cr, chess->reset_button_hovered ? 1.0 : 0.9, 
                         chess->reset_button_hovered ? 0.8 : 0.7, 
                         chess->reset_button_hovered ? 0.3 : 0.4);
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, "RESET");
}

void draw_chess_pvsa_button(BeatChessVisualization *chess, cairo_t *cr, int width, int height) {
    // Button position and size - LEFT SIDE, below RESET button
    double button_width = 120;
    double button_height = 40;
    double button_x = 20;  // LEFT side, same as RESET
    double button_y = 70;  // Below RESET (20 + 40 + 10 spacing)
    
    // Store button position for hit detection
    chess->pvsa_button_x = button_x;
    chess->pvsa_button_y = button_y;
    chess->pvsa_button_width = button_width;
    chess->pvsa_button_height = button_height;
    
    // Background
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_rectangle(cr, button_x, button_y, button_width, button_height);
    cairo_fill(cr);
    
    // Glow effect if hovered
    if (chess->pvsa_button_hovered || chess->pvsa_button_glow > 0) {
        double glow_alpha = chess->pvsa_button_glow * 0.5;
        if (chess->pvsa_button_hovered) glow_alpha = 0.4;
        
        cairo_set_source_rgba(cr, 1.0, 0.7, 0.2, glow_alpha);
        cairo_rectangle(cr, button_x - 3, button_y - 3, button_width + 6, button_height + 6);
        cairo_stroke(cr);
    }
    
    // Border
    cairo_set_source_rgb(cr, chess->pvsa_button_hovered ? 1.0 : 0.7, 
                         chess->pvsa_button_hovered ? 0.7 : 0.5, 
                         chess->pvsa_button_hovered ? 0.2 : 0.3);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, button_x, button_y, button_width, button_height);
    cairo_stroke(cr);
    
    // Text - show current mode
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12);
    
    const char *button_text = chess->player_vs_ai ? "P vs AI" : "AI vs AI";
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, button_text, &extents);
    
    double text_x = button_x + (button_width - extents.width) / 2;
    double text_y = button_y + (button_height + extents.height) / 2;
    
    cairo_set_source_rgb(cr, chess->pvsa_button_hovered ? 1.0 : 0.9, 
                         chess->pvsa_button_hovered ? 0.8 : 0.7, 
                         chess->pvsa_button_hovered ? 0.3 : 0.4);
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, button_text);
}

void draw_chess_flip_button(BeatChessVisualization *chess, cairo_t *cr, int width, int height) {
    // Only show flip button in Player vs AI mode
    if (!chess->player_vs_ai) return;
    
    // Button position and size - LEFT SIDE, below PvsA button
    double button_width = 120;
    double button_height = 40;
    double button_x = 20;  // LEFT side, same as other buttons
    double button_y = 120;  // Below PvsA button (70 + 40 + 10 spacing)
    
    // Store button position for hit detection
    chess->flip_button_x = button_x;
    chess->flip_button_y = button_y;
    chess->flip_button_width = button_width;
    chess->flip_button_height = button_height;
    
    // Background
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_rectangle(cr, button_x, button_y, button_width, button_height);
    cairo_fill(cr);
    
    // Glow effect if hovered
    if (chess->flip_button_hovered || chess->flip_button_glow > 0) {
        double glow_alpha = chess->flip_button_glow * 0.5;
        if (chess->flip_button_hovered) glow_alpha = 0.4;
        
        cairo_set_source_rgba(cr, 0.2, 0.7, 1.0, glow_alpha);  // Blue glow
        cairo_rectangle(cr, button_x - 3, button_y - 3, button_width + 6, button_height + 6);
        cairo_stroke(cr);
    }
    
    // Border - highlight if board is flipped
    cairo_set_source_rgb(cr, chess->flip_button_hovered ? 0.3 : (chess->board_flipped ? 0.4 : 0.5), 
                         chess->flip_button_hovered ? 0.9 : (chess->board_flipped ? 0.9 : 0.7), 
                         chess->flip_button_hovered ? 1.0 : (chess->board_flipped ? 1.0 : 0.6));
    cairo_set_line_width(cr, chess->board_flipped ? 3 : 2);
    cairo_rectangle(cr, button_x, button_y, button_width, button_height);
    cairo_stroke(cr);
    
    // Text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12);
    
    const char *button_text = "FLIP BOARD";
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, button_text, &extents);
    
    double text_x = button_x + (button_width - extents.width) / 2;
    double text_y = button_y + (button_height + extents.height) / 2;
    
    cairo_set_source_rgb(cr, chess->flip_button_hovered ? 0.3 : (chess->board_flipped ? 0.4 : 0.8), 
                         chess->flip_button_hovered ? 0.9 : (chess->board_flipped ? 0.9 : 0.6), 
                         chess->flip_button_hovered ? 1.0 : (chess->board_flipped ? 1.0 : 0.2));
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, button_text);
}


void draw_chess_undo_button(BeatChessVisualization *chess, cairo_t *cr, int width, int height) {
    // Only show undo button in Player vs AI mode
    if (!chess->player_vs_ai) return;
    
    // Button position and size - LEFT SIDE, below FLIP button
    double button_width = 120;
    double button_height = 40;
    double button_x = 20;  // LEFT side, same as other buttons
    double button_y = 170;  // Below FLIP button (120 + 40 + 10 spacing)
    
    // Store button position for hit detection
    chess->undo_button_x = button_x;
    chess->undo_button_y = button_y;
    chess->undo_button_width = button_width;
    chess->undo_button_height = button_height;
    
    // Disable button if can't undo
    bool can_undo = chess_can_undo(chess);
    
    // Background
    cairo_set_source_rgb(cr, can_undo ? 0.15 : 0.08, can_undo ? 0.15 : 0.08, can_undo ? 0.15 : 0.08);
    cairo_rectangle(cr, button_x, button_y, button_width, button_height);
    cairo_fill(cr);
    
    // Glow effect if hovered and enabled
    if ((chess->undo_button_hovered || chess->undo_button_glow > 0) && can_undo) {
        double glow_alpha = chess->undo_button_glow * 0.5;
        if (chess->undo_button_hovered) glow_alpha = 0.4;
        
        cairo_set_source_rgba(cr, 1.0, 0.4, 0.2, glow_alpha);
        cairo_rectangle(cr, button_x - 3, button_y - 3, button_width + 6, button_height + 6);
        cairo_stroke(cr);
    }
    
    // Border
    if (can_undo) {
        cairo_set_source_rgb(cr, chess->undo_button_hovered ? 1.0 : 0.6, 
                             chess->undo_button_hovered ? 0.4 : 0.3, 
                             chess->undo_button_hovered ? 0.2 : 0.2);
    } else {
        cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    }
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, button_x, button_y, button_width, button_height);
    cairo_stroke(cr);
    
    // Text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, "UNDO", &extents);
    
    double text_x = button_x + (button_width - extents.width) / 2;
    double text_y = button_y + (button_height + extents.height) / 2;
    
    if (can_undo) {
        cairo_set_source_rgb(cr, chess->undo_button_hovered ? 1.0 : 0.8, 
                             chess->undo_button_hovered ? 0.6 : 0.4, 
                             chess->undo_button_hovered ? 0.3 : 0.2);
    } else {
        cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
    }
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, "UNDO");
}

void draw_beat_chess(void *vis_ptr, cairo_t *cr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    BeatChessVisualization *chess = &vis->beat_chess;
    
    // Calculate board layout
    int width = vis->width;
    int height = vis->height;
    
    double available_width = width * 0.8;
    double available_height = height * 0.8;
    
    chess->cell_size = fmin(available_width / 8, available_height / 8);
    chess->board_offset_x = (width - chess->cell_size * 8) / 2;
    chess->board_offset_y = (height - chess->cell_size * 8) / 2;
    
    // Draw components
    draw_chess_board(chess, cr);
    draw_chess_last_move_highlight(chess, cr);
    draw_chess_pieces(chess, cr);
    draw_chess_eval_bar(chess, cr, width, height);
    draw_chess_status(chess, cr, width, height);
    // Draw buttons
    draw_chess_reset_button(chess, cr, width, height);
    draw_chess_pvsa_button(chess, cr, width, height);
    draw_chess_flip_button(chess, cr, width, height);
    draw_chess_undo_button(chess, cr, width, height);
}

void chess_cleanup_thinking_state(ChessThinkingState *ts) {
    // Stop thinking
    pthread_mutex_lock(&ts->lock);
    ts->thinking = false;
    pthread_mutex_unlock(&ts->lock);
    
    // Cancel and wait for thread to finish
    pthread_cancel(ts->thread);
    pthread_join(ts->thread, NULL);
    
    // Destroy mutex
    pthread_mutex_destroy(&ts->lock);
}
