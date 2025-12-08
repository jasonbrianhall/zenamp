#ifndef AUDIO_WAD_H
#define AUDIO_WAD_H

#include <SDL2/SDL_mixer.h>
#include <stdbool.h>
#include "wad.h"

// Define separate channels for music and SFX
#define MUSIC_CHANNEL -1  // Reserved for music only
#define SFX_CHANNELS 0    // SFX use channels 0-30 (31 total for SFX)

typedef struct {
    // Music tracks (up to 10 background music files)
    Mix_Music *music_tracks[10];
    int music_track_count;
    int current_music_track;
    
    // Shuffle tracking - keeps order for random playback without repeats
    int shuffle_order[10];       // Randomized order of track indices
    int shuffle_position;        // Current position in the shuffle order
    bool shuffle_initialized;    // Whether shuffle_order has been initialized
    
    // Sound effects
    Mix_Chunk *sfx_fire;
    Mix_Chunk *sfx_alien_fire;
    Mix_Chunk *sfx_explosion;
    Mix_Chunk *sfx_hit;
    Mix_Chunk *sfx_boost;
    Mix_Chunk *sfx_game_over;
    Mix_Chunk *sfx_wave_complete;
    
    WadArchive wad;          // WAD file archive
    int master_volume;       // Overall master volume (kept for compatibility)
    int music_volume;        // Music-specific volume (0-128)
    int sfx_volume;          // SFX-specific volume (0-128)
    bool audio_enabled;
    
} AudioManager;

// Audio initialization and cleanup
bool audio_init(AudioManager *audio);
void audio_cleanup(AudioManager *audio);

// Load sounds from WAD file
bool audio_load_wad(AudioManager *audio, const char *wad_filename);

// Volume control - Master (legacy)
void audio_set_volume(AudioManager *audio, int volume);
int audio_get_volume(AudioManager *audio);

// Volume control - Separate Music and SFX (NEW)
void audio_set_music_volume(AudioManager *audio, int volume);
int audio_get_music_volume(AudioManager *audio);

void audio_set_sfx_volume(AudioManager *audio, int volume);
int audio_get_sfx_volume(AudioManager *audio);

// Music playback
void audio_play_music(AudioManager *audio, const char *internal_path, bool loop);
void audio_play_random_music(AudioManager *audio);  // Play random track from loaded list (no repeats until all played)
void audio_stop_music(AudioManager *audio);
void audio_pause_music(AudioManager *audio);
void audio_resume_music(AudioManager *audio);

// Sound effects
void audio_play_sound(AudioManager *audio, Mix_Chunk *sound);

#endif  // AUDIO_WAD_H
