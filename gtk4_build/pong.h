#ifndef PONG_H
#define PONG_H

#include <gtk/gtk.h>
#include <cairo.h>
#define PONG_BALL_SIZE 8.0
#define PONG_PADDLE_WIDTH 15.0
#define PONG_PADDLE_HEIGHT 120.0
#define PONG_PADDLE_SPEED 800.0
#define PONG_BALL_SPEED 600.0
#define PONG_BALL_SPEED_INCREMENT 15.0  // Speed increase per paddle hit
#define PONG_MAX_BALL_SPEED 1200.0        // Cap ball speed

typedef struct {
    double x, y;
    double vx, vy;
    double size;
    double current_speed;              // Track current speed for caps
    int hit_count;                     // Number of paddle hits
    double beat_glow;                  // Glow from beat detection
    double beat_color_hue;             // Color based on beat intensity
    double base_size;                  // Original size before scaling
} PongBall;

typedef struct {
    double x, y;
    double width, height;
    int score;
    double target_y;
    double glow;
} PongPaddle;

typedef struct {
    PongBall ball;
    PongPaddle player;
    PongPaddle ai;
    int width, height;
    int last_width, last_height;       // Track last known dimensions
    double game_time;
    double reset_timer;
    int ai_difficulty;
    double last_beat_time;             // For beat detection
    gboolean game_over;                // Game over state
    int winner;                        // 0 = player, 1 = ai, -1 = none
    double game_over_display_time;     // How long to show win message
} PongGame;

#endif
