// Zenamp (c) Jason Hall 2026 - Main GTK4 File

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
#include <SDL2/SDL_mixer.h>
#include <signal.h>

#include <sys/stat.h>

#ifndef _WIN32
#include <sys/types.h>
#else
#include <shlobj.h>
#include <gdk/win32/gdkwin32.h>
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
#include "karafun.h"
#include "kar.h"

// ============================================================================
// Directory Scanner - Integrated Music Import
// ============================================================================

#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <iterator>
#include <thread>
#include <atomic>
#include <dirent.h>

// Supported music file extensions
static const std::vector<std::string> SUPPORTED_FORMATS = {
    ".flac", ".alac", ".ape", ".wv", ".tta",
    ".mp3", ".aac", ".ogg", ".opus", ".m4a", ".wma",
    ".wav", ".aiff", ".aif",
    ".mid", ".midi", ".xm", ".mod", ".s3m", ".it",
    ".webm", ".mp2",
    ".kfn", ".kar",
    ".m3u", ".m3u8"
};

struct ScanProgressState {
    GtkWidget *dialog;
    GtkWidget *label;
    GtkWidget *counter_label;
    GtkWidget *progress_bar;
    GtkWidget *file_tree;
    GtkListStore *file_store;
    std::atomic<bool> cancel_requested{false};
    std::atomic<int> total_files{0};
};

static ScanProgressState *g_scan_state = nullptr;
static std::string g_to_lower(const std::string& str);
static std::string g_get_file_extension(const std::string& filepath);
static bool g_is_supported_music_file(const std::string& filepath);
static std::vector<std::string> g_scan_directory_recursive(const std::string& path);
static void g_scan_directory_impl(const std::string& directory_path, bool recursive, std::vector<std::string>& results);
static gboolean g_on_scan_progress_update(gpointer user_data);
static void g_scan_progress_callback(const std::string& current_file, int total_scanned);
static gboolean g_on_scan_progress_delete(GtkWindow *window, gpointer user_data);
static void g_on_scan_cancel_clicked(GtkButton *button, gpointer user_data);
static void g_create_and_show_scan_dialog(const std::string& directory, bool recursive);
static gpointer g_scan_thread_func(gpointer user_data);
static void g_on_import_directory_response(GtkDialog *dialog, gint response_id, gpointer user_data);
static void g_on_import_directory_clicked(GtkWidget *widget, gpointer user_data);

extern double playTime;
extern bool isPlaying;
extern bool paused;
extern int globalVolume;
extern void processEvents(void);
extern double playwait;

AudioPlayer *player = NULL;

// GTK4 removed the indexed gdk_display_get_monitor(display, n) accessor -
// monitors are exposed as a GListModel via gdk_display_get_monitors() now.
// This just grabs the first one as a stand-in "primary monitor".
static GdkMonitor *gtk4_get_primary_monitor(GdkDisplay *display) {
    if (!display) return NULL;
    GListModel *monitors = gdk_display_get_monitors(display);
    if (!monitors || g_list_model_get_n_items(monitors) == 0) return NULL;
    return GDK_MONITOR(g_list_model_get_item(monitors, 0));
}

// GTK4 removed gtk_main()/gtk_main_quit()/gtk_events_pending()/gtk_main_iteration()
// with no direct replacement on the GTK side - apps either wrap everything in
// a GtkApplication, or drive a plain GLib main loop themselves. This app does
// the latter, since it isn't structured around GtkApplication.
static GMainLoop *g_app_main_loop = NULL;

// ============================================================================
// Helper functions for standalone CDG file loading
// ============================================================================

// Thread function for non-blocking file access check with timeout
static gboolean file_access_timeout_callback(gpointer user_data) {
    // This is just a marker - the actual timeout is handled by the OS fopen() call
    return FALSE;
}

// GTK4 removed gtk_dialog_run() - dialogs are shown non-modally now and close
// themselves on response instead of blocking the caller. This replaces the
// gtk_dialog_run()+gtk_widget_destroy() pairs that used to show a message and
// wait for the user to click OK before continuing.
//
// g++ rejects G_CALLBACK(gtk_window_destroy) directly here ("overloaded
// function with no contextual type information") since the C-style cast
// can't resolve an unqualified function name to GCallback on its own in
// C++ - wrapping it in a plainly-typed function sidesteps that.
static void destroy_dialog_on_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void show_info_dialog(GtkWindow *parent, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                 "%s", message);
    g_signal_connect(dialog, "response", G_CALLBACK(destroy_dialog_on_response), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

// Check if file is accessible without blocking (with implicit OS timeout)
static bool is_file_accessible_with_timeout(const char *filepath) {
    if (!filepath) return false;
    
    // fopen() will timeout after OS default (usually 5-30 seconds on network issues)
    // For local files, this returns instantly
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        SDL_Log("File inaccessible: %s (errno: %d)", filepath, errno);
        return false;
    }
    fclose(f);
    return true;
}

// Get filename without extension
static void get_filename_without_ext(const char *filepath, char *out, size_t out_size) {
    const char *filename = strrchr(filepath, '/');
    if (!filename) filename = strrchr(filepath, '\\');
    if (!filename) filename = filepath;
    else filename++;  // Skip the separator
    
    strncpy(out, filename, out_size - 1);
    out[out_size - 1] = '\0';
    
    // Remove extension
    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
}

// Check if file exists
static bool file_exists(const char *filepath) {
    struct stat buffer;
    return (stat(filepath, &buffer) == 0);
}

// Try to load CDG file with same name as audio file
// Returns true if CDG was found and loaded, false otherwise
static bool try_load_standalone_cdg(AudioPlayer *player, const char *audio_filepath) {
    if (!player || !player->cdg_display || !audio_filepath) {
        SDL_Log("CDG load skipped: player=%p, cdg_display=%p, filepath=%p", 
               player, player ? player->cdg_display : NULL, audio_filepath);
        return false;
    }
    
    SDL_Log("Attempting to load standalone CDG for: %s", audio_filepath);
    
    // Get directory of audio file
    char dir_path[4096];
    strncpy(dir_path, audio_filepath, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    
    // Find last separator and truncate there
    char *last_sep = strrchr(dir_path, '/');
#ifdef _WIN32
    char *last_sep_win = strrchr(dir_path, '\\');
    if (!last_sep || (last_sep_win && last_sep_win > last_sep)) {
        last_sep = last_sep_win;
    }
#endif
    
    if (!last_sep) {
        // No directory separator, file is in current directory
        strcpy(dir_path, ".");
    } else {
        *last_sep = '\0';
    }
    
    SDL_Log("CDG search directory: %s", dir_path);
    
    // Get filename without extension
    char base_filename[256];
    get_filename_without_ext(audio_filepath, base_filename, sizeof(base_filename));
    
    SDL_Log("CDG base filename: %s", base_filename);
    
    // Construct CDG filepath
    char cdg_filepath[4096];
    snprintf(cdg_filepath, sizeof(cdg_filepath), "%s/%s.cdg", dir_path, base_filename);
    
    SDL_Log("CDG full path: %s", cdg_filepath);
    
    // Try to load it
    if (file_exists(cdg_filepath)) {
        SDL_Log("Found standalone CDG file: %s", cdg_filepath);
        if (cdg_load_file(player->cdg_display, cdg_filepath)) {
            SDL_Log("Successfully loaded CDG from: %s", cdg_filepath);
            return true;
        } else {
            SDL_Log("Failed to load CDG file: %s", cdg_filepath);
            return false;
        }
    } else {
        SDL_Log("CDG file not found: %s", cdg_filepath);
    }
    
    return false;
}

// ============================================================================
// Directory Scanner Implementation
// ============================================================================

static std::string g_to_lower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower;
}

static std::string g_get_file_extension(const std::string& filepath) {
    size_t dot_pos = filepath.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "";
    }
    return g_to_lower(filepath.substr(dot_pos));
}

static bool g_is_supported_music_file(const std::string& filepath) {
    std::string ext = g_get_file_extension(filepath);
    if (ext.empty()) return false;
    return std::find(SUPPORTED_FORMATS.begin(), SUPPORTED_FORMATS.end(), ext) != SUPPORTED_FORMATS.end();
}

static void g_scan_directory_impl(const std::string& directory_path, bool recursive, std::vector<std::string>& results) {
    DIR* dir = opendir(directory_path.c_str());
    if (!dir) {
        fprintf(stderr, "Failed to open directory: %s\n", directory_path.c_str());
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string full_path = directory_path + "/" + entry->d_name;
        struct stat st;

        if (stat(full_path.c_str(), &st) == -1) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (recursive) {
                g_scan_directory_impl(full_path, recursive, results);
            }
        } else if (S_ISREG(st.st_mode)) {
            if (g_is_supported_music_file(full_path)) {
                results.push_back(full_path);
                g_scan_progress_callback(full_path, results.size());
            }
        }
    }

    closedir(dir);
}

struct UpdateUIData {
    std::string current_file;
    int total_scanned;
};

static gboolean g_on_scan_progress_update(gpointer user_data) {
    UpdateUIData *data = static_cast<UpdateUIData *>(user_data);
    
    if (!g_scan_state || !g_scan_state->dialog) {
        delete data;
        return FALSE;
    }

    size_t last_slash = data->current_file.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos)
        ? data->current_file.substr(last_slash + 1)
        : data->current_file;

    gtk_label_set_text(GTK_LABEL(g_scan_state->label), filename.c_str());

    char counter_text[64];
    snprintf(counter_text, sizeof(counter_text), "Found: %d files", data->total_scanned);
    gtk_label_set_text(GTK_LABEL(g_scan_state->counter_label), counter_text);

    GtkTreeIter iter;
    gtk_list_store_append(g_scan_state->file_store, &iter);
    gtk_list_store_set(g_scan_state->file_store, &iter, 0, filename.c_str(), -1);

    g_scan_state->total_files = data->total_scanned;

    delete data;
    return FALSE;
}

static void g_scan_progress_callback(const std::string& current_file, int total_scanned) {
    if (!g_scan_state) return;

    if (g_scan_state->cancel_requested) {
        return;
    }

    UpdateUIData *data = new UpdateUIData{current_file, total_scanned};
    g_idle_add(g_on_scan_progress_update, data);
    g_usleep(1000);
}

struct ScanThreadData {
    std::string directory;
    bool recursive;
    std::vector<std::string> results;
};

static gpointer g_scan_thread_func(gpointer user_data) {
    ScanThreadData *data = static_cast<ScanThreadData *>(user_data);

    SDL_Log("Scan thread started for: %s", data->directory.c_str());

    g_scan_directory_impl(data->directory, data->recursive, data->results);

    SDL_Log("Scan thread complete: found %zu files", data->results.size());

    g_idle_add([](gpointer ud) -> gboolean {
        ScanThreadData *scan_data = static_cast<ScanThreadData *>(ud);

        if (g_scan_state && g_scan_state->dialog) {
            gtk_window_destroy(GTK_WINDOW(g_scan_state->dialog));
            g_scan_state->dialog = nullptr;
        }

        if (scan_data->results.size() > 0) {
            SDL_Log("Adding %zu files to queue (bulk import)...", scan_data->results.size());
            
            // Directly add to queue array without any UI updates or duplicate checks
            // This matches the pattern used in load_m3u_playlist
            for (size_t idx = 0; idx < scan_data->results.size(); idx++) {
                const auto& file = scan_data->results[idx];
                
                if (player->queue.count >= player->queue.capacity) {
                    // Resize queue if needed
                    // If capacity is 0, start with 256; otherwise double it
                    int new_capacity = (player->queue.capacity == 0) ? 256 : (player->queue.capacity * 2);
                    SDL_Log("  Resizing queue from %d to %d", player->queue.capacity, new_capacity);
                    
                    char **new_files = (player->queue.files == NULL) 
                        ? (char**)malloc(new_capacity * sizeof(char*))
                        : (char**)realloc(player->queue.files, new_capacity * sizeof(char*));
                    
                    if (!new_files) {
                        SDL_Log("ERROR: Failed to resize queue");
                        break;
                    }
                    player->queue.files = new_files;
                    player->queue.capacity = new_capacity;
                }
                
                // Directly allocate and add file path
                player->queue.files[player->queue.count] = (char*)malloc(file.length() + 1);
                if (player->queue.files[player->queue.count]) {
                    strcpy(player->queue.files[player->queue.count], file.c_str());
                    player->queue.count++;
                    
                    // Print progress every 1000 files
                    if ((idx + 1) % 1000 == 0) {
                        SDL_Log("  Added %zu / %zu files...", idx + 1, scan_data->results.size());
                    }
                }
            }

            SDL_Log("Finished adding files to queue. Total: %d", player->queue.count);
            SDL_Log("Starting display render...");
            // For bulk imports, skip metadata extraction and just show filenames.
            // That fast path only knows about the flat GtkListStore; if the
            // grouped-by-artist view is active, fall back to the normal
            // (slower, but grouping-aware) renderer instead.
            if (player->queue_grouped_view) {
                update_queue_display_with_filter(player, false);
            } else if (player->queue_store) {
                SDL_Log("Clearing queue store...");
                gtk_list_store_clear(player->queue_store);
                
                SDL_Log("Rendering %d items to display...", player->queue.count);
                for (int i = 0; i < player->queue.count; i++) {
                    const char *filepath = player->queue.files[i];
                    char *basename = g_path_get_basename(filepath);
                    const char *ext = strrchr(filepath, '.');
                    
                    GtkTreeIter iter;
                    gtk_list_store_append(player->queue_store, &iter);
                    
                    const char *indicator = (i == player->queue.current_index) ? "▶" : "";
                    const char *cdgk_indicator = (ext && (strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".kfn") == 0)) ? "✓" : "";
                    
                    gtk_list_store_set(player->queue_store, &iter,
                        COL_FILEPATH, filepath,
                        COL_PLAYING, indicator,
                        COL_FILENAME, basename,
                        COL_TITLE, "(queued)",
                        COL_ARTIST, "",
                        COL_ALBUM, "",
                        COL_GENRE, "",
                        COL_DURATION, "",
                        COL_CDGK, cdgk_indicator,
                        COL_QUEUE_INDEX, i,
                        -1);
                    
                    g_free(basename);
                    
                    // Keep UI responsive every 1000 items
                    if ((i + 1) % 1000 == 0) {
                        SDL_Log("  Rendered %d items...", i + 1);
                        // GTK4 removed gtk_events_pending()/gtk_main_iteration();
                        // pump the default GLib main context directly instead.
                        while (g_main_context_pending(NULL)) {
                            g_main_context_iteration(NULL, FALSE);
                        }
                    }
                }
                SDL_Log("Display rendering complete.");
            }
            
            update_gui_state(player);
            SDL_Log("GUI state updated.");

            char msg[256];
            snprintf(msg, sizeof(msg), "Successfully imported %d music files", player->queue.count);
            SDL_Log("Showing completion dialog: %s", msg);
            
            GtkWidget *completion_dialog = gtk_message_dialog_new(
                GTK_WINDOW(player->window),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                "%s", msg
            );
            g_signal_connect(completion_dialog, "response", G_CALLBACK(destroy_dialog_on_response), NULL);
            gtk_window_present(GTK_WINDOW(completion_dialog));
            SDL_Log("Completion dialog shown.");
            
            // Start deduplication in background thread
            SDL_Log("Starting deduplication thread...");
            std::thread dedup_thread([](AudioPlayer *p) {
                if (!p || !p->queue.files || p->queue.count <= 1) {
                    SDL_Log("Deduplication skipped: queue too small or NULL");
                    return;
                }
                
                SDL_Log("Deduplication thread started. Checking %d files for duplicates...", p->queue.count);
                
                // Lock the queue for the entire operation
                pthread_mutex_lock(&p->audio_mutex);
                
                // Use a simple hash set to track seen basenames
                GHashTable *seen_basenames = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
                std::vector<int> duplicates_to_remove;
                
                for (int i = 0; i < p->queue.count; i++) {
                    const char *filepath = p->queue.files[i];
                    if (!filepath) continue;
                    
                    char *basename = g_path_get_basename(filepath);
                    
                    if (g_hash_table_contains(seen_basenames, basename)) {
                        SDL_Log("  Found duplicate: %s", basename);
                        duplicates_to_remove.push_back(i);
                    } else {
                        g_hash_table_insert(seen_basenames, g_strdup(basename), GINT_TO_POINTER(1));
                    }
                    
                    g_free(basename);
                    
                    // Progress update every 1000 files
                    if ((i + 1) % 1000 == 0) {
                        SDL_Log("  Checked %d / %d files...", i + 1, p->queue.count);
                    }
                }
                
                g_hash_table_destroy(seen_basenames);
                
                // Remove duplicates in reverse order (to maintain indices)
                int removed = 0;
                for (int i = (int)duplicates_to_remove.size() - 1; i >= 0; i--) {
                    int index = duplicates_to_remove[i];
                    if (index >= 0 && index < p->queue.count && p->queue.files[index]) {
                        free(p->queue.files[index]);
                        
                        // Shift remaining files down
                        for (int j = index; j < p->queue.count - 1; j++) {
                            p->queue.files[j] = p->queue.files[j + 1];
                        }
                        p->queue.count--;
                        removed++;
                    }
                }
                
                SDL_Log("Deduplication complete: removed %d duplicates. Queue now has %d files.", removed, p->queue.count);
                
                // Unlock the queue
                pthread_mutex_unlock(&p->audio_mutex);
                
                // Quick UI update on main thread (just refresh counts/state)
                g_idle_add([](gpointer data) -> gboolean {
                    AudioPlayer *p = (AudioPlayer*)data;
                    if (p) {
                        SDL_Log("Quick UI update after deduplication...");
                        update_gui_state(p);  // Quick update only
                    }
                    return FALSE;
                }, p);
                
                // Post display update to main thread (atomic operation, prevents conflicts)
                std::thread([](AudioPlayer *p) {
                    SDL_Log("Starting background display update thread...");
                    g_usleep(200000);  // 200ms delay to let UI update first
                    
                    SDL_Log("Posting queue display update (%d files)...", p->queue.count);
                    
                    // Post as atomic g_idle_add to main thread
                    g_idle_add([](gpointer data) -> gboolean {
                        AudioPlayer *p = (AudioPlayer*)data;
                        SDL_Log("Updating queue display on main thread");
                        update_queue_display_with_filter(p);
                        SDL_Log("Display update complete");
                        return FALSE;
                    }, p);
                }, p).detach();
            }, player);
            dedup_thread.detach();
        } else {
            show_info_dialog(GTK_WINDOW(player->window), "No music files found in directory");
        }

        delete scan_data;
        if (g_scan_state) {
            delete g_scan_state;
            g_scan_state = nullptr;
        }

        return FALSE;
    }, data);

    return nullptr;
}

// GTK4's "close-request" replaces "delete-event": it hands back the window
// itself (not a GdkEvent), and TRUE still means "block the close".
static gboolean g_on_scan_progress_delete(GtkWindow *window, gpointer user_data) {
    if (g_scan_state) {
        g_scan_state->cancel_requested = true;
    }
    return TRUE;
}

static void g_on_scan_cancel_clicked(GtkButton *button, gpointer user_data) {
    if (g_scan_state) {
        g_scan_state->cancel_requested = true;
    }
}

static void g_create_and_show_scan_dialog(const std::string& directory, bool recursive) {
    g_scan_state = new ScanProgressState();

    g_scan_state->dialog = gtk_dialog_new_with_buttons(
        "Scanning Directory...",
        GTK_WINDOW(player->window),
        GTK_DIALOG_MODAL,
        "_Cancel",
        GTK_RESPONSE_CANCEL,
        nullptr
    );

    gtk_window_set_default_size(GTK_WINDOW(g_scan_state->dialog), 500, 400);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(g_scan_state->dialog));
    gtk_box_set_spacing(GTK_BOX(content), 10);
    // GTK4 removed GtkContainer (and gtk_container_set_border_width with it) -
    // widgets get their own margins now.
    gtk_widget_set_margin_start(content, 15);
    gtk_widget_set_margin_end(content, 15);
    gtk_widget_set_margin_top(content, 15);
    gtk_widget_set_margin_bottom(content, 15);

    g_scan_state->label = gtk_label_new("Initializing scan...");
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(g_scan_state->label), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_append(GTK_BOX(content), g_scan_state->label);

    g_scan_state->counter_label = gtk_label_new("Found: 0 files");
    gtk_box_append(GTK_BOX(content), g_scan_state->counter_label);

    g_scan_state->progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(g_scan_state->progress_bar));
    gtk_box_append(GTK_BOX(content), g_scan_state->progress_bar);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);

    g_scan_state->file_store = gtk_list_store_new(1, G_TYPE_STRING);
    g_scan_state->file_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(g_scan_state->file_store));

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "Files Found",
        renderer,
        "text", 0,
        nullptr
    );
    gtk_tree_view_append_column(GTK_TREE_VIEW(g_scan_state->file_tree), column);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), g_scan_state->file_tree);
    gtk_box_append(GTK_BOX(content), scrolled);

    // GTK4 widgets are visible by default - no gtk_widget_show_all() needed.

    // "delete-event" is gone in GTK4; "close-request" is its replacement
    // (return TRUE from the handler to block the close, matching the old
    // gint-returning convention).
    g_signal_connect(g_scan_state->dialog, "close-request",
                     G_CALLBACK(g_on_scan_progress_delete), nullptr);
    g_signal_connect(g_scan_state->dialog, "response",
                     G_CALLBACK(g_on_scan_cancel_clicked), nullptr);

    gtk_window_present(GTK_WINDOW(g_scan_state->dialog));

    ScanThreadData *scan_data = new ScanThreadData{directory, recursive, {}};
    std::thread scan_thread(g_scan_thread_func, scan_data);
    scan_thread.detach();

    g_timeout_add(100, [](gpointer data) -> gboolean {
        if (g_scan_state && g_scan_state->dialog && GTK_IS_WIDGET(g_scan_state->dialog) &&
            g_scan_state->progress_bar) {
            // Progress bar is tracked directly on the scan state now, so
            // there's no need to walk the content area's children (GtkContainer
            // and gtk_container_get_children are both gone in GTK4).
            gtk_progress_bar_pulse(GTK_PROGRESS_BAR(g_scan_state->progress_bar));
            return TRUE;
        }
        return FALSE;
    }, nullptr);
}

static void g_on_import_directory_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        // GTK4 removed gtk_file_chooser_get_filename() entirely - file
        // choosers work in terms of GFile now.
        GFile *folder = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        if (folder) {
            char *folder_path = g_file_get_path(folder);
            if (folder_path) {
                SDL_Log("Starting import from: %s", folder_path);
                g_create_and_show_scan_dialog(folder_path, true);
                g_free(folder_path);
            }
            g_object_unref(folder);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void g_on_import_directory_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Import Music Directory",
        GTK_WINDOW(player->window),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Import", GTK_RESPONSE_ACCEPT,
        nullptr
    );

    // gtk_file_chooser_set_current_folder() now takes a GFile* instead of a path string.
    GFile *home = g_file_new_for_path(g_get_home_dir());
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), home, nullptr);
    g_object_unref(home);

    g_signal_connect(dialog, "response", G_CALLBACK(g_on_import_directory_response), nullptr);
    gtk_window_present(GTK_WINDOW(dialog));
}

// ============================================================================
// End Directory Scanner
// ============================================================================

static guint queue_update_timeout_id = 0;
static int last_queue_index = -1;  // Track last updated index to detect changes

// Minimal fast update - only updates the "▶" playing indicator without
// metadata extraction. Walks whichever model (flat or artist-grouped) the
// tree view currently has, via gtk_tree_model_foreach so it correctly
// descends into grouped children too, not just top-level rows.
struct QueueIndicatorCtx {
    AudioPlayer *player;
};

static gboolean update_queue_indicator_row(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
    (void)path;
    auto *ctx = (QueueIndicatorCtx *)data;

    int queue_index = -1;
    gtk_tree_model_get(model, iter, COL_QUEUE_INDEX, &queue_index, -1);
    const char *indicator = (queue_index == ctx->player->queue.current_index) ? "▶" : "";

    if (ctx->player->queue_grouped_view) {
        gtk_tree_store_set(GTK_TREE_STORE(model), iter, COL_PLAYING, indicator, -1);
    } else {
        gtk_list_store_set(GTK_LIST_STORE(model), iter, COL_PLAYING, indicator, -1);
    }
    return FALSE;  // keep going
}

void update_queue_display_minimal(AudioPlayer *player) {
    if (!player || !player->queue_tree_view) return;

    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(player->queue_tree_view));
    if (!model) return;

    QueueIndicatorCtx ctx{player};
    gtk_tree_model_foreach(model, update_queue_indicator_row, &ctx);
}

static gboolean queue_update_debounce_callback(gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    queue_update_timeout_id = 0;
    // Do a full update after debounce
    update_queue_display_with_filter(player);
    return FALSE;
}

// Debounced version - only does full update after a delay
static void update_queue_display_debounced(AudioPlayer *player) {
    if (!player) return;
    
    // Always do a minimal update immediately
    update_queue_display_minimal(player);
    
    // Schedule full update after 500ms debounce
    if (queue_update_timeout_id) {
        g_source_remove(queue_update_timeout_id);
    }
    queue_update_timeout_id = g_timeout_add(500, queue_update_debounce_callback, player);
}

// Remembers whatever visualization type was active before we auto-switched

// Remembers whatever visualization type was active before we auto-switched
// into karaoke mode (for a .kfn, CDG zip, or LRC-generated karaoke load),
// so a later non-karaoke file can restore it instead of getting stuck on
// VIS_KARAOKE forever.
static VisualizationType s_pre_karaoke_vis_type = VIS_WAVEFORM;
static bool s_vis_in_karaoke_mode = false;

static void enter_karaoke_visualization(AudioPlayer *p) {
    if (!p || !p->visualizer) return;
    if (!s_vis_in_karaoke_mode) {
        s_pre_karaoke_vis_type = p->visualizer->type;
    }
    s_vis_in_karaoke_mode = true;
    visualizer_set_type(p->visualizer, VIS_KARAOKE);
}

static void leave_karaoke_visualization(AudioPlayer *p) {
    if (!p || !p->visualizer) return;
    if (s_vis_in_karaoke_mode) {
        visualizer_set_type(p->visualizer, s_pre_karaoke_vis_type);
        s_vis_in_karaoke_mode = false;
    }
}

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        SDL_Log("\nReceived signal %d, initiating graceful shutdown...", sig);
        
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
            
            SDL_Log("Cleaning up Audio");
            if (player->audio_buffer.data) free(player->audio_buffer.data);

            if (player->cdg_display) {
                cdg_display_free(player->cdg_display);
            }    

            SDL_Log("Closing SDL audio device");
            if (player->audio_device) SDL_CloseAudioDevice(player->audio_device);

            SDL_Log("Cleaning Equalizer");
            if (player->equalizer) {
                equalizer_free(player->equalizer);
            }
            
            SDL_Log("Destroying audio mutex");
            pthread_mutex_destroy(&player->audio_mutex);

            SDL_Log("Freeing player");
            g_free(player);
            player = NULL;
        }

        SDL_Log("Closing SDL");
        SDL_Quit();
        
        SDL_Log("Exiting application");
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

// Auto-hide the mouse cursor in fullscreen visualization after a period of inactivity
static guint vis_cursor_hide_timeout_id = 0;
static GdkCursor *vis_blank_cursor = NULL;
#define VIS_CURSOR_HIDE_DELAY_MS 3000

static gboolean hide_vis_cursor(gpointer user_data) {
    if (vis_fullscreen_window) {
        // GTK4 sets cursors on the GtkWidget directly (gtk_widget_set_cursor)
        // rather than on a GdkWindow, and there's no more GDK_BLANK_CURSOR enum -
        // an invisible cursor is just the named cursor "none".
        if (!vis_blank_cursor) {
            vis_blank_cursor = gdk_cursor_new_from_name("none", NULL);
        }
        gtk_widget_set_cursor(vis_fullscreen_window, vis_blank_cursor);
    }
    vis_cursor_hide_timeout_id = 0;
    return FALSE; // one-shot
}

static void reset_vis_cursor_timer() {
    if (vis_cursor_hide_timeout_id) {
        g_source_remove(vis_cursor_hide_timeout_id);
        vis_cursor_hide_timeout_id = 0;
    }
    if (vis_fullscreen_window) {
        gtk_widget_set_cursor(vis_fullscreen_window, NULL); // restore default visible cursor
    }
    vis_cursor_hide_timeout_id = g_timeout_add(VIS_CURSOR_HIDE_DELAY_MS, hide_vis_cursor, NULL);
}

// GTK4 replaces the "motion-notify-event" signal with GtkEventControllerMotion,
// whose "motion" handler receives the pointer coordinates directly instead of a
// GdkEventMotion.
static void on_vis_fullscreen_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
    reset_vis_cursor_timer();
}

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
    ofn.lpstrFilter = "All Supported\0*.mid;*.midi;*.wav;*.mp3;*.m4a;*.aiff;*.aif;*.ogg;*.flac;*.opus;*.wma;*.lrc;*.kfn;*.zip\0"
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
                      "Karafun Files\0*.kfn\0"
                      "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (multiple) {
        ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
        SDL_Log("Opening Windows file dialog for multiple selection");
    } else {
        SDL_Log("Opening Windows file dialog for single selection");
    }
    
    BOOL result = GetOpenFileName(&ofn);
    
    if (result) {
        SDL_Log("File dialog returned successfully");
        return true;
    } else {
        DWORD error = CommDlgExtendedError();
        if (error != 0) {
            SDL_Log("File dialog error: %lu", error);
        } else {
            SDL_Log("File dialog cancelled by user");
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

void on_remove_from_queue_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;
    
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "queue_index"));
    AudioPlayer *player = (AudioPlayer*)g_object_get_data(G_OBJECT(button), "player");
    
    SDL_Log("Removing item %d from queue", index);
    
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
                SDL_Log("Failed to load next track after removal");
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
            SDL_Log("Trying SDL audio driver: %s", drivers[i]);
            if (SDL_Init(SDL_INIT_AUDIO) == 0) {
                SDL_Log("Successfully initialized with driver: %s", drivers[i]);
                break;
            } else {
                SDL_Log("Failed with driver %s: %s", drivers[i], SDL_GetError());
                SDL_Quit();
            }
        }
    }
#else
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        SDL_Log("SDL initialization failed: %s", SDL_GetError());
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
        SDL_Log("Audio device open failed: %s", SDL_GetError());
        return false;
    }
    
    SDL_Log("Audio: %d Hz, %d channels", player->audio_spec.freq, player->audio_spec.channels);
    
    // Reinitialize equalizer with new sample rate if it exists
    if (player->equalizer && player->equalizer->sample_rate != sample_rate) {
        SDL_Log("Reinitializing equalizer for new sample rate: %d Hz", sample_rate);
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
        
        SDL_Log("Loaded from cache: %zu samples", cached->length);
        return true;
    }
    
    // Not in cache, load from file
    FILE* wav_file = fopen(wav_path, "rb");
    if (!wav_file) {
        SDL_Log("Cannot open WAV file: %s", wav_path);
        return false;
    }
    
    // Read WAV header
    char header[44];
    if (fread(header, 1, 44, wav_file) != 44) {
        SDL_Log("Cannot read WAV header");
        fclose(wav_file);
        return false;
    }
    
    // Verify WAV format
    if (strncmp(header, "RIFF", 4) != 0 || strncmp(header + 8, "WAVE", 4) != 0) {
        SDL_Log("Invalid WAV format");
        fclose(wav_file);
        return false;
    }
    
    // Extract WAV info
    player->sample_rate = *(int*)(header + 24);
    player->channels = *(short*)(header + 22);
    player->bits_per_sample = *(short*)(header + 34);
    
    SDL_Log("WAV: %d Hz, %d channels, %d bits", player->sample_rate, player->channels, player->bits_per_sample);
    
    // Reinitialize audio with the correct sample rate and channels
    if (!init_audio(player, player->sample_rate, player->channels)) {
        SDL_Log("Failed to reinitialize audio for WAV format");
        fclose(wav_file);
        return false;
    }
    
    // Get file size and calculate duration
    fseek(wav_file, 0, SEEK_END);
    long file_size = ftell(wav_file);
    long data_size = file_size - 44;
    
    player->song_duration = data_size / (double)(player->sample_rate * player->channels * (player->bits_per_sample / 8));
    SDL_Log("WAV duration: %.2f seconds", player->song_duration);
    
    // Allocate and read audio data
    int16_t* wav_data = (int16_t*)malloc(data_size);
    if (!wav_data) {
        SDL_Log("Memory allocation failed");
        fclose(wav_file);
        return false;
    }
    
    fseek(wav_file, 44, SEEK_SET);
    if (fread(wav_data, 1, data_size, wav_file) != (size_t)data_size) {
        SDL_Log("WAV data read failed");
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
    
    SDL_Log("Loaded %zu samples", player->audio_buffer.length);
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
    
    SDL_Log("Speed changed to: %.2fx", speed);
}


bool load_file(AudioPlayer *player, const char *filename) {
    SDL_Log("load_file called for: %s", filename);
    
    // Store original filename for CDG lookup (before any recursive calls that change it)
    static char original_filename[2048];
    static bool has_original = false;
    
    // Only set original filename on first call (not on recursive calls)
    if (strncmp(filename, "virtual_", 8) != 0 && !has_original) {
        strncpy(original_filename, filename, sizeof(original_filename) - 1);
        original_filename[sizeof(original_filename) - 1] = '\0';
        has_original = true;
        SDL_Log("Stored original filename for CDG lookup: %s", original_filename);
    }
    
    // Stop current playback and clean up timer
    if (player->is_playing || player->update_timer_id > 0) {
        SDL_Log("Stopping current playback...");
        pthread_mutex_lock(&player->audio_mutex);
        player->is_playing = false;
        player->is_paused = false;
        SDL_PauseAudioDevice(player->audio_device, 1);
        pthread_mutex_unlock(&player->audio_mutex);
        
        if (player->update_timer_id > 0) {
            g_source_remove(player->update_timer_id);
            player->update_timer_id = 0;
            SDL_Log("Removed existing timer");
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
        SDL_Log("Loading virtual WAV file: %s", filename);
        return load_virtual_wav_file(player, filename);
    }
    
    // Determine file type for regular files
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        SDL_Log("Unknown file type");
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
    
    // Check for Karafun (.kfn) files FIRST
    if (strcmp(ext_lower, ".kfn") == 0) {
        SDL_Log("Loading Karafun file: %s", filename);
        if (karafun_load(filename)) {
            const char *mixed_path = karafun_get_mixed_path();
            if (mixed_path) {
                SDL_Log("Karafun loaded, playing mixed vocal+backing track");
                success = load_file(player, mixed_path);
                if (success && player->visualizer) {
                    enter_karaoke_visualization(player);
                }
            }
        }
        return success;
    }

    // Check for KAR (Karaoke MIDI) files - Extract lyrics and convert audio
    if (strcmp(ext_lower, ".kar") == 0) {
        SDL_Log("Loading KAR file: %s", filename);
        if (kar_load(filename)) {
            KarafunState* kar_state = kar_get_state();
            const char *mixed_path = karafun_get_mixed_path();
            if (mixed_path && kar_state) {
                SDL_Log("KAR loaded with %d words in %d lines, playing audio", 
                       kar_state->word_count, kar_state->line_count);
                success = load_file(player, mixed_path);
                if (success && player->visualizer) {
                    enter_karaoke_visualization(player);
                }
            }
        }
        return success;
    }

    // Any non-.kfn/.kar load means we're leaving karaoke-lyrics mode. Clear stale
    // karafun state (words/sync/lines, active flag, temp files) so the lyric
    // overlay stops rendering — but not when this call IS the recursive
    // load of karafun's own mixed vocal+backing WAV right after karafun_load() or kar_load().
    {
        const char *karafun_mixed = karafun_get_mixed_path();
        if (!(karafun_mixed && strcmp(filename, karafun_mixed) == 0)) {
            karafun_stop();
            leave_karaoke_visualization(player);
        }
    }

    if (strcmp(ext_lower, ".wav") == 0) {
        SDL_Log("Loading WAV file: %s", filename);
        success = load_wav_file(player, filename);
    } else if (strcmp(ext_lower, ".mid") == 0 || strcmp(ext_lower, ".midi") == 0) {
        SDL_Log("Loading MIDI file: %s", filename);
        if (convert_midi_to_wav(player, filename)) {
            SDL_Log("Now loading converted virtual WAV file: %s", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".mp3") == 0) {
        SDL_Log("Loading MP3 file: %s", filename);
        if (convert_mp3_to_wav(player, filename)) {
            SDL_Log("Now loading converted virtual WAV file: %s", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".ogg") == 0) {
        SDL_Log("Loading OGG file: %s", filename);
        if (convert_ogg_to_wav(player, filename)) {
            SDL_Log("Now loading converted virtual WAV file: %s", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".flac") == 0) {
        SDL_Log("Loading FLAC file: %s", filename);
        if (convert_flac_to_wav(player, filename)) {
            SDL_Log("Now loading converted virtual WAV file: %s", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".aif") == 0 || strcmp(ext_lower, ".aiff") == 0) {
        SDL_Log("Loading AIFF file: %s", filename);
        if (convert_aiff_to_wav(player, filename)) {
            SDL_Log("Now loading converted virtual WAV file: %s", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".opus") == 0) {
        SDL_Log("Loading Opus file: %s", filename);
        if (convert_opus_to_wav(player, filename)) {
            SDL_Log("Now loading converted virtual WAV file: %s", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".m4a") == 0) {
        SDL_Log("Loading M4A file: %s", filename);
        if (convert_m4a_to_wav(player, filename)) {
            SDL_Log("Now loading converted virtual WAV file: %s", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".wma") == 0) {
        SDL_Log("Loading WMA file: %s", filename);
        if (convert_wma_to_wav(player, filename)) {
            SDL_Log("Now loading converted virtual WAV file: %s", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        }
    } else if (strcmp(ext_lower, ".lrc") == 0) {
        SDL_Log("Generating karaoke ZIP from LRC: %s", filename);
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
                        enter_karaoke_visualization(player);
                    }

                    success = load_file(player, zip_contents.audio_file);
                    player->is_loading_cdg_from_zip = false;

                    if (success) {
                        SDL_Log("Loaded karaoke ZIP successfully");
                        strncpy(player->current_file, zip_path.c_str(), 1023);
                        player->current_file[1023] = '\0';

                        char *metadata = extract_metadata(zip_contents.audio_file);
                        gtk_label_set_markup(GTK_LABEL(player->metadata_label), metadata);
                        g_free(metadata);
                    } else {
                        SDL_Log("Failed to load audio from generated ZIP");
                        cleanup_karaoke_temp_files(&player->karaoke_temp_files);
                    }
                } else {
                    SDL_Log("Failed to load CDG from generated ZIP");
                    cleanup_karaoke_temp_files(&zip_contents);
                }
            } else {
                SDL_Log("Failed to extract generated karaoke ZIP");
            }
        } else {
            SDL_Log("Failed to generate karaoke ZIP from LRC");
        }
    } else if (strcmp(ext_lower, ".zip") == 0) {
        SDL_Log("Loading karaoke ZIP file: %s", filename);
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
                    enter_karaoke_visualization(player);
                }
            
                success = load_file(player, zip_contents.audio_file);
                
                player->is_loading_cdg_from_zip = false;
            
                if (success) {
                    SDL_Log("Loaded karaoke ZIP successfully");
                    strncpy(player->current_file, filename, 1023);
                    player->current_file[1023] = '\0';
                    
                    // Extract metadata from the actual audio file inside the ZIP
                    char *metadata = extract_metadata(zip_contents.audio_file);
                    gtk_label_set_markup(GTK_LABEL(player->metadata_label), metadata);
                    g_free(metadata);
                } else {
                    SDL_Log("Failed to load audio from ZIP");
                    cleanup_karaoke_temp_files(&player->karaoke_temp_files);
                }
            } else {
                SDL_Log("Failed to load CDG from ZIP");
                cleanup_karaoke_temp_files(&zip_contents);
            }
        } else {
            SDL_Log("Failed to extract karaoke ZIP");
        }
    } else {
        SDL_Log("Trying to load unknown file: %s", filename);
        if (convert_audio_to_wav(player, filename)) {
            SDL_Log("Now loading converted virtual WAV file: %s", player->temp_wav_file);
            success = load_virtual_wav_file(player, player->temp_wav_file);
        } else {
            SDL_Log("File isn't supported");
        }
    }
    
    if (success && !is_zip_file) {
        strncpy(player->current_file, filename, 1023);
        player->current_file[1023] = '\0';
        player->is_loaded = true;
        player->is_playing = false;
        player->is_paused = false;
        playTime = 0;

        // Try to load standalone CDG file with ORIGINAL audio filename (not converted virtual WAV)
        if (!player->cdg_display) {
            player->cdg_display = cdg_display_new();
        }
        
        // Use original filename if available (for converted files like MP3, M4A, etc)
        const char *cdg_lookup_name = original_filename;
        if (!has_original || original_filename[0] == '\0') {
            cdg_lookup_name = filename;
        }
        
        if (player->cdg_display && try_load_standalone_cdg(player, cdg_lookup_name)) {
            player->has_cdg = true;
            if (player->visualizer) {
                player->visualizer->cdg_display = player->cdg_display;
                enter_karaoke_visualization(player);
            }
            SDL_Log("Loaded CDG for: %s", cdg_lookup_name);
        } else if (player->has_cdg && player->visualizer) {
            enter_karaoke_visualization(player);
        }
        
        // Clear the original filename flag for next file
        has_original = false;
        original_filename[0] = '\0';
        
        // Extract and display metadata (for non-ZIP files)
        char *metadata = extract_metadata(filename);
        gtk_label_set_markup(GTK_LABEL(player->metadata_label), metadata);
        g_free(metadata);
        
        gtk_range_set_range(GTK_RANGE(player->progress_scale), 0.0, player->song_duration);
        gtk_range_set_value(GTK_RANGE(player->progress_scale), 0.0);
        
        if (player->audio_buffer.length == 0 || player->song_duration <= 0.1) {
            SDL_Log("Warning: File loaded but has no/minimal audio data (duration: %.2f, samples: %zu)", 
                   player->song_duration, player->audio_buffer.length);
            SDL_Log("Skipping this file and advancing to next...");
            
            if (strncmp(player->temp_wav_file, "virtual_", 8) == 0) {
                delete_virtual_file(player->temp_wav_file);
            }
            
            update_gui_state(player);
            
            if (player->queue.count > 1) {
                g_timeout_add(100, [](gpointer data) -> gboolean {
                    AudioPlayer *p = (AudioPlayer*)data;
                    SDL_Log("Auto-advancing from invalid file...");
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
        
        SDL_Log("File successfully loaded (duration: %.2f, samples: %zu), auto-starting playback", 
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
            SDL_Log("Warning: File loaded but has no/minimal audio data (duration: %.2f, samples: %zu)", 
                   player->song_duration, player->audio_buffer.length);
            SDL_Log("Skipping this file and advancing to next...");
            
            if (strncmp(player->temp_wav_file, "virtual_", 8) == 0) {
                delete_virtual_file(player->temp_wav_file);
            }
            
            update_gui_state(player);
            
            if (player->queue.count > 1) {
                g_timeout_add(100, [](gpointer data) -> gboolean {
                    AudioPlayer *p = (AudioPlayer*)data;
                    SDL_Log("Auto-advancing from invalid file...");
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
        
        SDL_Log("File successfully loaded (duration: %.2f, samples: %zu), auto-starting playback", 
               player->song_duration, player->audio_buffer.length);
        
        start_playback(player);
        update_gui_state(player);
    } else {
        SDL_Log("Failed to load file: %s", filename);
    }
    
    return success;
}

bool load_file_from_queue(AudioPlayer *player) {
    const char *filename = get_current_queue_file(&player->queue);
    if (!filename) return false;
    
    // Guard against infinite loops: track starting position to detect when we've cycled through all files
    static int attempted_start_index = -1;
    static int attempted_count = 0;
    
    // Initialize on first call
    if (attempted_start_index == -1) {
        attempted_start_index = player->queue.current_index;
        attempted_count = 0;
    }
    
    // Try to access file with timeout (OS will timeout on network drives)
    SDL_Log("Checking file accessibility: %s", filename);
    if (!is_file_accessible_with_timeout(filename)) {
        SDL_Log("File not accessible (skipping): %s", filename);
        if (player->visualizer) {
            snprintf(player->visualizer->error_message, sizeof(player->visualizer->error_message),
                     "Skipped (not accessible): %s", filename);
            player->visualizer->showing_error = true;
            player->visualizer->error_display_time = 0.5;  // Brief message
        }
        
        // Skip to next file
        if (advance_queue(&player->queue)) {
            attempted_count++;
            
            // Prevent infinite loop: if we've cycled back to start after checking all files, stop
            if (attempted_count >= player->queue.count) {
                SDL_Log("All files in queue are inaccessible");
                attempted_start_index = -1;
                attempted_count = 0;
                return false;
            }
            
            SDL_Log("Trying next file in queue (attempt %d/%d)...", attempted_count, player->queue.count);
            return load_file_from_queue(player);  // Recursively try next file
        }
        
        SDL_Log("No more files in queue to try");
        attempted_start_index = -1;
        attempted_count = 0;
        return false;
    }
    
    // Reset static variables on successful accessibility check
    attempted_start_index = -1;
    attempted_count = 0;
    
    SDL_Log("File accessible, attempting load: %s", filename);
    if (!load_file(player, filename)) {
        // File was accessible but failed to load (corrupted, unsupported format, etc)
        if (player->visualizer) {
            snprintf(player->visualizer->error_message, sizeof(player->visualizer->error_message),
                     "Failed to load: %s", filename);
            player->visualizer->showing_error = true;
            player->visualizer->error_display_time = 1.0;
        }
        
        SDL_Log("Failed to load: %s", filename);
        
        // Try next file
        if (advance_queue(&player->queue)) {
            SDL_Log("Trying next file after load failure...");
            return load_file_from_queue(player);
        }
        
        SDL_Log("No more files in queue");
        return false;
    }
    
    SDL_Log("Successfully loaded: %s", filename);
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
        SDL_Log("Cannot start playback - no audio data loaded");
        return;
    }
    
    SDL_Log("Starting WAV playback");
    
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
                    SDL_Log("Song finished - reached end of buffer (pos: %zu, len: %zu)", 
                           p->audio_buffer.position, p->audio_buffer.length);
                }
                // Also check if is_playing was set to false by audio callback
                else if (!currently_playing && p->audio_buffer.position > 0) {
                    song_finished = true;
                    SDL_Log("Song finished - detected by audio callback");
                }
            }
            
            // Update playback position if playing
            if (currently_playing && p->audio_buffer.data && p->sample_rate > 0 && p->channels > 0) {
                double samples_per_second = (double)(p->sample_rate * p->channels);
                playTime = (double)p->audio_buffer.position / samples_per_second;
                
                // Update KAR lyrics synchronization
                kar_update(playTime);
            }
            
            pthread_mutex_unlock(&p->audio_mutex);
            
            // Handle song completion
            if (song_finished && p->queue.count > 0) {
                SDL_Log("Song completed. Calling next_song()...");
                
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

// Toggles mute on the currently-playing Karafun (.kfn) file's vocal or
// backing track. Karafun plays back a single pre-mixed WAV (see
// karafun_prepare_mixed_track()), so there's no live per-channel volume to
// flip — instead this re-derives that mixed WAV honoring the new mute
// state, hot-swaps it in via load_file(), and seeks back to where playback
// was so the swap is inaudible as anything but the track dropping out.
// No-op (and returns false) if a Karafun file isn't currently active.
static bool karafun_toggle_track_and_reload(AudioPlayer *p, bool toggle_vocal) {
    if (!p) return false;
    KarafunState *kfn = karafun_get_state();
    if (!kfn || !kfn->active) return false;

    bool ok = toggle_vocal ? karafun_toggle_vocal_mute() : karafun_toggle_backing_mute();
    if (!ok) {
        SDL_Log("KARAFUN: Failed to toggle %s mute", toggle_vocal ? "vocal" : "backing");
        return false;
    }

    const char *mixed_path = karafun_get_mixed_path();
    if (!mixed_path) return false;

    double saved_position = playTime;
    bool was_playing = p->is_playing && !p->is_paused;

    if (load_file(p, mixed_path)) {
        seek_to_position(p, saved_position);
        if (was_playing) {
            start_playback(p);
        }
    }

    SDL_Log("KARAFUN: %s track %s", toggle_vocal ? "Vocal" : "Backing",
           (toggle_vocal ? kfn->vocal_muted : kfn->backing_muted) ? "muted" : "unmuted");
    return true;
}

// Wire these to the 'V' and 'B' keys respectively. Signature is void* (not
// AudioPlayer*) to match the declaration in karafun.h, which avoids that
// header needing to pull in audio_player.h.
void karafun_toggle_vocal_and_reload(void *player) {
    karafun_toggle_track_and_reload((AudioPlayer*)player, true);
}

void karafun_toggle_backing_and_reload(void *player) {
    karafun_toggle_track_and_reload((AudioPlayer*)player, false);
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

// GTK4's "close-request" replaces "delete-event" (GtkWindow*, gpointer -> gboolean).
gboolean on_vis_fullscreen_delete_event(GtkWindow *window, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    toggle_vis_fullscreen(player);
    return TRUE; // Prevent actual close, just exit fullscreen
}

bool is_visualizer_fullscreen() {
    return is_vis_fullscreen;
}

// GTK4 removed the generic GtkContainer API (gtk_container_add/remove), so
// "put this widget in that widget" is now type-specific. This covers the
// parent types the visualizer drawing area actually gets reparented into/out
// of (a GtkWindow for the fullscreen view, a GtkBox for wherever it normally
// lives). If it ever needs to go into some other container type, add a case here.
static void gtk4_reparent_add_child(GtkWidget *parent, GtkWidget *child) {
    if (GTK_IS_WINDOW(parent)) {
        gtk_window_set_child(GTK_WINDOW(parent), child);
    } else if (GTK_IS_BOX(parent)) {
        gtk_box_append(GTK_BOX(parent), child);
    } else {
        gtk_widget_set_parent(child, parent);
    }
}

// Symmetric counterpart to gtk4_reparent_add_child(): detaches `child` from
// `parent`. This must NOT just be a blind gtk_widget_unparent() call.
// Single-child containers like GtkWindow (and GtkScrolledWindow, GtkButton,
// etc.) keep their own private "child" pointer/property alongside the
// generic widget-tree links; gtk_widget_unparent() only updates the generic
// links, so a GtkWindow that had its child detached this way still believes
// it owns that widget. If the child then gets reparented somewhere else and
// the window is destroyed, the window's teardown reaches in through that
// stale pointer and corrupts a widget it no longer actually owns - which is
// exactly what was happening here: exiting visualizer fullscreen would
// intermittently free the Visualizer mid-session (via its
// g_object_set_data_full() GDestroyNotify) and corrupt the heap, because
// vis_fullscreen_window's stale child pointer got touched by
// gtk_window_destroy() after the drawing area had already been reparented
// back into the normal layout. Route single-child containers through their
// own child-clearing API; only fall back to gtk_widget_unparent() for
// containers (like GtkBox) that don't keep that kind of private pointer.
static void gtk4_reparent_remove_child(GtkWidget *parent, GtkWidget *child) {
    if (GTK_IS_WINDOW(parent)) {
        gtk_window_set_child(GTK_WINDOW(parent), NULL);
    } else {
        gtk_widget_unparent(child);
    }
}

void toggle_vis_fullscreen(AudioPlayer *player) {
    if (!player->visualizer || !player->visualizer->drawing_area) {
        SDL_Log("No visualizer available for fullscreen mode");
        return;
    }
    
    if (!is_vis_fullscreen) {
        // Enter visualization fullscreen mode
        SDL_Log("Entering visualization fullscreen mode");
        
        // Store original parent and size
        original_vis_parent = gtk_widget_get_parent(player->visualizer->drawing_area);
        gtk_widget_get_size_request(player->visualizer->drawing_area, &original_vis_width, &original_vis_height);
        
        // Create dedicated fullscreen window for visualization only.
        // gtk_window_new() takes no arguments in GTK4 (GTK_WINDOW_TOPLEVEL is gone -
        // GtkWindow is always effectively a toplevel now).
        vis_fullscreen_window = gtk_window_new();
        gtk_window_set_title(GTK_WINDOW(vis_fullscreen_window), "Audio Visualizer - Press F9 to exit");
        gtk_window_fullscreen(GTK_WINDOW(vis_fullscreen_window));
        gtk_window_set_decorated(GTK_WINDOW(vis_fullscreen_window), FALSE);
        // gtk_window_set_keep_above() is gone in GTK4 - client-side "always on
        // top" requests were dropped (Wayland compositors don't honor them
        // anyway), and fullscreening the window already keeps it on top in
        // practice, so there's nothing to replace this with.

        // Set black background for better visualization contrast.
        // gtk_widget_override_background_color() is gone - GTK4 styling goes
        // through CSS, so this loads a tiny inline stylesheet instead.
        {
            static GtkCssProvider *bg_provider = NULL;
            if (!bg_provider) {
                bg_provider = gtk_css_provider_new();
                gtk_css_provider_load_from_data(bg_provider,
                    ".vis-fullscreen-bg { background-color: black; }", -1);
                gtk_style_context_add_provider_for_display(
                    gdk_display_get_default(),
                    GTK_STYLE_PROVIDER(bg_provider),
                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            }
            gtk_widget_add_css_class(vis_fullscreen_window, "vis-fullscreen-bg");
        }
        
        // Reparent the visualization drawing area. GTK4 has no generic
        // gtk_container_remove() - gtk_widget_unparent() works regardless of
        // the parent's type.
        if (original_vis_parent) {
            g_object_ref(player->visualizer->drawing_area);
            gtk4_reparent_remove_child(original_vis_parent, player->visualizer->drawing_area);
        }
        
        gtk4_reparent_add_child(vis_fullscreen_window, player->visualizer->drawing_area);
        g_object_unref(player->visualizer->drawing_area);
        
        // Set visualization to full screen size. GdkScreen is gone in GTK4 -
        // monitor geometry comes from GdkMonitor instead.
        {
            GdkDisplay *display = gtk_widget_get_display(vis_fullscreen_window);
            GdkMonitor *monitor = gtk4_get_primary_monitor(display);
            GdkRectangle geom = {0, 0, 1920, 1080};
            if (monitor) {
                gdk_monitor_get_geometry(monitor, &geom);
                g_object_unref(monitor);
            }
            gtk_widget_set_size_request(player->visualizer->drawing_area, geom.width, geom.height);
        }
        
        // Set up key handler for F9 and Escape to exit fullscreen.
        // GTK4 replaces "key-press-event" with GtkEventControllerKey; its
        // "key-pressed" signal has a different callback signature
        // (keyval/keycode/state instead of a GdkEventKey), so
        // on_vis_fullscreen_key_press's definition needs updating to match -
        // it wasn't in any of the files shared so far.
        GtkEventController *key_controller = gtk_event_controller_key_new();
        g_signal_connect(key_controller, "key-pressed",
                        G_CALLBACK(on_vis_fullscreen_key_press), player);
        gtk_widget_add_controller(vis_fullscreen_window, key_controller);
        
        // Handle window close button
        g_signal_connect(vis_fullscreen_window, "close-request", 
                        G_CALLBACK(on_vis_fullscreen_delete_event), player);
        
        // Auto-hide the mouse cursor after a few seconds of inactivity.
        // GTK4 replaces the GDK_POINTER_MOTION_MASK + "motion-notify-event"
        // pair with a GtkEventControllerMotion.
        GtkEventController *motion_controller = gtk_event_controller_motion_new();
        g_signal_connect(motion_controller, "motion",
                        G_CALLBACK(on_vis_fullscreen_motion), player);
        gtk_widget_add_controller(vis_fullscreen_window, motion_controller);
        
        // GTK4 widgets are visible by default - no gtk_widget_show_all() needed.
        gtk_window_present(GTK_WINDOW(vis_fullscreen_window));
        
        is_vis_fullscreen = true;
        reset_vis_cursor_timer();
        gtk_widget_set_tooltip_text(player->visualizer->drawing_area, NULL);
        gtk_widget_set_has_tooltip(player->visualizer->drawing_area, FALSE);
        SDL_Log("Visualization fullscreen activated (F9 or Escape to exit)");
        
    } else {
        // Exit visualization fullscreen mode
        SDL_Log("Exiting visualization fullscreen mode");
        
        // Stop the cursor auto-hide timer before the window goes away
        if (vis_cursor_hide_timeout_id) {
            g_source_remove(vis_cursor_hide_timeout_id);
            vis_cursor_hide_timeout_id = 0;
        }
        
        if (vis_fullscreen_window && player->visualizer && player->visualizer->drawing_area) {
            // Reparent visualization back to original location
            g_object_ref(player->visualizer->drawing_area);
            gtk4_reparent_remove_child(vis_fullscreen_window, player->visualizer->drawing_area);
            
            if (original_vis_parent) {
                gtk4_reparent_add_child(original_vis_parent, player->visualizer->drawing_area);
                
                // Restore original size
                gtk_widget_set_size_request(player->visualizer->drawing_area, 
                                           original_vis_width, original_vis_height);
            }
            
            g_object_unref(player->visualizer->drawing_area);
            
            // Destroy fullscreen window
            gtk_window_destroy(GTK_WINDOW(vis_fullscreen_window));
            vis_fullscreen_window = NULL;
        }
        
        // Reset state
        is_vis_fullscreen = false;
        original_vis_parent = NULL;
        original_vis_width = 0;
        original_vis_height = 0;
        if (player->visualizer && player->visualizer->drawing_area) {
            gtk_widget_set_has_tooltip(player->visualizer->drawing_area, TRUE);
        }
        
        SDL_Log("Visualization returned to normal view");
    }
}


void cleanup_vis_fullscreen() {
    if (is_vis_fullscreen && vis_fullscreen_window) {
        // Force exit visualization fullscreen before cleanup
        if (player) {
            toggle_vis_fullscreen(player);
        } else if (vis_fullscreen_window) {
            gtk_window_destroy(GTK_WINDOW(vis_fullscreen_window));
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
    
    SDL_Log("Rewinded 5 seconds to %.2f", new_time);
}

void fast_forward_5_seconds(AudioPlayer *player) {
    if (!player->is_loaded) return;
    
    double current_time = playTime;
    double new_time = current_time + 5.0;
    if (new_time > player->song_duration) new_time = player->song_duration;
    
    seek_to_position(player, new_time);
    gtk_range_set_value(GTK_RANGE(player->progress_scale), new_time);
    
    SDL_Log("Fast forwarded 5 seconds to %.2f", new_time);
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
                    update_queue_display_minimal(player);  // Performance: minimal update for song switch
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
        SDL_Log("No tree view in next_song, using simple next");
        if (advance_queue(&player->queue)) {
            if (load_file_from_queue(player)) {
                update_queue_display_minimal(player);  // Performance: minimal update for song switch
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
        bool column_sort_active = gtk_tree_sortable_get_sort_column_id(sortable, &sort_column_id, &sort_type);

        // Follow the on-screen order whenever it's not plain queue order -
        // either a column sort is active, or the grouped-by-artist view is
        // showing. get_queue_display_order() walks the tree model
        // depth-first, so it covers a grouped tree's children too.
        if (column_sort_active || player->queue_grouped_view) {
            std::vector<int> order = get_queue_display_order(player);
            auto pos = std::find(order.begin(), order.end(), player->queue.current_index);

            bool found_next = false;
            if (pos != order.end()) {
                auto next_it = std::next(pos);
                if (next_it != order.end()) {
                    player->queue.current_index = *next_it;
                    found_next = true;
                } else if (player->queue.repeat_queue && !order.empty()) {
                    player->queue.current_index = order.front();
                    found_next = true;
                }
            }

            if (found_next) {
                if (load_file_from_queue(player)) {
                    update_queue_display_minimal(player);  // Performance: minimal update for song switch
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
            update_queue_display_minimal(player);  // Performance: minimal update for song switch
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
                    update_queue_display_minimal(player);
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
        SDL_Log("No tree view in previous_song, using simple previous");
        if (previous_queue(&player->queue)) {
            if (load_file_from_queue(player)) {
                update_queue_display_minimal(player);
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
        bool column_sort_active = gtk_tree_sortable_get_sort_column_id(sortable, &sort_column_id, &sort_type);

        // Same reasoning as next_song(): follow the on-screen order for
        // either an active column sort or the grouped-by-artist view.
        if (column_sort_active || player->queue_grouped_view) {
            std::vector<int> order = get_queue_display_order(player);
            auto pos = std::find(order.begin(), order.end(), player->queue.current_index);

            bool found_prev = false;
            if (pos != order.end() && pos != order.begin()) {
                player->queue.current_index = *std::prev(pos);
                found_prev = true;
            } else if (pos == order.begin() && player->queue.repeat_queue && !order.empty()) {
                player->queue.current_index = order.back();
                found_prev = true;
            }

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
                update_queue_display_minimal(player);  // Performance: minimal update for song switch
                update_gui_state(player);
                start_playback(player);
                SDL_Log("Next filtered song: %s (index %d)", 
                       get_current_queue_file(&player->queue), check_index);
            }
            return;
        }
        
        search_count++;
    }
    
    // No matching song found in filter
    SDL_Log("No next song matches current filter");
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
                update_queue_display_minimal(player);  // Performance: minimal update for song switch
                update_gui_state(player);
                start_playback(player);
                SDL_Log("Previous filtered song: %s (index %d)", 
                       get_current_queue_file(&player->queue), check_index);
            }
            return;
        }
        
        search_count++;
    }
    
    // No matching song found in filter
    SDL_Log("No previous song matches current filter");
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
// GTK4 removed gtk_dialog_run() - file choosers show non-modally and report
// back via a "response" signal instead of blocking the caller.
static void on_add_to_queue_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        // gtk_file_chooser_get_filenames() is gone too - multi-selection
        // results come back as a GListModel of GFile now.
        GListModel *files = gtk_file_chooser_get_files(GTK_FILE_CHOOSER(dialog));
        guint n = g_list_model_get_n_items(files);

        bool was_empty_queue = (player->queue.count == 0);

        for (guint i = 0; i < n; i++) {
            GFile *file = G_FILE(g_list_model_get_item(files, i));
            char *filename = g_file_get_path(file);
            if (filename) {
                if (!filename_exists_in_queue(&player->queue, filename)) {
                    add_to_queue(&player->queue, filename);
                }
                g_free(filename);
            }
            g_object_unref(file);
        }
        g_object_unref(files);

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

    gtk_window_destroy(GTK_WINDOW(dialog));
}

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
                    strcmp(ext_lower, ".kar") == 0 ||
                    strcmp(ext_lower, ".wav") == 0 ||
                    strcmp(ext_lower, ".mp3") == 0 ||
                    strcmp(ext_lower, ".m4a") == 0 ||
                    strcmp(ext_lower, ".ogg") == 0 ||
                    strcmp(ext_lower, ".aif") == 0 ||
                    strcmp(ext_lower, ".aiff") == 0 ||
                    strcmp(ext_lower, ".opus") == 0 ||
                    strcmp(ext_lower, ".flac") == 0 ||
                    strcmp(ext_lower, ".kfn") == 0 ||
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
            
            SDL_Log("Multiple files selected, directory: %s", directory);
            
            // Add each file (with extension validation)
            while (*ptr) {
                char full_path[2048];
                snprintf(full_path, sizeof(full_path), "%s\\%s", directory, ptr);
                
                SDL_Log("Processing file: %s", full_path);
                
                if (is_supported_extension(full_path)) {
                    add_to_queue(&player->queue, full_path);
                    SDL_Log("Added to queue: %s", full_path);
                } else {
                    SDL_Log("Skipping unsupported file: %s", full_path);
                }
                
                // Move to next filename
                ptr += strlen(ptr) + 1;
            }
        } else {
            // Single file (with extension validation)
            SDL_Log("Single file selected: %s", filename);
            
            if (is_supported_extension(filename)) {
                add_to_queue(&player->queue, filename);
                SDL_Log("Added single file to queue: %s", filename);
            } else {
                SDL_Log("Unsupported file type: %s", filename);
                
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
        
        SDL_Log("Total files in queue: %d", player->queue.count);
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
    gtk_file_filter_add_pattern(all_filter, "*.kar");
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
    gtk_file_filter_add_pattern(all_filter, "*.kfn");
    gtk_file_filter_add_pattern(all_filter, "*.zip");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all_filter);
    
    GtkFileFilter *midi_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(midi_filter, "MIDI Files (*.mid, *.midi, *.kar)");
    gtk_file_filter_add_pattern(midi_filter, "*.mid");
    gtk_file_filter_add_pattern(midi_filter, "*.midi");
    gtk_file_filter_add_pattern(midi_filter, "*.kar");
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

    
    g_signal_connect(dialog, "response", G_CALLBACK(on_add_to_queue_response), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
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

void on_repeat_queue_toggled(GtkCheckButton *button, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    player->queue.repeat_queue = gtk_check_button_get_active(button);
    
    SDL_Log("Queue repeat: %s", player->queue.repeat_queue ? "ON" : "OFF");
}

// Menu callbacks
// NOTE(gtk4): these signatures are loosened to GtkWidget* just to compile.
// GTK4 removed GtkMenuItem/GtkMenuBar/GtkMenu entirely - menus are built from
// a GMenu model now (GAction callbacks take (GSimpleAction*, GVariant*,
// gpointer), or a GtkPopoverMenuBar/GtkMenuButton can drive plain widget
// signals instead). Whichever menu bar actually builds these in layout.cpp
// needs the real conversion; this file just needs to compile against it.
void on_menu_import_directory(GtkWidget *menuitem, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    g_on_import_directory_clicked(nullptr, nullptr);
}

// GTK4 removed gtk_dialog_run() - see on_add_to_queue_response() above for
// the same conversion applied to this single-file chooser.
static void on_menu_open_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        // gtk_file_chooser_get_filename() is gone - use GFile + g_file_get_path().
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        char *filename = file ? g_file_get_path(file) : NULL;

        if (filename) {
            bool was_empty_queue = (player->queue.count == 0);
            int existing_index = find_file_in_queue(&player->queue, filename);

            // If file already exists in queue, jump to it
            if (existing_index >= 0) {
                SDL_Log("File already in queue at index %d, jumping to it", existing_index);
                player->queue.current_index = existing_index;
                if (load_file_from_queue(player)) {
                    SDL_Log("Jumped to: %s", filename);
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
                        SDL_Log("Successfully loaded: %s", filename);
                        update_queue_display_with_filter(player);
                        update_gui_state(player);
                    }
                } else if (!was_empty_queue) {
                    // Queue wasn't empty, just set to play this new file immediately
                    player->queue.current_index = player->queue.count - 1;
                    if (load_file_from_queue(player)) {
                        SDL_Log("Successfully loaded: %s", filename);
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
        if (file) {
            g_object_unref(file);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

void on_menu_open(GtkWidget *menuitem, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
#ifdef _WIN32
    char filename[32768];
    if (open_windows_file_dialog(filename, sizeof(filename))) {
        bool was_empty_queue = (player->queue.count == 0);
        int existing_index = find_file_in_queue(&player->queue, filename);
        
        // If file already exists in queue, jump to it
        if (existing_index >= 0) {
            SDL_Log("File already in queue at index %d, jumping to it", existing_index);
            player->queue.current_index = existing_index;
            if (load_file_from_queue(player)) {
                SDL_Log("Jumped to: %s", filename);
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
                    SDL_Log("Successfully loaded: %s", filename);
                    update_queue_display_with_filter(player);
                    update_gui_state(player);
                }
            } else if (!was_empty_queue) {
                // Queue wasn't empty, just set to play this new file immediately
                player->queue.current_index = player->queue.count - 1;
                if (load_file_from_queue(player)) {
                    SDL_Log("Successfully loaded: %s", filename);
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
    gtk_file_filter_add_pattern(all_filter, "*.kar");
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
    gtk_file_filter_add_pattern(all_filter, "*.kfn");
    gtk_file_filter_add_pattern(all_filter, "*.zip");


    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all_filter);
    
    GtkFileFilter *midi_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(midi_filter, "MIDI Files (*.mid, *.midi, *.kar)");
    gtk_file_filter_add_pattern(midi_filter, "*.mid");
    gtk_file_filter_add_pattern(midi_filter, "*.midi");
    gtk_file_filter_add_pattern(midi_filter, "*.kar");

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


    
    g_signal_connect(dialog, "response", G_CALLBACK(on_menu_open_response), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
#endif
}


void on_window_resize(GtkWidget *widget, gpointer user_data) {

    // Get screen resolution. GTK4 removed GdkScreen - monitor geometry comes
    // from GdkMonitor now (already in logical/scale-adjusted pixels).
    GdkDisplay *display = gtk_widget_get_display(widget);
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(gtk_widget_get_root(widget)));
    bool monitor_owned = !surface; // gdk_display_get_monitor_at_surface() is borrowed; our fallback isn't
    GdkMonitor *monitor = surface
        ? gdk_display_get_monitor_at_surface(display, surface)
        : gtk4_get_primary_monitor(display);
    GdkRectangle monitor_geom = {0, 0, 1920, 1080};
    if (monitor) {
        gdk_monitor_get_geometry(monitor, &monitor_geom);
        if (monitor_owned) {
            g_object_unref(monitor);
        }
    }
    int screen_width = monitor_geom.width;
    int screen_height = monitor_geom.height;
    //SDL_Log("Screen %i %i", screen_width, screen_height);

    //SDL_Log("Screen resolution: %dx%d", screen_width, screen_height);

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
        //SDL_Log("Using very small screen base sizes");
    } else if (screen_width < 1200 || screen_height < 900) {
        // Medium screens (1024x768, etc.) - moderately smaller visualization
        base_window_width = 800;
        base_window_height = 600;
        base_player_width = 400;
        base_vis_width = 260;  // Smaller visualization
        base_vis_height = 120; // Smaller visualization
        base_queue_width = 250;
        base_queue_height = 350;
        //SDL_Log("Using medium-screen base sizes");
    } else {
        // Large screens (1920x1080+) - keep current size
        base_window_width = 900;
        base_window_height = 700;
        base_player_width = 500;
        base_vis_width = 400;
        base_vis_height = 200;
        base_queue_width = 300;
        base_queue_height = 400;
        //SDL_Log("Using large-screen base sizes");
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

    // NOTE(gtk4): GTK3's gdk_screen_get_width() returned physical pixels, so
    // this used to divide back down to logical pixels for HiDPI screens.
    // GdkMonitor geometry is already logical, so that division is no longer
    // needed here - screen_width/screen_height above are already the right units.


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

    //SDL_Log("Final sizes: window=%dx%d, player=%d, vis=%dx%d, queue=%dx%d", window_width, window_height, player_width, vis_width, vis_height, queue_width, queue_height);

    // Resize window
    //gtk_window_resize(GTK_WINDOW(widget), window_width, window_height);

    // Adjust player vbox width. GTK4 removed GtkContainer/gtk_container_get_children -
    // walk first-child instead (widget is the top-level window here, so its
    // first child is main_hbox, whose first child is player_vbox).
    GtkWidget *main_hbox = gtk_widget_get_first_child(widget);
    if (main_hbox) {
        GtkWidget *player_vbox = gtk_widget_get_first_child(main_hbox);
        if (player_vbox) {
            gtk_widget_set_size_request(player_vbox, player_width, -1);
        }
    }

    // Adjust visualizer size
    if (player->visualizer && player->visualizer->drawing_area) {
        gtk_widget_set_size_request(player->visualizer->drawing_area, vis_width, vis_height);
        //SDL_Log("Set visualizer size to: %dx%d", vis_width, vis_height);
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
    
    // GTK4 replaces per-widget GdkWindow with GdkSurface, available only via
    // the widget's root (GtkNative).
    GtkNative *native = gtk_widget_get_native(widget);
    GdkSurface *surface = native ? gtk_native_get_surface(native) : NULL;
    if (!surface) {
        return 1.0;
    }
    
    GdkDisplay *display = gdk_surface_get_display(surface);
    GdkMonitor *monitor = gdk_display_get_monitor_at_surface(display, surface);
    
    if (monitor) {
        return gdk_monitor_get_scale_factor(monitor);
    }
    
    return 1.0;
}

void on_menu_quit(GtkWidget *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    SDL_Log("Menu quit selected - triggering cleanup");
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

// GTK4 replaces "delete-event" with "close-request" (GtkWindow*, gpointer ->
// gboolean instead of GtkWidget*, GdkEvent*, gpointer). The g_signal_connect
// call site in layout.cpp needs to switch from "delete-event" to
// "close-request" to match when that file gets converted.
gboolean on_window_delete_event(GtkWindow *window, gpointer user_data) {
    (void)window;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    SDL_Log("Window close button pressed, cleaning up...");
    
    // Save current queue before exit
    save_current_queue_on_exit(player);

    save_player_settings(player);
    
    stop_playback(player);
    clear_queue(&player->queue);
    cleanup_queue_filter(player);
    cleanup_conversion_cache(&player->conversion_cache);
    cleanup_audio_cache(&player->audio_cache); 
    cleanup_virtual_filesystem();
    
    SDL_Log("Cleaing up Audio");
    if (player->audio_buffer.data) free(player->audio_buffer.data);

    if (player->cdg_display) {
        cdg_display_free(player->cdg_display);
    }    

    SDL_Log("Closing  SDL 1");
    if (player->audio_device) SDL_CloseAudioDevice(player->audio_device);

    SDL_Log("Cleaning Equalizer");
    if (player->equalizer) {
        equalizer_free(player->equalizer);
    }
    
 

    SDL_Log("Freeing player");
    if (player) {
        //delete player;
    }

    SDL_Log("Closing  SDL");
    SDL_Quit();
    
    SDL_Log("Closing main window");
    if (g_app_main_loop) {
        g_main_loop_quit(g_app_main_loop);
    }

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
    
    fclose(f);
    SDL_Log("Settings saved to: %s", settings_path);
    return true;
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
    
    SDL_Log("Settings loaded successfully");
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
        
        SDL_Log("Received file from another instance: %s", filepath);
        
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
        SDL_Log("Sent file to existing instance: %s", filepath);
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
        SDL_Log("Failed to parse D-Bus introspection XML");
        return;
    }
    
    player->dbus_connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!player->dbus_connection) {
        SDL_Log("Failed to connect to D-Bus");
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
    SDL_Log("D-Bus service registered: %s", ZENAMP_DBUS_NAME);
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
    
    SDL_Log("Sent file to existing instance: %s", filepath);
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
                SDL_Log("Received file from another instance: %s", filepath);
                
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
    
    // Hook into GTK window's Win32 HWND.
    // GTK4 renames GdkWindow to GdkSurface (only reachable via GtkNative).
    // gdk_win32_surface_get_handle() takes a GdkWin32Surface*, not a plain
    // GdkSurface*, so it must go through the GDK_WIN32_SURFACE() cast macro,
    // and it returns an integer handle that needs casting to HWND.
    GdkSurface *gdk_surface = gtk_native_get_surface(GTK_NATIVE(player->window));
    if (gdk_surface && GDK_IS_WIN32_SURFACE(gdk_surface)) {
        HWND hwnd = (HWND)gdk_win32_surface_get_handle(GDK_WIN32_SURFACE(gdk_surface));
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)player);
        
        // Subclass window to receive WM_COPYDATA
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)window_proc_wrapper);
    }
    
    SDL_Log("Windows single instance mutex created");
}

void cleanup_windows_single_instance(AudioPlayer *player) {
    if (player->single_instance_mutex) {
        CloseHandle(player->single_instance_mutex);
    }
}
#endif


#ifdef _WIN32
// gdk-pixbuf finds its loader modules (svg, png, etc.) via a cache file
// whose location it resolves either from the GDK_PIXBUF_MODULE_FILE env
// var or a compiled-in default - neither of which is reliable for a
// portable/relocatable install. collect_dlls.sh bundles
// lib\gdk-pixbuf-2.0\<version>\loaders.cache next to the exe, but relies on
// the app being launched with its own directory as the working directory.
// This computes an absolute path to that cache from the exe's own location
// (via GetModuleFileNameA) and points GDK_PIXBUF_MODULE_FILE at it
// directly, so loader discovery works no matter where the app is invoked
// from. Must run before gtk_init() - GTK's own icon-theme loading already
// exercises gdk-pixbuf loaders during init.
static void setup_gdk_pixbuf_module_file(void) {
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return; // couldn't resolve our own path - leave gdk-pixbuf to its defaults
    }

    // Strip the executable filename, leaving the directory it lives in
    char *last_slash = strrchr(exe_path, '\\');
    if (!last_slash) {
        return;
    }
    *last_slash = '\0';

    char pixbuf_dir[MAX_PATH];
    snprintf(pixbuf_dir, sizeof(pixbuf_dir), "%s\\lib\\gdk-pixbuf-2.0", exe_path);

    // Version subdirectory name isn't known at compile time - it's whatever
    // collect_dlls.sh found on the build machine's sysroot - so scan for
    // whichever subdirectory actually has a loaders.cache in it.
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*", pixbuf_dir);

    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(search_pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        SDL_Log("gdk-pixbuf: no %s directory found, leaving loader discovery to defaults", pixbuf_dir);
        return;
    }

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) continue;

        char cache_path[MAX_PATH];
        snprintf(cache_path, sizeof(cache_path), "%s\\%s\\loaders.cache", pixbuf_dir, find_data.cFileName);

        DWORD attrs = GetFileAttributesA(cache_path);
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            SetEnvironmentVariableA("GDK_PIXBUF_MODULE_FILE", cache_path);
            SDL_Log("gdk-pixbuf: using loaders.cache at %s", cache_path);
            FindClose(find_handle);
            return;
        }
    } while (FindNextFileA(find_handle, &find_data));

    FindClose(find_handle);
    SDL_Log("gdk-pixbuf: found %s but no loaders.cache inside any subdirectory - SVG/PNG loader discovery may fail at runtime", pixbuf_dir);
}
#endif

int main(int argc, char *argv[]) {
#ifdef _WIN32
    setup_gdk_pixbuf_module_file();
#endif
    // GTK4's gtk_init() takes no arguments - it no longer strips its own
    // flags out of argv, so argc/argv below are exactly what the OS gave us.
    gtk_init();
    
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
                    SDL_Log("Sent file to existing instance: %s", abs_path);
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
                SDL_Log("All files forwarded to existing instance, exiting");
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
                SDL_Log("Found existing Zenamp window, sending files...");
                for (int i = 1; i < argc; i++) {
                    char abs_path[4096];
                    _fullpath(abs_path, argv[i], sizeof(abs_path));
                    
                    COPYDATASTRUCT cds;
                    cds.dwData = 1;
                    cds.cbData = strlen(abs_path) + 1;
                    cds.lpData = (void*)abs_path;
                    
                    SendMessage(hwnd, WM_COPYDATA, 0, (LPARAM)&cds);
                    SDL_Log("Sent file to existing instance: %s", abs_path);
                }
                
                SetForegroundWindow(hwnd);
                ShowWindow(hwnd, SW_RESTORE);
                SDL_Log("All files forwarded to existing instance, exiting");
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
        SDL_Log("Audio initialization failed");
        cleanup_conversion_cache(&player->conversion_cache);
        cleanup_virtual_filesystem();
        return 1;
    }
    
    player->equalizer = equalizer_new(SAMPLE_RATE);
    if (!player->equalizer) {
        SDL_Log("Failed to initialize equalizer");
    }
    
    OPL_Init(SAMPLE_RATE);
    OPL_LoadInstruments();
    
    player->cdg_display = cdg_display_new();
    player->has_cdg = false;
    
    create_main_window(player);
    SDL_Log("DEBUG: create_main_window returned");
    update_gui_state(player);
    SDL_Log("DEBUG: update_gui_state returned");
    // GTK4 widgets are visible by default - gtk_widget_show_all() is gone.
    gtk_window_present(GTK_WINDOW(player->window));
    SDL_Log("DEBUG: gtk_window_present returned");
    
    // Force UI to render immediately before any blocking operations.
    // GTK4 removed gtk_events_pending()/gtk_main_iteration(); pump the
    // default GLib main context directly instead.
    while (g_main_context_pending(NULL)) {
        g_main_context_iteration(NULL, FALSE);
    }
    SDL_Log("DEBUG: initial main-context pump complete");
    
#ifdef _WIN32
    // Setup Windows single instance AFTER window is shown
    SDL_Log("DEBUG: about to call CreateMutexA");
    HANDLE single_instance_mutex = CreateMutexA(NULL, TRUE, ZENAMP_MUTEX_NAME);
    player->single_instance_mutex = single_instance_mutex;
    SDL_Log("DEBUG: CreateMutexA returned %p", (void*)single_instance_mutex);
    
    SDL_Log("DEBUG: about to call gtk_native_get_surface");
    GdkSurface *gdk_surface = gtk_native_get_surface(GTK_NATIVE(player->window));
    SDL_Log("DEBUG: gtk_native_get_surface returned %p", (void*)gdk_surface);
    if (gdk_surface && GDK_IS_WIN32_SURFACE(gdk_surface)) {
        SDL_Log("DEBUG: surface is a WIN32 surface, about to call gdk_win32_surface_get_handle");
        HWND hwnd = (HWND)gdk_win32_surface_get_handle(GDK_WIN32_SURFACE(gdk_surface));
        SDL_Log("DEBUG: gdk_win32_surface_get_handle returned %p", (void*)hwnd);
        if (hwnd) {
            // Set a window property to identify this as Zenamp
            SetPropA(hwnd, "ZenampInstance", (HANDLE)1);
            SDL_Log("DEBUG: SetPropA(ZenampInstance) done");
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)player);
            SDL_Log("DEBUG: SetWindowLongPtr(GWLP_USERDATA) done");
            
            // Subclass to handle WM_COPYDATA
            WNDPROC old_proc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
            SDL_Log("DEBUG: GetWindowLongPtr(GWLP_WNDPROC) returned %p", (void*)old_proc);
            SetProp(hwnd, TEXT("OldWndProc"), (HANDLE)old_proc);
            SDL_Log("DEBUG: SetProp(OldWndProc) done, about to subclass wndproc");
            
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)+[](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                if (msg == WM_COPYDATA) {
                    COPYDATASTRUCT *cds = (COPYDATASTRUCT*)lParam;
                    if (cds->dwData == 1) {
                        char *filepath = (char*)cds->lpData;
                        AudioPlayer *player = (AudioPlayer*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
                        
                        if (player) {
                            SDL_Log("Received file from another instance: %s", filepath);
                            
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
            
            SDL_Log("Windows message handler installed on HWND %p", hwnd);
        }
    }
    
    SDL_Log("Windows single instance mutex created");
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
                        
                        SDL_Log("Received file from another instance: %s", filepath);
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
            
            SDL_Log("D-Bus service registered: %s", ZENAMP_DBUS_NAME);
        }
        
        g_dbus_node_info_unref(introspection_data);
    }
    
    if (error) {
        SDL_Log("D-Bus setup error: %s", error->message);
        g_error_free(error);
    }
#endif
    
    SDL_Log("DEBUG: about to call load_player_settings");
    load_player_settings(player);
    SDL_Log("DEBUG: load_player_settings returned");
    
    char last_playlist[1024];
    bool loaded_last_playlist = false;
    if (load_last_playlist_path(last_playlist, sizeof(last_playlist))) {
        SDL_Log("Auto-loading last playlist: %s", last_playlist);
        if (load_m3u_playlist(player, last_playlist)) {
            SDL_Log("Successfully loaded last playlist");
            loaded_last_playlist = true;
            
            int saved_index = 0;
            double saved_position = 0.0;
            if (load_playlist_state(&saved_index, &saved_position)) {
                if (saved_index >= 0 && saved_index < player->queue.count) {
                    player->queue.current_index = saved_index;
                    SDL_Log("Restored queue index to %d", saved_index);
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
            SDL_Log("Loading new M3U playlist: %s", abs_playlist_path);
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
                SDL_Log("Loaded and auto-starting file from queue");
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
                    SDL_Log("File already in queue at index %d, jumping to it", found_index);
                    player->queue.current_index = found_index;
                    if (load_file_from_queue(player)) {
                        SDL_Log("Loaded and auto-starting existing file from queue");
                        update_queue_display_with_filter(player);
                        update_gui_state(player);
                    }
                } else {
                    SDL_Log("File not in queue, adding and playing it");
                    if (!filename_exists_in_queue(&player->queue, abs_file_path)) {
                        add_to_queue(&player->queue, abs_file_path);
                        player->queue.current_index = player->queue.count - 1;
                    }
                    if (load_file_from_queue(player)) {
                        SDL_Log("Loaded and auto-starting new file");
                        update_queue_display_with_filter(player);
                        update_gui_state(player);
                    }
                }
            }
        }
    } else if (loaded_last_playlist && player->queue.count > 0) {
        SDL_Log("Auto-loading first accessible file from last playlist...");
        
        // Try to load first accessible file from queue
        // Will automatically skip any inaccessible files with timeout
        if (load_file_from_queue(player)) {
            int saved_index = 0;
            double saved_position = 0.0;
            if (load_playlist_state(&saved_index, &saved_position)) {
                if (saved_position > 0 && saved_position < player->song_duration) {
                    seek_to_position(player, saved_position);
                    gtk_range_set_value(GTK_RANGE(player->progress_scale), saved_position);
                    SDL_Log("Restored playback position to %.2f", saved_position);
                }
            }
        } else {
            SDL_Log("No accessible files found in playlist on startup");
            if (player->visualizer) {
                snprintf(player->visualizer->error_message, sizeof(player->visualizer->error_message),
                         "No accessible files in playlist");
                player->visualizer->showing_error = true;
                player->visualizer->error_display_time = 3.0;
            }
        }
        
        // Queue population posted back to main thread (atomic operation)
        SDL_Log("Queuing background population of %d files...", player->queue.count);
        std::thread([](AudioPlayer *p) {
            SDL_Log("Background population thread started");
            g_usleep(100000);  // 100ms to ensure UI is fully rendered
            
            SDL_Log("Populating queue display (%d files)...", p->queue.count);
            
            // Post the entire update as atomic operation on main thread
            // This prevents conflicts between concurrent imports
            g_idle_add([](gpointer data) -> gboolean {
                AudioPlayer *p = (AudioPlayer*)data;
                SDL_Log("Queue update on main thread (%d files)", p->queue.count);
                update_queue_display_with_filter(p);
                SDL_Log("Queue display complete");
                return FALSE;
            }, p);
        }, player).detach();
    }
    
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // GTK4 removed gtk_main()/gtk_main_quit() - windows already pump the
    // default GLib main context once shown, so a plain GMainLoop drives the
    // app the same way gtk_main() used to. on_window_delete_event() calls
    // g_main_loop_quit(g_app_main_loop) to stop this when the window closes.
    g_app_main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(g_app_main_loop);
    g_main_loop_unref(g_app_main_loop);
    g_app_main_loop = NULL;
    
#ifdef _WIN32
    if (single_instance_mutex) {
        CloseHandle(single_instance_mutex);
    }
#endif
    
    g_free(player);
    return 0;
}
