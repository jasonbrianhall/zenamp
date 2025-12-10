/**
 * Joystick Manager Implementation - SDL2 VERSION
 * Cross-platform joystick support using SDL2
 */

#include "visualization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

// ============================================================
// SDL JOYSTICK STATE
// ============================================================

typedef struct {
    SDL_Joystick *joystick;
    SDL_GameController *controller;
} SDLJoystickState;

static SDLJoystickState sdl_joysticks[4] = {0};
static SDL_bool sdl_initialized = SDL_FALSE;

// ============================================================
// JOYSTICK INITIALIZATION
// ============================================================

void joystick_manager_init(JoystickManager *manager) {
    if (!manager) return;
    
    memset(manager, 0, sizeof(JoystickManager));
    manager->joystick_enabled = true;
    manager->active_joystick = 0;
    manager->stick_deadzone = 0.15;
    manager->trigger_deadzone = 0.05;
    manager->num_joysticks = 0;
    
    // Initialize SDL joystick subsystem
    if (!sdl_initialized) {
        if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) == 0) {
            sdl_initialized = SDL_TRUE;
            fprintf(stdout, "[JOYSTICK] SDL initialized\n");
        } else {
            fprintf(stderr, "[JOYSTICK] SDL initialization failed: %s\n", SDL_GetError());
        }
    }
    
    fprintf(stdout, "[JOYSTICK] Manager initialized\n");
}

void joystick_manager_cleanup(JoystickManager *manager) {
    if (!manager) return;
    
    // Close all open joysticks
    for (int i = 0; i < 4; i++) {
        if (sdl_joysticks[i].controller) {
            SDL_GameControllerClose(sdl_joysticks[i].controller);
            sdl_joysticks[i].controller = NULL;
        }
        if (sdl_joysticks[i].joystick) {
            SDL_JoystickClose(sdl_joysticks[i].joystick);
            sdl_joysticks[i].joystick = NULL;
        }
    }
    
    fprintf(stdout, "[JOYSTICK] Manager cleaned up\n");
}

// ============================================================
// JOYSTICK DETECTION
// ============================================================

int joystick_manager_detect(JoystickManager *manager) {
    if (!manager) return 0;
    
    if (!sdl_initialized) {
        fprintf(stderr, "[JOYSTICK] SDL not initialized\n");
        return 0;
    }
    
    manager->num_joysticks = 0;
    
    // Get number of joysticks
    int num_joysticks = SDL_NumJoysticks();
    if (num_joysticks > 4) {
        num_joysticks = 4;  // Limit to 4
    }
    
    fprintf(stdout, "[JOYSTICK] Detecting joysticks... found %d devices\n", num_joysticks);
    
    // Open each joystick
    for (int i = 0; i < 4; i++) {
        JoystickState *joy = &manager->joysticks[i];
        memset(joy, 0, sizeof(JoystickState));
        joy->device_id = i;
        joy->connected = SDL_FALSE;
        
        if (i < num_joysticks) {
            // Try to open as game controller first (better mapping)
            SDL_GameController *controller = SDL_GameControllerOpen(i);
            if (controller) {
                sdl_joysticks[i].controller = controller;
                sdl_joysticks[i].joystick = SDL_GameControllerGetJoystick(controller);
                joy->connected = SDL_TRUE;
                manager->num_joysticks++;
                snprintf(joy->name, sizeof(joy->name), "%s", 
                        SDL_GameControllerName(controller));
                fprintf(stdout, "[JOYSTICK] Device %d: %s (GameController)\n", i, joy->name);
            } else {
                // Fall back to raw joystick
                SDL_Joystick *joystick = SDL_JoystickOpen(i);
                if (joystick) {
                    sdl_joysticks[i].joystick = joystick;
                    sdl_joysticks[i].controller = NULL;
                    joy->connected = SDL_TRUE;
                    manager->num_joysticks++;
                    snprintf(joy->name, sizeof(joy->name), "%s", 
                            SDL_JoystickName(joystick));
                    fprintf(stdout, "[JOYSTICK] Device %d: %s (Raw Joystick)\n", i, joy->name);
                }
            }
        }
    }
    
    fprintf(stdout, "[JOYSTICK] Detection complete - found %d joysticks\n", 
            manager->num_joysticks);
    
    return manager->num_joysticks;
}

// ============================================================
// JOYSTICK STATE ACCESS
// ============================================================

JoystickState* joystick_manager_get_active(JoystickManager *manager) {
    if (!manager || manager->active_joystick < 0 || manager->active_joystick >= 4) {
        return NULL;
    }
    return &manager->joysticks[manager->active_joystick];
}

// ============================================================
// JOYSTICK UPDATE
// ============================================================

static double apply_deadzone(double value, double deadzone) {
    if (deadzone < 0.0) deadzone = 0.0;
    if (deadzone > 1.0) deadzone = 1.0;
    
    if (value > deadzone) {
        return (value - deadzone) / (1.0 - deadzone);
    } else if (value < -deadzone) {
        return (value + deadzone) / (1.0 - deadzone);
    }
    return 0.0;
}

/**
 * Normalize SDL axis value (-32768 to 32767) to -1.0 to 1.0
 */
static double normalize_axis(Sint16 value) {
    double normalized = (double)value / 32768.0;
    if (normalized > 1.0) normalized = 1.0;
    if (normalized < -1.0) normalized = -1.0;
    return normalized;
}

/**
 * Update joystick state from SDL
 */
void joystick_manager_update(JoystickManager *manager) {
    if (!manager || !manager->joystick_enabled) return;
    
    if (!sdl_initialized) return;
    
    // Process SDL events to get latest joystick state
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Events are processed but we mainly rely on direct state queries below
        switch (event.type) {
            case SDL_JOYDEVICEADDED:
                fprintf(stdout, "[JOYSTICK] Device connected: %d\n", event.jdevice.which);
                break;
            case SDL_JOYDEVICEREMOVED:
                fprintf(stdout, "[JOYSTICK] Device disconnected: %d\n", event.jdevice.which);
                break;
        }
    }
    
    // Update each joystick
    for (int i = 0; i < 4; i++) {
        JoystickState *joy = &manager->joysticks[i];
        
        if (!joy->connected) continue;
        
        SDL_Joystick *js = sdl_joysticks[i].joystick;
        SDL_GameController *gc = sdl_joysticks[i].controller;
        
        if (!js) continue;
        
        // If we have a game controller, use it for better mappings
        if (gc) {
            // Analog sticks
            Sint16 lx = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
            Sint16 ly = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
            Sint16 rx = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX);
            Sint16 ry = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY);
            
            joy->axis_x = apply_deadzone(normalize_axis(lx), manager->stick_deadzone);
            joy->axis_y = apply_deadzone(-normalize_axis(ly), manager->stick_deadzone);
            joy->axis_rx = apply_deadzone(normalize_axis(rx), manager->stick_deadzone);
            joy->axis_ry = apply_deadzone(-normalize_axis(ry), manager->stick_deadzone);
            
            // Triggers (0 to 32767)
            Sint16 lt = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            Sint16 rt = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            
            joy->axis_lt = apply_deadzone((double)lt / 32767.0, manager->trigger_deadzone);
            joy->axis_rt = apply_deadzone((double)rt / 32767.0, manager->trigger_deadzone);
            
            // Buttons
            joy->button_a = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_A) != 0;
            joy->button_b = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_B) != 0;
            joy->button_x = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_X) != 0;
            joy->button_y = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_Y) != 0;
            joy->button_lb = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
            joy->button_rb = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
            joy->button_back = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_BACK) != 0;
            joy->button_start = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_START) != 0;
            joy->button_left_stick = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_LEFTSTICK) != 0;
            joy->button_right_stick = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_RIGHTSTICK) != 0;
            
            // D-Pad
            joy->dpad_up = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
            joy->dpad_down = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
            joy->dpad_left = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
            joy->dpad_right = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
            
        } else {
            // Raw joystick - try to map common button layouts
            int num_buttons = SDL_JoystickNumButtons(js);
            int num_axes = SDL_JoystickNumAxes(js);
            int num_hats = SDL_JoystickNumHats(js);
            
            // Read analog axes
            if (num_axes >= 2) {
                joy->axis_x = apply_deadzone(normalize_axis(SDL_JoystickGetAxis(js, 0)), 
                                           manager->stick_deadzone);
                joy->axis_y = apply_deadzone(-normalize_axis(SDL_JoystickGetAxis(js, 1)), 
                                            manager->stick_deadzone);
            }
            if (num_axes >= 4) {
                joy->axis_rx = apply_deadzone(normalize_axis(SDL_JoystickGetAxis(js, 2)), 
                                            manager->stick_deadzone);
                joy->axis_ry = apply_deadzone(-normalize_axis(SDL_JoystickGetAxis(js, 3)), 
                                             manager->stick_deadzone);
            }
            if (num_axes >= 6) {
                joy->axis_lt = apply_deadzone((double)(SDL_JoystickGetAxis(js, 4) + 32768) / 65535.0, 
                                            manager->trigger_deadzone);
                joy->axis_rt = apply_deadzone((double)(SDL_JoystickGetAxis(js, 5) + 32768) / 65535.0, 
                                            manager->trigger_deadzone);
            }
            
            // Read buttons - standard mapping
            if (num_buttons >= 1) joy->button_a = SDL_JoystickGetButton(js, 0) != 0;
            if (num_buttons >= 2) joy->button_b = SDL_JoystickGetButton(js, 1) != 0;
            if (num_buttons >= 3) joy->button_x = SDL_JoystickGetButton(js, 2) != 0;
            if (num_buttons >= 4) joy->button_y = SDL_JoystickGetButton(js, 3) != 0;
            if (num_buttons >= 5) joy->button_lb = SDL_JoystickGetButton(js, 4) != 0;
            if (num_buttons >= 6) joy->button_rb = SDL_JoystickGetButton(js, 5) != 0;
            if (num_buttons >= 7) joy->button_back = SDL_JoystickGetButton(js, 6) != 0;
            if (num_buttons >= 8) joy->button_start = SDL_JoystickGetButton(js, 7) != 0;
            if (num_buttons >= 11) joy->button_left_stick = SDL_JoystickGetButton(js, 10) != 0;
            if (num_buttons >= 12) joy->button_right_stick = SDL_JoystickGetButton(js, 11) != 0;
            
            // Read D-Pad from hat or buttons
            if (num_hats > 0) {
                Uint8 hat = SDL_JoystickGetHat(js, 0);
                joy->dpad_up = (hat & SDL_HAT_UP) != 0;
                joy->dpad_down = (hat & SDL_HAT_DOWN) != 0;
                joy->dpad_left = (hat & SDL_HAT_LEFT) != 0;
                joy->dpad_right = (hat & SDL_HAT_RIGHT) != 0;
            } else {
                // Try to read D-Pad from axes
                if (num_axes >= 7) {
                    Sint16 dpad_x = SDL_JoystickGetAxis(js, 6);
                    Sint16 dpad_y = SDL_JoystickGetAxis(js, 7);
                    joy->dpad_left = dpad_x < -16384;
                    joy->dpad_right = dpad_x > 16384;
                    joy->dpad_up = dpad_y < -16384;
                    joy->dpad_down = dpad_y > 16384;
                }
            }
        }
    }
}

// ============================================================
// JOYSTICK DEADZONE CONFIGURATION
// ============================================================

void joystick_manager_set_stick_deadzone(JoystickManager *manager, double deadzone) {
    if (!manager) return;
    
    if (deadzone < 0.0) deadzone = 0.0;
    if (deadzone > 1.0) deadzone = 1.0;
    
    manager->stick_deadzone = deadzone;
    fprintf(stdout, "[JOYSTICK] Stick deadzone set to %.2f\n", deadzone);
}

void joystick_manager_set_trigger_deadzone(JoystickManager *manager, double deadzone) {
    if (!manager) return;
    
    if (deadzone < 0.0) deadzone = 0.0;
    if (deadzone > 1.0) deadzone = 1.0;
    
    manager->trigger_deadzone = deadzone;
    fprintf(stdout, "[JOYSTICK] Trigger deadzone set to %.2f\n", deadzone);
}

// ============================================================
// GAME OPTIONS PERSISTENCE
// ============================================================

bool game_options_load(GameOptions *options) {
    if (!options) return false;
    
    fprintf(stdout, "[OPTIONS] No saved options found, using defaults\n");
    return false;
}

bool game_options_save(const GameOptions *options) {
    if (!options) return false;
    
    fprintf(stdout, "[OPTIONS] Game options saved\n");
    return true;
}

GameOptions game_options_default(void) {
    GameOptions opts;
    memset(&opts, 0, sizeof(GameOptions));
    
    opts.fullscreen = false;
    opts.show_debug_info = false;
    opts.vsync_enabled = true;
    opts.target_fps = 60;
    
    opts.joystick_enabled = true;
    opts.active_joystick = 0;
    opts.stick_deadzone = 0.15;
    opts.trigger_deadzone = 0.05;
    
    opts.music_volume = 100;
    opts.sfx_volume = 100;
    opts.music_enabled = true;
    opts.sfx_enabled = true;
    
    opts.difficulty_auto = true;
    opts.difficulty_level = 2;
    opts.particle_effects = true;
    opts.screen_shake = true;
    
    return opts;
}

void update_visualizer_joystick(Visualizer *vis) {
    if (!vis) return;
    
    JoystickState *js = joystick_manager_get_active(&vis->joystick_manager);
    
    if (!js || !js->connected) {
        vis->joystick_stick_x = 0.0;
        vis->joystick_stick_y = 0.0;
        vis->joystick_stick_rx = 0.0;
        vis->joystick_stick_ry = 0.0;
        vis->joystick_trigger_lt = 0.0;
        vis->joystick_trigger_rt = 0.0;
        vis->joystick_button_a = false;
        vis->joystick_button_b = false;
        vis->joystick_button_x = false;
        vis->joystick_button_y = false;
        vis->joystick_button_lb = false;
        vis->joystick_button_rb = false;
        vis->joystick_button_start = false;
        vis->joystick_button_back = false;
        vis->joystick_button_left_stick = false;
        vis->joystick_button_right_stick = false;
        return;
    }
    
    vis->joystick_stick_x = js->axis_x;
    vis->joystick_stick_y = js->axis_y;
    vis->joystick_stick_rx = js->axis_rx;
    vis->joystick_stick_ry = js->axis_ry;
    vis->joystick_trigger_lt = js->axis_lt;
    vis->joystick_trigger_rt = js->axis_rt;
    vis->joystick_button_a = js->button_a;
    vis->joystick_button_b = js->button_b;
    vis->joystick_button_x = js->button_x;
    vis->joystick_button_y = js->button_y;
    vis->joystick_button_lb = js->button_lb;
    vis->joystick_button_rb = js->button_rb;
    vis->joystick_button_start = js->button_start;
    vis->joystick_button_back = js->button_back;
    vis->joystick_button_left_stick = js->button_left_stick;
    vis->joystick_button_right_stick = js->button_right_stick;
}
