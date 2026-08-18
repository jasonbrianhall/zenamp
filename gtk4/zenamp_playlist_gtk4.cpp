// Zenamp (c) Jason Hall 2026 - Queue, Playlist, Settings & Metadata Cache
//
// Split out of zenamp_main.cpp: PlayQueue management, M3U playlist
// load/save, last-playlist/playback-state persistence, player settings
// persistence, and the metadata cache path + save-on-exit helper.

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <sys/stat.h>
#include <fstream>
#include <string>

#ifndef _WIN32
#include <sys/types.h>
#else
#include <shlobj.h>
#include <windows.h>
#endif

#include "zenamp_playlist.h"
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
#include "karafun.h"
#include "kar.h"

// Externs shared with zenamp_main.cpp
extern double playTime;
extern int globalVolume;

// GTK4 removed gtk_dialog_run() - dialogs are shown non-modally now and
// close themselves on response instead of blocking the caller. Small
// local copy of the same helper used in zenamp_main.cpp, kept private
// to this file so the two translation units stay decoupled.
static void destroy_dialog_on_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    gtk_window_destroy(GTK_WINDOW(dialog));
}

// ============================================================================
// Queue management
// ============================================================================

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
        SDL_Log("advance_queue: Empty queue");
        return false;
    }
    
    if (queue->count == 1) {
        SDL_Log("advance_queue: Single song queue - %s repeat", 
               queue->repeat_queue ? "restarting (repeat on)" : "stopping (repeat off)");
        if (queue->repeat_queue) {
            // For single song, just stay at index 0
            queue->current_index = 0;
            return true;
        } else {
            return false;
        }
    }
    
    SDL_Log("advance_queue: Before - index %d of %d", queue->current_index, queue->count);
    
    queue->current_index++;
    
    if (queue->current_index >= queue->count) {
        if (queue->repeat_queue) {
            queue->current_index = 0;
            SDL_Log("advance_queue: Wrapped to beginning (repeat on)");
            return true;
        } else {
            queue->current_index = queue->count - 1; // Stay at last song
            SDL_Log("advance_queue: At end, no repeat");
            return false;
        }
    }
    
    SDL_Log("advance_queue: After - index %d of %d", queue->current_index, queue->count);
    return true;
}

bool previous_queue(PlayQueue *queue) {
    if (queue->count == 0) {
        SDL_Log("previous_queue: Empty queue");
        return false;
    }
    
    SDL_Log("previous_queue: Before - index %d of %d", queue->current_index, queue->count);
    
    queue->current_index--;
    
    if (queue->current_index < 0) {
        if (queue->repeat_queue) {
            queue->current_index = queue->count - 1;
            SDL_Log("previous_queue: Wrapped to end (repeat on)");
            return true;
        } else {
            queue->current_index = 0;
            SDL_Log("previous_queue: At beginning, no repeat");
            return false;
        }
    }
    
    SDL_Log("previous_queue: After - index %d of %d", queue->current_index, queue->count);
    return true;
}

// remove_from_queue lives further down in the original file (near
// toggle_pause) but is queue bookkeeping, so it belongs here too.
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

// ============================================================================
// Playlist (M3U) load/save, last-playlist tracking, player settings, and
// the metadata cache path + save-on-exit helper
// ============================================================================

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

// GTK4 removed gtk_dialog_run() - async response callback, same pattern as
// on_menu_open_response() above.
static void on_menu_load_playlist_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        char *filename = file ? g_file_get_path(file) : NULL;
        if (filename) {
            if (isValidM3U(filename)) {
                if (load_m3u_playlist(player, filename)) {
                    add_to_recent_files(filename, "audio/x-mpegurl");
                    save_last_playlist_path(filename);
                }
            } else {
                SDL_Log("Playlist appears empty or corrupted.");
            }
            g_free(filename);
        }
        if (file) {
            g_object_unref(file);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

void on_menu_load_playlist(GtkWidget *menuitem, gpointer user_data) {
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
            SDL_Log("Playlist appears empty or corrupted");
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
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_menu_load_playlist_response), player);
    gtk_window_present(GTK_WINDOW(dialog));
#endif
}

bool save_current_queue_on_exit(AudioPlayer *player) {
    if (player->queue.count == 0) {
        SDL_Log("No queue to save on exit");
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
        SDL_Log("Failed to create temp queue file");
        return false;
    }
    
    fprintf(f, "#EXTM3U\n");
    
    // Save ALL files from the actual queue, not just the filtered display
    for (int i = 0; i < player->queue.count; i++) {
        fprintf(f, "%s\n", player->queue.files[i]);
    }
    
    fclose(f);
    SDL_Log("Saved current queue to: %s", temp_playlist_path);
    
    // Save current index and playback position
    f = fopen(position_path, "w");
    if (f) {
        fprintf(f, "%d\n", player->queue.current_index);
        fprintf(f, "%.2f\n", playTime);
        fclose(f);
        SDL_Log("Saved playback state: index=%d, time=%.2f", 
               player->queue.current_index, playTime);
    }
    
    if (save_last_playlist_path(temp_playlist_path)) {
        SDL_Log("Set temp queue as last playlist");
        return true;
    }
    
    return false;
}

// GTK4 removed gtk_dialog_run() - async response callback, same pattern as
// on_menu_open_response()/on_menu_load_playlist_response() above.
static void on_menu_save_playlist_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        char *filename = file ? g_file_get_path(file) : NULL;
        if (filename) {
            if (save_m3u_playlist(player, filename)) {
                // ADD TO RECENT FILES
                add_to_recent_files(filename, "audio/x-mpegurl");
            }
            g_free(filename);
        }
        if (file) {
            g_object_unref(file);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

void on_menu_save_playlist(GtkWidget *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    if (player->queue.count == 0) {
        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(player->window),
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_MESSAGE_WARNING,
                                                         GTK_BUTTONS_OK,
                                                         "No files in queue to save");
        g_signal_connect(error_dialog, "response", G_CALLBACK(destroy_dialog_on_response), NULL);
        gtk_window_present(GTK_WINDOW(error_dialog));
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
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_menu_save_playlist_response), player);
    gtk_window_present(GTK_WINDOW(dialog));
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
        SDL_Log("Failed to save last playlist path");
        return false;
    }
    
    fprintf(f, "%s\n", playlist_path);
    fclose(f);
    SDL_Log("Saved last playlist path: %s", playlist_path);
    return true;
}

bool load_last_playlist_path(char *playlist_path, size_t path_size) {
    char config_path[1024];
    if (!get_last_playlist_path(config_path, sizeof(config_path))) {
        return false;
    }
    
    FILE *f = fopen(config_path, "r");
    if (!f) {
        SDL_Log("No last playlist file found");
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
        SDL_Log("Last playlist no longer exists: %s", playlist_path);
        return false;
    }
    fclose(test);
    
    SDL_Log("Found last playlist: %s", playlist_path);
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
    SDL_Log("Loaded playback state: index=%d, time=%.2f", *current_index, *position);
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

bool get_metadata_cache_path(char *path, size_t path_size) {
    char app_data[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, app_data) != S_OK) {
        return false;
    }
    snprintf(path, path_size, "%s\\Zenamp\\cache.txt", app_data);

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

bool get_metadata_cache_path(char *path, size_t path_size) {
    const char *home = getenv("HOME");
    if (!home) {
        return false;
    }
    snprintf(path, path_size, "%s/.zenamp/cache.txt", home);

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
        SDL_Log("Failed to get settings path");
        return false;
    }
    
    FILE *f = fopen(settings_path, "w");
    if (!f) {
        SDL_Log("Failed to save settings");
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

    // Queue sort order: the "Group by" mode (None/Artist/Album/Genre), the
    // karaoke-only checkbox, and - for the flat (ungrouped) view - which
    // column is sorted and in which direction, so next launch reopens to
    // the same view instead of always starting flat/unsorted.
    fprintf(f, "queue_group_mode=%d\n", (int)player->queue_group_mode);
    fprintf(f, "queue_karaoke_only=%d\n", player->queue_karaoke_only_filter ? 1 : 0);
    if (player->queue_store) {
        GtkTreeSortable *sortable = GTK_TREE_SORTABLE(player->queue_store);
        gint sort_column_id = -1;
        GtkSortType sort_type = GTK_SORT_ASCENDING;
        if (gtk_tree_sortable_get_sort_column_id(sortable, &sort_column_id, &sort_type)) {
            fprintf(f, "queue_sort_column=%d\n", sort_column_id);
            fprintf(f, "queue_sort_type=%d\n", (int)sort_type);
        }
    }
    
    fclose(f);
    SDL_Log("Settings saved to: %s", settings_path);
    return true;
}

// Small wrapper so the two shutdown paths (signal_handler and
// on_window_delete_event) don't each have to duplicate the path lookup.
void save_metadata_cache_on_exit() {
    char cache_path[1024];
    if (get_metadata_cache_path(cache_path, sizeof(cache_path))) {
        save_queue_metadata_cache(cache_path);
    }
}

bool load_player_settings(AudioPlayer *player) {
    char settings_path[1024];
    if (!get_settings_path(settings_path, sizeof(settings_path))) {
        SDL_Log("Failed to get settings path");
        return false;
    }
    
    FILE *f = fopen(settings_path, "r");
    if (!f) {
        SDL_Log("No settings file found, using defaults");
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
    int queue_group_mode = QUEUE_GROUP_NONE;
    int queue_karaoke_only = 0;
    int queue_sort_column = -1;
    int queue_sort_type = (int)GTK_SORT_ASCENDING;
    
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Parse key=value pairs
        if (sscanf(line, "volume=%lf", &volume) == 1) {
            SDL_Log("Loaded volume: %.2f", volume);
        }
        else if (sscanf(line, "speed=%lf", &speed) == 1) {
            SDL_Log("Loaded speed: %.2f", speed);
        }
        else if (sscanf(line, "eq_enabled=%d", (int*)&eq_enabled) == 1) {
            SDL_Log("Loaded eq_enabled: %d", eq_enabled);
        }
        else if (sscanf(line, "bass_gain=%f", &bass_gain) == 1) {
            SDL_Log("Loaded bass_gain: %.2f", bass_gain);
        }
        else if (sscanf(line, "mid_gain=%f", &mid_gain) == 1) {
            SDL_Log("Loaded mid_gain: %.2f", mid_gain);
        }
        else if (sscanf(line, "treble_gain=%f", &treble_gain) == 1) {
            SDL_Log("Loaded treble_gain: %.2f", treble_gain);
        }
        else if (sscanf(line, "vis_type=%d", &vis_type) == 1) {
            SDL_Log("Loaded vis_type: %d", vis_type);
        }
        else if (sscanf(line, "vis_sensitivity=%f", &vis_sensitivity) == 1) {
            SDL_Log("Loaded vis_sensitivity: %.2f", vis_sensitivity);
        }
        else if (sscanf(line, "queue_group_mode=%d", &queue_group_mode) == 1) {
            SDL_Log("Loaded queue_group_mode: %d", queue_group_mode);
        }
        else if (sscanf(line, "queue_karaoke_only=%d", &queue_karaoke_only) == 1) {
            SDL_Log("Loaded queue_karaoke_only: %d", queue_karaoke_only);
        }
        else if (sscanf(line, "queue_sort_column=%d", &queue_sort_column) == 1) {
            SDL_Log("Loaded queue_sort_column: %d", queue_sort_column);
        }
        else if (sscanf(line, "queue_sort_type=%d", &queue_sort_type) == 1) {
            SDL_Log("Loaded queue_sort_type: %d", queue_sort_type);
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

    // Queue sort order. Set the column sort directly on the store first
    // (cheap - just a property, applies as rows get added later), then
    // drive the group dropdown and karaoke checkbox through their normal
    // setters so their existing "notify::selected"/"toggled" handlers do
    // the actual set_queue_group_mode()/display-rebuild work, the same as
    // if the user had just clicked them.
    if (player->queue_store && queue_sort_column >= 0) {
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(player->queue_store),
                                              queue_sort_column, (GtkSortType)queue_sort_type);
    }
    if (player->queue_group_dropdown &&
        queue_group_mode >= QUEUE_GROUP_NONE && queue_group_mode <= QUEUE_GROUP_GENRE) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(player->queue_group_dropdown), (guint)queue_group_mode);
    }
    if (player->queue_karaoke_filter_check) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(player->queue_karaoke_filter_check),
                                     queue_karaoke_only != 0);
    }
    
    SDL_Log("Settings loaded successfully");
    return true;
}
