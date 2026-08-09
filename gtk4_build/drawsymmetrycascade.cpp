#include "visualization.h"
#include <math.h>

void draw_waveform_symmetry_cascade(Visualizer *vis, cairo_t *cr)  {
    if (vis->width <= 0 || vis->height <= 0) return;

    int num_bands = VIS_FREQUENCY_BARS;
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    double band_width = (double)vis->width / num_bands;
    double max_height = vis->height * 0.45;

    for (int i = 0; i < num_bands; i++) {
        double amplitude = vis->frequency_bands[i];
        double height = amplitude * max_height;

        double x = i * band_width;
        double y_top = center_y - height;
        double y_bottom = center_y + height;

        // Color gradient based on band index
        double hue = (double)i / num_bands;
        double sat = fmin(1.0, amplitude * 2.0);
        double val = 1.0;

        // HSV to RGB
        double r, g, b;
        int h_i = (int)(hue * 6);
        double f = hue * 6 - h_i;
        double p = val * (1 - sat);
        double q = val * (1 - f * sat);
        double t = val * (1 - (1 - f) * sat);
        switch (h_i % 6) {
            case 0: r = val; g = t; b = p; break;
            case 1: r = q; g = val; b = p; break;
            case 2: r = p; g = val; b = t; break;
            case 3: r = p; g = q; b = val; break;
            case 4: r = t; g = p; b = val; break;
            case 5: r = val; g = p; b = q; break;
        }

        cairo_set_source_rgba(cr, r, g, b, 0.9);

        // Top pulse
        cairo_rectangle(cr, x + 1, y_top, band_width - 2, height);
        cairo_fill(cr);

        // Bottom mirrored pulse
        cairo_rectangle(cr, x + 1, center_y, band_width - 2, height);
        cairo_fill(cr);
    }
}

