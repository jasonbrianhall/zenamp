#ifndef RAINBOW_H
#define RAINBOW_H

#include <cairo.h>
#include <glib.h>
#include <math.h>

#define MAX_RAINBOW_PARTICLES 2000
#define MAX_RAINBOW_WAVES 20

typedef struct {
    double x, y;                // Position
    double vx, vy;              // Velocity
    double life;                // Remaining life (0.0 - 1.0)
    double hue;                 // Color hue (0.0 - 1.0)
    double size;                // Particle size
    double rotation;            // Rotation angle
    double trail_length;        // Length of trail
    int shape;                  // 0=circle, 1=square, 2=star
    gboolean active;
} RainbowParticle;

typedef struct {
    double x, y;                // Center position
    double radius;              // Current radius
    double max_radius;          // Max expansion radius
    double life;                // Remaining life (0.0 - 1.0)
    double hue_start;           // Starting hue for rainbow
    double thickness;           // Wave ring thickness
    double intensity;           // Brightness intensity
    gboolean active;
} RainbowWave;

typedef struct {
    double x, y;                // Center point
    double base_x, base_y;      // Base position for interaction
    double magnitude;           // Wave magnitude
    double frequency;           // Wave frequency
    gboolean active;
} RainbowVortex;

typedef struct {
    RainbowParticle particles[MAX_RAINBOW_PARTICLES];
    RainbowWave waves[MAX_RAINBOW_WAVES];
    RainbowVortex vortex;
    
    int particle_count;
    int wave_count;
    
    double global_hue_offset;   // Animating hue shift
    double time_elapsed;        // Total animation time
    double last_audio_level;    // For audio responsiveness
    double spawn_timer;         // Particle spawn timer
    double background_glow;     // Background intensity
    int particle_shape_mode;    // Which shapes to draw
    gboolean audio_reactive;    // Should react to audio?
    gboolean mouse_interactive; // Should react to mouse?
    double interaction_intensity;
    gboolean random_spawn_locations;  // Spawn from edges instead of center?
    gboolean spawn_waves_randomly;    // Spawn waves at random locations?
    double screen_width;        // Screen width for random spawning
    double screen_height;       // Screen height for random spawning
} RainbowSystem;

// Function declarations
void init_rainbow_system(RainbowSystem *rainbow);
void update_rainbow_system(RainbowSystem *rainbow, double dt, double audio_level,
                          double mouse_x, double mouse_y, gboolean mouse_active);
void draw_rainbow_system(cairo_t *cr, RainbowSystem *rainbow, 
                        int width, int height);
void spawn_rainbow_particle(RainbowSystem *rainbow, double x, double y, 
                           double vx, double vy, double hue, int shape);
void spawn_rainbow_particle_random_location(RainbowSystem *rainbow, double width, double height,
                                           double hue, int shape, double speed);
void spawn_rainbow_wave(RainbowSystem *rainbow, double x, double y, double hue);
void spawn_rainbow_wave_random_location(RainbowSystem *rainbow, double width, double height,
                                       double hue);
void rainbow_on_mouse_click(RainbowSystem *rainbow, double x, double y);
void rainbow_on_scroll(RainbowSystem *rainbow, double x, double y, int direction);
void cleanup_rainbow_system(RainbowSystem *rainbow);

#endif // RAINBOW_H
