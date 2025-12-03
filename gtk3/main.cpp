#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <ctype.h>
#include <SDL2/SDL.h>
#include <signal.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <shlobj.h>
#include <gdk/gdkwin32.h>
#include <windows.h>
#endif

#include "visualization.h"
#include "midiplayer.h"
#include "dbopl_wrapper.h"
#include "wav_converter.h"
#include "audioconverter.h"
#include "convertoggtowav.h"
#include "convertopustowav.h"
#include "audio_player.h"
#include "vfs.h"
#include "aiff.h"
#include "equalizer.h"
#include "zip_support.h"

extern double playTime;
extern bool isPlaying;
extern bool paused;
extern int globalVolume;
extern void processEvents(void);
extern double playwait;

AudioPlayer *player = NULL;

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nReceived signal %d, initiating graceful shutdown...\n", sig);
        
        if (player) {
            // Save current queue before exit
            save_current_queue_on_exit(player);
            save_player_settings(player);
            
            // Stop playback if playing
            if (isPlaying) {
                stop_playback(player);
            }
            
            // Cleanup all resources in the same order as on_window_delete_event
            clear_queue(&player->queue);
            cleanup_queue_filter(player);
            cleanup_conversion_cache(&player->conversion_cache);
            cleanup_audio_cache(&player->audio_cache); 
            cleanup_virtual_filesystem();
            
            printf("Cleaning up Audio\n");
            if (player->audio_buffer.data) free(player->audio_buffer.data);

            if (player->cdg_display) {
                cdg_display_free(player->cdg_display);
            }    

            printf("Closing SDL audio device\n");
            if (player->audio_device) SDL_CloseAudioDevice(player->audio_device);

            printf("Cleaning Equalizer\n");
            if (player->equalizer) {
                equalizer_free(player->equalizer);
            }
            
            printf("Destroying audio mutex\n");
            pthread_mutex_destroy(&player->audio_mutex);

            printf("Freeing player\n");
            g_free(player);
            player = NULL;
        }

        printf("Closing SDL\n");
        SDL_Quit();
        
        printf("Exiting application\n");
        exit(0);
    }
}

// Prevent Windows from sleeping during playback
static void prevent_system_sleep(void) {
    #ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    #endif
}

// Allow Windows to sleep normally
static void allow_system_sleep(void) {
    #ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
    #endif
}

bool ends_with_zip(const char *filename) {
    size_t len = strlen(filename);
    if (len < 4) return false;

    const char *ext = filename + len - 4;
    return tolower(ext[0]) == '.' &&
           tolower(ext[1]) == 'z' &&
           tolower(ext[2]) == 'i' &&
           tolower(ext[3]) == 'p';
}

// Custom sort function for duration column (convert MM:SS to seconds for numeric sorting)
static gint duration_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data) {
    (void)user_data;  // unused
    
    char *duration_a = NULL;
    char *duration_b = NULL;
    
    gtk_tree_model_get(model, a, COL_DURATION, &duration_a, -1);
    gtk_tree_model_get(model, b, COL_DURATION, &duration_b, -1);
    
    int seconds_a = 0, seconds_b = 0;
    
    // Parse "MM:SS" format to total seconds
    if (duration_a && sscanf(duration_a, "%d:%d", &seconds_a, &seconds_b) == 2) {
        // First number is minutes, second is seconds
        int temp = seconds_a * 60 + seconds_b;
        seconds_a = temp;
    } else {
        seconds_a = 0;
    }
    
    seconds_b = 0;
    int dummy = 0;
    if (duration_b && sscanf(duration_b, "%d:%d", &dummy, &seconds_b) == 2) {
        // First number is minutes, second is seconds
        int temp = dummy * 60 + seconds_b;
        seconds_b = temp;
    } else {
        seconds_b = 0;
    }
    
    g_free(duration_a);
    g_free(duration_b);
    
    if (seconds_a < seconds_b) return -1;
    if (seconds_a > seconds_b) return 1;
    return 0;
}

static GtkWidget *vis_fullscreen_window = NULL;
static bool is_vis_fullscreen = false;
static GtkWidget *original_vis_parent = NULL;
static int original_vis_width = 0;
static int original_vis_height = 0;

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>



bool open_windows_file_dialog(char* filename, size_t filename_size, bool multiple = false) {
    OPENFILENAME ofn;
    
    // Clear the buffer
    memset(filename, 0, filename_size);
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = (DWORD)filename_size;
    ofn.lpstrFilter = "All Supported\0*.mid;*.midi;*.wav;*.mp3;*.m4a;*.aiff;*.aif;*.ogg;*.flac;*.opus;*.wma;*.lrc;*.zip\0"
                      "MIDI Files\0*.mid;*.midi\0"
                      "WAV Files\0*.wav\0"
                      "MP3 Files\0*.mp3\0"
                      "M4A Files\0*.m4a\0"
                      "OGG Files\0*.ogg\0"
                      "FLAC Files\0*.flac\0"
                      "AIFF Files\0*.aiff\0"
                      "Opus Files\0*.opus\0"
                      "WMA Files\0*.wma\0"
                      "CD+G Files\0*.zip\0"
                      "Lyric Files\0*.lrc\0"
                      "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (multiple) {
        ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
        printf("Opening Windows file dialog for multiple selection\n");
    } else {
        printf("Opening Windows file dialog for single selection\n");
    }
    
    BOOL result = GetOpenFileName(&ofn);
    
    if (result) {
        printf("File dialog returned successfully\n");
        return true;
    } else {
        DWORD error = CommDlgExtendedError();
        if (error != 0) {
            printf("File dialog error: %lu\n", error);
        } else {
            printf("File dialog cancelled by user\n");
        }
        return false;
    }
}
#endif

// Queue management functions
void init_queue(PlayQueue *queue) {
    queue->files = NULL;
    queue->count = 0;
    queue->capacity = 0;
    queue->current_index = -1;
    queue->repeat_queue = true;
}

void clear_queue(PlayQueue *queue) {
    for (int i = 0; i < queue->count; i++) {
        g_free(queue->files[i]);
    }
    g_free(queue->files);
    queue->files = NULL;
    queue->count = 0;
    queue->capacity = 0;
    queue->current_index = -1;
}

bool add_to_queue(PlayQueue *queue, const char *filename) {
    if (queue->count >= queue->capacity) {
        int new_capacity = queue->capacity == 0 ? 10 : queue->capacity * 2;
        char **new_files = g_realloc(queue->files, new_capacity * sizeof(char*));
        if (!new_files) return false;
        
        queue->files = new_files;
        queue->capacity = new_capacity;
    }
    
    queue->files[queue->count] = g_strdup(filename);
    queue->count++;
    
    if (queue->current_index == -1) {
        queue->current_index = 0;
    }
    
    return true;
}

const char* get_current_queue_file(PlayQueue *queue) {
    if (queue->count == 0 || queue->current_index < 0 || queue->current_index >= queue->count) {
        return NULL;
    }
    return queue->files[queue->current_index];
}

bool advance_queue(PlayQueue *queue) {
    if (queue->count == 0) {
        printf("advance_queue: Empty queue\n");
        return false;
    }
    
    if (queue->count == 1) {
        printf("advance_queue: Single song queue - %s repeat\n", 
               queue->repeat_queue ? "restarting (repeat on)" : "stopping (repeat off)");
        if (queue->repeat_queue) {
            // For single song, just stay at index 0
            queue->current_index = 0;
            return true;
        } else {
            return false;
        }
    }
    
    printf("advance_queue: Before - index %d of %d\n", queue->current_index, queue->count);
    
    queue->current_index++;
    
    if (queue->current_index >= queue->count) {
        if (queue->repeat_queue) {
            queue->current_index = 0;
            printf("advance_queue: Wrapped to beginning (repeat on)\n");
            return true;
        } else {
            queue->current_index = queue->count - 1; // Stay at last song
            printf("advance_queue: At end, no repeat\n");
            return false;
        }
    }
    
    printf("advance_queue: After - index %d of %d\n", queue->current_index, queue->count);
    return true;
}

bool previous_queue(PlayQueue *queue) {
    if (queue->count == 0) {
        printf("previous_queue: Empty queue\n");
        return false;
    }
    
    printf("previous_queue: Before - index %d of %d\n", queue->current_index, queue->count);
    
    queue->current_index--;
    
    if (queue->current_index < 0) {
        if (queue->repeat_queue) {
            queue->current_index = queue->count - 1;
            printf("previous_queue: Wrapped to end (repeat on)\n");
            return true;
        } else {
            queue->current_index = 0;
            printf("previous_queue: At beginning, no repeat\n");
            return false;
        }
    }
    
    printf("previous_queue: After - index %d of %d\n", queue->current_index, queue->count);
    return true;
}

void on_remove_from_queue_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;
    
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "queue_index"));
    AudioPlayer *player = (AudioPlayer*)g_object_get_data(G_OBJECT(button), "player");
    
    printf("Removing item %d from queue\n", index);
    
    bool was_current_playing = (index == player->queue.current_index && player->is_playing);
    bool queue_will_be_empty = (player->queue.count <= 1);
    
    if (remove_from_queue(&player->queue, index)) {
        if (queue_will_be_empty) {
            // Queue is now empty, stop playback and clear everything
            stop_playback(player);
            player->is_loaded = false;
            gtk_label_set_text(GTK_LABEL(player->file_label), "No file loaded");
        } else if (was_current_playing) {
            // We removed the currently playing song, load the next one
            stop_playback(player);
            if (load_file_from_queue(player)) {
                update_gui_state(player);
                start_playback(player);
            } else {
                // Failed to load next file - ensure consistent UI state
                printf("Failed to load next track after removal\n");
                player->is_loaded = false;
                gtk_label_set_text(GTK_LABEL(player->file_label), "No file loaded");
                update_gui_state(player);
            }
            if (player->cdg_display) {
                cdg_reset(player->cdg_display);
                player->cdg_display->packet_count = 0; // Mark as invalid
                player->has_cdg = false;
            }
        }
        
        update_queue_display_with_filter(player);
        update_gui_state(player);
    }
}

void audio_callback(void* userdata, Uint8* stream, int len) {
    AudioPlayer* player = (AudioPlayer*)userdata;
    memset(stream, 0, len);
    
    if (pthread_mutex_trylock(&player->audio_mutex) != 0) return;
    
    if (!player->is_playing || player->is_paused || !player->audio_buffer.data) {
        pthread_mutex_unlock(&player->audio_mutex);
        return;
    }
    
    int16_t* output = (int16_t*)stream;
    int samples_requested = len / sizeof(int16_t);
    size_t samples_remaining = player->audio_buffer.length - player->audio_buffer.position;
    
    // Apply speed control
    double speed = player->playback_speed;
    if (speed <= 0.0) speed = 1.0; // Safety check
    
    int samples_to_process = 0;
    
    for (int i = 0; i < samples_requested && player->audio_buffer.position < player->audio_buffer.length; i++) {
        // Get current sample with volume and EQ processing
        int32_t sample = player->audio_buffer.data[player->audio_buffer.position];
        sample = (sample * globalVolume) / 100;
        if (sample > 32767) sample = 32767;
        else if (sample < -32768) sample = -32768;
        
        // Apply equalizer
        int16_t eq_sample = equalizer_process_sample(player->equalizer, (int16_t)sample);
        output[i] = eq_sample;
        
        samples_to_process++;
        
        // Advance position based on speed
        player->speed_accumulator += speed;
        
        // Move to next sample when accumulator >= 1.0
        while (player->speed_accumulator >= 1.0 && player->audio_buffer.position < player->audio_buffer.length) {
            player->audio_buffer.position++;
            player->speed_accumulator -= 1.0;
        }
    }
    
    // Feed processed audio to visualizer
    if (player->visualizer && samples_to_process > 0) {
        size_t sample_count = samples_to_process / player->channels;
        visualizer_update_audio_data(player->visualizer, output, sample_count, player->channels);
    }
    
    // Check if playback finished
    if (player->audio_buffer.position >= player->audio_buffer.length) {
        player->is_playing = false;
    }
    
    pthread_mutex_unlock(&player->audio_mutex);
}

bool init_audio(AudioPlayer *player, int sample_rate, int channels) {
#ifdef _WIN32
    // Try different audio drivers in order of preference
    const char* drivers[] = {"directsound", "winmm", "wasapi", NULL};
    for (int i = 0; drivers[i]; i++) {
        if (SDL_SetHint(SDL_HINT_AUDIODRIVER, drivers[i])) {
            printf("Trying SDL audio driver: %s\n", drivers[i]);
            if (SDL_Init(SDL_INIT_AUDIO) == 0) {
                printf("Successfully initialized with driver: %s\n", drivers[i]);
                break;
            } else {
                printf("Failed with driver %s: %s\n", drivers[i], SDL_GetError());
                SDL_Quit();
            }
        }
    }
#else
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }
#endif

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = sample_rate;  // Use actual file sample rate
    want.format = AUDIO_S16SYS;
    want.channels = channels;  // Use actual file channels
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = player;
    
    // Close existing audio device if open
    if (player->audio_device) {
        SDL_CloseAudioDevice(player->audio_device);
    }
    
    player->audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &player->audio_spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (player->audio_device == 0) {
        printf("Audio device open failed: %s\n", SDL_GetError());
        return false;
    }
    
    printf("Audio: %d Hz, %d channels\n", player->audio_spec.freq, player->audio_spec.channels);
    
    // Reinitialize equalizer with new sample rate if it exists
    if (player->equalizer && player->equalizer->sample_rate != sample_rate) {
        printf("Reinitializing equalizer for new sample rate: %d Hz\n", sample_rate);
        equalizer_free(player->equalizer);
        player->equalizer = equalizer_new(sample_rate);
    }
    
    return true;
}

bool load_wav_file(AudioPlayer *player, const char* wav_path) {
    // Check cache first
    CachedAudioBuffer *cached = find_in_cache(&player->audio_cache, wav_path);
    if (cached) {
        player->sample_rate = cached->sample_rate;
        player->channels = cached->channels;
        player->bits_per_sample = cached->bits_per_sample;
        player->song_duration = cached->song_duration;
        
        if (!init_audio(player, player->sample_rate, player->channels)) {
            return false;
        }
        
        // COPY data from cache (don't give away the cached pointer)
        int16_t *data_copy = malloc(cached->length * sizeof(int16_t));
        if (!data_copy) return false;
        memcpy(data_copy, cached->data, cached->length * sizeof(int16_t));
        
        pthread_mutex_lock(&player->audio_mutex);
        if (player->audio_buffer.data) free(player->audio_buffer.data);
        player->audio_buffer.data = data_copy;
        player->audio_buffer.length = cached->length;
        player->audio_buffer.position = 0;
        pthread_mutex_unlock(&player->audio_mutex);
        
        printf("Loaded from cache: %zu samples\n", cached->length);
        return true;
    }
    
    // Not in cache, load from file
    FILE* wav_file = fopen(wav_path, "rb");
    if (!wav_file) {
        printf("Cannot open WAV file: %s\n", wav_path);
        return false;
    }
    
    // Read WAV header
    char header[44];
    if (fread(header, 1, 44, wav_file) != 44) {
        printf("Cannot read WAV header\n");
        fclose(wav_file);
        return false;
    }
    
    // Verify WAV format
    if (strncmp(header, "RIFF", 4) != 0 || strncmp(header + 8, "WAVE", 4) != 0) {
        printf("Invalid WAV format\n");
        fclose(wav_file);
        return false;
    }
    
    // Extract WAV info
    player->sample_rate = *(int*)(header + 24);
    player->channels = *(short*)(header + 22);
    player->bits_per_sample = *(short*)(header + 34);
    
    printf("WAV: %d Hz, %d channels, %d bits\n", player->sample_rate, player->channels, player->bits_per_sample);
    
    // Reinitialize audio with the correct sample rate and channels
    if (!init_audio(player, player->sample_rate, player->channels)) {
        printf("Failed to reinitialize audio for WAV format\n");
        fclose(wav_file);
        return false;
    }
    
    // Get file size and calculate duration
    fseek(wav_file, 0, SEEK_END);
    long file_size = ftell(wav_file);
    long data_size = file_size - 44;
    
    player->song_duration = data_size / (double)(player->sample_rate * player->channels * (player->bits_per_sample / 8));
    printf("WAV duration: %.2f seconds\n", player->song_duration);
    
    // Allocate and read audio data
    int16_t* wav_data = (int16_t*)malloc(data_size);
    if (!wav_data) {
        printf("Memory allocation failed\n");
        fclose(wav_file);
        return false;
    }
    
    fseek(wav_file, 44, SEEK_SET);
    if (fread(wav_data, 1, data_size, wav_file) != (size_t)data_size) {
        printf("WAV data read failed\n");
        free(wav_data);
        fclose(wav_file);
        return false;
    }
    
    fclose(wav_file);
    
    // Make a copy for cache
    int16_t *cache_copy = malloc(data_size);
    if (cache_copy) {
        memcpy(cache_copy, wav_data, data_size);
        add_to_cache(&player->audio_cache, wav_path, cache_copy, 
                     data_size / sizeof(int16_t), player->sample_rate,
                     player->channels, player->bits_per_sample, 
                     player->song_duration);
    }
    
    // Store in audio buffer
    pthread_mutex_lock(&player->audio_mutex);
    if (player->audio_buffer.data) free(player->audio_buffer.data);
    player->audio_buffer.data = wav_data;
    player->audio_buffer.length = data_size / sizeof(int16_t);
    player->audio_buffer.position = 0;
    pthread_mutex_unlock(&player->audio_mutex);
    
    printf("Loaded %zu samples\n", player->audio_buffer.length);
    return true;
}

void on_speed_changed(GtkRange *range, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    double speed = gtk_range_get_value(range);
    
    pthread_mutex_lock(&player->audio_mutex);
    player->playback_speed = speed;
    // Reset accumulator when speed changes to avoid glitches
    player->speed_accumulator = 0.0;
    pthread_mutex_unlock(&player->audio_mutex);
    
    // Update the tooltip to show current speed
    char tooltip[64];
    snprintf(tooltip, sizeof(tooltip), "Playback speed: %.2fx", speed);
    gtk_widget_set_tooltip_text(GTK_WIDGET(range), tooltip);
    
    printf("Speed changed to: %.2fx\n", speed);
}


bool load_file(AudioPlayer *player, const char *filename) {
    printf("load_file called for: %s\n", filename);
    
    // Stop current playback and clean up timer
    if (player->is_playing || player->update_timer_id > 0) {
        printf("Stopping current playback...\n");
        pthread_mutex_lock(&player->audio_mutex);
        player->is_playing = false;
        player->is_paused = false;
        SDL_PauseAudioDevice(player->audio_device, 1);
        pthread_mutex_unlock(&player->audio_mutex);
        
        if (player->update_timer_id > 0) {
            g_source_remove(player->update_timer_id);
            player->update_timer_id = 0;
            printf("Removed existing timer\n");
        }
    }
    
    // Clear CDG state ONLY if we're not loading from inside a ZIP
    if (!player->is_loading_cdg_from_zip && player->cdg_display) {
        cdg_reset(player->cdg_display);
        player->cdg_display->packet_count = 0;
        player->has_cdg = false;
    }
    
    // Check if this is a virtual file (starts with "virtual_")
    if (strncmp(filename, "virtual_", 8) == 0) {
        printf("Loading virtual WAV file: %s\n", filename);
        return load_virtual_wav_file(player, filename);
    }
    
    // Determine file type for regular files
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        printf("Unknown file type\n");
        return false;
    }
    
    // Convert extension to lowercase
    char ext_lower[10];
    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
    ext_lower[sizeof(ext_lower) - 1] = '\0';
    for (int i = 0; ext_lower[i]; i++) {
        ext_lower[i] = tolower(ext_lower[i]);
    }
    
    bool success = false;
    bool is_zip_file = false;
    
    if (strcmp(ext_lower, ".wav") == 0) {
        printf("Loading WAV file: %s\n", filename);
        success = load_wav_file(player, filename);
    } else if (strcmp(ext_lower, ".mid") == 0 || strcmp(ext_lower, ".midi") == 0) {
        printf("Loading MIDI file: %s\n", filename);
        if (convert_midi_to_wav(player, filename)) {
            printf("Now loading converted virtual WAV file: %s\n", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".mp3") == 0) {
        printf("Loading MP3 file: %s\n", filename);
        if (convert_mp3_to_wav(player, filename)) {
            printf("Now loading converted virtual WAV file: %s\n", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".ogg") == 0) {
        printf("Loading OGG file: %s\n", filename);
        if (convert_ogg_to_wav(player, filename)) {
            printf("Now loading converted virtual WAV file: %s\n", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".flac") == 0) {
        printf("Loading FLAC file: %s\n", filename);
        if (convert_flac_to_wav(player, filename)) {
            printf("Now loading converted virtual WAV file: %s\n", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".aif") == 0 || strcmp(ext_lower, ".aiff") == 0) {
        printf("Loading AIFF file: %s\n", filename);
        if (convert_aiff_to_wav(player, filename)) {
            printf("Now loading converted virtual WAV file: %s\n", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".opus") == 0) {
        printf("Loading Opus file: %s\n", filename);
        if (convert_opus_to_wav(player, filename)) {
            printf("Now loading converted virtual WAV file: %s\n", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".m4a") == 0) {
        printf("Loading M4A file: %s\n", filename);
        if (convert_m4a_to_wav(player, filename)) {
            printf("Now loading converted virtual WAV file: %s\n", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".wma") == 0) {
        printf("Loading WMA file: %s\n", filename);
        if (convert_wma_to_wav(player, filename)) {
            printf("Now loading converted virtual WAV file: %s\n", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".lrc") == 0) {
        printf("Generating karaoke ZIP from LRC: %s\n", filename);
        is_zip_file = true;
        // Generate ZIP from LRC and matching audio
        std::string zip_path;
        bool zip_success = generate_karaoke_zip_from_lrc(filename, zip_path);  // <-- utility function

        if (zip_success) {
            KaraokeZipContents zip_contents;
            if (extract_karaoke_zip(zip_path.c_str(), &zip_contents)) {
                player->karaoke_temp_files = zip_contents;

                if (!player->cdg_display) {
                    player->cdg_display = cdg_display_new();
                }

                if (player->cdg_display && cdg_load_file(player->cdg_display, zip_contents.cdg_file)) {
                    player->has_cdg = true;
                    player->is_loading_cdg_from_zip = true;
  
                    if (player->visualizer) {
                        player->visualizer->cdg_display = player->cdg_display;
                        visualizer_set_type(player->visualizer, VIS_KARAOKE);
                    }

                    success = load_file(player, zip_contents.audio_file);
                    player->is_loading_cdg_from_zip = false;

                    if (success) {
                        printf("Loaded karaoke ZIP successfully\n");
                        strncpy(player->current_file, zip_path.c_str(), 1023);
                        player->current_file[1023] = '\0';

                        char *metadata = extract_metadata(zip_contents.audio_file);
                        gtk_label_set_markup(GTK_LABEL(player->metadata_label), metadata);
                        g_free(metadata);
                    } else {
                        printf("Failed to load audio from generated ZIP\n");
                        cleanup_karaoke_temp_files(&player->karaoke_temp_files);
                    }
                } else {
                    printf("Failed to load CDG from generated ZIP\n");
                    cleanup_karaoke_temp_files(&zip_contents);
                }
            } else {
                printf("Failed to extract generated karaoke ZIP\n");
            }
        } else {
            printf("Failed to generate karaoke ZIP from LRC\n");
        }
    } else if (strcmp(ext_lower, ".zip") == 0) {
        printf("Loading karaoke ZIP file: %s\n", filename);
        is_zip_file = true;
    
        KaraokeZipContents zip_contents;
        if (extract_karaoke_zip(filename, &zip_contents)) {
            player->karaoke_temp_files = zip_contents;
    
            if (!player->cdg_display) {
                player->cdg_display = cdg_display_new();
            }
        
            if (player->cdg_display && cdg_load_file(player->cdg_display, zip_contents.cdg_file)) {
                player->has_cdg = true;
                player->is_loading_cdg_from_zip = true;
            
                if (player->visualizer) {
                    player->visualizer->cdg_display = player->cdg_display;
                    visualizer_set_type(player->visualizer, VIS_KARAOKE);
                }
            
                success = load_file(player, zip_contents.audio_file);
                
                player->is_loading_cdg_from_zip = false;
            
                if (success) {
                    printf("Loaded karaoke ZIP successfully\n");
                    strncpy(player->current_file, filename, 1023);
                    player->current_file[1023] = '\0';
                    
                    // Extract metadata from the actual audio file inside the ZIP
                    char *metadata = extract_metadata(zip_contents.audio_file);
                    gtk_label_set_markup(GTK_LABEL(player->metadata_label), metadata);
                    g_free(metadata);
                } else {
                    printf("Failed to load audio from ZIP\n");
                    cleanup_karaoke_temp_files(&player->karaoke_temp_files);
                }
            } else {
                printf("Failed to load CDG from ZIP\n");
                cleanup_karaoke_temp_files(&zip_contents);
            }
        } else {
            printf("Failed to extract karaoke ZIP\n");
        }
    } else {
        printf("Trying to load unknown file: %s\n", filename);
        if (convert_audio_to_wav(player, filename)) {
            printf("Now loading converted virtual WAV file: %s\n", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        } else {
            printf("File isn't supported\n");
        }
    }
    
    if (success && !is_zip_file) {
        strncpy(player->current_file, filename, 1023);
        player->current_file[1023] = '\0';
        player->is_loaded = true;
        player->is_playing = false;
        player->is_paused = false;
        playTime = 0;

        if (player->has_cdg && player->visualizer) {
            visualizer_set_type(player->visualizer, VIS_KARAOKE);
        }
        
        // Extract and display metadata (for non-ZIP files)
        char *metadata = extract_metadata(filename);
        gtk_label_set_markup(GTK_LABEL(player->metadata_label), metadata);
        g_free(metadata);
        
        gtk_range_set_range(GTK_RANGE(player->progress_scale), 0.0, player->song_duration);
        gtk_range_set_value(GTK_RANGE(player->progress_scale), 0.0);
        
        if (player->audio_buffer.length == 0 || player->song_duration <= 0.1) {
            printf("Warning: File loaded but has no/minimal audio data (duration: %.2f, samples: %zu)\n", 
                   player->song_duration, player->audio_buffer.length);
            printf("Skipping this file and advancing to next...\n");
            
            if (strncmp(player->temp_wav_file, "virtual_", 8) == 0) {
                delete_virtual_file(player->temp_wav_file);
            }
            
            update_gui_state(player);
            
            if (player->queue.count > 1) {
                g_timeout_add(100, [](gpointer data) -> gboolean {
                    AudioPlayer *p = (AudioPlayer*)data;
                    printf("Auto-advancing from invalid file...\n");
                    if (advance_queue(&p->queue)) {
                        if (load_file_from_queue(p)) {
                            update_queue_display(p);
                            update_gui_state(p);
                        }
                    }
                    return FALSE;
                }, player);
            }
            
            return true;
        }
        
        printf("File successfully loaded (duration: %.2f, samples: %zu), auto-starting playback\n", 
               player->song_duration, player->audio_buffer.length);
        
        start_playback(player);
        update_gui_state(player);
    } else if (success && is_zip_file) {
        // For ZIP files, the metadata was already set above
        player->is_loaded = true;
        player->is_playing = false;
        player->is_paused = false;
        playTime = 0;
        
        gtk_range_set_range(GTK_RANGE(player->progress_scale), 0.0, player->song_duration);
        gtk_range_set_value(GTK_RANGE(player->progress_scale), 0.0);
        
        if (player->audio_buffer.length == 0 || player->song_duration <= 0.1) {
            printf("Warning: File loaded but has no/minimal audio data (duration: %.2f, samples: %zu)\n", 
                   player->song_duration, player->audio_buffer.length);
            printf("Skipping this file and advancing to next...\n");
            
            if (strncmp(player->temp_wav_file, "virtual_", 8) == 0) {
                delete_virtual_file(player->temp_wav_file);
            }
            
            update_gui_state(player);
            
            if (player->queue.count > 1) {
                g_timeout_add(100, [](gpointer data) -> gboolean {
                    AudioPlayer *p = (AudioPlayer*)data;
                    printf("Auto-advancing from invalid file...\n");
                    if (advance_queue(&p->queue)) {
                        if (load_file_from_queue(p)) {
                            update_queue_display(p);
                            update_gui_state(p);
                        }
                    }
                    return FALSE;
                }, player);
            }
            
            return true;
        }
        
        printf("File successfully loaded (duration: %.2f, samples: %zu), auto-starting playback\n", 
               player->song_duration, player->audio_buffer.length);
        
        start_playback(player);
        update_gui_state(player);
    } else {
        printf("Failed to load file: %s\n", filename);
    }
    
    return success;
}

bool load_file_from_queue(AudioPlayer *player) {
    const char *filename = get_current_queue_file(&player->queue);
    if (!filename) return false;
    
    if (!load_file(player, filename)) {
        // File failed to load - show error in visualization
        if (player->visualizer) {
            snprintf(player->visualizer->error_message, sizeof(player->visualizer->error_message),
                     "Can't open: %s", filename);
            player->visualizer->showing_error = true;
            player->visualizer->error_display_time = 1.0;  // Show for 1 second
        }
        
        printf("Failed to load: %s\n", filename);
        
        // Silently skip to next file
        if (advance_queue(&player->queue)) {
            return load_file_from_queue(player);  // Try next file
        }
        
        return false;
    }
    
    return true;
}

void seek_to_position(AudioPlayer *player, double position_seconds) {
    if (!player->is_loaded || !player->audio_buffer.data || player->song_duration <= 0) {
        return;
    }
    
    // Clamp position to valid range
    if (position_seconds < 0) position_seconds = 0;
    if (position_seconds > player->song_duration) position_seconds = player->song_duration;
    
    pthread_mutex_lock(&player->audio_mutex);
    
    // Calculate the sample position based on the time position
    double samples_per_second = (double)(player->sample_rate * player->channels);
    size_t new_position = (size_t)(position_seconds * samples_per_second);
    
    // Ensure position is within bounds
    if (new_position >= player->audio_buffer.length) {
        new_position = player->audio_buffer.length - 1;
    }
    
    player->audio_buffer.position = new_position;
    playTime = position_seconds;
    
    pthread_mutex_unlock(&player->audio_mutex);
}

void start_playback(AudioPlayer *player) {
    if (!player->is_loaded || !player->audio_buffer.data) {
        printf("Cannot start playback - no audio data loaded\n");
        return;
    }
    
    printf("Starting WAV playback\n");
    
    pthread_mutex_lock(&player->audio_mutex);
    // If we're at the end, restart from beginning
    if (player->audio_buffer.position >= player->audio_buffer.length) {
        player->audio_buffer.position = 0;
        playTime = 0;
    }
    player->is_playing = true;
    player->is_paused = false;
    pthread_mutex_unlock(&player->audio_mutex);
    
    // Prevent system sleep during playback
    prevent_system_sleep();
    
    SDL_PauseAudioDevice(player->audio_device, 0);
    
    if (player->update_timer_id == 0) {
        player->update_timer_id = g_timeout_add(100, (GSourceFunc)([](gpointer data) -> gboolean {
            AudioPlayer *p = (AudioPlayer*)data;
            
            pthread_mutex_lock(&p->audio_mutex);
            bool song_finished = false;
            bool currently_playing = p->is_playing;
            
            // Check if song has finished
            if (p->audio_buffer.data && p->audio_buffer.length > 0) {
                // Song finished if we've reached the end of the buffer
                if (p->audio_buffer.position >= p->audio_buffer.length) {
                    if (currently_playing) {
                        p->is_playing = false;
                        currently_playing = false;
                    }
                    song_finished = true;
                    printf("Song finished - reached end of buffer (pos: %zu, len: %zu)\n", 
                           p->audio_buffer.position, p->audio_buffer.length);
                }
                // Also check if is_playing was set to false by audio callback
                else if (!currently_playing && p->audio_buffer.position > 0) {
                    song_finished = true;
                    printf("Song finished - detected by audio callback\n");
                }
            }
            
            // Update playback position if playing
            if (currently_playing && p->audio_buffer.data && p->sample_rate > 0 && p->channels > 0) {
                double samples_per_second = (double)(p->sample_rate * p->channels);
                playTime = (double)p->audio_buffer.position / samples_per_second;
            }
            
            pthread_mutex_unlock(&p->audio_mutex);
            
            // Handle song completion
            if (song_finished && p->queue.count > 0) {
                printf("Song completed. Calling next_song()...\n");
                
                // Stop the current timer
                p->update_timer_id = 0;
                
                // Call next_song() after a short delay
                g_timeout_add(50, [](gpointer data) -> gboolean {
                    AudioPlayer *player = (AudioPlayer*)data;
                    next_song_filtered(player);
                    return FALSE;
                }, p);
                
                return FALSE; // Stop this timer
            }
            
            // If not playing anymore (but not due to song completion), stop timer
            if (!currently_playing && !song_finished) {
                update_gui_state(p);
                p->update_timer_id = 0;
                return FALSE;
            }
            
            // Update GUI elements (only if still playing)
            if (currently_playing) {
                // Update progress scale (only if not currently seeking)
                if (!p->seeking) {
                    gtk_range_set_value(GTK_RANGE(p->progress_scale), playTime);
                }
                
                // Update time label
                int min = (int)(playTime / 60);
                int sec = (int)playTime % 60;
                int total_min = (int)(p->song_duration / 60);
                int total_sec = (int)p->song_duration % 60;
                
                char time_text[64];
                snprintf(time_text, sizeof(time_text), "%02d:%02d / %02d:%02d", min, sec, total_min, total_sec);
                gtk_label_set_text(GTK_LABEL(p->time_label), time_text);
            }
            
            return TRUE; // Continue timer
        }), player);
    }
}

bool remove_from_queue(PlayQueue *queue, int index) {
    if (index < 0 || index >= queue->count) {
        return false;
    }
    
    // Free the filename at this index
    g_free(queue->files[index]);
    
    // Shift all items after this index down by one
    for (int i = index; i < queue->count - 1; i++) {
        queue->files[i] = queue->files[i + 1];
    }
    
    queue->count--;
    
    // Adjust current_index if necessary
    if (index < queue->current_index) {
        // Removed item was before current, so decrease current index
        queue->current_index--;
    } else if (index == queue->current_index) {
        // Removed the currently playing item
        if (queue->count == 0) {
            // Queue is now empty
            queue->current_index = -1;
        } else if (queue->current_index >= queue->count) {
            // Current index is now beyond the end, wrap to beginning
            queue->current_index = 0;
        }
        // If current_index < queue->count, it stays the same (next song takes its place)
    }
    // If index > current_index, current_index stays the same
    
    return true;
}

void toggle_pause(AudioPlayer *player) {
    if (!player->is_playing) return;
    
    pthread_mutex_lock(&player->audio_mutex);
    player->is_paused = !player->is_paused;
    
    if (player->is_paused) {
        SDL_PauseAudioDevice(player->audio_device, 1);
        
        // Zero out frequency bands when paused so visualizations stop
        if (player->visualizer && player->visualizer->frequency_bands) {
            for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
                player->visualizer->frequency_bands[i] = 0.0;
            }
        }
        
        // Also zero peak data
        if (player->visualizer && player->visualizer->peak_data) {
            for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
                player->visualizer->peak_data[i] = 0.0;
            }
        }
    } else {
        SDL_PauseAudioDevice(player->audio_device, 0);
    }
    pthread_mutex_unlock(&player->audio_mutex);
    
    gtk_button_set_label(GTK_BUTTON(player->pause_button), player->is_paused ? "⏯" : "⏸");
}

gboolean on_vis_fullscreen_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    toggle_vis_fullscreen(player);
    return TRUE; // Prevent actual close, just exit fullscreen
}

bool is_visualizer_fullscreen() {
    return is_vis_fullscreen;
}

void toggle_vis_fullscreen(AudioPlayer *player) {
    if (!player->visualizer || !player->visualizer->drawing_area) {
        printf("No visualizer available for fullscreen mode\n");
        return;
    }
    
    if (!is_vis_fullscreen) {
        // Enter visualization fullscreen mode
        printf("Entering visualization fullscreen mode\n");
        
        // Store original parent and size
        original_vis_parent = gtk_widget_get_parent(player->visualizer->drawing_area);
        gtk_widget_get_size_request(player->visualizer->drawing_area, &original_vis_width, &original_vis_height);
        
        // Create dedicated fullscreen window for visualization only
        vis_fullscreen_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(vis_fullscreen_window), "Audio Visualizer - Press F9 to exit");
        gtk_window_fullscreen(GTK_WINDOW(vis_fullscreen_window));
        gtk_window_set_decorated(GTK_WINDOW(vis_fullscreen_window), FALSE);
        gtk_window_set_keep_above(GTK_WINDOW(vis_fullscreen_window), TRUE);
        
        // Set black background for better visualization contrast
        GdkRGBA black = {0.0, 0.0, 0.0, 1.0};
        gtk_widget_override_background_color(vis_fullscreen_window, GTK_STATE_FLAG_NORMAL, &black);
        
        // Reparent the visualization drawing area
        if (original_vis_parent) {
            g_object_ref(player->visualizer->drawing_area);
            gtk_container_remove(GTK_CONTAINER(original_vis_parent), player->visualizer->drawing_area);
        }
        
        gtk_container_add(GTK_CONTAINER(vis_fullscreen_window), player->visualizer->drawing_area);
        g_object_unref(player->visualizer->drawing_area);
        
        // Set visualization to full screen size
        GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(vis_fullscreen_window));
        int screen_width = gdk_screen_get_width(screen);
        int screen_height = gdk_screen_get_height(screen);
        gtk_widget_set_size_request(player->visualizer->drawing_area, screen_width, screen_height);
        
        // Set up key handler for F9 and Escape to exit fullscreen
        g_signal_connect(vis_fullscreen_window, "key-press-event", 
                        G_CALLBACK(on_vis_fullscreen_key_press), player);
        
        // Handle window close button
        g_signal_connect(vis_fullscreen_window, "delete-event", 
                        G_CALLBACK(on_vis_fullscreen_delete_event), player);
        
        // Show fullscreen window
        gtk_widget_show_all(vis_fullscreen_window);
        gtk_window_present(GTK_WINDOW(vis_fullscreen_window));
        
        is_vis_fullscreen = true;
        printf("Visualization fullscreen activated (F9 or Escape to exit)\n");
        
    } else {
        // Exit visualization fullscreen mode
        printf("Exiting visualization fullscreen mode\n");
        
        if (vis_fullscreen_window && player->visualizer && player->visualizer->drawing_area) {
            // Reparent visualization back to original location
            g_object_ref(player->visualizer->drawing_area);
            gtk_container_remove(GTK_CONTAINER(vis_fullscreen_window), player->visualizer->drawing_area);
            
            if (original_vis_parent) {
                gtk_container_add(GTK_CONTAINER(original_vis_parent), player->visualizer->drawing_area);
                
                // Restore original size
                gtk_widget_set_size_request(player->visualizer->drawing_area, 
                                           original_vis_width, original_vis_height);
            }
            
            g_object_unref(player->visualizer->drawing_area);
            
            // Destroy fullscreen window
            gtk_widget_destroy(vis_fullscreen_window);
            vis_fullscreen_window = NULL;
        }
        
        // Reset state
        is_vis_fullscreen = false;
        original_vis_parent = NULL;
        original_vis_width = 0;
        original_vis_height = 0;
        
        printf("Visualization returned to normal view\n");
    }
}


void cleanup_vis_fullscreen() {
    if (is_vis_fullscreen && vis_fullscreen_window) {
        // Force exit visualization fullscreen before cleanup
        if (player) {
            toggle_vis_fullscreen(player);
        } else if (vis_fullscreen_window) {
            gtk_widget_destroy(vis_fullscreen_window);
            vis_fullscreen_window = NULL;
            is_vis_fullscreen = false;
        }
    }
}

void stop_playback(AudioPlayer *player) {
    pthread_mutex_lock(&player->audio_mutex);
    player->is_playing = false;
    player->is_paused = false;
    player->audio_buffer.position = 0;
    playTime = 0;
    pthread_mutex_unlock(&player->audio_mutex);
    
    // Allow system to sleep when playback stops
    allow_system_sleep();
    
    SDL_PauseAudioDevice(player->audio_device, 1);
    
    if (player->update_timer_id > 0) {
        g_source_remove(player->update_timer_id);
        player->update_timer_id = 0;
    }
    
    gtk_range_set_value(GTK_RANGE(player->progress_scale), 0.0);
    gtk_label_set_text(GTK_LABEL(player->time_label), "00:00 / 00:00");
    gtk_button_set_label(GTK_BUTTON(player->pause_button), "⏸");
}

void rewind_5_seconds(AudioPlayer *player) {
    if (!player->is_loaded) return;
    
    double current_time = playTime;
    double new_time = current_time - 5.0;
    if (new_time < 0) new_time = 0;
    
    seek_to_position(player, new_time);
    gtk_range_set_value(GTK_RANGE(player->progress_scale), new_time);
    
    printf("Rewinded 5 seconds to %.2f\n", new_time);
}

void fast_forward_5_seconds(AudioPlayer *player) {
    if (!player->is_loaded) return;
    
    double current_time = playTime;
    double new_time = current_time + 5.0;
    if (new_time > player->song_duration) new_time = player->song_duration;
    
    seek_to_position(player, new_time);
    gtk_range_set_value(GTK_RANGE(player->progress_scale), new_time);
    
    printf("Fast forwarded 5 seconds to %.2f\n", new_time);
}

void next_song(AudioPlayer *player) {
    if (player->queue.count <= 1) return;
    
    stop_playback(player);
    
    // Check if a filter is active - if so, use filter-aware navigation
    const char *filter = player->queue_filter_text;
    bool has_filter = (filter && filter[0] != '\0');
    
    if (has_filter) {
        // When filtering, find next matching song
        int start_index = player->queue.current_index + 1;
        int search_count = 0;
        
        while (search_count < player->queue.count) {
            int check_index = (start_index + search_count) % player->queue.count;
            
            // Extract metadata for this file
            char *metadata = extract_metadata(player->queue.files[check_index]);
            char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
            parse_metadata(metadata, title, artist, album, genre);
            g_free(metadata);
            
            char *basename = g_path_get_basename(player->queue.files[check_index]);
            
            // Check if matches filter
            bool matches = matches_filter(basename, filter) ||
                          matches_filter(title, filter) ||
                          matches_filter(artist, filter) ||
                          matches_filter(album, filter) ||
                          matches_filter(genre, filter);
            
            g_free(basename);
            
            if (matches) {
                player->queue.current_index = check_index;
                if (load_file_from_queue(player)) {
                    update_queue_display_with_filter(player);
                    update_gui_state(player);
                    start_playback(player);
                }
                return;
            }
            
            search_count++;
        }
        
        // No matching song found, stay on current
        start_playback(player);
        return;
    }
    
    // No filter active, check for sorted display order
    // Check if queue_tree_view exists first
    if (!player->queue_tree_view) {
        printf("No tree view in next_song, using simple next\n");
        if (advance_queue(&player->queue)) {
            if (load_file_from_queue(player)) {
                update_queue_display_with_filter(player);
                update_gui_state(player);
                start_playback(player);
            }
        }
        return;
    }
    
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(player->queue_tree_view));
    if (model && GTK_IS_TREE_MODEL(model)) {
        GtkTreeSortable *sortable = GTK_TREE_SORTABLE(model);
        gint sort_column_id = -1;
        GtkSortType sort_type = GTK_SORT_ASCENDING;
        
        // Check if a sort is active
        if (gtk_tree_sortable_get_sort_column_id(sortable, &sort_column_id, &sort_type)) {
            // A sort is active, follow the sorted order
            GtkTreeIter iter;
            gboolean found_current = FALSE;
            gboolean found_next = FALSE;
            
            // Find current playing track in sorted order
            if (gtk_tree_model_get_iter_first(model, &iter)) {
                do {
                    int queue_index = -1;
                    gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &queue_index, -1);
                    
                    if (queue_index == player->queue.current_index) {
                        found_current = TRUE;
                        break;
                    }
                } while (gtk_tree_model_iter_next(model, &iter));
            }
            
            // If we found current, try to move to next
            if (found_current && gtk_tree_model_iter_next(model, &iter)) {
                int next_queue_index = -1;
                gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &next_queue_index, -1);
                if (next_queue_index >= 0 && next_queue_index < player->queue.count) {
                    player->queue.current_index = next_queue_index;
                    found_next = TRUE;
                }
            }
            
            // If we couldn't find next (at end of sorted list)
            if (!found_next) {
                if (player->queue.repeat_queue) {
                    // Go back to first in sorted order
                    if (gtk_tree_model_get_iter_first(model, &iter)) {
                        int first_queue_index = -1;
                        gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &first_queue_index, -1);
                        if (first_queue_index >= 0) {
                            player->queue.current_index = first_queue_index;
                            found_next = TRUE;
                        }
                    }
                }
            }
            
            // If we successfully found and set next track
            if (found_next) {
                if (load_file_from_queue(player)) {
                    update_queue_display_with_filter(player);
                    update_gui_state(player);
                    start_playback(player);
                }
                return;
            }
        }
    }
    
    // Fall back to normal unsorted next
    if (advance_queue(&player->queue)) {
        if (load_file_from_queue(player)) {
            update_queue_display_with_filter(player);
            update_gui_state(player);
            start_playback(player);
        }
    }
}

void previous_song(AudioPlayer *player) {
    if (player->queue.count <= 1) return;
    
    stop_playback(player);
    
    // Check if a filter is active - if so, use filter-aware navigation
    const char *filter = player->queue_filter_text;
    bool has_filter = (filter && filter[0] != '\0');
    
    if (has_filter) {
        // When filtering, find previous matching song
        int start_index = player->queue.current_index - 1;
        int search_count = 0;
        
        while (search_count < player->queue.count) {
            int check_index = start_index - search_count;
            if (check_index < 0) {
                check_index = player->queue.count + check_index;
            }
            
            // Extract metadata for this file
            char *metadata = extract_metadata(player->queue.files[check_index]);
            char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
            parse_metadata(metadata, title, artist, album, genre);
            g_free(metadata);
            
            char *basename = g_path_get_basename(player->queue.files[check_index]);
            
            // Check if matches filter
            bool matches = matches_filter(basename, filter) ||
                          matches_filter(title, filter) ||
                          matches_filter(artist, filter) ||
                          matches_filter(album, filter) ||
                          matches_filter(genre, filter);
            
            g_free(basename);
            
            if (matches) {
                player->queue.current_index = check_index;
                if (load_file_from_queue(player)) {
                    update_queue_display_with_filter(player);
                    update_gui_state(player);
                    start_playback(player);
                }
                return;
            }
            
            search_count++;
        }
        
        // No matching song found, stay on current
        start_playback(player);
        return;
    }
    
    // No filter active, check for sorted display order
    // Check if queue_tree_view exists first
    if (!player->queue_tree_view) {
        printf("No tree view in previous_song, using simple previous\n");
        if (previous_queue(&player->queue)) {
            if (load_file_from_queue(player)) {
                update_queue_display_with_filter(player);
                update_gui_state(player);
                start_playback(player);
            }
        }
        return;
    }
    
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(player->queue_tree_view));
    if (model && GTK_IS_TREE_MODEL(model)) {
        GtkTreeSortable *sortable = GTK_TREE_SORTABLE(model);
        gint sort_column_id = -1;
        GtkSortType sort_type = GTK_SORT_ASCENDING;
        
        // Check if a sort is active
        if (gtk_tree_sortable_get_sort_column_id(sortable, &sort_column_id, &sort_type)) {
            // A sort is active, follow the sorted order
            GtkTreeIter iter;
            GtkTreeIter prev_iter;
            gboolean found_current = FALSE;
            gboolean found_prev = FALSE;
            gboolean first_iter = TRUE;
            
            // Find current playing track in sorted order
            if (gtk_tree_model_get_iter_first(model, &iter)) {
                do {
                    int queue_index = -1;
                    gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &queue_index, -1);
                    
                    if (queue_index == player->queue.current_index) {
                        found_current = TRUE;
                        if (!first_iter) {
                            // We have a previous iterator
                            int prev_queue_index = -1;
                            gtk_tree_model_get(model, &prev_iter, COL_QUEUE_INDEX, &prev_queue_index, -1);
                            if (prev_queue_index >= 0 && prev_queue_index < player->queue.count) {
                                player->queue.current_index = prev_queue_index;
                                found_prev = TRUE;
                            }
                        }
                        break;
                    }
                    
                    prev_iter = iter;
                    first_iter = FALSE;
                } while (gtk_tree_model_iter_next(model, &iter));
            }
            
            // If we couldn't find previous (at beginning of sorted list)
            if (!found_prev && found_current) {
                if (player->queue.repeat_queue) {
                    // Go to last in sorted order
                    GtkTreeIter last_iter;
                    if (gtk_tree_model_get_iter_first(model, &iter)) {
                        last_iter = iter;
                        while (gtk_tree_model_iter_next(model, &iter)) {
                            last_iter = iter;
                        }
                        int last_queue_index = -1;
                        gtk_tree_model_get(model, &last_iter, COL_QUEUE_INDEX, &last_queue_index, -1);
                        if (last_queue_index >= 0) {
                            player->queue.current_index = last_queue_index;
                            found_prev = TRUE;
                        }
                    }
                }
            }
            
            // If we successfully found and set previous track
            if (found_prev) {
                if (load_file_from_queue(player)) {
                    update_queue_display_with_filter(player);
                    update_gui_state(player);
                    start_playback(player);
                }
                return;
            }
        }
    }
    
    // Fall back to normal unsorted previous
    if (previous_queue(&player->queue)) {
        if (load_file_from_queue(player)) {
            update_queue_display_with_filter(player);
            update_gui_state(player);
            start_playback(player);
        }
    }
}

void next_song_filtered(AudioPlayer *player) {
    if (player->queue.count == 0) {
        return;
    }
    
    const char *filter = player->queue_filter_text;
    bool has_filter = (filter && filter[0] != '\0');
    
    /*if (!has_filter) {
        // No filter active, use normal next_song
        next_song(player);
        return;
    }*/
    
    // Find the next visible (non-filtered) song
    int start_index = player->queue.current_index + 1;
    int search_count = 0;

    
    while (search_count < player->queue.count) {
        int check_index = (start_index + search_count) % player->queue.count;
        
        // Extract metadata for this file
        char *metadata = extract_metadata(player->queue.files[check_index]);
        char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
        parse_metadata(metadata, title, artist, album, genre);
        if (!ends_with_zip(player->queue.files[check_index])) {
            show_track_info_overlay(player->visualizer, title, artist, album,
                get_file_duration(player->queue.files[player->queue.current_index]));
        }
        g_free(metadata);
        
        char *basename = g_path_get_basename(player->queue.files[check_index]);
        
        // Check if this item matches the filter
        bool matches = matches_filter(basename, filter) ||
                      matches_filter(title, filter) ||
                      matches_filter(artist, filter) ||
                      matches_filter(album, filter) ||
                      matches_filter(genre, filter);
        
        g_free(basename);
        
        if (matches) {
            // Found the next visible song
            stop_playback(player);
            player->queue.current_index = check_index;
            
            if (load_file_from_queue(player)) {
                update_queue_display_with_filter(player);
                update_gui_state(player);
                start_playback(player);
                printf("Next filtered song: %s (index %d)\n", 
                       get_current_queue_file(&player->queue), check_index);
            }
            return;
        }
        
        search_count++;
    }
    
    // No matching song found in filter
    printf("No next song matches current filter\n");
}

void previous_song_filtered(AudioPlayer *player) {
    if (player->queue.count == 0) {
        return;
    }
    
    const char *filter = player->queue_filter_text;
    bool has_filter = (filter && filter[0] != '\0');
    
    /*if (!has_filter) {
        // No filter active, use normal previous_song
        previous_song(player);
        return;
    }*/
    
    // Find the previous visible (non-filtered) song
    int start_index = player->queue.current_index - 1;
    if (start_index < 0) {
        start_index = player->queue.count - 1;
    }
    
    int search_count = 0;
    
    while (search_count < player->queue.count) {
        int check_index = start_index - search_count;
        if (check_index < 0) {
            check_index += player->queue.count;
        }
        
        // Extract metadata for this file
        char *metadata = extract_metadata(player->queue.files[check_index]);
        char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
        parse_metadata(metadata, title, artist, album, genre);
        if (!ends_with_zip(player->queue.files[check_index])) {
            show_track_info_overlay(player->visualizer, title, artist, album,
                get_file_duration(player->queue.files[player->queue.current_index]));
        }
        g_free(metadata);
        
        char *basename = g_path_get_basename(player->queue.files[check_index]);
        
        // Check if this item matches the filter
        bool matches = matches_filter(basename, filter) ||
                      matches_filter(title, filter) ||
                      matches_filter(artist, filter) ||
                      matches_filter(album, filter) ||
                      matches_filter(genre, filter);
        
        g_free(basename);
        
        if (matches) {
            // Found the previous visible song
            stop_playback(player);
            player->queue.current_index = check_index;
            
            if (load_file_from_queue(player)) {
                update_queue_display_with_filter(player);
                update_gui_state(player);
                start_playback(player);
                printf("Previous filtered song: %s (index %d)\n", 
                       get_current_queue_file(&player->queue), check_index);
            }
            return;
        }
        
        search_count++;
    }
    
    // No matching song found in filter
    printf("No previous song matches current filter\n");
}

void update_gui_state(AudioPlayer *player) {
    gtk_widget_set_sensitive(player->play_button, player->is_loaded && !player->is_playing);
    gtk_widget_set_sensitive(player->pause_button, player->is_playing);
    gtk_widget_set_sensitive(player->stop_button, player->is_playing || player->is_paused);
    gtk_widget_set_sensitive(player->rewind_button, player->is_loaded);
    gtk_widget_set_sensitive(player->fast_forward_button, player->is_loaded);
    gtk_widget_set_sensitive(player->progress_scale, player->is_loaded);
    gtk_widget_set_sensitive(player->next_button, player->queue.count > 1);
    gtk_widget_set_sensitive(player->prev_button, player->queue.count > 1);
    
    if (player->is_loaded) {
        char *basename = g_path_get_basename(player->current_file);
        char label_text[512];
        snprintf(label_text, sizeof(label_text), "File: %s (%.1f sec) [%d/%d]", 
                basename, player->song_duration, player->queue.current_index + 1, player->queue.count);
        gtk_label_set_text(GTK_LABEL(player->file_label), label_text);
        g_free(basename);
    } else {
        gtk_label_set_text(GTK_LABEL(player->file_label), "No file loaded");
    }
}

// Progress scale value changed handler for seeking
void on_progress_scale_value_changed(GtkRange *range, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    if (!player->is_loaded || player->seeking) {
        return;
    }
    
    double new_position = gtk_range_get_value(range);
    
    // Set seeking flag to prevent feedback
    player->seeking = true;
    
    // Seek to the new position
    seek_to_position(player, new_position);
    
    // Clear seeking flag after a short delay
    g_timeout_add(50, [](gpointer data) -> gboolean {
        AudioPlayer *p = (AudioPlayer*)data;
        p->seeking = false;
        return FALSE; // Don't repeat
    }, player);
}

// Queue button callbacks
void on_add_to_queue_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
#ifdef _WIN32
    char filename[32768] = "";  // Much larger buffer for multiple files
    if (open_windows_file_dialog(filename, sizeof(filename), true)) {  // true for multiple selection
        bool was_empty_queue = (player->queue.count == 0);
        
        // Helper function to check if file extension is supported
        auto is_supported_extension = [](const char* filename) -> bool {
            const char* ext = strrchr(filename, '.');
            if (!ext) return false;
            
            // Convert to lowercase for comparison
            char ext_lower[10];
            strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
            ext_lower[sizeof(ext_lower) - 1] = '\0';
            for (int i = 0; ext_lower[i]; i++) {
                ext_lower[i] = tolower(ext_lower[i]);
            }
            
            return (strcmp(ext_lower, ".mid") == 0 || 
                    strcmp(ext_lower, ".midi") == 0 ||
                    strcmp(ext_lower, ".wav") == 0 ||
                    strcmp(ext_lower, ".mp3") == 0 ||
                    strcmp(ext_lower, ".m4a") == 0 ||
                    strcmp(ext_lower, ".ogg") == 0 ||
                    strcmp(ext_lower, ".aif") == 0 ||
                    strcmp(ext_lower, ".aiff") == 0 ||
                    strcmp(ext_lower, ".opus") == 0 ||
                    strcmp(ext_lower, ".flac") == 0 ||
                    strcmp(ext_lower, ".zip") == 0 ||
                    strcmp(ext_lower, ".wma") == 0);
        };
        
        // Parse multiple filenames from Windows dialog
        // Windows returns multiple files as: "directory\0file1.ext\0file2.ext\0\0"
        // or single file as: "full\path\to\file.ext\0"
        
        char *ptr = filename;
        char directory[1024] = "";
        
        // Find the first null terminator
        size_t first_string_len = strlen(ptr);
        char *after_first_null = ptr + first_string_len + 1;
        
        // Check if this is multiple files (there's another string after the first null)
        if (*after_first_null != '\0') {
            // Multiple files: first string is directory
            strncpy(directory, ptr, sizeof(directory) - 1);
            directory[sizeof(directory) - 1] = '\0';
            ptr = after_first_null;
            
            printf("Multiple files selected, directory: %s\n", directory);
            
            // Add each file (with extension validation)
            while (*ptr) {
                char full_path[2048];
                snprintf(full_path, sizeof(full_path), "%s\\%s", directory, ptr);
                
                printf("Processing file: %s\n", full_path);
                
                if (is_supported_extension(full_path)) {
                    add_to_queue(&player->queue, full_path);
                    printf("Added to queue: %s\n", full_path);
                } else {
                    printf("Skipping unsupported file: %s\n", full_path);
                }
                
                // Move to next filename
                ptr += strlen(ptr) + 1;
            }
        } else {
            // Single file (with extension validation)
            printf("Single file selected: %s\n", filename);
            
            if (is_supported_extension(filename)) {
                add_to_queue(&player->queue, filename);
                printf("Added single file to queue: %s\n", filename);
            } else {
                printf("Unsupported file type: %s\n", filename);
                
                // Show error message for unsupported single file
                char error_msg[1024];
                snprintf(error_msg, sizeof(error_msg), "Unsupported file type: %s", filename);
                MessageBoxA(NULL, error_msg, "Unsupported File", MB_OK | MB_ICONWARNING);
            }
        }
        
        // If this was the first file(s) added to an empty queue, load and start playing
        if (was_empty_queue && player->queue.count > 0) {
            if (load_file_from_queue(player)) {
                update_gui_state(player);
                // load_file now auto-starts playback
            }
        }
        
        update_queue_display_with_filter(player);
        update_gui_state(player);
        
        printf("Total files in queue: %d\n", player->queue.count);
    }
#else
    // Your existing GTK file dialog code for Linux/Mac
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Add to Queue",
                                                    GTK_WINDOW(player->window),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Add", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    
    // Add file filters
    GtkFileFilter *all_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(all_filter, "All Supported Files");
    gtk_file_filter_add_pattern(all_filter, "*.mid");
    gtk_file_filter_add_pattern(all_filter, "*.midi");
    gtk_file_filter_add_pattern(all_filter, "*.wav");
    gtk_file_filter_add_pattern(all_filter, "*.mp3");
    gtk_file_filter_add_pattern(all_filter, "*.m4a");
    gtk_file_filter_add_pattern(all_filter, "*.ogg");
    gtk_file_filter_add_pattern(all_filter, "*.flac");
    gtk_file_filter_add_pattern(all_filter, "*.aif");
    gtk_file_filter_add_pattern(all_filter, "*.aiff");
    gtk_file_filter_add_pattern(all_filter, "*.opus");
    gtk_file_filter_add_pattern(all_filter, "*.wma");
    gtk_file_filter_add_pattern(all_filter, "*.lrc");
    gtk_file_filter_add_pattern(all_filter, "*.zip");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all_filter);
    
    GtkFileFilter *midi_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(midi_filter, "MIDI Files (*.mid, *.midi)");
    gtk_file_filter_add_pattern(midi_filter, "*.mid");
    gtk_file_filter_add_pattern(midi_filter, "*.midi");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), midi_filter);
    
    GtkFileFilter *wav_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(wav_filter, "WAV Files (*.wav)");
    gtk_file_filter_add_pattern(wav_filter, "*.wav");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), wav_filter);
    
    GtkFileFilter *mp3_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(mp3_filter, "MP3 Files (*.mp3)");
    gtk_file_filter_add_pattern(mp3_filter, "*.mp3");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), mp3_filter);
    
    GtkFileFilter *ogg_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(ogg_filter, "OGG Files (*.ogg)");
    gtk_file_filter_add_pattern(ogg_filter, "*.ogg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ogg_filter);

    GtkFileFilter *flac_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(flac_filter, "FLAC Files (*.flac)");
    gtk_file_filter_add_pattern(flac_filter, "*.flac");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), flac_filter);

    GtkFileFilter *aiff_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(aiff_filter, "AIFF Files (*.aiff)");
    gtk_file_filter_add_pattern(aiff_filter, "*.aiff");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), aiff_filter);

    GtkFileFilter *opus_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(opus_filter, "OPUS Files (*.opus)");
    gtk_file_filter_add_pattern(opus_filter, "*.opus");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), opus_filter);

    GtkFileFilter *m4a_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(m4a_filter, "M4A Files (*.m4a)");
    gtk_file_filter_add_pattern(m4a_filter, "*.m4a");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), m4a_filter);

    GtkFileFilter *wma_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(wma_filter, "wma Files (*.wma)");
    gtk_file_filter_add_pattern(wma_filter, "*.wma");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), wma_filter);

    GtkFileFilter *lrc_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(lrc_filter, "lrc Files (*.lrc)");
    gtk_file_filter_add_pattern(lrc_filter, "*.lrc");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), lrc_filter);


    GtkFileFilter *cdg_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(cdg_filter, "cdg Files (*.zip)");
    gtk_file_filter_add_pattern(cdg_filter, "*.zip");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), cdg_filter);

    GtkFileFilter *generic_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(generic_filter, "All other files (*.*)");
    gtk_file_filter_add_pattern(generic_filter, "*.*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), generic_filter);

    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GSList *filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
        
        bool was_empty_queue = (player->queue.count == 0);
        
        for (GSList *iter = filenames; iter != NULL; iter = g_slist_next(iter)) {
            char *filename = (char*)iter->data;
            if (!filename_exists_in_queue(&player->queue, filename)) {
                add_to_queue(&player->queue, filename);
            }
            g_free(filename);
        }
        
        g_slist_free(filenames);
        
        // If this was the first file(s) added to an empty queue, load and start playing
        if (was_empty_queue && player->queue.count > 0) {
            if (load_file_from_queue(player)) {
                update_gui_state(player);
                // load_file now auto-starts playback
            }
        }
        
        update_queue_display_with_filter(player);
        update_gui_state(player);
    }
    
    gtk_widget_destroy(dialog);
#endif
}


void on_clear_queue_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    stop_playback(player);
    clear_queue(&player->queue);
    update_queue_display_with_filter(player);
    update_gui_state(player);
    player->is_loaded = false;
    
    gtk_label_set_text(GTK_LABEL(player->file_label), "No file loaded");
}

void on_repeat_queue_toggled(GtkToggleButton *button, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    player->queue.repeat_queue = gtk_toggle_button_get_active(button);
    
    printf("Queue repeat: %s\n", player->queue.repeat_queue ? "ON" : "OFF");
}

// Menu callbacks
void on_menu_open(GtkMenuItem *menuitem, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
#ifdef _WIN32
    char filename[32768];
    if (open_windows_file_dialog(filename, sizeof(filename))) {
        bool was_empty_queue = (player->queue.count == 0);
        int existing_index = find_file_in_queue(&player->queue, filename);
        
        // If file already exists in queue, jump to it
        if (existing_index >= 0) {
            printf("File already in queue at index %d, jumping to it\n", existing_index);
            player->queue.current_index = existing_index;
            if (load_file_from_queue(player)) {
                printf("Jumped to: %s\n", filename);
                update_queue_display_with_filter(player);
                update_gui_state(player);
            }
        } else {
            // File not in queue, add it
            add_to_queue(&player->queue, filename);
            
            // If queue was empty, set current index to the newly added file and load it
            if (was_empty_queue && player->queue.count > 0) {
                player->queue.current_index = player->queue.count - 1;
                if (load_file_from_queue(player)) {
                    printf("Successfully loaded: %s\n", filename);
                    update_queue_display_with_filter(player);
                    update_gui_state(player);
                }
            } else if (!was_empty_queue) {
                // Queue wasn't empty, just set to play this new file immediately
                player->queue.current_index = player->queue.count - 1;
                if (load_file_from_queue(player)) {
                    printf("Successfully loaded: %s\n", filename);
                    update_queue_display_with_filter(player);
                    update_gui_state(player);
                }
            } else {
                update_queue_display_with_filter(player);
                update_gui_state(player);
            }
        }
    }
#else
    // Your existing GTK file dialog code for Linux/Mac
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open Audio File",
                                                    GTK_WINDOW(player->window),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Open", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    
    // File filters
    GtkFileFilter *all_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(all_filter, "All Supported Files");
    gtk_file_filter_add_pattern(all_filter, "*.mid");
    gtk_file_filter_add_pattern(all_filter, "*.midi");
    gtk_file_filter_add_pattern(all_filter, "*.wav");
    gtk_file_filter_add_pattern(all_filter, "*.mp3");
    gtk_file_filter_add_pattern(all_filter, "*.ogg");
    gtk_file_filter_add_pattern(all_filter, "*.flac");
    gtk_file_filter_add_pattern(all_filter, "*.aiff");
    gtk_file_filter_add_pattern(all_filter, "*.aif");
    gtk_file_filter_add_pattern(all_filter, "*.opus");
    gtk_file_filter_add_pattern(all_filter, "*.m4a");
    gtk_file_filter_add_pattern(all_filter, "*.wma");
    gtk_file_filter_add_pattern(all_filter, "*.lrc");
    gtk_file_filter_add_pattern(all_filter, "*.zip");


    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all_filter);
    
    GtkFileFilter *midi_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(midi_filter, "MIDI Files (*.mid, *.midi)");
    gtk_file_filter_add_pattern(midi_filter, "*.mid");
    gtk_file_filter_add_pattern(midi_filter, "*.midi");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), midi_filter);
    
    GtkFileFilter *wav_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(wav_filter, "WAV Files (*.wav)");
    gtk_file_filter_add_pattern(wav_filter, "*.wav");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), wav_filter);
    
    GtkFileFilter *mp3_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(mp3_filter, "MP3 Files (*.mp3)");
    gtk_file_filter_add_pattern(mp3_filter, "*.mp3");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), mp3_filter);
    
    GtkFileFilter *ogg_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(ogg_filter, "OGG Files (*.ogg)");
    gtk_file_filter_add_pattern(ogg_filter, "*.ogg");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ogg_filter);

    GtkFileFilter *flac_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(flac_filter, "FLAC Files (*.flac)");
    gtk_file_filter_add_pattern(flac_filter, "*.flac");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), flac_filter);

    GtkFileFilter *aiff_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(aiff_filter, "AIFF Files (*.aiff)");
    gtk_file_filter_add_pattern(aiff_filter, "*.aiff");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), aiff_filter);

    GtkFileFilter *opus_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(opus_filter, "OPUS Files (*.opus)");
    gtk_file_filter_add_pattern(opus_filter, "*.opus");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), opus_filter);

    GtkFileFilter *m4a_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(m4a_filter, "M4A Files (*.m4a)");
    gtk_file_filter_add_pattern(m4a_filter, "*.m4a");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), m4a_filter);

    GtkFileFilter *wma_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(wma_filter, "WMA Files (*.wma)");
    gtk_file_filter_add_pattern(wma_filter, "*.wma");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), wma_filter);

    GtkFileFilter *cdg_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(cdg_filter, "CDG Files (*.zip)");
    gtk_file_filter_add_pattern(cdg_filter, "*.zip");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), cdg_filter);

    GtkFileFilter *lrc_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(lrc_filter, "LRC Files (*.lrc)");
    gtk_file_filter_add_pattern(lrc_filter, "*.lrc");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), lrc_filter);

    GtkFileFilter *generic_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(generic_filter, "All Other Files (*.*)");
    gtk_file_filter_add_pattern(generic_filter, "*.*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), generic_filter);


    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        bool was_empty_queue = (player->queue.count == 0);
        int existing_index = find_file_in_queue(&player->queue, filename);
        
        // If file already exists in queue, jump to it
        if (existing_index >= 0) {
            printf("File already in queue at index %d, jumping to it\n", existing_index);
            player->queue.current_index = existing_index;
            if (load_file_from_queue(player)) {
                printf("Jumped to: %s\n", filename);
                update_queue_display_with_filter(player);
                update_gui_state(player);
            }
        } else {
            // File not in queue, add it
            add_to_queue(&player->queue, filename);
            
            // If queue was empty, set current index to the newly added file and load it
            if (was_empty_queue && player->queue.count > 0) {
                player->queue.current_index = player->queue.count - 1;
                if (load_file_from_queue(player)) {
                    printf("Successfully loaded: %s\n", filename);
                    update_queue_display_with_filter(player);
                    update_gui_state(player);
                }
            } else if (!was_empty_queue) {
                // Queue wasn't empty, just set to play this new file immediately
                player->queue.current_index = player->queue.count - 1;
                if (load_file_from_queue(player)) {
                    printf("Successfully loaded: %s\n", filename);
                    update_queue_display_with_filter(player);
                    update_gui_state(player);
                }
            } else {
                update_queue_display_with_filter(player);
                update_gui_state(player);
            }
        }
        
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
#endif
}


void on_window_resize(GtkWidget *widget, gpointer user_data) {

    // Get screen resolution
    GdkScreen *screen = gtk_widget_get_screen(widget);
    int screen_width = gdk_screen_get_width(screen);
    int screen_height = gdk_screen_get_height(screen);
    //printf("Screen %i %i\n", screen_width, screen_height);

    //printf("Screen resolution: %dx%d\n", screen_width, screen_height);

    // Adaptive base sizes based on screen resolution category
    int base_window_width, base_window_height, base_player_width;
    int base_vis_width, base_vis_height, base_queue_width, base_queue_height;
    
    if (screen_width <= 800 || screen_height <= 600) {
        // Very small screens (800x600, etc.) - use much smaller visualization
        base_window_width = screen_width-50;
        base_window_height = screen_height-50;
        base_player_width = 200;
        base_vis_width = 100;  // Much smaller visualization
        base_vis_height = 80;  // Much smaller visualization
        base_queue_width = 100;
        base_queue_height = 100;
        //printf("Using very small screen base sizes\n");
    } else if (screen_width < 1200 || screen_height < 900) {
        // Medium screens (1024x768, etc.) - moderately smaller visualization
        base_window_width = 800;
        base_window_height = 600;
        base_player_width = 400;
        base_vis_width = 260;  // Smaller visualization
        base_vis_height = 120; // Smaller visualization
        base_queue_width = 250;
        base_queue_height = 350;
        //printf("Using medium-screen base sizes\n");
    } else {
        // Large screens (1920x1080+) - keep current size
        base_window_width = 900;
        base_window_height = 700;
        base_player_width = 500;
        base_vis_width = 400;
        base_vis_height = 200;
        base_queue_width = 300;
        base_queue_height = 400;
        //printf("Using large-screen base sizes\n");
    }

    // Use a more appropriate reference resolution based on screen category
    int ref_width = (screen_width < 1200) ? 1024 : 1920;
    int ref_height = (screen_height < 900) ? 768 : 1080;

    // Calculate appropriate sizes
    int window_width = scale_size(base_window_width, screen_width, ref_width);
    int window_height = scale_size(base_window_height, screen_height, ref_height);
    int player_width = scale_size(base_player_width, screen_width, ref_width);
    int vis_width = scale_size(base_vis_width, screen_width, ref_width);
    int vis_height = scale_size(base_vis_height, screen_height, ref_height);
    int queue_width = scale_size(base_queue_width, screen_width, ref_width);
    int queue_height = scale_size(base_queue_height, screen_height, ref_height);

    int scale = gtk_widget_get_scale_factor(player->window);
    if (scale>1) {
        window_width/=scale;
        window_height/=scale;
        player_width/=scale;
        vis_width/=scale;
        vis_height/=scale;
        queue_width/=scale;
        queue_height/=scale;
    }


    // Apply more aggressive minimum sizes for very small screens
    if (screen_width <= 800) {
        //window_width = fmax(window_width, screen_width - 50);  // Leave some margin
        //window_height = fmax(window_height, screen_height - 50);        player_width = fmax(player_width, 300);
        window_width = screen_width;
        window_height = screen_height;
        vis_width = fmax(vis_width, 180);   // Smaller minimum
        vis_height = fmax(vis_height, 60);  // Much smaller minimum
        queue_width = fmax(queue_width, 180);
        queue_height = fmax(queue_height, 250);
    } else if (screen_width <= 1024) {
        window_width = fmax(window_width, 800);
        window_height = fmax(window_height, 600);
        player_width = fmax(player_width, 400);
        vis_width = fmax(vis_width, 220);   // Smaller minimum
        vis_height = fmax(vis_height, 100); // Smaller minimum
        queue_width = fmax(queue_width, 250);
        queue_height = fmax(queue_height, 300);
    } else {
        window_width = fmax(window_width, 800);
        window_height = fmax(window_height, 600);
        player_width = fmax(player_width, 400);
        vis_width = fmax(vis_width, 300);
        vis_height = fmax(vis_height, 150);
        queue_width = fmax(queue_width, 250);
        queue_height = fmax(queue_height, 300);
    }

    //printf("Final sizes: window=%dx%d, player=%d, vis=%dx%d, queue=%dx%d\n", window_width, window_height, player_width, vis_width, vis_height, queue_width, queue_height);

    // Resize window
    //gtk_window_resize(GTK_WINDOW(widget), window_width, window_height);

    // Adjust player vbox width
    GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
    if (children && children->data) {
        GtkWidget *main_hbox = GTK_WIDGET(children->data);
        GList *hbox_children = gtk_container_get_children(GTK_CONTAINER(main_hbox));
        if (hbox_children && hbox_children->data) {
            GtkWidget *player_vbox = GTK_WIDGET(hbox_children->data);
            gtk_widget_set_size_request(player_vbox, player_width, -1);
        }
        g_list_free(hbox_children);
    }
    g_list_free(children);

    // Adjust visualizer size
    if (player->visualizer && player->visualizer->drawing_area) {
        gtk_widget_set_size_request(player->visualizer->drawing_area, vis_width, vis_height);
        //printf("Set visualizer size to: %dx%d\n", vis_width, vis_height);
    }

    // Adjust queue scrolled window
    if (player->queue_scrolled_window) {
        gtk_widget_set_size_request(player->queue_scrolled_window, queue_width, queue_height);
    }

}

void on_window_realize(GtkWidget *widget, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
}

int scale_size(int base_size, int screen_dimension, int base_dimension) {
    if (screen_dimension < base_dimension) {
        // Scale down for smaller screens, but with a minimum ratio
        double scale_ratio = (double)screen_dimension / base_dimension;
        
        // Don't scale below 60% of original size to keep things usable
        scale_ratio = fmax(scale_ratio, 0.6);
        
        return (int)(base_size * scale_ratio);
    } else {
        // Keep base size or scale up slightly for larger screens
        double scale = fmin(1.5, (double)screen_dimension / base_dimension);
        return (int)(base_size * scale);
    }
}

double get_scale_factor(GtkWidget *widget) {
    if (!widget || !gtk_widget_get_realized(widget)) {
        return 1.0;
    }
    
    GdkWindow *window = gtk_widget_get_window(widget);
    if (!window) {
        return 1.0;
    }
    
    GdkDisplay *display = gdk_window_get_display(window);
    GdkMonitor *monitor = gdk_display_get_monitor_at_window(display, window);
    
    if (monitor) {
        return gdk_monitor_get_scale_factor(monitor);
    }
    
    return 1.0;
}

void on_menu_quit(GtkMenuItem *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    printf("Menu quit selected - triggering cleanup\n");
    fflush(stdout);
    
    // Trigger the same cleanup as clicking the X button
    // This will call on_window_delete_event which does all the cleanup
    gtk_window_close(GTK_WINDOW(player->window));
}

// Button callbacks
void on_play_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    start_playback((AudioPlayer*)user_data);
    update_gui_state((AudioPlayer*)user_data);
}

void on_pause_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    toggle_pause((AudioPlayer*)user_data);
    update_gui_state((AudioPlayer*)user_data);
}

void on_stop_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    stop_playback((AudioPlayer*)user_data);
    update_gui_state((AudioPlayer*)user_data);
}

void on_rewind_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    rewind_5_seconds((AudioPlayer*)user_data);
}

void on_fast_forward_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    fast_forward_5_seconds((AudioPlayer*)user_data);
}

void on_next_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    next_song_filtered((AudioPlayer*)user_data);
}

void on_previous_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    previous_song_filtered((AudioPlayer*)user_data);
}

void on_volume_changed(GtkRange *range, gpointer user_data) {
    (void)user_data;
    double value = gtk_range_get_value(range);
    globalVolume = (int)(value * 100);
}

void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    (void)user_data;
    
}

gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)widget;
    (void)event;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    printf("Window close button pressed, cleaning up...\n");
    
    // Save current queue before exit
    save_current_queue_on_exit(player);

    save_player_settings(player);
    
    stop_playback(player);
    clear_queue(&player->queue);
    cleanup_queue_filter(player);
    cleanup_conversion_cache(&player->conversion_cache);
    cleanup_audio_cache(&player->audio_cache); 
    cleanup_virtual_filesystem();
    
    printf("Cleaing up Audio\n");
    if (player->audio_buffer.data) free(player->audio_buffer.data);

    if (player->cdg_display) {
        cdg_display_free(player->cdg_display);
    }    

    printf("Closing  SDL 1\n");
    if (player->audio_device) SDL_CloseAudioDevice(player->audio_device);

    printf("Cleaning Equalizer\n");
    if (player->equalizer) {
        equalizer_free(player->equalizer);
    }
    
 

    printf("Freeing player\n");
    if (player) {
        //delete player;
    }

    printf("Closing  SDL\n");
    SDL_Quit();
    
    printf("Closing main window\n");
    gtk_main_quit();

    return TRUE; // Allow the window to be destroyed
}

bool isValidM3U(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.find_first_not_of(" \t\r\n") != std::string::npos)
            return true; // Found meaningful content
    }
    return false;
}

void on_menu_load_playlist(GtkMenuItem *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
#ifdef _WIN32
    char filename[32768];
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = "M3U Playlists\0*.m3u;*.m3u8\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    filename[0] = '\0';
    
    if (GetOpenFileName(&ofn)) {
        if (isValidM3U(filename)) {
            if (load_m3u_playlist(player, filename)) {
                add_to_recent_files(filename, "audio/x-mpegurl");
            }
        } else {
            printf("Playlist appears empty or corrupted\n");
        }
    }
#else
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Load Playlist",
                                                    GTK_WINDOW(player->window),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Load", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    
    GtkFileFilter *m3u_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(m3u_filter, "M3U Playlists (*.m3u, *.m3u8)");
    gtk_file_filter_add_pattern(m3u_filter, "*.m3u");
    gtk_file_filter_add_pattern(m3u_filter, "*.m3u8");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), m3u_filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (isValidM3U(filename)) {
            if (load_m3u_playlist(player, filename)) {
                add_to_recent_files(filename, "audio/x-mpegurl");
                save_last_playlist_path(filename); 
            }
        } else {
            printf("Playlist appears empty or corrupted.");
        }
        g_free(filename);
    }

    
    gtk_widget_destroy(dialog);
#endif
}

bool save_current_queue_on_exit(AudioPlayer *player) {
    if (player->queue.count == 0) {
        printf("No queue to save on exit\n");
        return false;
    }
    
    char temp_playlist_path[1024];
    char position_path[1024];
    char config_dir[512];
    
#ifdef _WIN32
    char app_data[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, app_data) != S_OK) {
        return false;
    }
    snprintf(config_dir, sizeof(config_dir), "%s\\Zenamp", app_data);
    snprintf(temp_playlist_path, sizeof(temp_playlist_path), "%s\\temp_queue.m3u", config_dir);
    snprintf(position_path, sizeof(position_path), "%s\\temp_queue_state.txt", config_dir);
    CreateDirectoryA(config_dir, NULL);
#else
    const char *home = getenv("HOME");
    if (!home) {
        return false;
    }
    snprintf(config_dir, sizeof(config_dir), "%s/.zenamp", home);
    snprintf(temp_playlist_path, sizeof(temp_playlist_path), "%s/temp_queue.m3u", config_dir);
    snprintf(position_path, sizeof(position_path), "%s/temp_queue_state.txt", config_dir);
    mkdir(config_dir, 0755);
#endif
    
    FILE *f = fopen(temp_playlist_path, "w");
    if (!f) {
        printf("Failed to create temp queue file\n");
        return false;
    }
    
    fprintf(f, "#EXTM3U\n");
    
    // Save ALL files from the actual queue, not just the filtered display
    for (int i = 0; i < player->queue.count; i++) {
        fprintf(f, "%s\n", player->queue.files[i]);
    }
    
    fclose(f);
    printf("Saved current queue to: %s\n", temp_playlist_path);
    
    // Save current index and playback position
    f = fopen(position_path, "w");
    if (f) {
        fprintf(f, "%d\n", player->queue.current_index);
        fprintf(f, "%.2f\n", playTime);
        fclose(f);
        printf("Saved playback state: index=%d, time=%.2f\n", 
               player->queue.current_index, playTime);
    }
    
    if (save_last_playlist_path(temp_playlist_path)) {
        printf("Set temp queue as last playlist\n");
        return true;
    }
    
    return false;
}

void on_menu_save_playlist(GtkMenuItem *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    if (player->queue.count == 0) {
        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(player->window),
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_MESSAGE_WARNING,
                                                         GTK_BUTTONS_OK,
                                                         "No files in queue to save");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        return;
    }
    
#ifdef _WIN32
    char filename[32768];
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = "M3U Playlists\0*.m3u\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "m3u";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    
    snprintf(filename, sizeof(filename), "%s", "playlist.m3u");
    
    if (GetSaveFileName(&ofn)) {
        if (save_m3u_playlist(player, filename)) {
            // ADD TO RECENT FILES
            add_to_recent_files(filename, "audio/x-mpegurl");
        }
    }
#else
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save Playlist",
                                                    GTK_WINDOW(player->window),
                                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Save", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "playlist.m3u");
    
    GtkFileFilter *m3u_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(m3u_filter, "M3U Playlists (*.m3u)");
    gtk_file_filter_add_pattern(m3u_filter, "*.m3u");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), m3u_filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (save_m3u_playlist(player, filename)) {
            // ADD TO RECENT FILES
            add_to_recent_files(filename, "audio/x-mpegurl");
        }
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
#endif
}

#ifdef _WIN32

bool get_last_playlist_path(char *path, size_t path_size) {
    char app_data[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, app_data) != S_OK) {
        return false;
    }
    snprintf(path, path_size, "%s\\Zenamp\\last_playlist.txt", app_data);
    
    // Create directory if it doesn't exist
    char dir_path[MAX_PATH];
    snprintf(dir_path, sizeof(dir_path), "%s\\Zenamp", app_data);
    CreateDirectoryA(dir_path, NULL);
    
    return true;
}
#else
bool get_last_playlist_path(char *path, size_t path_size) {
    const char *home = getenv("HOME");
    if (!home) {
        return false;
    }
    snprintf(path, path_size, "%s/.zenamp/last_playlist.txt", home);
    
    // Create directory if it doesn't exist
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/.zenamp", home);
    mkdir(dir_path, 0755);
    
    return true;
}
#endif

bool save_last_playlist_path(const char *playlist_path) {
    char config_path[1024];
    if (!get_last_playlist_path(config_path, sizeof(config_path))) {
        return false;
    }
    
    FILE *f = fopen(config_path, "w");
    if (!f) {
        printf("Failed to save last playlist path\n");
        return false;
    }
    
    fprintf(f, "%s\n", playlist_path);
    fclose(f);
    printf("Saved last playlist path: %s\n", playlist_path);
    return true;
}

bool load_last_playlist_path(char *playlist_path, size_t path_size) {
    char config_path[1024];
    if (!get_last_playlist_path(config_path, sizeof(config_path))) {
        return false;
    }
    
    FILE *f = fopen(config_path, "r");
    if (!f) {
        printf("No last playlist file found\n");
        return false;
    }
    
    if (!fgets(playlist_path, path_size, f)) {
        fclose(f);
        return false;
    }
    
    fclose(f);
    
    // Remove trailing newline
    size_t len = strlen(playlist_path);
    if (len > 0 && playlist_path[len-1] == '\n') {
        playlist_path[len-1] = '\0';
    }
    
    // Check if file still exists
    FILE *test = fopen(playlist_path, "r");
    if (!test) {
        printf("Last playlist no longer exists: %s\n", playlist_path);
        return false;
    }
    fclose(test);
    
    printf("Found last playlist: %s\n", playlist_path);
    return true;
}

bool load_playlist_state(int *current_index, double *position) {
    char position_path[1024];
    
#ifdef _WIN32
    char app_data[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, app_data) != S_OK) {
        return false;
    }
    snprintf(position_path, sizeof(position_path), "%s\\Zenamp\\temp_queue_state.txt", app_data);
#else
    const char *home = getenv("HOME");
    if (!home) {
        return false;
    }
    snprintf(position_path, sizeof(position_path), "%s/.zenamp/temp_queue_state.txt", home);
#endif
    
    FILE *f = fopen(position_path, "r");
    if (!f) {
        return false;
    }
    
    if (fscanf(f, "%d\n", current_index) != 1) {
        fclose(f);
        return false;
    }
    
    if (fscanf(f, "%lf\n", position) != 1) {
        fclose(f);
        return false;
    }
    
    fclose(f);
    printf("Loaded playback state: index=%d, time=%.2f\n", *current_index, *position);
    return true;
}

#ifdef _WIN32
bool get_settings_path(char *path, size_t path_size) {
    char app_data[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, app_data) != S_OK) {
        return false;
    }
    snprintf(path, path_size, "%s\\Zenamp\\settings.txt", app_data);
    
    // Create directory if it doesn't exist
    char dir_path[MAX_PATH];
    snprintf(dir_path, sizeof(dir_path), "%s\\Zenamp", app_data);
    CreateDirectoryA(dir_path, NULL);
    
    return true;
}
#else
bool get_settings_path(char *path, size_t path_size) {
    const char *home = getenv("HOME");
    if (!home) {
        return false;
    }
    snprintf(path, path_size, "%s/.zenamp/settings.txt", home);
    
    // Create directory if it doesn't exist
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/.zenamp", home);
    mkdir(dir_path, 0755);
    
    return true;
}
#endif

bool save_player_settings(AudioPlayer *player) {
    char settings_path[1024];
    if (!get_settings_path(settings_path, sizeof(settings_path))) {
        printf("Failed to get settings path\n");
        return false;
    }
    
    FILE *f = fopen(settings_path, "w");
    if (!f) {
        printf("Failed to save settings\n");
        return false;
    }
    
    // Get current volume from the scale widget
    double volume = gtk_range_get_value(GTK_RANGE(player->volume_scale));
    
    // Write settings
    fprintf(f, "# Zenamp Settings\n");
    fprintf(f, "volume=%.2f\n", volume);
    fprintf(f, "speed=%.2f\n", player->playback_speed);
    
    // Equalizer settings
    if (player->equalizer) {
        fprintf(f, "eq_enabled=%d\n", player->equalizer->enabled ? 1 : 0);
        fprintf(f, "bass_gain=%.2f\n", player->equalizer->bass_gain_db);
        fprintf(f, "mid_gain=%.2f\n", player->equalizer->mid_gain_db);
        fprintf(f, "treble_gain=%.2f\n", player->equalizer->treble_gain_db);
    }
    
    // Visualization settings
    if (player->visualizer) {
        fprintf(f, "vis_type=%d\n", player->visualizer->type);
        fprintf(f, "vis_sensitivity=%.2f\n", player->visualizer->sensitivity);
    }
    
    fclose(f);
    printf("Settings saved to: %s\n", settings_path);
    return true;
}

bool load_player_settings(AudioPlayer *player) {
    char settings_path[1024];
    if (!get_settings_path(settings_path, sizeof(settings_path))) {
        printf("Failed to get settings path\n");
        return false;
    }
    
    FILE *f = fopen(settings_path, "r");
    if (!f) {
        printf("No settings file found, using defaults\n");
        return false;
    }
    
    char line[256];
    double volume = 1.0;
    double speed = 1.0;
    bool eq_enabled = false;
    float bass_gain = 0.0f;
    float mid_gain = 0.0f;
    float treble_gain = 0.0f;
    int vis_type = 0;
    float vis_sensitivity = 1.0f;
    
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Parse key=value pairs
        if (sscanf(line, "volume=%lf", &volume) == 1) {
            printf("Loaded volume: %.2f\n", volume);
        }
        else if (sscanf(line, "speed=%lf", &speed) == 1) {
            printf("Loaded speed: %.2f\n", speed);
        }
        else if (sscanf(line, "eq_enabled=%d", (int*)&eq_enabled) == 1) {
            printf("Loaded eq_enabled: %d\n", eq_enabled);
        }
        else if (sscanf(line, "bass_gain=%f", &bass_gain) == 1) {
            printf("Loaded bass_gain: %.2f\n", bass_gain);
        }
        else if (sscanf(line, "mid_gain=%f", &mid_gain) == 1) {
            printf("Loaded mid_gain: %.2f\n", mid_gain);
        }
        else if (sscanf(line, "treble_gain=%f", &treble_gain) == 1) {
            printf("Loaded treble_gain: %.2f\n", treble_gain);
        }
        else if (sscanf(line, "vis_type=%d", &vis_type) == 1) {
            printf("Loaded vis_type: %d\n", vis_type);
        }
        else if (sscanf(line, "vis_sensitivity=%f", &vis_sensitivity) == 1) {
            printf("Loaded vis_sensitivity: %.2f\n", vis_sensitivity);
        }
    }
    
    fclose(f);
    
    // Apply loaded settings
    
    // Volume
    gtk_range_set_value(GTK_RANGE(player->volume_scale), volume);
    globalVolume = (int)(volume * 100);
    
    // Speed
    player->playback_speed = speed;
    gtk_range_set_value(GTK_RANGE(player->speed_scale), speed);
    
    // Equalizer
    if (player->equalizer) {
        player->equalizer->enabled = eq_enabled;
        player->equalizer->bass_gain_db = bass_gain;
        player->equalizer->mid_gain_db = mid_gain;
        player->equalizer->treble_gain_db = treble_gain;
        
        // Update GUI controls
        if (player->eq_enable_check) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(player->eq_enable_check), eq_enabled);
        }
        if (player->bass_scale) {
            gtk_range_set_value(GTK_RANGE(player->bass_scale), bass_gain);
        }
        if (player->mid_scale) {
            gtk_range_set_value(GTK_RANGE(player->mid_scale), mid_gain);
        }
        if (player->treble_scale) {
            gtk_range_set_value(GTK_RANGE(player->treble_scale), treble_gain);
        }
    }
    
    // Visualization
    if (player->visualizer) {
        player->visualizer->sensitivity = vis_sensitivity;
        visualizer_set_type(player->visualizer, (VisualizationType)vis_type);
    }
    
    printf("Settings loaded successfully\n");
    return true;
}

void parse_metadata(const char *metadata_str, char *title, char *artist, 
                   char *album, char *genre) {
    // Initialize with default values
    strcpy(title, "Unknown Title");
    strcpy(artist, "Unknown Artist");
    strcpy(album, "Unknown Album");
    strcpy(genre, "Unknown Genre");
    
    // NULL check FIRST before processing user data
    if (!metadata_str) return;
    
    // Parse the markup text - extract_metadata returns markup like:
    // <b>Title:</b> Something\n<b>Artist:</b> Someone\n...
    
    const char *title_start = strstr(metadata_str, "<b>Title:</b>");
    if (title_start) {
        title_start = strchr(title_start + 13, ' ');
        if (title_start) {
            title_start++; // skip space
            const char *title_end = strchr(title_start, '\n');
            if (title_end) {
                int len = title_end - title_start;
                if (len > 0 && len < 255) {
                    strncpy(title, title_start, len);
                    title[len] = '\0';
                }
            } else {
                // No newline, copy to end
                strncpy(title, title_start, 255);
                title[255] = '\0';
            }
        }
    }
    
    const char *artist_start = strstr(metadata_str, "<b>Artist:</b>");
    if (artist_start) {
        artist_start = strchr(artist_start + 14, ' ');
        if (artist_start) {
            artist_start++;
            const char *artist_end = strchr(artist_start, '\n');
            if (artist_end) {
                int len = artist_end - artist_start;
                if (len > 0 && len < 255) {
                    strncpy(artist, artist_start, len);
                    artist[len] = '\0';
                }
            } else {
                strncpy(artist, artist_start, 255);
                artist[255] = '\0';
            }
        }
    }
    
    const char *album_start = strstr(metadata_str, "<b>Album:</b>");
    if (album_start) {
        album_start = strchr(album_start + 13, ' ');
        if (album_start) {
            album_start++;
            const char *album_end = strchr(album_start, '\n');
            if (album_end) {
                int len = album_end - album_start;
                if (len > 0 && len < 255) {
                    strncpy(album, album_start, len);
                    album[len] = '\0';
                }
            } else {
                strncpy(album, album_start, 255);
                album[255] = '\0';
            }
        }
    }
    
    const char *genre_start = strstr(metadata_str, "<b>Genre:</b>");
    if (genre_start) {
        genre_start = strchr(genre_start + 13, ' ');
        if (genre_start) {
            genre_start++;
            const char *genre_end = strchr(genre_start, '\n');
            if (genre_end) {
                int len = genre_end - genre_start;
                if (len > 0 && len < 255) {
                    strncpy(genre, genre_start, len);
                    genre[len] = '\0';
                }
            } else {
                strncpy(genre, genre_start, 255);
                genre[255] = '\0';
            }
        }
    }
}

void add_column(GtkWidget *tree_view, const char *title, int col_id,  int width, gboolean sortable) {
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        title, renderer, "text", col_id, NULL);
    
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column, width);
    gtk_tree_view_column_set_resizable(column, TRUE);
    
    if (sortable) {
        gtk_tree_view_column_set_sort_column_id(column, col_id);
        gtk_tree_view_column_set_clickable(column, TRUE);
        
        // Use custom sort function for duration column
        if (col_id == COL_DURATION) {
            GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));
            GtkTreeSortable *sortable_model = GTK_TREE_SORTABLE(model);
            gtk_tree_sortable_set_sort_func(sortable_model, COL_DURATION, 
                                           duration_sort_func, NULL, NULL);
        }
    }
    
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
}

#ifndef _WIN32
static void handle_dbus_method_call(GDBusConnection *connection,
                                    const gchar *sender,
                                    const gchar *object_path,
                                    const gchar *interface_name,
                                    const gchar *method_name,
                                    GVariant *parameters,
                                    GDBusMethodInvocation *invocation,
                                    gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    if (g_strcmp0(method_name, "AddAndPlay") == 0) {
        const gchar *filepath;
        g_variant_get(parameters, "(s)", &filepath);
        
        printf("Received file from another instance: %s\n", filepath);
        
        // Add to queue and play
        if (!filename_exists_in_queue(&player->queue, filepath)) {
            add_to_queue(&player->queue, filepath);
        }
        player->queue.current_index = player->queue.count - 1;
        
        if (load_file_from_queue(player)) {
            update_queue_display_with_filter(player);
            update_gui_state(player);
            start_playback(player);
        }
        
        // Bring window to front
        gtk_window_present(GTK_WINDOW(player->window));
        
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

static const GDBusInterfaceVTable interface_vtable = {
    handle_dbus_method_call,
    NULL,
    NULL
};

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='com.zenamp.AudioPlayer'>"
    "    <method name='AddAndPlay'>"
    "      <arg type='s' name='filepath' direction='in'/>"
    "    </method>"
    "  </interface>"
    "</node>";

bool try_send_to_existing_instance(const char *filepath) {
    GError *error = NULL;
    GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    
    if (!connection) {
        g_error_free(error);
        return false;
    }
    
    GVariant *result = g_dbus_connection_call_sync(
        connection,
        ZENAMP_DBUS_NAME,
        ZENAMP_DBUS_PATH,
        "com.zenamp.AudioPlayer",
        "AddAndPlay",
        g_variant_new("(s)", filepath),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );
    
    if (result) {
        g_variant_unref(result);
        g_object_unref(connection);
        printf("Sent file to existing instance: %s\n", filepath);
        return true;
    }
    
    if (error) {
        g_error_free(error);
    }
    g_object_unref(connection);
    return false;
}

void setup_dbus_service(AudioPlayer *player) {
    GError *error = NULL;
    GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    
    if (!introspection_data) {
        printf("Failed to parse D-Bus introspection XML\n");
        return;
    }
    
    player->dbus_connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!player->dbus_connection) {
        printf("Failed to connect to D-Bus\n");
        return;
    }
    
    g_dbus_connection_register_object(
        player->dbus_connection,
        ZENAMP_DBUS_PATH,
        introspection_data->interfaces[0],
        &interface_vtable,
        player,
        NULL,
        &error
    );
    
    player->dbus_owner_id = g_bus_own_name_on_connection(
        player->dbus_connection,
        ZENAMP_DBUS_NAME,
        G_BUS_NAME_OWNER_FLAGS_NONE,
        NULL,
        NULL,
        NULL,
        NULL
    );
    
    g_dbus_node_info_unref(introspection_data);
    printf("D-Bus service registered: %s\n", ZENAMP_DBUS_NAME);
}

void cleanup_dbus_service(AudioPlayer *player) {
    if (player->dbus_owner_id > 0) {
        g_bus_unown_name(player->dbus_owner_id);
    }
    if (player->dbus_connection) {
        g_object_unref(player->dbus_connection);
    }
}
#endif

#ifdef _WIN32
bool try_send_to_existing_instance(const char *filepath) {
    HANDLE mutex = OpenMutexA(SYNCHRONIZE, FALSE, ZENAMP_MUTEX_NAME);
    
    if (!mutex) {
        // No existing instance
        return false;
    }
    
    CloseHandle(mutex);
    
    // Find existing Zenamp window
    HWND hwnd = FindWindowA(NULL, "Zenamp Audio Player");
    if (!hwnd) {
        return false;
    }
    
    // Send filepath via WM_COPYDATA
    COPYDATASTRUCT cds;
    cds.dwData = 1; // Custom identifier
    cds.cbData = strlen(filepath) + 1;
    cds.lpData = (void*)filepath;
    
    SendMessage(hwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    
    // Bring window to front
    SetForegroundWindow(hwnd);
    
    printf("Sent file to existing instance: %s\n", filepath);
    return true;
}

LRESULT CALLBACK window_proc_wrapper(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COPYDATA) {
        COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lParam;
        if (cds->dwData == 1) {
            char *filepath = (char*)cds->lpData;
            
            // Get player from window data
            AudioPlayer *player = (AudioPlayer*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (player) {
                printf("Received file from another instance: %s\n", filepath);
                
                // Add to queue and play
                if (!filename_exists_in_queue(&player->queue, filepath)) {
                    add_to_queue(&player->queue, filepath);
                    player->queue.current_index = player->queue.count - 1;
                }
                
                if (load_file_from_queue(player)) {
                    update_queue_display_with_filter(player);
                    update_gui_state(player);
                    start_playback(player);
                }
            }
            return TRUE;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void setup_windows_single_instance(AudioPlayer *player) {
    // Create mutex
    player->single_instance_mutex = CreateMutexA(NULL, TRUE, ZENAMP_MUTEX_NAME);
    
    // Hook into GTK window's Win32 HWND
    GdkWindow *gdk_window = gtk_widget_get_window(player->window);
    if (gdk_window) {
        HWND hwnd = (HWND)gdk_win32_window_get_handle(gdk_window);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)player);
        
        // Subclass window to receive WM_COPYDATA
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)window_proc_wrapper);
    }
    
    printf("Windows single instance mutex created\n");
}

void cleanup_windows_single_instance(AudioPlayer *player) {
    if (player->single_instance_mutex) {
        CloseHandle(player->single_instance_mutex);
    }
}
#endif


int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
#ifndef _WIN32
    // Check if instance already running on Linux
    if (argc > 1) {
        GError *error = NULL;
        GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
        
        if (connection) {
            bool sent_all = true;
            for (int i = 1; i < argc; i++) {
                char abs_path[4096];
                if (!realpath(argv[i], abs_path)) {
                    strncpy(abs_path, argv[i], sizeof(abs_path) - 1);
                }
                
                GVariant *result = g_dbus_connection_call_sync(
                    connection,
                    ZENAMP_DBUS_NAME,
                    ZENAMP_DBUS_PATH,
                    "com.zenamp.AudioPlayer",
                    "AddAndPlay",
                    g_variant_new("(s)", abs_path),
                    NULL,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    NULL,
                    &error
                );
                
                if (result) {
                    g_variant_unref(result);
                    printf("Sent file to existing instance: %s\n", abs_path);
                } else {
                    sent_all = false;
                    if (error) {
                        g_error_free(error);
                        error = NULL;
                    }
                }
            }
            
            g_object_unref(connection);
            
            if (sent_all) {
                printf("All files forwarded to existing instance, exiting\n");
                return 0;
            }
        }
        
        if (error) {
            g_error_free(error);
        }
    }
#else
    // Check if instance already running on Windows
    if (argc > 1) {
        HANDLE mutex = OpenMutexA(SYNCHRONIZE, FALSE, ZENAMP_MUTEX_NAME);
        
        if (mutex) {
            CloseHandle(mutex);
            
            // Find Zenamp window by property
            HWND hwnd = NULL;
            EnumWindows([](HWND hwnd_enum, LPARAM lParam) -> BOOL {
                if (GetPropA(hwnd_enum, "ZenampInstance")) {
                    *(HWND*)lParam = hwnd_enum;
                    return FALSE; // Stop enumeration
                }
                return TRUE; // Continue
            }, (LPARAM)&hwnd);
            
            if (hwnd) {
                printf("Found existing Zenamp window, sending files...\n");
                for (int i = 1; i < argc; i++) {
                    char abs_path[4096];
                    _fullpath(abs_path, argv[i], sizeof(abs_path));
                    
                    COPYDATASTRUCT cds;
                    cds.dwData = 1;
                    cds.cbData = strlen(abs_path) + 1;
                    cds.lpData = (void*)abs_path;
                    
                    SendMessage(hwnd, WM_COPYDATA, 0, (LPARAM)&cds);
                    printf("Sent file to existing instance: %s\n", abs_path);
                }
                
                SetForegroundWindow(hwnd);
                ShowWindow(hwnd, SW_RESTORE);
                printf("All files forwarded to existing instance, exiting\n");
                return 0;
            }
        }
    }
#endif
    
    init_virtual_filesystem();
    
    player = (AudioPlayer*)g_malloc0(sizeof(AudioPlayer));
    pthread_mutex_init(&player->audio_mutex, NULL);
    player->playback_speed = 1.0; 
    player->speed_accumulator = 0.0;    
    
    init_queue(&player->queue);
    init_conversion_cache(&player->conversion_cache);
    init_audio_cache(&player->audio_cache, 500);
   
    if (!init_audio(player)) {
        printf("Audio initialization failed\n");
        cleanup_conversion_cache(&player->conversion_cache);
        cleanup_virtual_filesystem();
        return 1;
    }
    
    player->equalizer = equalizer_new(SAMPLE_RATE);
    if (!player->equalizer) {
        printf("Failed to initialize equalizer\n");
    }
    
    OPL_Init(SAMPLE_RATE);
    OPL_LoadInstruments();
    
    player->cdg_display = cdg_display_new();
    player->has_cdg = false;
    
    create_main_window(player);
    update_gui_state(player);
    gtk_widget_show_all(player->window);
    
#ifdef _WIN32
    // Setup Windows single instance AFTER window is shown
    HANDLE single_instance_mutex = CreateMutexA(NULL, TRUE, ZENAMP_MUTEX_NAME);
    player->single_instance_mutex = single_instance_mutex;
    
    GdkWindow *gdk_window = gtk_widget_get_window(player->window);
    if (gdk_window) {
        HWND hwnd = (HWND)gdk_win32_window_get_handle(gdk_window);
        if (hwnd) {
            // Set a window property to identify this as Zenamp
            SetPropA(hwnd, "ZenampInstance", (HANDLE)1);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)player);
            
            // Subclass to handle WM_COPYDATA
            WNDPROC old_proc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
            SetProp(hwnd, TEXT("OldWndProc"), (HANDLE)old_proc);
            
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)+[](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                if (msg == WM_COPYDATA) {
                    COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lParam;
                    if (cds->dwData == 1) {
                        char *filepath = (char*)cds->lpData;
                        AudioPlayer *player = (AudioPlayer*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
                        
                        if (player) {
                            printf("Received file from another instance: %s\n", filepath);
                            
                            if (!filename_exists_in_queue(&player->queue, filepath)) {
                                 add_to_queue(&player->queue, filepath);
                                 player->queue.current_index = player->queue.count - 1;
                            }
                            
                            if (load_file_from_queue(player)) {
                                update_queue_display_with_filter(player);
                                update_gui_state(player);
                                start_playback(player);
                            }
                            
                            // Bring window to front
                            SetForegroundWindow(hwnd);
                            ShowWindow(hwnd, SW_RESTORE);
                        }
                        return TRUE;
                    }
                }
                
                WNDPROC old_proc = (WNDPROC)GetProp(hwnd, TEXT("OldWndProc"));
                if (old_proc) {
                    return CallWindowProc(old_proc, hwnd, msg, wParam, lParam);
                }
                return DefWindowProc(hwnd, msg, wParam, lParam);
            });
            
            printf("Windows message handler installed on HWND %p\n", hwnd);
        }
    }
    
    printf("Windows single instance mutex created\n");
#endif
    
#ifndef _WIN32
    // Setup D-Bus service on Linux
    GError *error = NULL;
    static const gchar introspection_xml[] =
        "<node>"
        "  <interface name='com.zenamp.AudioPlayer'>"
        "    <method name='AddAndPlay'>"
        "      <arg type='s' name='filepath' direction='in'/>"
        "    </method>"
        "  </interface>"
        "</node>";
    
    GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    
    if (introspection_data) {
        GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
        
        if (connection) {
            static const GDBusInterfaceVTable interface_vtable = {
                [](GDBusConnection *connection, const gchar *sender, const gchar *object_path,
                   const gchar *interface_name, const gchar *method_name, GVariant *parameters,
                   GDBusMethodInvocation *invocation, gpointer user_data) {
                    AudioPlayer *player = (AudioPlayer*)user_data;
                    
                    if (g_strcmp0(method_name, "AddAndPlay") == 0) {
                        const gchar *filepath;
                        g_variant_get(parameters, "(s)", &filepath);
                        
                        printf("Received file from another instance: %s\n", filepath);
                        if (!filename_exists_in_queue(&player->queue, filepath)) {
                            add_to_queue(&player->queue, filepath);
                            player->queue.current_index = player->queue.count - 1;
                        }
                        
                        if (load_file_from_queue(player)) {
                            update_queue_display_with_filter(player);
                            update_gui_state(player);
                            start_playback(player);
                        }
                        
                        gtk_window_present(GTK_WINDOW(player->window));
                        g_dbus_method_invocation_return_value(invocation, NULL);
                    }
                },
                NULL,
                NULL
            };
            
            g_dbus_connection_register_object(
                connection,
                ZENAMP_DBUS_PATH,
                introspection_data->interfaces[0],
                &interface_vtable,
                player,
                NULL,
                &error
            );
            
            g_bus_own_name_on_connection(
                connection,
                ZENAMP_DBUS_NAME,
                G_BUS_NAME_OWNER_FLAGS_NONE,
                NULL,
                NULL,
                NULL,
                NULL
            );
            
            printf("D-Bus service registered: %s\n", ZENAMP_DBUS_NAME);
        }
        
        g_dbus_node_info_unref(introspection_data);
    }
    
    if (error) {
        printf("D-Bus setup error: %s\n", error->message);
        g_error_free(error);
    }
#endif
    
    load_player_settings(player);
    
    char last_playlist[1024];
    bool loaded_last_playlist = false;
    if (load_last_playlist_path(last_playlist, sizeof(last_playlist))) {
        printf("Auto-loading last playlist: %s\n", last_playlist);
        if (load_m3u_playlist(player, last_playlist)) {
            printf("Successfully loaded last playlist\n");
            loaded_last_playlist = true;
            
            int saved_index = 0;
            double saved_position = 0.0;
            if (load_playlist_state(&saved_index, &saved_position)) {
                if (saved_index >= 0 && saved_index < player->queue.count) {
                    player->queue.current_index = saved_index;
                    printf("Restored queue index to %d\n", saved_index);
                }
            }
        }
    }
    
    if (argc > 1) {
        const char *first_arg = argv[1];
        const char *ext = strrchr(first_arg, '.');
        
        if (ext && (strcasecmp(ext, ".m3u") == 0 || strcasecmp(ext, ".m3u8") == 0)) {
            char abs_playlist_path[4096];
#ifdef _WIN32
            if (!_fullpath(abs_playlist_path, first_arg, sizeof(abs_playlist_path))) {
                strncpy(abs_playlist_path, first_arg, sizeof(abs_playlist_path) - 1);
            }
#else
            if (!realpath(first_arg, abs_playlist_path)) {
                strncpy(abs_playlist_path, first_arg, sizeof(abs_playlist_path) - 1);
            }
#endif
            printf("Loading new M3U playlist: %s\n", abs_playlist_path);
            clear_queue(&player->queue);
            load_m3u_playlist(player, abs_playlist_path);
            save_last_playlist_path(abs_playlist_path);
            
            for (int i = 2; i < argc; i++) {
                char abs_file_path[4096];
#ifdef _WIN32
                if (!_fullpath(abs_file_path, argv[i], sizeof(abs_file_path))) {
                    strncpy(abs_file_path, argv[i], sizeof(abs_file_path) - 1);
                }
#else
                if (!realpath(argv[i], abs_file_path)) {
                    strncpy(abs_file_path, argv[i], sizeof(abs_file_path) - 1);
                }
#endif
                if (!filename_exists_in_queue(&player->queue, abs_file_path)) {
                    add_to_queue(&player->queue, abs_file_path);
                }
            }
            
            if (player->queue.count > 0 && load_file_from_queue(player)) {
                printf("Loaded and auto-starting file from queue\n");
                update_queue_display_with_filter(player);
                update_gui_state(player);
            }
        } else {
            for (int i = 1; i < argc; i++) {
                char abs_file_path[4096];
#ifdef _WIN32
                if (!_fullpath(abs_file_path, argv[i], sizeof(abs_file_path))) {
                    strncpy(abs_file_path, argv[i], sizeof(abs_file_path) - 1);
                }
#else
                if (!realpath(argv[i], abs_file_path)) {
                    strncpy(abs_file_path, argv[i], sizeof(abs_file_path) - 1);
                }
#endif
                
                int found_index = -1;
                
                for (int j = 0; j < player->queue.count; j++) {
                    if (strcmp(player->queue.files[j], abs_file_path) == 0) {
                        found_index = j;
                        break;
                    }
                }
                
                if (found_index >= 0) {
                    printf("File already in queue at index %d, jumping to it\n", found_index);
                    player->queue.current_index = found_index;
                    if (load_file_from_queue(player)) {
                        printf("Loaded and auto-starting existing file from queue\n");
                        update_queue_display_with_filter(player);
                        update_gui_state(player);
                    }
                } else {
                    printf("File not in queue, adding and playing it\n");
                    if (!filename_exists_in_queue(&player->queue, abs_file_path)) {
                        add_to_queue(&player->queue, abs_file_path);
                        player->queue.current_index = player->queue.count - 1;
                    }
                    if (load_file_from_queue(player)) {
                        printf("Loaded and auto-starting new file\n");
                        update_queue_display_with_filter(player);
                        update_gui_state(player);
                    }
                }
            }
        }
    } else if (loaded_last_playlist && player->queue.count > 0) {
        if (load_file_from_queue(player)) {
            int saved_index = 0;
            double saved_position = 0.0;
            if (load_playlist_state(&saved_index, &saved_position)) {
                if (saved_position > 0 && saved_position < player->song_duration) {
                    seek_to_position(player, saved_position);
                    gtk_range_set_value(GTK_RANGE(player->progress_scale), saved_position);
                    printf("Restored playback position to %.2f\n", saved_position);
                }
            }
        }
        
        update_queue_display_with_filter(player);
        update_gui_state(player);
    }
    
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    gtk_main();
    
#ifdef _WIN32
    if (single_instance_mutex) {
        CloseHandle(single_instance_mutex);
    }
#endif
    
    g_free(player);
    return 0;
}
