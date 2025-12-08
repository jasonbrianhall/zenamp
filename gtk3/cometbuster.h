#ifndef COMETBUSTER_H
#define COMETBUSTER_H

#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <cairo.h>

// Static memory allocation constants
#define MAX_COMETS 32
#define MAX_BULLETS 128
#define MAX_PARTICLES 512
#define MAX_FLOATING_TEXT 32
#define MAX_HIGH_SCORES 10

typedef enum {
    COMET_SMALL = 0,
    COMET_MEDIUM = 1,
    COMET_LARGE = 2,
    COMET_SPECIAL = 3,
    COMET_MEGA = 4
} CometSize;

typedef struct {
    double x, y;                // Position
    double vx, vy;              // Velocity
    double radius;
    CometSize size;
    int frequency_band;         // 0=bass, 1=mid, 2=treble
    double rotation;            // For rotating visual (degrees)
    double rotation_speed;      // degrees per second
    double base_angle;          // Base rotation angle (radians) for vector asteroids
    double color[3];            // RGB
    bool active;
    int health;                 // For special comets
} Comet;

typedef struct {
    double x, y;                // Position
    double vx, vy;              // Velocity
    double angle;               // Direction
    double lifetime;            // Seconds remaining
    double max_lifetime;
    bool active;
} Bullet;

typedef struct {
    double x, y;                // Position
    double vx, vy;              // Velocity
    double lifetime;            // Seconds remaining
    double max_lifetime;
    double size;                // Radius
    double color[3];            // RGB
    bool active;
} Particle;

typedef struct {
    double x, y;                // Position
    double lifetime;            // Seconds remaining
    double max_lifetime;
    char text[64];              // Text to display
    double color[3];            // RGB
    bool active;
} FloatingText;

typedef struct {
    int score;
    int wave;
    char player_name[32];       // Static string buffer
    time_t timestamp;
} HighScore;

// Keyboard input state for arcade-style controls
typedef struct {
    bool key_a_pressed;         // A key - turn left
    bool key_d_pressed;         // D key - turn right
    bool key_w_pressed;         // W key - forward thrust
    bool key_s_pressed;         // S key - backward thrust
    bool key_z_pressed;         // Z key - omnidirectional fire
    bool key_x_pressed;         // X key - boost
    bool key_space_pressed;     // SPACE key - boost
    bool key_ctrl_pressed;      // CTRL key - fire
} KeyboardInput;

typedef struct {
    double x, y;                // Position
    double vx, vy;              // Velocity
    double angle;               // Direction facing
    int health;                 // 1 hit = destroyed
    double shoot_cooldown;      // Time until next shot
    double path_time;           // Time along sine wave path (for wave motion)
    double base_vx, base_vy;    // Original velocity direction (for sine calculation)
    int ship_type;              // 0 = patrol (blue), 1 = aggressive (red), 2 = hunter (green)
    bool active;
    
    // Shield system for enemy ships
    int shield_health;          // Current shield points
    int max_shield_health;      // Maximum shield points (varies by ship type)
    double shield_impact_timer; // Visual impact effect timer
    double shield_impact_angle; // Angle of shield impact
} EnemyShip;

#define MAX_ENEMY_SHIPS 4
#define MAX_ENEMY_BULLETS 64

// Boss (Death Star) structure
typedef struct {
    double x, y;                // Position
    double vx, vy;              // Velocity
    double angle;               // Direction facing
    int health;                 // Boss health (large number like 50-100)
    int max_health;             // Maximum health
    double shoot_cooldown;      // Time until next shot
    int phase;                  // 0 = normal, 1 = shield up, 2 = enraged
    double phase_timer;         // Time in current phase
    double phase_duration;      // How long until phase changes
    
    // Shield system
    int shield_health;          // Shield durability (10-20 points)
    int max_shield_health;      // Maximum shield durability
    bool shield_active;         // Is shield currently up?
    double shield_impact_timer; // Visual effect
    double shield_impact_angle; // Where shield was hit
    
    // Firing pattern
    double fire_pattern_timer;  // For special firing patterns
    int fire_pattern;           // Different firing modes
    
    // Visual effects
    double rotation;            // Visual rotation
    double rotation_speed;
    double damage_flash_timer;  // Flash when taking damage
    
    bool active;                // Is the boss alive?
} BossShip;

typedef struct {
    // Ship state
    double ship_x, ship_y;
    double ship_vx, ship_vy;
    double ship_angle;          // Radians
    double ship_speed;          // Current velocity magnitude
    double ship_rotation_angle; // Target angle toward mouse
    int ship_lives;
    double invulnerability_time; // Seconds of invincibility after being hit
    
    // Shield system
    int shield_health;          // Current shield points (0 means no shield)
    int max_shield_health;      // Maximum shield capacity
    double shield_regen_timer;  // Timer for shield regeneration
    double shield_regen_delay;  // Delay before shield starts regenerating after hit
    double shield_regen_rate;   // Health points per second when regenerating
    double shield_impact_angle; // Angle where shield was hit (for visual effect)
    double shield_impact_timer; // Timer for impact flash effect
    
    // Game state
    int score;
    int comets_destroyed;
    double score_multiplier;    // Current multiplier (1.0 - 5.0+)
    int consecutive_hits;       // Hits without being damage for multiplier
    int current_wave;
    int wave_comets;            // Comets destroyed this wave
    int last_life_milestone;    // Last score milestone where extra life was granted (5000, 10000, etc.)
    bool game_over;
    bool game_won;              // Optional: wave complete
    
    // Arrays
    Comet comets[MAX_COMETS];
    int comet_count;
    Bullet bullets[MAX_BULLETS];
    int bullet_count;
    Particle particles[MAX_PARTICLES];
    int particle_count;
    FloatingText floating_texts[MAX_FLOATING_TEXT];
    int floating_text_count;
    EnemyShip enemy_ships[MAX_ENEMY_SHIPS];
    int enemy_ship_count;
    Bullet enemy_bullets[MAX_ENEMY_BULLETS];
    int enemy_bullet_count;
    
    // Boss (Death Star) - appears in wave 5+
    BossShip boss;
    bool boss_active;
    double boss_spawn_timer;
    
    // Timing & difficulty
    double spawn_timer;         // Seconds until next spawn
    double base_spawn_rate;     // Seconds between spawns
    double beat_fire_cooldown;  // Cooldown on beat fire
    double last_beat_time;
    bool auto_fire_enabled;
    double difficulty_timer;    // Tracks when to increase difficulty
    double enemy_ship_spawn_timer;  // Time until next enemy ship spawn
    double enemy_ship_spawn_rate;   // Base rate for enemy ship spawning
    
    // Audio data (updated each frame from visualizer)
    double frequency_bands[3];  // Bass, Mid, Treble [0.0-1.0]
    double volume_level;        // Current volume [0.0-1.0]
    
    // UI & timers
    bool show_game_over;
    double game_over_timer;
    double wave_complete_timer;
    
    // Muzzle flash animation
    double muzzle_flash_timer;
    
    // Mouse input for shooting
    bool mouse_left_pressed;       // Left mouse button state
    double mouse_fire_cooldown;    // Cooldown between shots
    
    // Advanced thrusters (right-click)
    bool mouse_right_pressed;      // Right mouse button state
    bool mouse_middle_pressed;     // Middle mouse button state (omnidirectional fire)
    double omni_fire_cooldown;     // Cooldown for omnidirectional fire
    double energy_amount;          // Current energy [0.0 - 100.0]
    double max_energy;             // Maximum energy capacity
    double energy_burn_rate;        // Energy burned per second at max thrust
    double energy_recharge_rate;    // Energy recharged per second when not boosting
    double boost_multiplier;       // Speed multiplier when boosting (e.g., 2.0x)
    bool is_boosting;              // Currently using advanced thrusters
    double boost_thrust_timer;     // Visual effect timer for boosting
    
    // High scores (static array)
    HighScore high_scores[MAX_HIGH_SCORES];
    int high_score_count;
    
    // Keyboard input (WASD movement)
    KeyboardInput keyboard;
    
} CometBusterGame;

// Initialization and cleanup
void init_comet_buster_system(void *vis);
void comet_buster_cleanup(CometBusterGame *game);
void comet_buster_reset_game(CometBusterGame *game);

// Main update
void update_comet_buster(void *vis, double dt);

// Update sub-systems
void comet_buster_update_ship(CometBusterGame *game, double dt, int mouse_x, int mouse_y, int width, int height, bool mouse_active);
void comet_buster_update_comets(CometBusterGame *game, double dt, int width, int height);
void comet_buster_update_shooting(CometBusterGame *game, double dt);  // New: click-to-shoot
void comet_buster_update_bullets(CometBusterGame *game, double dt, int width, int height, void *vis);
void comet_buster_update_particles(CometBusterGame *game, double dt);
void comet_buster_update_floating_text(CometBusterGame *game, double dt);
void comet_buster_update_fuel(CometBusterGame *game, double dt);  // Advanced thrusters fuel system
void comet_buster_update_enemy_ships(CometBusterGame *game, double dt, int width, int height);
void comet_buster_update_enemy_bullets(CometBusterGame *game, double dt, int width, int height, void *vis);

// Spawning
void comet_buster_spawn_comet(CometBusterGame *game, int frequency_band, int screen_width, int screen_height);
void comet_buster_spawn_random_comets(CometBusterGame *game, int count, int screen_width, int screen_height);
void comet_buster_spawn_wave(CometBusterGame *game, int screen_width, int screen_height);
int comet_buster_get_wave_comet_count(int wave);
double comet_buster_get_wave_speed_multiplier(int wave);
void comet_buster_spawn_bullet(CometBusterGame *game);
void comet_buster_spawn_omnidirectional_fire(CometBusterGame *game);
void comet_buster_spawn_explosion(CometBusterGame *game, double x, double y, int frequency_band, int particle_count);
void comet_buster_spawn_floating_text(CometBusterGame *game, double x, double y, const char *text, double r, double g, double b);
void comet_buster_spawn_enemy_ship(CometBusterGame *game, int screen_width, int screen_height);
void comet_buster_spawn_enemy_bullet(CometBusterGame *game, double x, double y, double vx, double vy);

// Boss functions
void comet_buster_spawn_boss(CometBusterGame *game, int screen_width, int screen_height);
void comet_buster_update_boss(CometBusterGame *game, double dt, int width, int height);
void comet_buster_boss_fire(CometBusterGame *game);
bool comet_buster_check_bullet_boss(Bullet *b, BossShip *boss);
void comet_buster_destroy_boss(CometBusterGame *game, int width, int height, void *vis);
void draw_comet_buster_boss(BossShip *boss, cairo_t *cr, int width, int height);

bool comet_buster_check_bullet_comet(Bullet *b, Comet *c);
bool comet_buster_check_ship_comet(CometBusterGame *game, Comet *c);
void comet_buster_handle_comet_collision(Comet *c1, Comet *c2, double dx, double dy, 
                                         double dist, double min_dist);
void comet_buster_destroy_comet(CometBusterGame *game, int comet_index, int width, int height, void *vis);
bool comet_buster_check_bullet_enemy_ship(Bullet *b, EnemyShip *e);
bool comet_buster_check_enemy_bullet_ship(CometBusterGame *game, Bullet *b);
void comet_buster_destroy_enemy_ship(CometBusterGame *game, int ship_index, int width, int height, void *vis);
bool comet_buster_hit_enemy_ship_provoke(CometBusterGame *game, int ship_index);  // New: provoke blue ships

// Audio integration
void comet_buster_fire_on_beat(CometBusterGame *game);
bool comet_buster_detect_beat(void *vis);

// Difficulty management
void comet_buster_increase_difficulty(CometBusterGame *game);
void comet_buster_update_wave_progression(CometBusterGame *game);

// Rendering
void draw_comet_buster(void *vis, cairo_t *cr);
void draw_comet_buster_ship(CometBusterGame *game, cairo_t *cr, int width, int height);
void draw_comet_buster_comets(CometBusterGame *game, cairo_t *cr, int width, int height);
void draw_comet_buster_bullets(CometBusterGame *game, cairo_t *cr, int width, int height);
void draw_comet_buster_particles(CometBusterGame *game, cairo_t *cr, int width, int height);
void draw_comet_buster_hud(CometBusterGame *game, cairo_t *cr, int width, int height);
void draw_comet_buster_game_over(CometBusterGame *game, cairo_t *cr, int width, int height);
void draw_comet_buster_enemy_ships(CometBusterGame *game, cairo_t *cr, int width, int height);
void draw_comet_buster_enemy_bullets(CometBusterGame *game, cairo_t *cr, int width, int height);

// Helper functions
void comet_buster_wrap_position(double *x, double *y, int width, int height);
double comet_buster_distance(double x1, double y1, double x2, double y2);
void comet_buster_get_frequency_color(int frequency_band, double *r, double *g, double *b);

// High score management
void comet_buster_load_high_scores(CometBusterGame *game);
void comet_buster_save_high_scores(CometBusterGame *game);
void comet_buster_add_high_score(CometBusterGame *game, int score, int wave, const char *name);
bool comet_buster_is_high_score(CometBusterGame *game, int score);

#endif // COMETBUSTER_H
