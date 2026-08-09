#define FOURIER_POINTS 64  // Number of frequency bins to visualize
#define FOURIER_HISTORY 128 // History length for trails

typedef struct {
    double real, imag;     // Complex number components
    double magnitude;      // Magnitude of this frequency bin
    double phase;          // Phase angle
    double x, y;           // Current position on circle
    double trail_x[FOURIER_HISTORY];  // Position history for trails
    double trail_y[FOURIER_HISTORY];  // Position history for trails
    int trail_index;       // Current position in trail buffer
    double hue;            // Color hue for this frequency
    double intensity;      // Visual intensity
} FourierBin;

