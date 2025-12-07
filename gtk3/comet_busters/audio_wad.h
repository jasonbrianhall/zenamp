#ifndef AUDIO_WAD_H
#define AUDIO_WAD_H

#ifdef ExternalSound

#include <SDL2/SDL_mixer.h>
#include <stdbool.h>
#include "wad.h"

typedef struct {
    Mix_Music *background_music;
    
    // Sound effects
    Mix_Chunk *sfx_fire;
    Mix_Chunk *sfx_explosion;
    Mix_Chunk *sfx_hit;
    Mix_Chunk *sfx_boost;
    Mix_Chunk *sfx_game_over;
    Mix_Chunk *sfx_wave_complete;
    
    WadArchive wad;          // WAD file archive
    int master_volume;
    bool audio_enabled;
    
} AudioManager;

// Audio initialization and cleanup
bool audio_init(AudioManager *audio);
void audio_cleanup(AudioManager *audio);

// Load sounds from WAD file
bool audio_load_wad(AudioManager *audio, const char *wad_filename);

// Volume control
void audio_set_volume(AudioManager *audio, int volume);
int audio_get_volume(AudioManager *audio);

// Music playback
void audio_play_music(AudioManager *audio, const char *internal_path, bool loop);
void audio_stop_music(AudioManager *audio);
void audio_pause_music(AudioManager *audio);
void audio_resume_music(AudioManager *audio);

// Sound effects
void audio_play_sound(AudioManager *audio, Mix_Chunk *sound);

#else  // !ExternalSound

// Stub implementations for when sound is disabled
typedef struct {
    int dummy;  // Placeholder to ensure non-zero size
} AudioManager;

// Stub functions - do nothing
static inline bool audio_init(AudioManager *audio) { return true; }
static inline void audio_cleanup(AudioManager *audio) { }
static inline bool audio_load_wad(AudioManager *audio, const char *wad_filename) { return true; }
static inline void audio_set_volume(AudioManager *audio, int volume) { }
static inline int audio_get_volume(AudioManager *audio) { return 128; }
static inline void audio_play_music(AudioManager *audio, const char *internal_path, bool loop) { }
static inline void audio_stop_music(AudioManager *audio) { }
static inline void audio_pause_music(AudioManager *audio) { }
static inline void audio_resume_music(AudioManager *audio) { }
static inline void audio_play_sound(AudioManager *audio, void *sound) { }

#endif  // ExternalSound

#endif  // AUDIO_WAD_H
