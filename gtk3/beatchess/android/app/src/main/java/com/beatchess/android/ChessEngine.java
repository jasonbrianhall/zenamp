package com.beatchess.android;

public class ChessEngine {
    // Load native library
    static {
        System.loadLibrary("beatchess_jni");
    }
    
    // Piece types
    public static final int EMPTY = 0;
    public static final int PAWN = 1;
    public static final int KNIGHT = 2;
    public static final int BISHOP = 3;
    public static final int ROOK = 4;
    public static final int QUEEN = 5;
    public static final int KING = 6;
    
    // Colors
    public static final int NONE = 0;
    public static final int WHITE = 1;
    public static final int BLACK = 2;
    
    // Game status
    public static final int CHESS_PLAYING = 0;
    public static final int CHESS_CHECKMATE_WHITE = 1;
    public static final int CHESS_CHECKMATE_BLACK = 2;
    public static final int CHESS_STALEMATE = 3;
    
    // Native methods - Game State
    public native void nativeInitialize();
    public native void nativeResetGame();
    public native int[] nativeGetBoardState();
    public native boolean nativeIsValidMove(int fromRow, int fromCol, int toRow, int toCol);
    public native boolean nativeMakeMove(int fromRow, int fromCol, int toRow, int toCol);
    public native int nativeGetGameStatus();
    public native int nativeGetCurrentTurn();
    public native int[] nativeGetAIMove();
    public native boolean nativeIsSquareUnderAttack(int row, int col, int attackerColor);
    public native int nativeGetMoveCount();
    public native boolean nativeIsInCheck(int color);
    public native void nativeCleanup();
    
    // Native methods - Cairo Rendering
    public native void nativeSetNativeWindow(android.view.Surface surface);
    public native void nativeRender(int width, int height, double deltaTime);
    public native void nativeHandleTouch(int x, int y, int width, int height);
    
    // Utility method to decode piece type from board state
    public static int getPieceType(int encoded) {
        return encoded & 0xFF;
    }
    
    // Utility method to decode piece color from board state
    public static int getPieceColor(int encoded) {
        return (encoded >> 8) & 0xFF;
    }
    
    // Get piece name for display
    public static String getPieceName(int type) {
        switch (type) {
            case PAWN: return "P";
            case KNIGHT: return "N";
            case BISHOP: return "B";
            case ROOK: return "R";
            case QUEEN: return "Q";
            case KING: return "K";
            default: return "";
        }
    }
}
