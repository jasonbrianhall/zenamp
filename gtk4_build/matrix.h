#define MAX_MATRIX_COLUMNS 60
#define MAX_CHARS_PER_COLUMN 30

typedef struct {
    int x;                    // Column x position
    double y;                 // Current y position (can be fractional)
    double speed;             // Fall speed
    int length;               // Length of the trail
    double intensity;         // Brightness multiplier
    const char* chars[MAX_CHARS_PER_COLUMN]; // Array of string pointers instead of chars
    double char_ages[MAX_CHARS_PER_COLUMN]; // Age of each character (for fading)
    gboolean active;          // Is this column active
    int frequency_band;       // Which frequency band controls this column
    bool power_mode;
    int flash_intensity;
    int wave_offset;
    int glitch_timer;
} MatrixColumn;
