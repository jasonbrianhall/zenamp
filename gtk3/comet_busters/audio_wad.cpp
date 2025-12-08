#include "audio_wad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    SDL_RWops *rw = SDL_RWFromMem(wad_file.data, wad_file.size);
    if (!rw) {
        fprintf(stderr, "Failed to create SDL_RWops for %s\n", filename);
        wad_free_file(&wad_file);
        return NULL;
    }
    
    // Load audio from memory using SDL_mixer
    Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
    
    if (!chunk) {
        fprintf(stderr, "Failed to decode audio from WAD: %s - %s\n", filename, Mix_GetError());
        fprintf(stderr, "Note: Check if SDL_mixer supports this format. Install SDL2_mixer-devel.\n");
        wad_free_file(&wad_file);
        return NULL;
    }
    
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
    
    Mix_AllocateChannels(32);  // Allow up to 32 simultaneous sounds
    
    // Initialize fields
    for (int i = 0; i < 10; i++) {
        audio->music_tracks[i] = NULL;
    }
    audio->music_track_count = 0;
    audio->current_music_track = 0;
    
    audio->sfx_fire = NULL;
    audio->sfx_alien_fire = NULL;
    audio->sfx_explosion = NULL;
    audio->sfx_hit = NULL;
    audio->sfx_boost = NULL;
    audio->sfx_game_over = NULL;
    audio->sfx_wave_complete = NULL;
    
    memset(&audio->wad, 0, sizeof(WadArchive));
    
    audio->master_volume = 128;
    audio->music_volume = 100;   // Start at reasonable level (0-128)
    audio->sfx_volume = 100;     // Start at reasonable level (0-128)
    audio->audio_enabled = true;
    
    // Set initial volumes
    Mix_VolumeMusic(100);        // Music volume
    Mix_Volume(-1, 100);         // SFX volume (all channels)
    
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
    audio->sfx_fire = load_sound_from_wad(&audio->wad, "sounds/fire.mp3");
    audio->sfx_alien_fire = load_sound_from_wad(&audio->wad, "sounds/alien_fire.mp3");
    audio->sfx_explosion = load_sound_from_wad(&audio->wad, "sounds/explosion.mp3");
    audio->sfx_hit = load_sound_from_wad(&audio->wad, "sounds/hit.mp3");
    audio->sfx_boost = load_sound_from_wad(&audio->wad, "sounds/boost.mp3");
    audio->sfx_game_over = load_sound_from_wad(&audio->wad, "sounds/game_over.mp3");
    audio->sfx_wave_complete = load_sound_from_wad(&audio->wad, "sounds/wave_complete.mp3");
    
    int loaded = 0;
    if (audio->sfx_fire) loaded++;
    if (audio->sfx_alien_fire) loaded++;
    if (audio->sfx_explosion) loaded++;
    if (audio->sfx_hit) loaded++;
    if (audio->sfx_boost) loaded++;
    if (audio->sfx_game_over) loaded++;
    if (audio->sfx_wave_complete) loaded++;
    
    fprintf(stdout, "✓ Loaded %d/%d sounds from WAD\n", loaded, 7);
    
    return loaded > 0;
}

// Clean up
void audio_cleanup(AudioManager *audio) {
    if (!audio) return;
    
    // Stop music
    if (Mix_PlayingMusic()) {
        Mix_HaltMusic();
    }
    
    // Free all music tracks
    for (int i = 0; i < 10; i++) {
        if (audio->music_tracks[i]) {
            Mix_FreeMusic(audio->music_tracks[i]);
            audio->music_tracks[i] = NULL;
        }
    }
    
    // Free sounds
    if (audio->sfx_fire) Mix_FreeChunk(audio->sfx_fire);
    if (audio->sfx_alien_fire) Mix_FreeChunk(audio->sfx_alien_fire);
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
}

// Set master volume (legacy - for backward compatibility)
void audio_set_volume(AudioManager *audio, int volume) {
    if (!audio) return;
    
    if (volume < 0) volume = 0;
    if (volume > 128) volume = 128;
    
    audio->master_volume = volume;
    // Update both music and SFX proportionally
    audio_set_music_volume(audio, volume);
    audio_set_sfx_volume(audio, volume);
}

// Get master volume (legacy - for backward compatibility)
int audio_get_volume(AudioManager *audio) {
    if (!audio) return 0;
    return audio->master_volume;
}

// Set music volume (NEW)
void audio_set_music_volume(AudioManager *audio, int volume) {
    if (!audio) return;
    
    if (volume < 0) volume = 0;
    if (volume > 128) volume = 128;
    
    audio->music_volume = volume;
    Mix_VolumeMusic(volume);
    
    int percent = (volume * 100) / 128;
}

// Get music volume (NEW)
int audio_get_music_volume(AudioManager *audio) {
    if (!audio) return 0;
    return audio->music_volume;
}

// Set SFX volume (NEW)
void audio_set_sfx_volume(AudioManager *audio, int volume) {
    if (!audio) return;
    
    if (volume < 0) volume = 0;
    if (volume > 128) volume = 128;
    
    audio->sfx_volume = volume;
    Mix_Volume(-1, volume);  // Apply to all channels
    
    int percent = (volume * 100) / 128;
}

// Get SFX volume (NEW)
int audio_get_sfx_volume(AudioManager *audio) {
    if (!audio) return 0;
    return audio->sfx_volume;
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
    Mix_Music *music = Mix_LoadMUS_RW(rw, 1);
    if (!music) {
        fprintf(stderr, "Failed to load music: %s\n", Mix_GetError());
        free(music_file.data);
        return;
    }
    
    // Store in music tracks array for random playback
    if (audio->music_track_count < 10) {
        audio->music_tracks[audio->music_track_count] = music;
        audio->music_track_count++;
    }
    
    int loops = loop ? -1 : 0;
    if (Mix_PlayMusic(music, loops) < 0) {
        fprintf(stderr, "Failed to play music: %s\n", Mix_GetError());
    } else {
        fprintf(stdout, "♪ Playing: %s\n", internal_path);
    }
}

// Play random music from loaded tracks
void audio_play_random_music(AudioManager *audio) {
    if (!audio || audio->music_track_count == 0) return;
    
    // Pick a random track
    int track_index = rand() % audio->music_track_count;
    Mix_Music *music = audio->music_tracks[track_index];
    
    if (!music) return;
    
    if (Mix_PlayMusic(music, -1) < 0) {
        fprintf(stderr, "Failed to play music: %s\n", Mix_GetError());
    } else {
        fprintf(stdout, "♪ Playing random track %d/%d\n", track_index + 1, audio->music_track_count);
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
        // All channels busy - shouldn't happen with 32 channels
    }
}
