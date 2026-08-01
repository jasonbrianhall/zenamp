#ifndef PENGUINS_H
#define PENGUINS_H

#include "visualization.h"
#include <cairo.h>

// Update function - call this every frame with dt (delta time)
void update_penguins(Visualizer *vis, double dt);

// Draw function - call this every frame to render
void draw_penguins(Visualizer *vis, cairo_t *cr);

#endif // PENGUINS_H
