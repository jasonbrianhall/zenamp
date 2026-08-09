#ifndef EQUALIZER_H
#define EQUALIZER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t
#include <gtk/gtk.h>
#include <string.h>

#define EQ_BANDS 3  // Bass, Mid, Treble

typedef struct {
    // Filter coefficients for each band
    double a[3];  // denominator coefficients
    double b[3];  // numerator coefficients
    
    // Filter state (delay line)
    double x[3];  // input history
    double y[3];  // output history
    
    // Band parameters
    double gain;     // Linear gain (0.0 to 2.0, 1.0 = no change)
    double frequency; // Center frequency
    double q_factor; // Quality factor
} EQBand;

typedef struct {
    EQBand bands[EQ_BANDS];
    bool enabled;
    int sample_rate;
    
    // Gain values in dB (-12 to +12)
    double bass_gain_db;
    double mid_gain_db;
    double treble_gain_db;
} Equalizer;

// Function declarations
Equalizer* equalizer_new(int sample_rate);
void equalizer_free(Equalizer *eq);
void equalizer_set_enabled(Equalizer *eq, bool enabled);
void equalizer_set_bass(Equalizer *eq, double gain_db);
void equalizer_set_mid(Equalizer *eq, double gain_db);
void equalizer_set_treble(Equalizer *eq, double gain_db);
void equalizer_reset(Equalizer *eq);
int16_t equalizer_process_sample(Equalizer *eq, int16_t input);
void equalizer_process_buffer(Equalizer *eq, int16_t *buffer, size_t length);

// Internal functions
void calculate_biquad_coefficients(EQBand *band, double frequency, double gain_db, double q, int sample_rate);
double db_to_linear(double db);
double biquad_filter(EQBand *band, double input);

#endif
