#include <cairo.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cometbuster.h"
#include "visualization.h"

#ifdef ExternalSound
#include "audio_wad.h"
#endif 

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void comet_buster_wrap_position(double *x, double *y, int width, int height) {
    if (*x < -50) *x = width + 50;
    if (*x > width + 50) *x = -50;
    if (*y < -50) *y = height + 50;
    if (*y > height + 50) *y = -50;
}

double comet_buster_distance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx*dx + dy*dy);
}

void comet_buster_get_frequency_color(int frequency_band, double *r, double *g, double *b) {
    switch (frequency_band % 3) {
        case 0:  // Bass - red
            *r = 1.0; *g = 0.3; *b = 0.2;
            break;
        case 1:  // Mid - yellow
            *r = 1.0; *g = 1.0; *b = 0.2;
            break;
        case 2:  // Treble - blue
            *r = 0.2; *g = 0.8; *b = 1.0;
            break;
    }
}

// ============================================================================
// AUDIO INTEGRATION (Stubs)
// ============================================================================

void comet_buster_update_frequency_bands(CometBusterGame *game, void *visualizer) {
    if (!game) return;
    (void)visualizer;   // Suppress unused parameter warning
    game->frequency_bands[0] = 0.5;
    game->frequency_bands[1] = 0.5;
    game->frequency_bands[2] = 0.5;
}

void comet_buster_fire_on_beat(CometBusterGame *game) {
    if (!game) return;
}

bool comet_buster_detect_beat(void *vis) {
    (void)vis;   // Suppress unused parameter warning
    return false;
}

void comet_buster_increase_difficulty(CometBusterGame *game) {
    if (!game) return;
    game->base_spawn_rate *= 0.9;
    if (game->base_spawn_rate < 0.3) game->base_spawn_rate = 0.3;
}
