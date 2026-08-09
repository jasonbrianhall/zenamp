#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <mutex>
#include "midiplayer.h"
#include "wav_converter.h"
#include "dbopl_wrapper.h"
#include "audioconverter.h"
#include "audio_player.h"
#include "vfs.h"

// Declare external variables for MIDI state
extern double playTime;
extern bool isPlaying;
extern double playwait;
extern int globalVolume;
extern void processEvents(void);

// Additional external variables (add these)
extern int tkPtr[MAX_TRACKS];
extern double tkDelay[MAX_TRACKS];
extern int tkStatus[MAX_TRACKS];
extern bool loopStart;
extern bool loopEnd;
extern int loPtr[MAX_TRACKS];
extern double loDelay[MAX_TRACKS];
extern int loStatus[MAX_TRACKS];
extern double loopwait;
extern int rbPtr[MAX_TRACKS];
extern double rbDelay[MAX_TRACKS];
extern int rbStatus[MAX_TRACKS];
extern int TrackCount;
extern double Tempo;
extern int DeltaTicks;
extern int ChPatch[16];
extern double ChBend[16];
extern int ChVolume[16];
extern int ChPanning[16];
extern int ChVibrato[16];
extern FILE* midiFile;

// Add a mutex to protect the entire function
static std::mutex conversion_mutex;

// Function to convert MIDI to WAV
bool convertMidiToWav(const char* midi_filename, const char* wav_filename, int volume) {
    // Lock the mutex at the beginning of the function
    conversion_mutex.lock();
    
    // Reset global state variables
    playTime = 0.0;
    isPlaying = false;  // Start with false
    playwait = 0.0;
    
    // Reset all state variables completely
    for (int tk = 0; tk < MAX_TRACKS; tk++) {
        tkPtr[tk] = 0;
        tkDelay[tk] = 0;
        tkStatus[tk] = 0;
        loPtr[tk] = 0;
        loDelay[tk] = 0;
        loStatus[tk] = 0;
        rbPtr[tk] = 0;
        rbDelay[tk] = 0;
        rbStatus[tk] = 0;
    }
    
    TrackCount = 0;
    Tempo = 500000;  // Default 120 BPM
    DeltaTicks = 0;
    
    // Reset channel state
    for (int i = 0; i < 16; i++) {
        ChPatch[i] = 0;
        ChBend[i] = 0;
        ChVolume[i] = 127;
        ChPanning[i] = 0;
        ChVibrato[i] = 0;
    }
    
    // Reset loop state
    loopStart = false;
    loopEnd = false;
    loopwait = 0;
    
    // Reset OPL state completely
    OPL_Shutdown();
    
    // Now it's safe to start playing
    isPlaying = true;
    
    // Set global volume
    globalVolume = volume;
    
    // Initialize SDL and audio systems
    if (!initSDL()) {
        fprintf(stderr, "Failed to initialize SDL\n");
        conversion_mutex.unlock();
        return false;
    }
    
    // Load MIDI file
    printf("Loading %s...\n", midi_filename);
    if (!loadMidiFile(midi_filename)) {
        fprintf(stderr, "Failed to load MIDI file\n");
        cleanup();
        conversion_mutex.unlock();
        return false;
    }
    
    // Prepare WAV converter
    WAVConverter* wav_converter = wav_converter_init(
        wav_filename, 
        SAMPLE_RATE, 
        AUDIO_CHANNELS
    );
    
    if (!wav_converter) {
        fprintf(stderr, "Failed to create WAV converter\n");
        cleanup();
        conversion_mutex.unlock();
        return false;
    }
    
    // Temporary buffer for audio generation
    int16_t audio_buffer[AUDIO_BUFFER * AUDIO_CHANNELS];
    
    printf("Converting %s to WAV (Volume: %d%%)...\n", midi_filename, globalVolume);
    
    // Duration of an audio buffer in seconds
    double buffer_duration = (double)AUDIO_BUFFER / SAMPLE_RATE;
    
    // Begin conversion
    int previous_seconds = -1;
    
    // Initialize playwait for the first events
    processEvents();
    
    // Continue processing as long as the MIDI is still playing
    while (isPlaying) {
        // Generate audio block
        memset(audio_buffer, 0, sizeof(audio_buffer));
        OPL_Generate(audio_buffer, AUDIO_BUFFER);
        
        // Write to WAV file
        if (!wav_converter_write(wav_converter, audio_buffer, AUDIO_BUFFER * AUDIO_CHANNELS)) {
            fprintf(stderr, "Failed to write audio data\n");
            break;
        }
        
        // Update playTime
        playTime += buffer_duration;
        
        // Process events if needed
        playwait -= buffer_duration;
        
        // Process events when timer reaches zero or below
        while (playwait <= 0 && isPlaying) {
            processEvents();
        }
        
        // Display progress
        int current_seconds = (int)playTime;
        if (current_seconds > previous_seconds) {
            printf("\rConverting... %d seconds", current_seconds);
            fflush(stdout);
            previous_seconds = current_seconds;
        }
    }
    
    printf("\nFinishing conversion...\n");
    
    // Finalize WAV file
    wav_converter_finish(wav_converter);
    wav_converter_free(wav_converter);
    
    // Cleanup
    //cleanup();
    
    // Reset state variables before unlocking
    playTime = 0;
    isPlaying = false;
    playwait = 0.0;
    
    // Make sure midiFile is NULL
    if (midiFile) {
        fclose(midiFile);
        midiFile = NULL;
    }
    
    conversion_mutex.unlock();
    
    return true;
}

bool convert_midi_to_wav(AudioPlayer *player, const char* filename) {
    // Check cache first
    const char* cached_file = get_cached_conversion(&player->conversion_cache, filename);
    if (cached_file) {
        strncpy(player->temp_wav_file, cached_file, sizeof(player->temp_wav_file) - 1);
        player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
        return true;
    }
    
    // Generate a unique virtual filename
    static int virtual_counter = 0;
    char virtual_filename[256];
    snprintf(virtual_filename, sizeof(virtual_filename), "virtual_midi_%d.wav", virtual_counter++);
    
    strncpy(player->temp_wav_file, virtual_filename, sizeof(player->temp_wav_file) - 1);
    player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
    
    printf("Converting MIDI to virtual WAV: %s -> %s\n", filename, virtual_filename);
    
    // Try conversion up to 2 times (initial attempt + 1 retry)
    int max_attempts = 2;
    bool conversion_successful = false;
    
    for (int attempt = 1; attempt <= max_attempts && !conversion_successful; attempt++) {
        if (attempt > 1) {
            printf("MIDI conversion attempt %d failed (duration: %.2f seconds), retrying...\n", 
                   attempt - 1, playTime);
            
            // Clean up any partial virtual file from previous attempt
            delete_virtual_file(virtual_filename);
            
            // Generate new virtual filename for retry
            snprintf(virtual_filename, sizeof(virtual_filename), "virtual_midi_%d_retry%d.wav", 
                     virtual_counter++, attempt - 1);
            strncpy(player->temp_wav_file, virtual_filename, sizeof(player->temp_wav_file) - 1);
            player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
            
            // Longer delay between attempts for more thorough cleanup
            SDL_Delay(500);
        }
        
        printf("MIDI conversion attempt %d starting...\n", attempt);
        
        // COMPLETELY shut down SDL before conversion
        if (player->audio_device) {
            SDL_PauseAudioDevice(player->audio_device, 1);  // Pause first
            SDL_CloseAudioDevice(player->audio_device);
            player->audio_device = 0;
            printf("Closed existing SDL audio device (attempt %d)\n", attempt);
        }
        
        // Complete SDL shutdown with more thorough cleanup
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_QuitSubSystem(SDL_INIT_TIMER);
        SDL_Quit();
        printf("SDL completely shut down (attempt %d)\n", attempt);
        
        // Longer delay to ensure complete cleanup
        SDL_Delay(200);
        
        // FORCE RESET ALL GLOBAL MIDI STATE
        // This is crucial - reset all the global variables used by the MIDI player
        playTime = 0.0;
        isPlaying = false;
        paused = false;
        playwait = 0.0;
        
        // Reset MIDI file state
        extern FILE* midiFile;
        extern int TrackCount;
        extern int DeltaTicks;
        extern double Tempo;
        extern int tkPtr[MAX_TRACKS];
        extern double tkDelay[MAX_TRACKS];
        extern int tkStatus[MAX_TRACKS];
        extern bool loopStart;
        extern bool loopEnd;
        extern int loPtr[MAX_TRACKS];
        extern double loDelay[MAX_TRACKS];
        extern int loStatus[MAX_TRACKS];
        extern double loopwait;
        extern int rbPtr[MAX_TRACKS];
        extern double rbDelay[MAX_TRACKS];
        extern int rbStatus[MAX_TRACKS];
        
        // Close any existing MIDI file handle
        if (midiFile) {
            fclose(midiFile);
            midiFile = NULL;
        }
        
        // Reset all track state arrays
        TrackCount = 0;
        DeltaTicks = 0;
        Tempo = 500000;  // Default 120 BPM
        loopStart = false;
        loopEnd = false;
        loopwait = 0.0;
        
        for (int i = 0; i < MAX_TRACKS; i++) {
            tkPtr[i] = 0;
            tkDelay[i] = 0.0;
            tkStatus[i] = 0;
            loPtr[i] = 0;
            loDelay[i] = 0.0;
            loStatus[i] = 0;
            rbPtr[i] = 0;
            rbDelay[i] = 0.0;
            rbStatus[i] = 0;
        }
        
        // Reset MIDI channel state
        extern int ChPatch[16];
        extern double ChBend[16];
        extern int ChVolume[16];
        extern int ChPanning[16];
        extern int ChVibrato[16];
        
        for (int i = 0; i < 16; i++) {
            ChPatch[i] = 0;
            ChBend[i] = 0.0;
            ChVolume[i] = 127;
            ChPanning[i] = 64;
            ChVibrato[i] = 0;
        }
        
        // Reset virtual mixer globals
        extern VirtualMixer* g_midi_mixer;
        extern int g_midi_mixer_channel;
        
        if (g_midi_mixer) {
            if (g_midi_mixer_channel >= 0) {
                mixer_release_channel(g_midi_mixer, g_midi_mixer_channel);
                g_midi_mixer_channel = -1;
            }
            mixer_free(g_midi_mixer);
            g_midi_mixer = NULL;
        }
        
        printf("Reset all global MIDI state (attempt %d)\n", attempt);
        
        // Initialize SDL fresh for MIDI conversion
        if (!initSDL()) {
            printf("SDL init for conversion failed (attempt %d)\n", attempt);
            // Try to restore audio for main player
            if (!init_audio(player)) {
                printf("Failed to restore main audio after conversion failure (attempt %d)\n", attempt);
            }
            continue; // Try next attempt or exit loop
        }
        printf("SDL reinitialized for MIDI conversion (attempt %d)\n", attempt);
        
        if (!loadMidiFile(filename)) {
            printf("MIDI file load failed (attempt %d)\n", attempt);
            cleanup();
            // Try to restore audio for main player
            if (!init_audio(player)) {
                printf("Failed to restore main audio after MIDI load failure (attempt %d)\n", attempt);
            }
            continue; // Try next attempt or exit loop
        }
        printf("MIDI file loaded successfully (attempt %d)\n", attempt);
        
        VirtualWAVConverter* wav_converter = virtual_wav_converter_init(virtual_filename, SAMPLE_RATE, AUDIO_CHANNELS);
        if (!wav_converter) {
            printf("Virtual WAV converter init failed (attempt %d)\n", attempt);
            cleanup();
            // Try to restore audio for main player
            if (!init_audio(player)) {
                printf("Failed to restore main audio after converter init failure (attempt %d)\n", attempt);
            }
            continue; // Try next attempt or exit loop
        }
        printf("Virtual WAV converter initialized (attempt %d)\n", attempt);
        
        // Reset timing before starting conversion
        playTime = 0.0;
        isPlaying = true;
        playwait = 0.0;
        
        int16_t audio_buffer[AUDIO_BUFFER * AUDIO_CHANNELS];
        double buffer_duration = (double)AUDIO_BUFFER / SAMPLE_RATE;
        
        // Initial event processing
        processEvents();
        
        printf("Starting MIDI audio generation (attempt %d)...\n", attempt);
        
        // Conversion loop with timeout protection
        int conversion_timeout = 300; // 5 minutes max
        int seconds_elapsed = 0;
        
        while (isPlaying && seconds_elapsed < conversion_timeout) {
            memset(audio_buffer, 0, sizeof(audio_buffer));
            OPL_Generate(audio_buffer, AUDIO_BUFFER);
            
            if (!virtual_wav_converter_write(wav_converter, audio_buffer, AUDIO_BUFFER * AUDIO_CHANNELS)) {
                printf("Virtual WAV write failed (attempt %d)\n", attempt);
                break;
            }
            
            playTime += buffer_duration;
            playwait -= buffer_duration;
            
            while (playwait <= 0 && isPlaying) {
                processEvents();
            }
            
            // Update timeout counter
            if (((int)playTime) != seconds_elapsed) {
                seconds_elapsed = (int)playTime;
                if (seconds_elapsed % 10 == 0 && seconds_elapsed > 0) {
                    printf("Converting... %d seconds (attempt %d)\n", seconds_elapsed, attempt);
                }
            }
        }
        
        // Check for timeout
        if (seconds_elapsed >= conversion_timeout) {
            printf("MIDI conversion timed out after %d seconds (attempt %d)\n", conversion_timeout, attempt);
            playTime = 0.0; // Force failure
        }
        
        virtual_wav_converter_finish(wav_converter);
        virtual_wav_converter_free(wav_converter);
        cleanup();  // This should clean up SDL used for conversion
        
        printf("Virtual conversion complete (attempt %d): %.2f seconds\n", attempt, playTime);
        
        // Check if conversion was successful (duration > 0.1 seconds)
        if (playTime > 0.1) {
            printf("MIDI conversion successful on attempt %d\n", attempt);
            conversion_successful = true;
            
            // Add to cache after successful conversion
            add_to_conversion_cache(&player->conversion_cache, filename, virtual_filename);
        } else {
            printf("MIDI conversion failed on attempt %d (duration too short: %.2f seconds)\n", 
                   attempt, playTime);
            
            // Clean up failed conversion virtual file
            delete_virtual_file(virtual_filename);
            
            if (attempt == max_attempts) {
                printf("All MIDI conversion attempts failed, giving up\n");
            }
        }
        
        // COMPLETELY reinitialize SDL for main player
        SDL_Quit();  // Ensure clean state again
        SDL_Delay(200);  // Longer delay for thorough cleanup
        
        // Reinitialize the main SDL audio system for playback
        printf("Reinitializing SDL audio for main player (attempt %d)...\n", attempt);
        if (!init_audio(player)) {
            printf("Failed to reinitialize audio for playback (attempt %d)\n", attempt);
            if (!conversion_successful) {
                return false;
            }
        } else {
            printf("SDL audio reinitialized successfully for main player (attempt %d)\n", attempt);
        }
    }
    
    return conversion_successful;
}
