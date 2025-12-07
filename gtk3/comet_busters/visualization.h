#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include <stdbool.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include "cometbuster.h"
#include "audio_wad.h"

typedef struct {
    int width, height;
    double volume_level;
    CometBusterGame comet_buster;
    AudioManager audio;              // Audio system
    int mouse_x;
    int mouse_y;
    bool mouse_left_pressed;
    bool mouse_right_pressed;
    bool mouse_middle_pressed;
} Visualizer;

// Game update and initialization
void init_comet_buster_system(void *vis_ptr);
void update_comet_buster(void *vis_ptr, double dt);
void draw_comet_buster(void *vis_ptr, cairo_t *cr);
void comet_buster_cleanup(CometBusterGame *game);
void comet_buster_on_ship_hit(CometBusterGame *game, Visualizer *visualizer);

#endif
