#define DNA_POINTS 200
#define DNA_STRANDS 2

#define MAX_DNA_SEGMENTS 80
#define DNA_BASE_PAIRS 4

typedef struct {
    double x, y, z;        // 3D position (z for depth simulation)
    double intensity;      // Audio intensity affecting this segment
    double base_type;      // 0-3 for A,T,G,C base pair types
    double connection_strength; // How strongly the base pairs connect
    double twist_offset;   // Individual twist offset for this segment
    gboolean active;       // Is this segment visible
} DNASegment;

typedef struct {
    double amplitude;      // How far from center line
    double frequency;      // How fast the helix twists
    double phase_offset;   // Phase difference between strands
    double flow_speed;     // How fast the helix flows horizontally
    double strand_colors[DNA_STRANDS][3]; // RGB colors for each strand
} DNAHelix;
