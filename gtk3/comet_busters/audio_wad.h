#ifndef AUDIO_WAD_H
#define AUDIO_WAD_H

#include <SDL2/SDL_mixer.h>
#include <stdbool.h>
#include "wad.h"

typedef struct {
    // Music tracks (up to 10 background music files)
    Mix_Music *music_tracks[10];
    int music_track_count;
    int current_music_track;
    
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
void audio_play_random_music(AudioManager *audio);  // Play random track from loaded list
void audio_stop_music(AudioManager *audio);
void audio_pause_music(AudioManager *audio);
void audio_resume_music(AudioManager *audio);

// Sound effects
void audio_play_sound(AudioManager *audio, Mix_Chunk *sound);

#endif  // AUDIO_WAD_H
