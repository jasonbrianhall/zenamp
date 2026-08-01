#ifndef BOUNCING_PENGUINS_H
#define BOUNCING_PENGUINS_H

#include "visualization.h"
#include <cairo.h>

// Initialize the game
void init_penguins_game(Visualizer *vis);

// Update function - call this every frame with dt (delta time)
void update_bouncing_penguins(Visualizer *vis, double dt);

// Draw function - call this every frame to render
void draw_bouncing_penguins(Visualizer *vis, cairo_t *cr);

#endif // BOUNCING_PENGUINS_H
