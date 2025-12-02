#include <jni.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <cairo.h>
#include <cairo-android.h>
#include <string.h>
#include <pthread.h>
#include "beatchess.h"

#define LOG_TAG "BeatChessJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global game context
typedef struct {
    ChessGameState game;
    ChessThinkingState thinking_state;
    ChessGameStatus status;
    jint move_count;
    
    // Cairo/rendering context
    cairo_surface_t *surface;
    cairo_t *cr;
    ANativeWindow *window;
    
    // Animation/visualization state
    BeatChessVisualization beat_chess;
    
    pthread_mutex_t lock;
} JNIChessContext;

static JNIChessContext g_chess_context;
static int g_initialized = 0;

// ============================================================================
// Core Initialization/Cleanup
// ============================================================================

JNIEXPORT void JNICALL Java_com_beatchess_android_ChessEngine_nativeInitialize(
        JNIEnv *env, jobject thiz) {
    
    if (g_initialized) return;
    
    pthread_mutex_init(&g_chess_context.lock, NULL);
    
    // Initialize game state
    chess_init_board(&g_chess_context.game);
    chess_init_thinking_state(&g_chess_context.thinking_state);
    
    g_chess_context.status = CHESS_PLAYING;
    g_chess_context.move_count = 0;
    
    // Initialize visualization
    memset(&g_chess_context.beat_chess, 0, sizeof(BeatChessVisualization));
    g_chess_context.beat_chess.game = g_chess_context.game;
    g_chess_context.beat_chess.player_vs_ai = true;
    g_chess_context.beat_chess.auto_play_enabled = true;
    
    chess_start_thinking(&g_chess_context.thinking_state, &g_chess_context.game);
    
    g_initialized = 1;
    LOGI("Chess engine initialized with Cairo rendering support");
}

JNIEXPORT void JNICALL Java_com_beatchess_android_ChessEngine_nativeSetNativeWindow(
        JNIEnv *env, jobject thiz, jobject surface) {
    
    pthread_mutex_lock(&g_chess_context.lock);
    
    // Clean up old surface if exists
    if (g_chess_context.cr) {
        cairo_destroy(g_chess_context.cr);
    }
    if (g_chess_context.surface) {
        cairo_surface_destroy(g_chess_context.surface);
    }
    if (g_chess_context.window) {
        ANativeWindow_release(g_chess_context.window);
    }
    
    // Get native window from Java Surface object
    g_chess_context.window = ANativeWindow_fromSurface(env, surface);
    
    if (g_chess_context.window) {
        // Create Cairo surface from Android native window
        g_chess_context.surface = cairo_android_surface_create_for_window(g_chess_context.window);
        
        if (g_chess_context.surface) {
            g_chess_context.cr = cairo_create(g_chess_context.surface);
            LOGI("Cairo surface created for native window");
        } else {
            LOGE("Failed to create Cairo surface");
        }
    } else {
        LOGE("Failed to get native window from Surface");
    }
    
    pthread_mutex_unlock(&g_chess_context.lock);
}

JNIEXPORT void JNICALL Java_com_beatchess_android_ChessEngine_nativeRender(
        JNIEnv *env, jobject thiz, jint width, jint height, jdouble dt) {
    
    pthread_mutex_lock(&g_chess_context.lock);
    
    if (!g_chess_context.cr) {
        pthread_mutex_unlock(&g_chess_context.lock);
        return;
    }
    
    // Update game state for animation/visualization
    Visualizer vis;
    vis.width = width;
    vis.height = height;
    vis.beat_chess = g_chess_context.beat_chess;
    
    // Update beat chess visualization with delta time
    update_beat_chess(&vis, dt);
    g_chess_context.beat_chess = vis.beat_chess;
    
    // Draw using Cairo (your original drawing functions)
    draw_beat_chess(&vis, g_chess_context.cr);
    
    // Flush Cairo drawing to surface
    cairo_surface_flush(g_chess_context.surface);
    
    pthread_mutex_unlock(&g_chess_context.lock);
}

JNIEXPORT void JNICALL Java_com_beatchess_android_ChessEngine_nativeResetGame(
        JNIEnv *env, jobject thiz) {
    
    pthread_mutex_lock(&g_chess_context.lock);
    
    chess_stop_thinking(&g_chess_context.thinking_state);
    chess_init_board(&g_chess_context.game);
    g_chess_context.status = CHESS_PLAYING;
    g_chess_context.move_count = 0;
    
    g_chess_context.beat_chess.game = g_chess_context.game;
    g_chess_context.beat_chess.status = CHESS_PLAYING;
    
    chess_start_thinking(&g_chess_context.thinking_state, &g_chess_context.game);
    
    pthread_mutex_unlock(&g_chess_context.lock);
}

JNIEXPORT void JNICALL Java_com_beatchess_android_ChessEngine_nativeCleanup(
        JNIEnv *env, jobject thiz) {
    
    if (!g_initialized) return;
    
    pthread_mutex_lock(&g_chess_context.lock);
    
    chess_stop_thinking(&g_chess_context.thinking_state);
    
    if (g_chess_context.cr) {
        cairo_destroy(g_chess_context.cr);
        g_chess_context.cr = NULL;
    }
    if (g_chess_context.surface) {
        cairo_surface_destroy(g_chess_context.surface);
        g_chess_context.surface = NULL;
    }
    if (g_chess_context.window) {
        ANativeWindow_release(g_chess_context.window);
        g_chess_context.window = NULL;
    }
    
    pthread_mutex_unlock(&g_chess_context.lock);
    pthread_mutex_destroy(&g_chess_context.lock);
    
    g_initialized = 0;
    LOGI("Chess engine cleaned up");
}

// ============================================================================
// Game State Access
// ============================================================================

JNIEXPORT jintArray JNICALL Java_com_beatchess_android_ChessEngine_nativeGetBoardState(
        JNIEnv *env, jobject thiz) {
    
    jintArray result = (*env)->NewIntArray(env, 64);
    jint board_array[64];
    
    pthread_mutex_lock(&g_chess_context.lock);
    
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int idx = r * 8 + c;
            ChessPiece piece = g_chess_context.game.board[r][c];
            board_array[idx] = (piece.type) | (piece.color << 8);
        }
    }
    
    pthread_mutex_unlock(&g_chess_context.lock);
    
    (*env)->SetIntArrayRegion(env, result, 0, 64, board_array);
    return result;
}

JNIEXPORT jboolean JNICALL Java_com_beatchess_android_ChessEngine_nativeIsValidMove(
        JNIEnv *env, jobject thiz, jint from_row, jint from_col, 
        jint to_row, jint to_col) {
    
    pthread_mutex_lock(&g_chess_context.lock);
    jboolean result = chess_is_valid_move(&g_chess_context.game, 
                                         from_row, from_col, to_row, to_col);
    pthread_mutex_unlock(&g_chess_context.lock);
    
    return result;
}

JNIEXPORT jboolean JNICALL Java_com_beatchess_android_ChessEngine_nativeMakeMove(
        JNIEnv *env, jobject thiz, jint from_row, jint from_col,
        jint to_row, jint to_col) {
    
    pthread_mutex_lock(&g_chess_context.lock);
    
    if (!chess_is_valid_move(&g_chess_context.game, from_row, from_col, to_row, to_col)) {
        pthread_mutex_unlock(&g_chess_context.lock);
        return JNI_FALSE;
    }
    
    ChessGameState temp = g_chess_context.game;
    ChessMove move = {from_row, from_col, to_row, to_col, 0};
    chess_make_move(&temp, move);
    
    if (chess_is_in_check(&temp, g_chess_context.game.turn)) {
        pthread_mutex_unlock(&g_chess_context.lock);
        return JNI_FALSE;
    }
    
    // Execute move
    chess_make_move(&g_chess_context.game, move);
    g_chess_context.move_count++;
    g_chess_context.status = chess_check_game_status(&g_chess_context.game);
    
    // Update visualization state
    g_chess_context.beat_chess.game = g_chess_context.game;
    g_chess_context.beat_chess.last_from_row = from_row;
    g_chess_context.beat_chess.last_from_col = from_col;
    g_chess_context.beat_chess.last_to_row = to_row;
    g_chess_context.beat_chess.last_to_col = to_col;
    
    if (g_chess_context.status == CHESS_PLAYING) {
        chess_start_thinking(&g_chess_context.thinking_state, &g_chess_context.game);
    }
    
    pthread_mutex_unlock(&g_chess_context.lock);
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_beatchess_android_ChessEngine_nativeGetGameStatus(
        JNIEnv *env, jobject thiz) {
    
    pthread_mutex_lock(&g_chess_context.lock);
    jint status = g_chess_context.status;
    pthread_mutex_unlock(&g_chess_context.lock);
    
    return status;
}

JNIEXPORT jint JNICALL Java_com_beatchess_android_ChessEngine_nativeGetCurrentTurn(
        JNIEnv *env, jobject thiz) {
    
    pthread_mutex_lock(&g_chess_context.lock);
    jint turn = g_chess_context.game.turn;
    pthread_mutex_unlock(&g_chess_context.lock);
    
    return turn;
}

JNIEXPORT jint JNICALL Java_com_beatchess_android_ChessEngine_nativeGetMoveCount(
        JNIEnv *env, jobject thiz) {
    
    pthread_mutex_lock(&g_chess_context.lock);
    jint count = g_chess_context.move_count;
    pthread_mutex_unlock(&g_chess_context.lock);
    
    return count;
}

JNIEXPORT jboolean JNICALL Java_com_beatchess_android_ChessEngine_nativeIsInCheck(
        JNIEnv *env, jobject thiz, jint color) {
    
    pthread_mutex_lock(&g_chess_context.lock);
    jboolean result = chess_is_in_check(&g_chess_context.game, (ChessColor)color);
    pthread_mutex_unlock(&g_chess_context.lock);
    
    return result;
}

// ============================================================================
// AI Move
// ============================================================================

JNIEXPORT jintArray JNICALL Java_com_beatchess_android_ChessEngine_nativeGetAIMove(
        JNIEnv *env, jobject thiz) {
    
    jintArray result = (*env)->NewIntArray(env, 4);
    jint move_data[4] = {-1, -1, -1, -1};
    
    pthread_mutex_lock(&g_chess_context.lock);
    
    ChessMove ai_move = chess_get_best_move_now(&g_chess_context.thinking_state);
    
    if (chess_is_valid_move(&g_chess_context.game, ai_move.from_row, ai_move.from_col,
                            ai_move.to_row, ai_move.to_col)) {
        ChessGameState temp = g_chess_context.game;
        chess_make_move(&temp, ai_move);
        
        if (!chess_is_in_check(&temp, g_chess_context.game.turn)) {
            move_data[0] = ai_move.from_row;
            move_data[1] = ai_move.from_col;
            move_data[2] = ai_move.to_row;
            move_data[3] = ai_move.to_col;
            
            chess_make_move(&g_chess_context.game, ai_move);
            g_chess_context.move_count++;
            g_chess_context.status = chess_check_game_status(&g_chess_context.game);
            
            g_chess_context.beat_chess.game = g_chess_context.game;
            g_chess_context.beat_chess.last_from_row = ai_move.from_row;
            g_chess_context.beat_chess.last_from_col = ai_move.from_col;
            g_chess_context.beat_chess.last_to_row = ai_move.to_row;
            g_chess_context.beat_chess.last_to_col = ai_move.to_col;
            
            if (g_chess_context.status == CHESS_PLAYING) {
                chess_start_thinking(&g_chess_context.thinking_state, &g_chess_context.game);
            }
        }
    }
    
    pthread_mutex_unlock(&g_chess_context.lock);
    
    (*env)->SetIntArrayRegion(env, result, 0, 4, move_data);
    return result;
}
