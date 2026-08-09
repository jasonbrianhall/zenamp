#ifndef RADIALWAVE_H
#define RADIALWAVE_H

#define MAX_STAR_PARTICLES 100
#define STAR_POINTS 5

typedef struct {
    double angle;
    double distance;
    double velocity;
    double life;
    double r, g, b;
    double size;
} StarParticle;

typedef struct {
    double scale;           // Current scale of star (pulsates with beat)
    double rotation;        // Star rotation angle
    double glow_intensity;  // Glow effect intensity
    double fracture_amount; // How fractured the star appears (on drops)
    double color_shift;     // Hue shift based on audio characteristics
    StarParticle particles[MAX_STAR_PARTICLES];
    int particle_count;
    double particle_spawn_timer;
} PulsarCore;

typedef struct {
    double ring_rotation;     // Current rotation angle of the ring
    double ring_radius;       // Base radius of the ring
    double segment_bulge[32]; // Per-segment bulge amounts
    double ripple_phase;      // Phase for ripple animation
    PulsarCore core;
} RadialWaveSystem;

#endif
