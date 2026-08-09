#include <sys/stat.h>
#include "vfs.h"
#include "audio_player.h"

void init_conversion_cache(ConversionCache *cache) {
    cache->entries = NULL;
    cache->count = 0;
    cache->capacity = 0;
}

void cleanup_conversion_cache(ConversionCache *cache) {
    for (int i = 0; i < cache->count; i++) {
        g_free(cache->entries[i].original_path);
        g_free(cache->entries[i].virtual_filename);
    }
    g_free(cache->entries);
    cache->entries = NULL;
    cache->count = 0;
    cache->capacity = 0;
}

bool is_file_modified(const char* filepath, time_t cached_time, off_t cached_size) {
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        return true; // File doesn't exist or can't be accessed, consider it modified
    }
    
    return (file_stat.st_mtime != cached_time || file_stat.st_size != cached_size);
}

const char* get_cached_conversion(ConversionCache *cache, const char* original_path) {
    for (int i = 0; i < cache->count; i++) {
        if (strcmp(cache->entries[i].original_path, original_path) == 0) {
            // Check if the original file has been modified since caching
            if (!is_file_modified(original_path, cache->entries[i].modification_time, cache->entries[i].file_size)) {
                // Check if virtual file still exists by trying to get it
                VirtualFile* vf = get_virtual_file(cache->entries[i].virtual_filename);
                if (vf != NULL) {
                    printf("Cache hit: Using cached conversion %s for %s\n", 
                           cache->entries[i].virtual_filename, original_path);
                    return cache->entries[i].virtual_filename;
                } else {
                    printf("Cache miss: Virtual file %s no longer exists\n", cache->entries[i].virtual_filename);
                }
            } else {
                printf("Cache miss: File %s has been modified since caching\n", original_path);
            }
            
            // Remove invalid cache entry
            g_free(cache->entries[i].original_path);
            g_free(cache->entries[i].virtual_filename);
            
            // Shift remaining entries
            for (int j = i; j < cache->count - 1; j++) {
                cache->entries[j] = cache->entries[j + 1];
            }
            cache->count--;
            return NULL;
        }
    }
    
    printf("Cache miss: No cached conversion found for %s\n", original_path);
    return NULL;
}

void add_to_conversion_cache(ConversionCache *cache, const char* original_path, const char* virtual_filename) {
    // Get file stats for the original file
    struct stat file_stat;
    if (stat(original_path, &file_stat) != 0) {
        printf("Warning: Cannot stat file %s for caching\n", original_path);
        return;
    }
    
    // Expand cache if needed
    if (cache->count >= cache->capacity) {
        int new_capacity = cache->capacity == 0 ? 10 : cache->capacity * 2;
        ConversionCacheEntry *new_entries = (ConversionCacheEntry*)g_realloc(cache->entries, 
                                                      new_capacity * sizeof(ConversionCacheEntry));
        if (!new_entries) {
            printf("Warning: Failed to expand conversion cache\n");
            return;
        }
        cache->entries = new_entries;
        cache->capacity = new_capacity;
    }
    
    // Add new entry
    cache->entries[cache->count].original_path = g_strdup(original_path);
    cache->entries[cache->count].virtual_filename = g_strdup(virtual_filename);
    cache->entries[cache->count].modification_time = file_stat.st_mtime;
    cache->entries[cache->count].file_size = file_stat.st_size;
    cache->count++;
    
    printf("Added to conversion cache: %s -> %s\n", original_path, virtual_filename);
}
