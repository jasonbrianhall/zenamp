#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include "vfs.h"
#include "audio_player.h"

// Global virtual filesystem
static GHashTable* virtual_filesystem = NULL;
static pthread_mutex_t vfs_mutex = PTHREAD_MUTEX_INITIALIZER;

void free_virtual_file(gpointer data) {
    VirtualFile* vf = (VirtualFile*)data;
    if (vf) {
        printf("free_virtual_file: Freeing VirtualFile at %p\n", (void*)vf);
        fflush(stdout);
        if (vf->data) {
            printf("free_virtual_file: Freeing data at %p (size: %zu)\n", (void*)vf->data, vf->size);
            fflush(stdout);
            free(vf->data);
            vf->data = NULL;
        }
        free(vf);
        printf("free_virtual_file: Done\n");
        fflush(stdout);
    }
}

// Initialize the virtual filesystem
void init_virtual_filesystem() {
    pthread_mutex_lock(&vfs_mutex);
    if (!virtual_filesystem) {
        printf("init_virtual_filesystem: Creating hash table\n");
        fflush(stdout);
        virtual_filesystem = g_hash_table_new_full(
            g_str_hash, 
            g_str_equal, 
            g_free,
            (GDestroyNotify)free_virtual_file
        );
        printf("init_virtual_filesystem: Hash table created at %p\n", (void*)virtual_filesystem);
        fflush(stdout);
    }
    pthread_mutex_unlock(&vfs_mutex);
}

// Simplified cleanup
void cleanup_virtual_filesystem() {
    printf("cleanup_virtual_filesystem: START\n");
    fflush(stdout);
    
    pthread_mutex_lock(&vfs_mutex);
    
    if (virtual_filesystem) {
        printf("cleanup_virtual_filesystem: Hash table at %p\n", (void*)virtual_filesystem);
        fflush(stdout);
        
        int size = g_hash_table_size(virtual_filesystem);
        printf("cleanup_virtual_filesystem: Hash table contains %d entries\n", size);
        fflush(stdout);
        
        printf("cleanup_virtual_filesystem: Calling g_hash_table_destroy...\n");
        fflush(stdout);
        
        g_hash_table_destroy(virtual_filesystem);
        
        printf("cleanup_virtual_filesystem: g_hash_table_destroy completed\n");
        fflush(stdout);
        
        virtual_filesystem = NULL;
        
        printf("cleanup_virtual_filesystem: COMPLETE\n");
        fflush(stdout);
    } else {
        printf("cleanup_virtual_filesystem: No filesystem to clean\n");
        fflush(stdout);
    }
    
    pthread_mutex_unlock(&vfs_mutex);
}

// Create a new virtual file
VirtualFile* create_virtual_file(const char* filename) {
    pthread_mutex_lock(&vfs_mutex);
    
    if (!virtual_filesystem) {
        pthread_mutex_unlock(&vfs_mutex);
        init_virtual_filesystem();
        pthread_mutex_lock(&vfs_mutex);
    }
    
    VirtualFile* vf = malloc(sizeof(VirtualFile));
    if (!vf) {
        pthread_mutex_unlock(&vfs_mutex);
        return NULL;
    }
    
    vf->data = malloc(1024);
    vf->size = 0;
    vf->capacity = 1024;
    vf->position = 0;
    
    if (!vf->data) {
        free(vf);
        pthread_mutex_unlock(&vfs_mutex);
        return NULL;
    }
    
    printf("create_virtual_file: Created VirtualFile at %p for '%s'\n", (void*)vf, filename);
    fflush(stdout);
    
    g_hash_table_insert(virtual_filesystem, g_strdup(filename), vf);
    
    pthread_mutex_unlock(&vfs_mutex);
    return vf;
}

// Get an existing virtual file
VirtualFile* get_virtual_file(const char* filename) {
    pthread_mutex_lock(&vfs_mutex);
    VirtualFile* vf = NULL;
    if (virtual_filesystem) {
        vf = (VirtualFile*)g_hash_table_lookup(virtual_filesystem, filename);
    }
    pthread_mutex_unlock(&vfs_mutex);
    return vf;
}

// Write data to virtual file
bool virtual_file_write(VirtualFile* vf, const void* data, size_t size) {
    if (!vf || !data) return false;
    
    // Expand capacity if needed
    if (vf->position + size > vf->capacity) {
        size_t new_capacity = vf->capacity;
        while (new_capacity < vf->position + size) {
            new_capacity *= 2;
        }
        
        char* new_data = realloc(vf->data, new_capacity);
        if (!new_data) return false;
        
        vf->data = new_data;
        vf->capacity = new_capacity;
    }
    
    memcpy(vf->data + vf->position, data, size);
    vf->position += size;
    
    if (vf->position > vf->size) {
        vf->size = vf->position;
    }
    
    return true;
}

// Read data from virtual file
size_t virtual_file_read(VirtualFile* vf, void* buffer, size_t size) {
    if (!vf || !buffer) return 0;
    
    size_t available = vf->size - vf->position;
    size_t to_read = (size < available) ? size : available;
    
    if (to_read > 0) {
        memcpy(buffer, vf->data + vf->position, to_read);
        vf->position += to_read;
    }
    
    return to_read;
}

// Seek in virtual file
bool virtual_file_seek(VirtualFile* vf, long offset, int whence) {
    if (!vf) return false;
    
    long new_position;
    
    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = vf->position + offset;
            break;
        case SEEK_END:
            new_position = vf->size + offset;
            break;
        default:
            return false;
    }
    
    if (new_position < 0 || (size_t)new_position > vf->size) {
        return false;
    }
    
    vf->position = new_position;
    return true;
}

// Get current position in virtual file
long virtual_file_tell(VirtualFile* vf) {
    if (!vf) return -1;
    return (long)vf->position;
}

// Get virtual file size
size_t virtual_file_size(VirtualFile* vf) {
    if (!vf) return 0;
    return vf->size;
}

// Delete a virtual file
bool delete_virtual_file(const char* filename) {
    pthread_mutex_lock(&vfs_mutex);
    bool result = false;
    if (virtual_filesystem) {
        result = g_hash_table_remove(virtual_filesystem, filename);
    }
    pthread_mutex_unlock(&vfs_mutex);
    return result;
}

VirtualWAVConverter* virtual_wav_converter_init(const char* filename, int sample_rate, int channels) {
    VirtualWAVConverter* converter = malloc(sizeof(VirtualWAVConverter));
    if (!converter) return NULL;
    
    converter->vf = create_virtual_file(filename);
    if (!converter->vf) {
        free(converter);
        return NULL;
    }
    
    converter->sample_rate = sample_rate;
    converter->channels = channels;
    converter->samples_written = 0;
    
    // Write WAV header (we'll update it later)
    char header[44] = {0};
    
    // RIFF header
    memcpy(header, "RIFF", 4);
    // File size (will be updated later)
    memcpy(header + 8, "WAVE", 4);
    
    // Format chunk
    memcpy(header + 12, "fmt ", 4);
    *(uint32_t*)(header + 16) = 16; // Chunk size
    *(uint16_t*)(header + 20) = 1;  // Audio format (PCM)
    *(uint16_t*)(header + 22) = channels;
    *(uint32_t*)(header + 24) = sample_rate;
    *(uint32_t*)(header + 28) = sample_rate * channels * 2; // Byte rate
    *(uint16_t*)(header + 32) = channels * 2; // Block align
    *(uint16_t*)(header + 34) = 16; // Bits per sample
    
    // Data chunk
    memcpy(header + 36, "data", 4);
    // Data size (will be updated later)
    
    virtual_file_write(converter->vf, header, 44);
    
    return converter;
}

bool virtual_wav_converter_write(VirtualWAVConverter* converter, int16_t* samples, size_t count) {
    if (!converter || !samples) return false;
    
    size_t bytes = count * sizeof(int16_t);
    bool success = virtual_file_write(converter->vf, samples, bytes);
    
    if (success) {
        converter->samples_written += count;
    }
    
    return success;
}

void virtual_wav_converter_finish(VirtualWAVConverter* converter) {
    if (!converter) return;
    
    // Update WAV header with correct sizes
    size_t data_size = converter->samples_written * sizeof(int16_t);
    size_t file_size = data_size + 36;
    
    // Seek to file size position and update
    virtual_file_seek(converter->vf, 4, SEEK_SET);
    uint32_t file_size_le = file_size;
    virtual_file_write(converter->vf, &file_size_le, 4);
    
    // Seek to data size position and update
    virtual_file_seek(converter->vf, 40, SEEK_SET);
    uint32_t data_size_le = data_size;
    virtual_file_write(converter->vf, &data_size_le, 4);
}

void virtual_wav_converter_free(VirtualWAVConverter* converter) {
    if (converter) {
        free(converter);
    }
}

// Modified functions for the audio player
bool convert_midi_to_virtual_wav(AudioPlayer *player, const char* filename) {
    // Generate a unique virtual filename
    static int virtual_counter = 0;
    char virtual_filename[256];
    snprintf(virtual_filename, sizeof(virtual_filename), "virtual_midi_%d.wav", virtual_counter++);
    
    strncpy(player->temp_wav_file, virtual_filename, sizeof(player->temp_wav_file) - 1);
    player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
    
    printf("Converting MIDI to virtual WAV: %s -> %s\n", filename, virtual_filename);
    
    // Temporarily shut down the main SDL audio system
    if (player->audio_device) {
        SDL_CloseAudioDevice(player->audio_device);
        player->audio_device = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    
    playTime = 0;
    isPlaying = true;
    
    if (!initSDL()) {
        printf("SDL init for conversion failed\n");
        return false;
    }
    
    if (!loadMidiFile(filename)) {
        printf("MIDI file load failed\n");
        cleanup();
        return false;
    }
    
    VirtualWAVConverter* wav_converter = virtual_wav_converter_init(virtual_filename, SAMPLE_RATE, AUDIO_CHANNELS);
    if (!wav_converter) {
        printf("Virtual WAV converter init failed\n");
        cleanup();
        return false;
    }
    
    int16_t audio_buffer[AUDIO_BUFFER * AUDIO_CHANNELS];
    double buffer_duration = (double)AUDIO_BUFFER / SAMPLE_RATE;
    
    processEvents();
    
    while (isPlaying) {
        memset(audio_buffer, 0, sizeof(audio_buffer));
        OPL_Generate(audio_buffer, AUDIO_BUFFER);
        
        if (!virtual_wav_converter_write(wav_converter, audio_buffer, AUDIO_BUFFER * AUDIO_CHANNELS)) {
            printf("Virtual WAV write failed\n");
            break;
        }
        
        playTime += buffer_duration;
        playwait -= buffer_duration;
        
        while (playwait <= 0 && isPlaying) {
            processEvents();
        }
        
        if (((int)playTime) % 10 == 0) {
            static int last_reported = -1;
            if ((int)playTime != last_reported) {
                printf("Converting... %d seconds\n", (int)playTime);
                last_reported = (int)playTime;
            }
        }
    }
    
    virtual_wav_converter_finish(wav_converter);
    virtual_wav_converter_free(wav_converter);
    cleanup();
    
    printf("Virtual conversion complete: %.2f seconds\n", playTime);
    
    // Reinitialize the main SDL audio system for playback
    printf("Reinitializing SDL audio for playback...\n");
    if (!init_audio(player)) {
        printf("Failed to reinitialize audio for playback\n");
        return false;
    }
    
    return true;
}

bool load_virtual_wav_file(AudioPlayer *player, const char* virtual_filename) {
    // Check cache first (use virtual filename as key)
    CachedAudioBuffer *cached = find_in_cache(&player->audio_cache, virtual_filename);
    if (cached) {
        player->sample_rate = cached->sample_rate;
        player->channels = cached->channels;
        player->bits_per_sample = cached->bits_per_sample;
        player->song_duration = cached->song_duration;
        
        if (!init_audio(player, player->sample_rate, player->channels)) {
            return false;
        }
        
        // COPY data from cache
        int16_t *data_copy = malloc(cached->length * sizeof(int16_t));
        if (!data_copy) return false;
        memcpy(data_copy, cached->data, cached->length * sizeof(int16_t));
        
        pthread_mutex_lock(&player->audio_mutex);
        if (player->audio_buffer.data) free(player->audio_buffer.data);
        player->audio_buffer.data = data_copy;
        player->audio_buffer.length = cached->length;
        player->audio_buffer.position = 0;
        pthread_mutex_unlock(&player->audio_mutex);
        
        printf("Loaded virtual file from cache: %zu samples\n", cached->length);
        
        // Can delete virtual file now since it's in cache
        delete_virtual_file(virtual_filename);
        return true;
    }
    
    VirtualFile* vf = get_virtual_file(virtual_filename);
    if (!vf) {
        printf("Cannot open virtual WAV file: %s\n", virtual_filename);
        return false;
    }
    
    // Reset position to beginning
    virtual_file_seek(vf, 0, SEEK_SET);
    
    // Read WAV header
    char header[44];
    if (virtual_file_read(vf, header, 44) != 44) {
        printf("Cannot read virtual WAV header\n");
        return false;
    }
    
    // Verify WAV format
    if (strncmp(header, "RIFF", 4) != 0 || strncmp(header + 8, "WAVE", 4) != 0) {
        printf("Invalid virtual WAV format\n");
        return false;
    }
    
    // Extract WAV info
    player->sample_rate = *(int*)(header + 24);
    player->channels = *(short*)(header + 22);
    player->bits_per_sample = *(short*)(header + 34);
    
    printf("Virtual WAV: %d Hz, %d channels, %d bits\n", 
           player->sample_rate, player->channels, player->bits_per_sample);
    
    // Reinitialize audio with the correct sample rate and channels
    if (!init_audio(player, player->sample_rate, player->channels)) {
        printf("Failed to reinitialize audio for virtual WAV format\n");
        return false;
    }
    
    // Calculate data size and duration
    size_t total_size = virtual_file_size(vf);
    size_t data_size = total_size - 44;
    
    player->song_duration = data_size / (double)(player->sample_rate * player->channels * (player->bits_per_sample / 8));
    printf("Virtual WAV duration: %.2f seconds\n", player->song_duration);
    
    // Allocate NEW buffer (separate from VirtualFile)
    int16_t* wav_data = (int16_t*)malloc(data_size);
    if (!wav_data) {
        printf("Memory allocation failed\n");
        return false;
    }
    
    // COPY data from virtual file (don't share the pointer)
    if (virtual_file_read(vf, wav_data, data_size) != data_size) {
        printf("Virtual WAV data read failed\n");
        free(wav_data);
        return false;
    }
    
    // Make a copy for cache
    int16_t *cache_copy = malloc(data_size);
    if (cache_copy) {
        memcpy(cache_copy, wav_data, data_size);
        add_to_cache(&player->audio_cache, virtual_filename, cache_copy,
                     data_size / sizeof(int16_t), player->sample_rate,
                     player->channels, player->bits_per_sample,
                     player->song_duration);
    }
    
    // Store in audio buffer
    pthread_mutex_lock(&player->audio_mutex);
    if (player->audio_buffer.data) {
        free(player->audio_buffer.data);
    }
    player->audio_buffer.data = wav_data;
    player->audio_buffer.length = data_size / sizeof(int16_t);
    player->audio_buffer.position = 0;
    pthread_mutex_unlock(&player->audio_mutex);
    
    printf("Loaded %zu samples from virtual file\n", player->audio_buffer.length);
    
    // NOW delete the virtual file since we've copied the data and cached it
    printf("Deleting virtual file '%s' after copying to cache\n", virtual_filename);
    fflush(stdout);
    delete_virtual_file(virtual_filename);
    
    return true;
}
