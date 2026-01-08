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

// Add these includes for access() and F_OK
#ifdef _WIN32
    #include <io.h>
    #define F_OK 0
#else
    #include <unistd.h>
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


bool is_m3u_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    
    char ext_lower[10];
    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
    ext_lower[sizeof(ext_lower) - 1] = '\0';
    for (int i = 0; ext_lower[i]; i++) {
        ext_lower[i] = tolower(ext_lower[i]);
    }
    
    return strcmp(ext_lower, ".m3u") == 0 || strcmp(ext_lower, ".m3u8") == 0;
}

bool load_m3u_playlist(AudioPlayer *player, const char *m3u_path) {
    FILE *file = fopen(m3u_path, "r");
    if (!file) {
        printf("Cannot open M3U file: %s\n", m3u_path);
        return false;
    }
    clear_queue(&player->queue);
    char line[1024];
    char m3u_dir[512];
    bool was_empty_queue = (player->queue.count == 0);
    int added_count = 0;
    
    // Get directory of M3U file for relative paths
    strncpy(m3u_dir, m3u_path, sizeof(m3u_dir) - 1);
    m3u_dir[sizeof(m3u_dir) - 1] = '\0';
    char *last_slash = strrchr(m3u_dir, '/');
    if (!last_slash) last_slash = strrchr(m3u_dir, '\\');
    if (last_slash) {
        *(last_slash + 1) = '\0';
    } else {
        strcpy(m3u_dir, "./");
    }
    
    char error_message[4096];
    error_message[0] = '\0';
    int error_buffer_pos = 0;
    bool has_errors = false;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = '\0';
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') continue;
        
        char full_path[1024];
        
        // Handle relative vs absolute paths
        if (line[0] == '/' || (strlen(line) > 1 && line[1] == ':')) {
            // Absolute path
            strncpy(full_path, line, sizeof(full_path) - 1);
        } else {
            // Relative path
            snprintf(full_path, sizeof(full_path), "%s%s", m3u_dir, line);
        }
        full_path[sizeof(full_path) - 1] = '\0';
        
        // Check if file exists
        bool file_exists = (access(full_path, F_OK) == 0);
        
        // ✅ FIXED: Add file to queue REGARDLESS of whether it exists
        // The queue display will handle marking it with ⚠ if inaccessible
        if (add_to_queue(&player->queue, full_path)) {
            added_count++;
            if (file_exists) {
                printf("Added to queue: %s\n", full_path);
            } else {
                printf("Warning: File not found, adding to queue anyway: %s (will show ⚠)\n", full_path);
            }
        }
        
        // Track missing files for user feedback
        if (!file_exists) {
            has_errors = true;
            
            if (!error_message[0]) {
                error_buffer_pos = snprintf(error_message, sizeof(error_message), "Not found: %s", full_path);
            } else {
                int remaining = sizeof(error_message) - error_buffer_pos - 1;
                if (remaining > strlen(full_path) + 2) {
                    error_buffer_pos += snprintf(error_message + error_buffer_pos, remaining, ", %s", full_path);
                } else if (remaining > 3) {
                    snprintf(error_message + error_buffer_pos, remaining, "...");
                }
            }
        }
    }
    
    fclose(file);
    
    printf("M3U loaded: %d files added\n", added_count);
    
    // Display accumulated errors
    if (has_errors && player->visualizer) {
        snprintf(player->visualizer->error_message, sizeof(player->visualizer->error_message),
                 "%s", error_message);
        player->visualizer->showing_error = true;
        player->visualizer->error_display_time = 3.0;  // Show for 3 seconds since the error can be long
    }
    
    // If queue was empty and we added files, load the first one
    if (was_empty_queue && player->queue.count > 0) {
        if (load_file_from_queue(player)) {
            update_gui_state(player);
        }
    }
    
    update_queue_display_with_filter(player);
    update_gui_state(player);
    
    return added_count > 0;
}

bool save_m3u_playlist(AudioPlayer *player, const char *m3u_path) {
    if (player->queue.count == 0) {
        printf("No files in queue to save\n");
        return false;
    }
    
    FILE *file = fopen(m3u_path, "w");
    if (!file) {
        printf("Cannot create M3U file: %s\n", m3u_path);
        return false;
    }
    
    // Write M3U header
    fprintf(file, "#EXTM3U\n");
    
    // Get directory of M3U file for relative paths
    char m3u_dir[512];
    strncpy(m3u_dir, m3u_path, sizeof(m3u_dir) - 1);
    m3u_dir[sizeof(m3u_dir) - 1] = '\0';
    char *last_slash = strrchr(m3u_dir, '/');
    if (!last_slash) last_slash = strrchr(m3u_dir, '\\');
    if (last_slash) {
        *(last_slash + 1) = '\0';
    } else {
        strcpy(m3u_dir, "");
    }
    
    for (int i = 0; i < player->queue.count; i++) {
        const char *file_path = player->queue.files[i];
        
        // Try to make path relative if it's in the same directory or subdirectory
        if (strlen(m3u_dir) > 0 && strncmp(file_path, m3u_dir, strlen(m3u_dir)) == 0) {
            // File is in M3U directory or subdirectory, use relative path
            fprintf(file, "%s\n", file_path + strlen(m3u_dir));
        } else {
            // Use absolute path
            fprintf(file, "%s\n", file_path);
        }
    }
    
    fclose(file);
    printf("M3U playlist saved: %s (%d files)\n", m3u_path, player->queue.count);
    return true;
}
