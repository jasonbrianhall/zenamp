#include "audio_player.h"

void init_audio_cache(AudioBufferCache *cache, size_t max_memory_mb) {
    cache->buffers = NULL;
    cache->count = 0;
    cache->capacity = 0;
    cache->total_memory = 0;
    cache->max_memory = max_memory_mb * 1024 * 1024;
    printf("Audio cache initialized with %zu MB limit\n", max_memory_mb);
}

CachedAudioBuffer* find_in_cache(AudioBufferCache *cache, const char *filepath) {
    for (int i = 0; i < cache->count; i++) {
        if (strcmp(cache->buffers[i]->filepath, filepath) == 0) {
            cache->buffers[i]->last_access = time(NULL);
            printf("Cache HIT: %s\n", filepath);
            return cache->buffers[i];
        }
    }
    printf("Cache MISS: %s\n", filepath);
    return NULL;
}

void evict_oldest_from_cache(AudioBufferCache *cache) {
    if (cache->count == 0) return;
    
    // Find oldest (LRU)
    int oldest_idx = 0;
    time_t oldest_time = cache->buffers[0]->last_access;
    
    for (int i = 1; i < cache->count; i++) {
        if (cache->buffers[i]->last_access < oldest_time) {
            oldest_time = cache->buffers[i]->last_access;
            oldest_idx = i;
        }
    }
    
    CachedAudioBuffer *evicted = cache->buffers[oldest_idx];
    printf("Cache EVICT: %s (%.2f MB, last used %ld sec ago)\n", 
           evicted->filepath,
           evicted->memory_size / (1024.0 * 1024.0),
           time(NULL) - evicted->last_access);
    
    cache->total_memory -= evicted->memory_size;
    free(evicted->data);
    free(evicted->filepath);
    free(evicted);
    
    // Shift array down
    for (int i = oldest_idx; i < cache->count - 1; i++) {
        cache->buffers[i] = cache->buffers[i + 1];
    }
    cache->count--;
}

void add_to_cache(AudioBufferCache *cache, const char *filepath, 
                  int16_t *data, size_t length, int sample_rate, 
                  int channels, int bits_per_sample, double song_duration) {
    
    size_t memory_size = length * sizeof(int16_t);
    
    // Evict until we have space
    while (cache->total_memory + memory_size > cache->max_memory && cache->count > 0) {
        evict_oldest_from_cache(cache);
    }
    
    // Don't cache if single file is larger than max
    if (memory_size > cache->max_memory) {
        printf("Cache SKIP: %s too large (%.2f MB)\n", 
               filepath, memory_size / (1024.0 * 1024.0));
        return;
    }
    
    // Expand capacity if needed
    if (cache->count >= cache->capacity) {
        int new_capacity = cache->capacity == 0 ? 10 : cache->capacity * 2;
        CachedAudioBuffer **new_buffers = realloc(cache->buffers, 
                                                   new_capacity * sizeof(CachedAudioBuffer*));
        if (!new_buffers) return;
        cache->buffers = new_buffers;
        cache->capacity = new_capacity;
    }
    
    // Create new cache entry
    CachedAudioBuffer *cached = malloc(sizeof(CachedAudioBuffer));
    cached->filepath = strdup(filepath);
    cached->data = data;  // Takes ownership
    cached->length = length;
    cached->sample_rate = sample_rate;
    cached->channels = channels;
    cached->bits_per_sample = bits_per_sample;
    cached->song_duration = song_duration;
    cached->last_access = time(NULL);
    cached->memory_size = memory_size;
    
    cache->buffers[cache->count++] = cached;
    cache->total_memory += memory_size;
    
    printf("Cache ADD: %s (%.2f MB) - Cache now %d files, %.2f/%.2f MB\n",
           filepath,
           memory_size / (1024.0 * 1024.0),
           cache->count,
           cache->total_memory / (1024.0 * 1024.0),
           cache->max_memory / (1024.0 * 1024.0));
}

void cleanup_audio_cache(AudioBufferCache *cache) {
    for (int i = 0; i < cache->count; i++) {
        free(cache->buffers[i]->data);
        free(cache->buffers[i]->filepath);
        free(cache->buffers[i]);
    }
    free(cache->buffers);
    cache->buffers = NULL;
    cache->count = 0;
    cache->capacity = 0;
    cache->total_memory = 0;
}
