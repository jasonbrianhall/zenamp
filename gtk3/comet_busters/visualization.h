#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include <stdbool.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include "cometbuster.h"
#include "audio_wad.h"

// ============================================================
// JOYSTICK SUPPORT STRUCTURES
// ============================================================

typedef struct {
    int device_id;
    char name[128];
    bool connected;
    
    // Analog axes (normalized to -1.0 to 1.0)
    double axis_x;          // Left stick X
    double axis_y;          // Left stick Y
    double axis_rx;         // Right stick X (if available)
    double axis_ry;         // Right stick Y (if available)
    double axis_lt;         // Left trigger (0.0 to 1.0)
    double axis_rt;         // Right trigger (0.0 to 1.0)
    
    // Digital buttons
    bool button_a;          // A / Cross
    bool button_b;          // B / Circle
    bool button_x;          // X / Square
    bool button_y;          // Y / Triangle
    bool button_lb;         // LB / L1
    bool button_rb;         // RB / R1
    bool button_back;       // Back / Select
    bool button_start;      // Start
    bool button_left_stick; // Left stick click
    bool button_right_stick;// Right stick click
    
    // D-Pad
    bool dpad_up;
    bool dpad_down;
    bool dpad_left;
    bool dpad_right;
    
    // Raw axis values for debugging (0-65535 or similar)
    int raw_axis[6];
    
} JoystickState;

typedef struct {
    JoystickState joysticks[4];  // Support up to 4 joysticks
    int num_joysticks;
    int active_joystick;         // Currently selected joystick (0-3)
    bool joystick_enabled;
    
    // Deadzone settings
    double stick_deadzone;        // Deadzone for analog sticks (0.0-1.0)
    double trigger_deadzone;      // Deadzone for triggers (0.0-1.0)
} JoystickManager;

// ============================================================
// GAME OPTIONS STRUCTURE
// ============================================================

typedef struct {
    bool fullscreen;
    bool show_debug_info;
    bool vsync_enabled;
    int target_fps;
    
    // Joystick options
    bool joystick_enabled;
    int active_joystick;
    double stick_deadzone;
    double trigger_deadzone;
    
    // Audio options
    int music_volume;           // 0-128
    int sfx_volume;             // 0-128
    bool music_enabled;
    bool sfx_enabled;
    
    // Gameplay options
    bool difficulty_auto;
    int difficulty_level;       // 1-5
    bool particle_effects;
    bool screen_shake;
    
} GameOptions;

// ============================================================
// VISUALIZER STRUCTURE
// ============================================================

typedef struct {
    int width, height;
    double volume_level;
    CometBusterGame comet_buster;
    AudioManager audio;              // Audio system
    
    // Input handling
    int mouse_x;
    int mouse_y;
    int last_mouse_x;               // Track last mouse position
    int last_mouse_y;
    double mouse_movement_timer;    // Time since last mouse movement
    bool mouse_just_moved;          // Did mouse move this frame?
    bool mouse_left_pressed;
    bool mouse_right_pressed;
    bool mouse_middle_pressed;
    
    // Arcade-style keyboard input
    bool key_a_pressed;         // A - turn left
    bool key_d_pressed;         // D - turn right
    bool key_w_pressed;         // W - forward thrust
    bool key_s_pressed;         // S - backward thrust
    bool key_z_pressed;         // Z - omnidirectional fire
    bool key_x_pressed;         // X - boost
    bool key_space_pressed;     // SPACE - boost
    bool key_ctrl_pressed;      // CTRL - fire
    
    // Joystick support
    JoystickManager joystick_manager;
    GameOptions options;
    
} Visualizer;

// ============================================================
// JOYSTICK MANAGEMENT FUNCTIONS
// ============================================================

/**
 * Initialize the joystick manager
 */
void joystick_manager_init(JoystickManager *manager);

/**
 * Cleanup joystick manager
 */
void joystick_manager_cleanup(JoystickManager *manager);

/**
 * Detect and connect to available joysticks
 * Returns number of joysticks found
 */
int joystick_manager_detect(JoystickManager *manager);

/**
 * Get the active joystick state
 */
JoystickState* joystick_manager_get_active(JoystickManager *manager);

/**
 * Update joystick state (call this regularly)
 */
void joystick_manager_update(JoystickManager *manager);

/**
 * Set deadzone for analog sticks (0.0-1.0)
 */
void joystick_manager_set_stick_deadzone(JoystickManager *manager, double deadzone);

/**
 * Set deadzone for triggers (0.0-1.0)
 */
void joystick_manager_set_trigger_deadzone(JoystickManager *manager, double deadzone);

// ============================================================
// GAME OPTIONS FUNCTIONS
// ============================================================

/**
 * Load game options from file
 */
bool game_options_load(GameOptions *options);

/**
 * Save game options to file
 */
bool game_options_save(const GameOptions *options);

/**
 * Get default game options
 */
GameOptions game_options_default(void);

// ============================================================
// GAME UPDATE AND INITIALIZATION
// ============================================================

void init_comet_buster_system(void *vis_ptr);
void update_comet_buster(void *vis_ptr, double dt);
void draw_comet_buster(void *vis_ptr, cairo_t *cr);
void comet_buster_cleanup(CometBusterGame *game);
void comet_buster_on_ship_hit(CometBusterGame *game, Visualizer *visualizer);

#endif
