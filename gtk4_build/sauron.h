#ifndef SAURON_H
#define SAURON_H

typedef struct {
    double pupil_size;
    double iris_rotation;
    double blink_timer;
    double blink_duration;
    gboolean is_blinking;
    double look_x, look_y;
    double look_target_x, look_target_y;
    double intensity_pulse;
    double fire_ring_rotation;
    double fire_intensity;
    
    // Fire particles around the eye
    struct {
        double angle;
        double radius;
        double height;
        double life;
        double speed;
        double size;
        int type;  // 0=flame, 1=ember, 2=spark
    } fire_particles[60];
    int fire_particle_count;
} EyeOfSauronState;

#endif
