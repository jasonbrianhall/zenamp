#include "audio_wad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ExternalSound

#include <SDL2/SDL.h>

// Helper function to load sound from WAD
// Supports WAV, MP3, OGG, FLAC, etc. (depending on SDL_mixer compilation)
static Mix_Chunk* load_sound_from_wad(WadArchive *wad, const char *filename) {
    if (!wad || !filename) return NULL;
    
    // Extract file from WAD
    WadFile wad_file;
    if (!wad_extract_file(wad, filename, &wad_file)) {
        fprintf(stderr, "Failed to load sound from WAD: %s\n", filename);
        return NULL;
    }
    
    // Create SDL_RWops from memory buffer
    // SDL_RWFromMem creates a read-only stream from existing memory
    SDL_RWops *rw = SDL_RWFromMem(wad_file.data, wad_file.size);
    if (!rw) {
        fprintf(stderr, "Failed to create SDL_RWops for %s\n", filename);
        wad_free_file(&wad_file);
        return NULL;
    }
    
    // Load audio from memory using SDL_mixer
    // Mix_LoadWAV_RW can load:
    //   - WAV (always supported)
    //   - MP3 (if SDL_mixer compiled with smpeg2 or libmad)
    //   - OGG (if SDL_mixer compiled with libogg/libvorbis)
    //   - FLAC (if SDL_mixer compiled with libFLAC)
    //   - MOD (if SDL_mixer compiled with libmikmod)
    // The '0' means don't free the SDL_RWops - we'll handle it
    Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 0);
    
    if (!chunk) {
        fprintf(stderr, "Failed to decode audio from WAD: %s - %s\n", filename, Mix_GetError());
        fprintf(stderr, "Note: Check if SDL_mixer supports this format. Install SDL2_mixer-devel.\n");
        SDL_RWclose(rw);
        wad_free_file(&wad_file);
        return NULL;
    }
    
    // Clean up: free the RWops and the WAD file data
    // Note: SDL_mixer copies the audio data internally, so the original can be freed
    SDL_RWclose(rw);
    wad_free_file(&wad_file);
    
    return chunk;
}

// Initialize audio system
bool audio_init(AudioManager *audio) {
    if (!audio) {
        fprintf(stderr, "Error: AudioManager is NULL\n");
        return false;
    }
    
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL audio initialization failed: %s\n", SDL_GetError());
        return false;
    }
    
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Mixer initialization failed: %s\n", Mix_GetError());
        SDL_Quit();
        return false;
    }
    
    Mix_AllocateChannels(8);
    
    // Initialize fields
    audio->background_music = NULL;
    audio->sfx_fire = NULL;
    audio->sfx_explosion = NULL;
    audio->sfx_hit = NULL;
    audio->sfx_boost = NULL;
    audio->sfx_game_over = NULL;
    audio->sfx_wave_complete = NULL;
    
    memset(&audio->wad, 0, sizeof(WadArchive));
    
    audio->master_volume = 128;
    audio->audio_enabled = true;
    
    fprintf(stdout, "✓ Audio system initialized\n");
    return true;
}

// Load all sounds from WAD file
bool audio_load_wad(AudioManager *audio, const char *wad_filename) {
    if (!audio || !wad_filename) {
        fprintf(stderr, "Error: Invalid parameters for audio_load_wad\n");
        return false;
    }
    
    // Open WAD file
    if (!wad_open(&audio->wad, wad_filename)) {
        fprintf(stderr, "Error: Failed to open WAD file: %s\n", wad_filename);
        return false;
    }
    
    fprintf(stdout, "Loading sounds from WAD: %s\n", wad_filename);
    
    // Load individual sounds
    // Note: Adjust paths to match how you organized files in the WAD
    audio->sfx_fire = load_sound_from_wad(&audio->wad, "sounds/fire.mp3");
    audio->sfx_explosion = load_sound_from_wad(&audio->wad, "sounds/explosion.mp3");
    audio->sfx_hit = load_sound_from_wad(&audio->wad, "sounds/hit.mp3");
    audio->sfx_boost = load_sound_from_wad(&audio->wad, "sounds/boost.mp3");
    audio->sfx_game_over = load_sound_from_wad(&audio->wad, "sounds/game_over.mp3");
    audio->sfx_wave_complete = load_sound_from_wad(&audio->wad, "sounds/wave_complete.mp3");
    
    int loaded = 0;
    if (audio->sfx_fire) loaded++;
    if (audio->sfx_explosion) loaded++;
    if (audio->sfx_hit) loaded++;
    if (audio->sfx_boost) loaded++;
    if (audio->sfx_game_over) loaded++;
    if (audio->sfx_wave_complete) loaded++;
    
    fprintf(stdout, "✓ Loaded %d/%d sounds from WAD\n", loaded, 6);
    
    return loaded > 0;  // Return true if at least one sound loaded
}

// Clean up
void audio_cleanup(AudioManager *audio) {
    if (!audio) return;
    
    // Stop music
    if (Mix_PlayingMusic()) {
        Mix_HaltMusic();
    }
    
    // Free music
    if (audio->background_music) {
        Mix_FreeMusic(audio->background_music);
        audio->background_music = NULL;
    }
    
    // Free sounds
    if (audio->sfx_fire) Mix_FreeChunk(audio->sfx_fire);
    if (audio->sfx_explosion) Mix_FreeChunk(audio->sfx_explosion);
    if (audio->sfx_hit) Mix_FreeChunk(audio->sfx_hit);
    if (audio->sfx_boost) Mix_FreeChunk(audio->sfx_boost);
    if (audio->sfx_game_over) Mix_FreeChunk(audio->sfx_game_over);
    if (audio->sfx_wave_complete) Mix_FreeChunk(audio->sfx_wave_complete);
    
    // Close WAD
    wad_close(&audio->wad);
    
    // Close mixer
    Mix_CloseAudio();
    SDL_Quit();
    
    fprintf(stdout, "✓ Audio system cleaned up\n");
}

// Set volume
void audio_set_volume(AudioManager *audio, int volume) {
    if (!audio) return;
    
    if (volume < 0) volume = 0;
    if (volume > 128) volume = 128;
    
    audio->master_volume = volume;
    Mix_Volume(-1, volume);
    
    fprintf(stdout, "Volume: %d/128\n", volume);
}

// Get volume
int audio_get_volume(AudioManager *audio) {
    if (!audio) return 0;
    return audio->master_volume;
}

// Play music from WAD
void audio_play_music(AudioManager *audio, const char *internal_path, bool loop) {
    if (!audio || !audio->audio_enabled || !internal_path) return;
    
    WadFile music_file;
    if (!wad_extract_file(&audio->wad, internal_path, &music_file)) {
        fprintf(stderr, "Failed to load music from WAD: %s\n", internal_path);
        return;
    }
    
    // Create SDL_RWops from memory
    SDL_RWops *rw = SDL_RWFromMem(music_file.data, music_file.size);
    if (!rw) {
        fprintf(stderr, "Failed to create SDL_RWops for music\n");
        wad_free_file(&music_file);
        return;
    }
    
    // Load music
    Mix_Music *music = Mix_LoadMUS_RW(rw, 1);  // 1 = free SDL_RWops
    if (!music) {
        fprintf(stderr, "Failed to load music: %s\n", Mix_GetError());
        free(music_file.data);
        return;
    }
    
    audio->background_music = music;
    
    int loops = loop ? -1 : 0;
    if (Mix_PlayMusic(music, loops) < 0) {
        fprintf(stderr, "Failed to play music: %s\n", Mix_GetError());
    } else {
        fprintf(stdout, "♪ Playing: %s\n", internal_path);
    }
}

// Stop music
void audio_stop_music(AudioManager *audio) {
    if (!audio) return;
    
    if (Mix_PlayingMusic()) {
        Mix_HaltMusic();
        fprintf(stdout, "♪ Music stopped\n");
    }
}

// Pause music
void audio_pause_music(AudioManager *audio) {
    if (!audio) return;
    
    if (Mix_PlayingMusic()) {
        Mix_PauseMusic();
        fprintf(stdout, "♪ Music paused\n");
    }
}

// Resume music
void audio_resume_music(AudioManager *audio) {
    if (!audio) return;
    
    if (Mix_PausedMusic()) {
        Mix_ResumeMusic();
        fprintf(stdout, "♪ Music resumed\n");
    }
}

// Play sound
void audio_play_sound(AudioManager *audio, Mix_Chunk *sound) {
    if (!audio || !audio->audio_enabled || !sound) return;
    
    int channel = Mix_PlayChannel(-1, sound, 0);
    if (channel < 0) {
        // All channels busy - this is normal during intense action
        // Silently ignore instead of spamming errors
    }
}

#else  // !ExternalSound

// Stub implementations when sound is disabled
bool audio_init(AudioManager *audio) {
    if (!audio) return false;
    audio->master_volume = 128;
    audio->audio_enabled = false;
    return true;
}

void audio_cleanup(AudioManager *audio) {
    if (!audio) return;
    // Nothing to clean up
}

bool audio_load_wad(AudioManager *audio, const char *wad_filename) {
    if (!audio || !wad_filename) return false;
    // Stub - always succeeds silently
    return true;
}

void audio_set_volume(AudioManager *audio, int volume) {
    if (!audio) return;
    if (volume < 0) volume = 0;
    if (volume > 128) volume = 128;
    audio->master_volume = volume;
}

int audio_get_volume(AudioManager *audio) {
    if (!audio) return 0;
    return audio->master_volume;
}

void audio_play_music(AudioManager *audio, const char *internal_path, bool loop) {
    (void)audio;
    (void)internal_path;
    (void)loop;
    // No-op stub
}

void audio_stop_music(AudioManager *audio) {
    (void)audio;
    // No-op stub
}

void audio_pause_music(AudioManager *audio) {
    (void)audio;
    // No-op stub
}

void audio_resume_music(AudioManager *audio) {
    (void)audio;
    // No-op stub
}

void audio_play_sound(AudioManager *audio, Mix_Chunk *sound) {
    (void)audio;
    (void)sound;
    // No-op stub
}

#endif  // ExternalSound
