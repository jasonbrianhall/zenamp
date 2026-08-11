#include "audio_player.h"
#include "miniz.h"
#include "kfn.h"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <glib.h>
#include <string.h>
#include <filesystem>
#include <sys/stat.h>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <algorithm>
   
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Queue metadata cache + background loader
//
// update_queue_display_with_filter() used to extract tag metadata and decode
// duration for EVERY file in the queue, synchronously, on the GTK main
// thread, every time it ran (startup, filter keystrokes, add/remove, dedup,
// ...). For large queues (thousands of files) that meant thousands of file
// opens/decodes/zip-extractions blocking the UI thread at once - the app
// would freeze on startup and on every keystroke in the filter box.
//
// Fix: cache extracted metadata per filepath (invalidated by mtime), and
// only ever do the actual extraction work on a background thread. The
// display always renders immediately from whatever's in the cache; files
// not yet cached show a "(loading...)" placeholder that gets refreshed as
// the background loader fills the cache in.
// ---------------------------------------------------------------------------

struct QueueMetaCacheEntry {
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    int duration_seconds = 0;
    bool is_karaoke = false;
    bool file_accessible = true;
    time_t mtime = 0;
    bool loaded = false;  // false = placeholder only, not yet extracted
};

static std::mutex g_queue_meta_mutex;
static std::unordered_map<std::string, QueueMetaCacheEntry> g_queue_meta_cache;
static std::atomic<bool> g_queue_meta_loader_running{false};

// Forward declarations - these are defined further down in this file, but
// extract_queue_item_metadata() (used by the background loader above the
// point they're defined) needs them.
static char *extract_kfn_metadata(const char *kfn_path);
static int get_kfn_duration(const char *kfn_path);
char* extract_audio_from_zip(const char *zip_path);

// Forward declaration - defined further down alongside the grouped-view
// machinery, but on_queue_context_menu() (used by on_queue_row_activated
// et al, further up in this file) needs it too.
static bool find_tree_path_for_queue_index(GtkTreeModel *model, int queue_index, GtkTreePath **out_path);

// Does the actual (potentially slow) per-file extraction work: tag reading,
// duration decoding, zip/kfn handling, and the .cdg sidecar check. This is
// exactly the logic that previously lived inline in the display loop - it
// must only ever be called from the background loader thread, never from
// the main thread.
static QueueMetaCacheEntry extract_queue_item_metadata(const char *filepath) {
    QueueMetaCacheEntry result;
    result.loaded = true;

    struct stat st;
    if (stat(filepath, &st) == 0) {
        result.mtime = st.st_mtime;
    }

    const char *ext = strrchr(filepath, '.');
    char *metadata = NULL;
    int duration_seconds = 0;
    bool file_accessible = true;

    if (ext && strcasecmp(ext, ".zip") == 0) {
        char *extracted_path = extract_audio_from_zip(filepath);
        if (extracted_path) {
            metadata = extract_metadata(extracted_path);
            duration_seconds = get_file_duration(extracted_path);
            unlink(extracted_path);
            g_free(extracted_path);
        } else {
            metadata = g_strdup("(File not accessible)");
            duration_seconds = 0;
            file_accessible = false;
            SDL_Log("Warning: ZIP file not accessible: %s", filepath);
        }
    } else if (ext && strcasecmp(ext, ".kfn") == 0) {
        metadata = extract_kfn_metadata(filepath);

        if (!metadata || strlen(metadata) == 0) {
            g_free(metadata);
            metadata = g_strdup("(File not accessible)");
            duration_seconds = 0;
            file_accessible = false;
            SDL_Log("Warning: KFN file not accessible: %s", filepath);
        } else {
            duration_seconds = get_kfn_duration(filepath);
        }
    } else {
        metadata = extract_metadata(filepath);

        if (!metadata || strlen(metadata) == 0) {
            g_free(metadata);
            metadata = g_strdup("(File not accessible)");
            duration_seconds = 0;
            file_accessible = false;
            SDL_Log("Warning: File not accessible: %s", filepath);
        } else {
            duration_seconds = get_file_duration(filepath);
        }
    }

    char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
    parse_metadata(metadata, title, artist, album, genre);
    g_free(metadata);

    // Karaoke indicator: .kfn/.zip/.kar, or an audio file with a matching .cdg
    bool is_karaoke = false;
    if (ext) {
        if (strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".kfn") == 0 || strcasecmp(ext, ".kar") == 0) {
            is_karaoke = true;
        } else {
            std::string lower_ext = ext;
            for (auto &c : lower_ext) c = tolower((unsigned char)c);
            bool is_audio = (lower_ext == ".ogg" || lower_ext == ".mp3" || lower_ext == ".wav" ||
                             lower_ext == ".m4a" || lower_ext == ".aac" || lower_ext == ".flac");

            if (is_audio) {
                std::string cdg_path = filepath;
                size_t pos = cdg_path.rfind(ext);
                if (pos != std::string::npos) {
                    cdg_path.replace(pos, strlen(ext), ".cdg");
                    if (fs::exists(cdg_path)) {
                        is_karaoke = true;
                    }
                }
            }
        }
    }

    result.title = title;
    result.artist = artist;
    result.album = album;
    result.genre = genre;
    result.duration_seconds = duration_seconds;
    result.is_karaoke = is_karaoke;
    result.file_accessible = file_accessible;

    return result;
}

// Looks up filepath in the cache. If it's missing, or present but stale
// (source file's mtime has changed since it was cached), sets *needs_load
// and returns whatever's available for immediate display (an empty
// placeholder, or the stale-but-still-useful old values). Cheap: only a
// stat() and a hash lookup, safe to call from the main thread.
static QueueMetaCacheEntry get_cached_or_placeholder(const char *filepath, bool *needs_load) {
    std::lock_guard<std::mutex> lock(g_queue_meta_mutex);

    auto it = g_queue_meta_cache.find(filepath);
    if (it == g_queue_meta_cache.end() || !it->second.loaded) {
        *needs_load = true;
        return QueueMetaCacheEntry{};
    }

    struct stat st;
    if (stat(filepath, &st) == 0 && st.st_mtime != it->second.mtime) {
        *needs_load = true;  // stale - reload in background, but use old values meanwhile
        return it->second;
    }

    *needs_load = false;
    return it->second;
}

// Runs on a background thread: extracts metadata for every path in
// `paths` and stores it in the cache, periodically hopping back to the
// main thread (via g_idle_add) to refresh the visible queue so long queues
// fill in progressively instead of appearing frozen or empty.
static void queue_metadata_loader_run(AudioPlayer *player, std::vector<std::string> paths) {
    SDL_Log("Queue metadata loader: extracting metadata for %zu file(s)...", paths.size());

    int since_last_refresh = 0;
    for (const auto &filepath : paths) {
        QueueMetaCacheEntry entry = extract_queue_item_metadata(filepath.c_str());

        {
            std::lock_guard<std::mutex> lock(g_queue_meta_mutex);
            g_queue_meta_cache[filepath] = entry;
        }

        // Refresh the visible list every 200 files so progress is visible
        // on very large queues, instead of blocking until everything is done.
        if (++since_last_refresh >= 200) {
            since_last_refresh = 0;
            g_idle_add([](gpointer data) -> gboolean {
                AudioPlayer *p = (AudioPlayer *)data;
                if (p) update_queue_display_with_filter(p, false);
                return FALSE;
            }, player);
        }
    }

    SDL_Log("Queue metadata loader: finished.");
    g_queue_meta_loader_running = false;

    // Final refresh to pick up the last batch, and to pick up any files
    // that were added to the queue (and so became "pending") while this
    // loader was already running - that refresh will spawn a fresh loader
    // for them since the running flag is now clear.
    g_idle_add([](gpointer data) -> gboolean {
        AudioPlayer *p = (AudioPlayer *)data;
        if (p) update_queue_display_with_filter(p, false);
        return FALSE;
    }, player);
}

// Kicks off the background loader for `paths` if one isn't already running.
// If a loader is already in flight, this is a no-op: that loader's own
// completion callback will trigger a fresh display refresh, which will
// pick up any still-pending files (including new ones added meanwhile)
// and start a new loader for them.
static void queue_metadata_loader_start(AudioPlayer *player, std::vector<std::string> paths) {
    if (paths.empty()) return;

    bool expected = false;
    if (!g_queue_meta_loader_running.compare_exchange_strong(expected, true)) {
        return;
    }

    std::thread([player, paths = std::move(paths)]() mutable {
        queue_metadata_loader_run(player, std::move(paths));
    }).detach();
}

// Karafun (.kfn) files aren't audio files, so extract_metadata()'s normal
// tag-reading finds nothing for them. The real title/artist live in the
// embedded Song.ini's [general] section, so pull them from there and format
// them into the same "<b>Title:</b> ...\n<b>Artist:</b> ..." markup that
// extract_metadata() produces, so parse_metadata() can pick them up as-is.
static char *extract_kfn_metadata(const char *kfn_path) {
    KFNArchive archive;
    if (!archive.open(kfn_path)) {
        return g_strdup("");
    }

    const KFNEntry *entry = archive.find("Song.ini");
    if (!entry) {
        return g_strdup("");
    }

    size_t size = 0;
    unsigned char *raw = archive.extract(*entry, size);
    if (!raw) {
        return g_strdup("");
    }

    std::string ini((const char *)raw, size);
    free(raw);

    std::string title, artist;
    bool in_general = false;
    size_t pos = 0;
    while (pos < ini.size()) {
        size_t eol = ini.find('\n', pos);
        size_t line_end = (eol == std::string::npos) ? ini.size() : eol;
        std::string line = ini.substr(pos, line_end - pos);
        pos = (eol == std::string::npos) ? ini.size() : eol + 1;

        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (line.front() == '[') {
            in_general = (strcasecmp(line.c_str(), "[general]") == 0);
            continue;
        }

        if (!in_general) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (strcasecmp(key.c_str(), "title") == 0) {
            title = value;
        } else if (strcasecmp(key.c_str(), "artist") == 0) {
            artist = value;
        }

        if (!title.empty() && !artist.empty()) break;
    }

    if (title.empty() && artist.empty()) {
        return g_strdup("");
    }

    std::string markup = "<b>Title:</b> " + (title.empty() ? "Unknown Title" : title) +
                          "\n<b>Artist:</b> " + (artist.empty() ? "Unknown Artist" : artist) + "\n";

    return g_strdup(markup.c_str());
}

// Measures the length of a .kfn karaoke file by pulling its backing track
// (falling back to the vocal track if there's no separate backing track)
// out to a temp file and running it through the normal duration reader —
// Song.ini doesn't carry a duration field, so this is the only real way.
static int get_kfn_duration(const char *kfn_path) {
    KFNArchive archive;
    if (!archive.open(kfn_path)) {
        return 0;
    }

    const KFNEntry *backing_entry = nullptr;
    const KFNEntry *vocal_entry = nullptr;

    for (const auto &entry : archive.entries()) {
        const char *dot = strrchr(entry.filename.c_str(), '.');
        if (!dot) continue;
        std::string ext = dot;
        for (auto &c : ext) c = tolower((unsigned char)c);
        bool is_audio = (ext == ".ogg" || ext == ".mp3" || ext == ".wav" ||
                          ext == ".m4a" || ext == ".aac" || ext == ".flac");
        if (!is_audio) continue;

        std::string lower = entry.filename;
        for (auto &c : lower) c = tolower((unsigned char)c);
        bool is_backing = lower.find("instru") != std::string::npos ||
                           lower.find("beat") != std::string::npos;

        if (is_backing && !backing_entry) {
            backing_entry = &entry;
        } else if (!is_backing && !vocal_entry) {
            vocal_entry = &entry;
        }
    }

    const KFNEntry *chosen = backing_entry ? backing_entry : vocal_entry;
    if (!chosen) {
        return 0;
    }

    const char *dot = strrchr(chosen->filename.c_str(), '.');
    std::string ext = dot ? dot : ".ogg";

    std::string tmp_path = archive.extractToTemp(*chosen, ext.c_str());
    if (tmp_path.empty()) {
        return 0;
    }

    int duration = get_file_duration(tmp_path.c_str());
    unlink(tmp_path.c_str());
    return duration;
}

#ifdef _WIN32
#include <windows.h>
#endif

std::string get_temp_directory_queue() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, buffer);
    return std::string(buffer, len);
#else
    const char* tmp = getenv("TMPDIR");
    return tmp ? tmp : "/tmp";
#endif
}
 
// Global variables for queue row-move tracking (used by
// on_queue_model_row_deleted/inserted, see below)
static int pending_delete_index = -1;
static char *pending_move_file = NULL;

// Check if a file already exists in the queue by filename
// Returns true if a file with the same basename already exists
bool filename_exists_in_queue(PlayQueue *queue, const char *filepath) {
    if (!queue || !filepath) {
        return false;
    }
    
    char *new_basename = g_path_get_basename(filepath);
    
    for (int i = 0; i < queue->count; i++) {
        char *existing_basename = g_path_get_basename(queue->files[i]);
        bool match = (strcmp(new_basename, existing_basename) == 0);
        g_free(existing_basename);
        
        if (match) {
            g_free(new_basename);
            return true;
        }
    }
    
    g_free(new_basename);
    return false;
}

// Find the index of a file in the queue by full path
// Returns the index if found, -1 if not found
int find_file_in_queue(PlayQueue *queue, const char *filepath) {
    if (!queue || !filepath) {
        return -1;
    }
    
    char *new_basename = g_path_get_basename(filepath);
    
    for (int i = 0; i < queue->count; i++) {
        char *existing_basename = g_path_get_basename(queue->files[i]);
        bool match = (strcmp(new_basename, existing_basename) == 0);
        g_free(existing_basename);
        
        if (match) {
            g_free(new_basename);
            return i;  // Return the index of the matching file
        }
    }
    
    g_free(new_basename);
    return -1;  // File not found in queue
}

// Remove all duplicate filenames from queue, keeping only the first occurrence
// Returns the number of duplicates removed
int deduplicate_queue(PlayQueue *queue) {
    if (!queue || queue->count <= 1) {
        return 0;
    }
    
    int duplicates_removed = 0;
    
    // Use a GHashTable to track seen basenames
    GHashTable *seen_files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    
    int write_index = 0;
    
    for (int read_index = 0; read_index < queue->count; read_index++) {
        char *basename = g_path_get_basename(queue->files[read_index]);
        
        if (!g_hash_table_contains(seen_files, basename)) {
            // First occurrence - keep this file
            g_hash_table_insert(seen_files, g_strdup(basename), GINT_TO_POINTER(1));
            queue->files[write_index] = queue->files[read_index];
            write_index++;
        } else {
            // Duplicate found - skip it
            g_free(queue->files[read_index]);
            duplicates_removed++;
            
            // Adjust current_index if needed
            if (read_index == queue->current_index) {
                // Currently playing item is being removed
                if (write_index < read_index) {
                    queue->current_index = write_index > 0 ? write_index - 1 : 0;
                }
            } else if (read_index < queue->current_index) {
                queue->current_index--;
            }
        }
        
        g_free(basename);
    }
    
    queue->count = write_index;
    g_hash_table_destroy(seen_files);
    
    return duplicates_removed;
}

// Count duplicate filenames in queue without removing them
// Returns the count of duplicates that would be removed
int count_queue_duplicates(PlayQueue *queue) {
    if (!queue || queue->count <= 1) {
        return 0;
    }
    
    GHashTable *seen_files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    int duplicate_count = 0;
    
    for (int i = 0; i < queue->count; i++) {
        char *basename = g_path_get_basename(queue->files[i]);
        
        if (g_hash_table_contains(seen_files, basename)) {
            duplicate_count++;
        } else {
            g_hash_table_insert(seen_files, g_strdup(basename), GINT_TO_POINTER(1));
        }
        
        g_free(basename);
    }
    
    g_hash_table_destroy(seen_files);
    return duplicate_count;
}

void on_queue_model_row_deleted(GtkTreeModel *model, GtkTreePath *path, gpointer user_data) {
    (void)model;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    gint *indices = gtk_tree_path_get_indices(path);
    pending_delete_index = indices[0];
    
    SDL_Log("Model row deleted at index: %d", pending_delete_index);
    
    // Store the file path before it gets removed
    if (pending_delete_index >= 0 && pending_delete_index < player->queue.count) {
        pending_move_file = player->queue.files[pending_delete_index];
    }
}

char* extract_audio_from_zip(const char *zip_path) {
    const char *audio_exts[] = { ".mp3", ".ogg", ".flac", ".wav", ".m4a" };
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) {
        return NULL;
    }

    int num_files = (int)mz_zip_reader_get_num_files(&zip);
    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) continue;

        const char *name = file_stat.m_filename;
        for (int j = 0; j < G_N_ELEMENTS(audio_exts); j++) {
            if (g_str_has_suffix(name, audio_exts[j])) {
                // Build cross-platform temp path
                std::string temp_dir = get_temp_directory_queue();
                std::string filename = fs::path(name).filename().string();
                std::string temp_path = (fs::path(temp_dir) / ("zenamp-" + filename)).string();

                void *data = mz_zip_reader_extract_to_heap(&zip, i, NULL, 0);
                if (data) {
                    g_file_set_contents(temp_path.c_str(), data, file_stat.m_uncomp_size, NULL);
                    mz_free(data);
                    mz_zip_reader_end(&zip);
                    return g_strdup(temp_path.c_str()); // Caller must g_free()
                }
            }
        }
    }

    mz_zip_reader_end(&zip);
    return NULL;
}


void on_queue_model_row_inserted(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data) {
    (void)iter;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    gint *indices = gtk_tree_path_get_indices(path);
    int insert_index = indices[0];
    
    SDL_Log("Model row inserted at index: %d (was at %d)", insert_index, pending_delete_index);
    
    if (pending_delete_index >= 0 && pending_move_file) {
        // Perform the actual queue reorder
        if (reorder_queue_item(&player->queue, pending_delete_index, insert_index)) {
            SDL_Log("Queue reordered: %d -> %d", pending_delete_index, insert_index);
            
            // Update only the play indicators, don't rebuild entire display
            GtkTreeIter temp_iter;
            gboolean valid = gtk_tree_model_get_iter_first(model, &temp_iter);
            int i = 0;
            
            while (valid) {
                const char *indicator = (i == player->queue.current_index) ? "▶" : "";
                gtk_list_store_set(player->queue_store, &temp_iter, COL_PLAYING, indicator, -1);
                valid = gtk_tree_model_iter_next(model, &temp_iter);
                i++;
            }
        }
        
        pending_delete_index = -1;
        pending_move_file = NULL;
    }
}


// Forward declaration of reorder function
bool reorder_queue_item(PlayQueue *queue, int from_index, int to_index) {
    if (from_index < 0 || from_index >= queue->count || 
        to_index < 0 || to_index >= queue->count || 
        from_index == to_index) {
        return false;
    }
    
    // Store the item being moved
    char *moving_item = queue->files[from_index];
    
    // Adjust current_index based on the move
    int new_current_index = queue->current_index;
    
    if (from_index == queue->current_index) {
        // Moving the currently playing item
        new_current_index = to_index;
    } else if (from_index < queue->current_index && to_index >= queue->current_index) {
        // Moving item from before current to after current
        new_current_index--;
    } else if (from_index > queue->current_index && to_index <= queue->current_index) {
        // Moving item from after current to before current
        new_current_index++;
    }
    
    // Perform the move
    if (from_index < to_index) {
        // Moving down: shift items up
        for (int i = from_index; i < to_index; i++) {
            queue->files[i] = queue->files[i + 1];
        }
    } else {
        // Moving up: shift items down
        for (int i = from_index; i > to_index; i--) {
            queue->files[i] = queue->files[i - 1];
        }
    }
    
    // Place the moved item in its new position
    queue->files[to_index] = moving_item;
    queue->current_index = new_current_index;
    
    return true;
}

void setup_queue_drag_and_drop(AudioPlayer *player) {
    GtkWidget *tree_view = player->queue_tree_view;
    
    // Enable reordering - this is the simple way for TreeView!
    // gtk_tree_view_set_reorderable() is deprecated (along with the rest of
    // GtkTreeView) but still functional in GTK4, and handles drag-to-reorder
    // entirely internally - no manual drag-and-drop wiring needed.
    //
    // NOTE(gtk4): the on_queue_drag_begin/data_get/data_received/end
    // functions that used to live here (using GtkTargetEntry/GdkDragContext/
    // GtkSelectionData, all removed entirely in GTK4) were confirmed dead
    // code - never connected to any signal, since reordering already works
    // through gtk_tree_view_set_reorderable() alone. Removed rather than
    // porting unused code against a fully-removed API. If manual drag-and-
    // drop is ever needed again, GTK4's replacements are GtkDragSource
    // ("prepare"/"drag-begin"/"drag-end") and GtkDropTarget ("drop") - see
    // audio_player.h's notes on the equivalent declarations.
    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(tree_view), TRUE);
    
    SDL_Log("Queue tree view set to reorderable");
}

void on_queue_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                           GtkTreeViewColumn *column, gpointer user_data) {
    (void)tree_view;
    (void)column;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;
    
    if (!gtk_tree_model_get_iter(model, &iter, path)) {
        return;
    }
    
    // Get the original queue index from the model
    int queue_index = -1;
    gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &queue_index, -1);
    
    if (queue_index < 0) {
        // Artist group header row (grouped view) - toggle expand/collapse
        // instead of trying to play it.
        if (gtk_tree_view_row_expanded(tree_view, path)) {
            gtk_tree_view_collapse_row(tree_view, path);
        } else {
            gtk_tree_view_expand_row(tree_view, path, FALSE);
        }
        return;
    }
    
    if (queue_index >= player->queue.count) {
        return;
    }
    
    SDL_Log("Queue row activated: original queue index %d", queue_index);
    
    // Get the filepath from the model to verify what file is actually being clicked
    char *filepath = NULL;
    gtk_tree_model_get(model, &iter, COL_FILEPATH, &filepath, -1);
    
    if (!filepath) {
        return;
    }
    
    // Check if already playing this exact file
    if (queue_index == player->queue.current_index && player->is_playing) {
        SDL_Log("Already playing this song");
        g_free(filepath);
        return;
    }
    
    // Only set current_index after we've verified it matches the filepath
    SDL_Log("Setting current_index to %d for file: %s", queue_index, filepath);
    
    stop_playback(player);
    player->queue.current_index = queue_index;
    
    if (load_file_from_queue(player)) {
        update_queue_display_minimal(player);  // Preserves karaoke checkmarks
        update_gui_state(player);
        start_playback(player);
        SDL_Log("Started playing: %s", get_current_queue_file(&player->queue));
        char *metadata = extract_metadata(get_current_queue_file(&player->queue));
        char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
        parse_metadata(metadata, title, artist, album, genre);
        if (!ends_with_zip(get_current_queue_file(&player->queue))) {
            show_track_info_overlay(player->visualizer, title, artist, album,
                get_file_duration(player->queue.files[player->queue.current_index]));
        }
        //SDL_Log("\n\n\nMy Queue %s %i\n\n", get_current_queue_file(&player->queue), !ends_with_zip(get_current_queue_file(&player->queue)));
        g_free(metadata);


    }
    
    g_free(filepath);
}

void update_queue_display(AudioPlayer *player) {
    if (player->queue_store) {
        gtk_list_store_clear(player->queue_store);
    }
    
    for (int i = 0; i < player->queue.count; i++) {
        GtkTreeIter iter;
        gtk_list_store_append(player->queue_store, &iter);
        
        char *metadata = NULL;
        int kfn_duration_seconds = -1;  // -1 = not a .kfn file

        const char *ext = strrchr(player->queue.files[i], '.');
        if (ext && strcasecmp(ext, ".zip") == 0) {
            char *extracted_path = extract_audio_from_zip(player->queue.files[i]);
            if (extracted_path) {
                metadata = extract_metadata(extracted_path);
                g_free(extracted_path);
            } else {
                metadata = g_strdup("No metadata available");
            }
        } else if (ext && strcasecmp(ext, ".kfn") == 0) {
            metadata = extract_kfn_metadata(player->queue.files[i]);
            if (!metadata || metadata[0] == '\0') {
                g_free(metadata);
                metadata = g_strdup("No metadata available");
            }
            kfn_duration_seconds = get_kfn_duration(player->queue.files[i]);
        } else {
            metadata = extract_metadata(player->queue.files[i]);
        }
        char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
        int duration_seconds = 0;
        
        parse_metadata(metadata, title, artist, album, genre);

        
        const char *duration_patterns[] = {
            "<b>Duration:</b>",
            "<b>Length:</b>",
            "Duration:",
            "Length:"
        };
        
        for (int j = 0; j < 4; j++) {
            const char *duration_start = strstr(metadata, duration_patterns[j]);
            if (duration_start) {
                duration_start = strchr(duration_start, ':');
                if (duration_start) {
                    duration_start++;
                    const char *time_start = strchr(duration_start, ':');
                    if (time_start) {
                        const char *scan = time_start - 1;
                        while (scan > duration_start && isdigit(*scan)) {
                            scan--;
                        }
                        scan++;
                        
                        int minutes = 0, seconds = 0;
                        if (sscanf(scan, "%d:%d", &minutes, &seconds) == 2) {
                            duration_seconds = minutes * 60 + seconds;
                            break;
                        }
                    }
                }
            }
        }
        
        if (kfn_duration_seconds >= 0) {
            duration_seconds = kfn_duration_seconds;
        }

        g_free(metadata);
        
        char *basename = g_path_get_basename(player->queue.files[i]);
        
        //const char *ext = strrchr(player->queue.files[i], '.');
        const char *cdgk_indicator = "";
        if (ext && (strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".kfn") == 0)) {
            cdgk_indicator = "✓";
        }
        
        char duration_str[16];
        if (duration_seconds > 0) {
            snprintf(duration_str, sizeof(duration_str), "%d:%02d", 
                     duration_seconds / 60, duration_seconds % 60);
        } else {
            strcpy(duration_str, "");
        }
        
        const char *indicator = (i == player->queue.current_index) ? "▶" : "";
        
        gtk_list_store_set(player->queue_store, &iter,
            COL_FILEPATH, player->queue.files[i],
            COL_PLAYING, indicator,
            COL_FILENAME, basename,
            COL_TITLE, title,
            COL_ARTIST, artist,
            COL_ALBUM, album,
            COL_GENRE, genre,
            COL_DURATION, duration_str,
            COL_CDGK, cdgk_indicator,
            COL_QUEUE_INDEX, i,
            -1);

        g_free(basename);
    }
    
    if (player->queue.current_index >= 0 && player->queue_tree_view) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(
            player->queue.current_index, -1);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(player->queue_tree_view),
                                    path, NULL, TRUE, 0.5, 0.0);
        GtkTreeSelection *selection = gtk_tree_view_get_selection(
            GTK_TREE_VIEW(player->queue_tree_view));
        gtk_tree_selection_select_path(selection, path);
        gtk_tree_path_free(path);
    }
}

void on_queue_delete_item(GtkWidget *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        // Get the actual queue index from COL_QUEUE_INDEX, not the visible row index
        int index = -1;
        gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &index, -1);
        
        if (index < 0 || index >= player->queue.count) {
            return;
        }
        
        SDL_Log("Removing item %d from queue", index);
        
        bool was_current_playing = (index == player->queue.current_index && player->is_playing);
        bool queue_will_be_empty = (player->queue.count <= 1);
        
        if (remove_from_queue(&player->queue, index)) {
            if (queue_will_be_empty) {
                stop_playback(player);
                player->is_loaded = false;
                gtk_label_set_text(GTK_LABEL(player->file_label), "No file loaded");
            } else if (was_current_playing) {
                stop_playback(player);
                if (load_file_from_queue(player)) {
                    update_gui_state(player);
                    start_playback(player);
                }
                if (player->cdg_display) {
                    cdg_reset(player->cdg_display);
                    player->cdg_display->packet_count = 0;
                    player->has_cdg = false;
                }
            }
            
            update_queue_display_with_filter(player, false);
            
            // Select the next item after deletion
            // If we deleted item at index N, the next item is now at index N (if it exists)
            // Otherwise select the previous item at index N-1
            int next_index = (index < player->queue.count) ? index : index - 1;
            if (next_index >= 0 && player->queue_tree_view) {
                GtkTreeIter next_iter;
                gboolean valid = gtk_tree_model_get_iter_first(
                    GTK_TREE_MODEL(player->queue_store), &next_iter);
                
                while (valid) {
                    int queue_index = -1;
                    gtk_tree_model_get(GTK_TREE_MODEL(player->queue_store), &next_iter,
                                       COL_QUEUE_INDEX, &queue_index, -1);
                    
                    if (queue_index == next_index) {
                        GtkTreePath *next_path = gtk_tree_model_get_path(
                            GTK_TREE_MODEL(player->queue_store), &next_iter);
                        gtk_tree_selection_select_path(selection, next_path);
                        gtk_tree_path_free(next_path);
                        break;
                    }
                    
                    valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(player->queue_store), &next_iter);
                }
            }
            
            update_gui_state(player);
        }
    }
}

// Rename a file on disk, handling .cdg paired files for karaoke
static bool rename_file_on_disk(const char *old_path, const char *new_filename) {
    if (!old_path || !new_filename || !new_filename[0]) {
        return false;
    }
    
    // Get directory and build new path
    char *dir = g_path_get_dirname(old_path);
    char *new_path = g_build_filename(dir, new_filename, NULL);
    g_free(dir);
    
    // Attempt to rename the file
    if (rename(old_path, new_path) != 0) {
        g_free(new_path);
        return false;
    }
    
    // If it's an audio file, check for matching .cdg file
    const char *old_ext = strrchr(old_path, '.');
    if (old_ext) {
        std::string lower_ext = old_ext;
        for (auto &c : lower_ext) c = tolower((unsigned char)c);
        bool is_audio = (lower_ext == ".ogg" || lower_ext == ".mp3" || lower_ext == ".wav" ||
                         lower_ext == ".m4a" || lower_ext == ".aac" || lower_ext == ".flac");
        
        if (is_audio) {
            // Build path to old .cdg file
            std::string old_cdg_path = old_path;
            size_t pos = old_cdg_path.rfind(old_ext);
            if (pos != std::string::npos) {
                old_cdg_path.replace(pos, strlen(old_ext), ".cdg");
                
                // Check if .cdg file exists
                if (fs::exists(old_cdg_path)) {
                    // Build path to new .cdg file
                    const char *new_ext = strrchr(new_filename, '.');
                    std::string new_cdg_filename = new_filename;
                    if (new_ext) {
                        size_t ext_pos = new_cdg_filename.rfind(new_ext);
                        if (ext_pos != std::string::npos) {
                            new_cdg_filename.replace(ext_pos, strlen(new_ext), ".cdg");
                        }
                    } else {
                        new_cdg_filename += ".cdg";
                    }
                    
                    char *dir2 = g_path_get_dirname(old_cdg_path.c_str());
                    char *new_cdg_path = g_build_filename(dir2, new_cdg_filename.c_str(), NULL);
                    g_free(dir2);
                    
                    // Rename the .cdg file
                    if (rename(old_cdg_path.c_str(), new_cdg_path) != 0) {
                        SDL_Log("Warning: Could not rename .cdg file from %s to %s", 
                               old_cdg_path.c_str(), new_cdg_path);
                    }
                    
                    g_free(new_cdg_path);
                }
            }
        }
    }
    
    g_free(new_path);
    return true;
}

// Dialog response handler for rename
// GTK4 removed gtk_dialog_run() - dialogs close themselves via "response"
// instead of blocking the caller. (File-local copy of the same helper used
// elsewhere - it's static/internal-linkage in each file, not shared across
// translation units.)
static void queue_destroy_dialog_on_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    (void)response_id;
    (void)user_data;
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_rename_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    if (response_id == GTK_RESPONSE_OK) {
        GtkEntry *entry = GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), "name_entry"));
        const char *old_path = (const char*)g_object_get_data(G_OBJECT(dialog), "old_path");
        
        // gtk_entry_get_text() is removed in GTK4 - use the GtkEditable
        // interface instead, which every text-entry widget implements.
        const char *new_filename = gtk_editable_get_text(GTK_EDITABLE(entry));
        
        if (new_filename && new_filename[0]) {
            if (rename_file_on_disk(old_path, new_filename)) {
                // Update queue with new path
                char *dir = g_path_get_dirname(old_path);
                char *new_path = g_build_filename(dir, new_filename, NULL);
                
                // Find and update the queue entry
                for (int i = 0; i < player->queue.count; i++) {
                    if (strcmp(player->queue.files[i], old_path) == 0) {
                        g_free(player->queue.files[i]);
                        player->queue.files[i] = g_strdup(new_path);
                        break;
                    }
                }
                
                g_free(dir);
                g_free(new_path);
                
                // Refresh display
                update_queue_display_with_filter(player, false);
                SDL_Log("File renamed successfully");
            } else {
                // GTK4 removed gtk_dialog_run() - show the error non-modally
                // and destroy it on response instead of blocking.
                GtkWidget *error_dialog = gtk_message_dialog_new(
                    GTK_WINDOW(dialog),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Failed to rename file. Check that the file exists and you have permission."
                );
                g_signal_connect(error_dialog, "response", G_CALLBACK(queue_destroy_dialog_on_response), NULL);
                gtk_window_present(GTK_WINDOW(error_dialog));
            }
        }
    }
    
    gtk_window_destroy(GTK_WINDOW(dialog));
}

void on_queue_rename_item(GtkWidget *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    
    char *filepath = NULL;
    gtk_tree_model_get(model, &iter, COL_FILEPATH, &filepath, -1);
    
    if (!filepath) {
        return;
    }
    
    // Extract just the filename for display
    char *basename = g_path_get_basename(filepath);
    
    // Create dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Rename File",
        GTK_WINDOW(player->window),
        GTK_DIALOG_MODAL,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Rename", GTK_RESPONSE_OK,
        NULL
    );
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget *label = gtk_label_new("New filename:");
    gtk_widget_set_margin_top(label, 5);
    gtk_widget_set_margin_bottom(label, 5);
    gtk_box_append(GTK_BOX(content_area), label);
    
    GtkWidget *entry = gtk_entry_new();
    // gtk_entry_set_text() is removed in GTK4 - use GtkEditable instead.
    gtk_editable_set_text(GTK_EDITABLE(entry), basename);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);  // Select all text
    gtk_widget_set_margin_top(entry, 5);
    gtk_widget_set_margin_bottom(entry, 5);
    gtk_box_append(GTK_BOX(content_area), entry);
    
    // Store data in dialog for handler. filepath's ownership transfers to
    // the dialog here (freed automatically via g_free when the dialog is
    // destroyed) since GTK4's async response flow means there's no single
    // point right after "showing" the dialog to free it anymore.
    g_object_set_data(G_OBJECT(dialog), "name_entry", entry);
    g_object_set_data_full(G_OBJECT(dialog), "old_path", filepath, g_free);
    
    // GTK4 widgets are visible by default - no gtk_widget_show_all() needed.
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_rename_response), player);
    
    // GTK4 removed gtk_dialog_run(); show non-modally instead of blocking.
    gtk_window_present(GTK_WINDOW(dialog));
    
    g_free(basename);
}

// Returns true if `filepath` is a format TagLib can read/write tags for
// directly. The karaoke wrapper formats (.zip/.kfn/.kar) aren't plain
// tagged audio files - .zip/.kfn are archives and .kar's metadata (if any)
// isn't something TagLib understands - so tag editing isn't offered for
// those.
static bool queue_file_supports_tag_editing(const char *filepath) {
    if (!filepath || filepath[0] == '\0') return false;
    const char *ext = strrchr(filepath, '.');
    if (!ext) return false;
    if (strcasecmp(ext, ".zip") == 0 || strcasecmp(ext, ".kfn") == 0 || strcasecmp(ext, ".kar") == 0) {
        return false;
    }
    return true;
}

// Writes title/artist/album/genre to filepath's tags via TagLib. Any field
// passed as an empty string clears that tag; NULL leaves it untouched.
// Returns false if TagLib couldn't open the file, it has no taggable
// stream, or the save failed (e.g. read-only file).
static bool write_tags_with_taglib(const char *filepath, const char *title, const char *artist,
                                    const char *album, const char *genre) {
    TagLib::FileRef file(filepath);
    if (file.isNull() || !file.tag()) {
        return false;
    }

    TagLib::Tag *tag = file.tag();
    if (title)  tag->setTitle(TagLib::String(title, TagLib::String::UTF8));
    if (artist) tag->setArtist(TagLib::String(artist, TagLib::String::UTF8));
    if (album)  tag->setAlbum(TagLib::String(album, TagLib::String::UTF8));
    if (genre)  tag->setGenre(TagLib::String(genre, TagLib::String::UTF8));

    return file.save();
}

static void on_edit_tags_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;

    if (response_id == GTK_RESPONSE_OK) {
        GtkEntry *title_entry  = GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), "title_entry"));
        GtkEntry *artist_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), "artist_entry"));
        GtkEntry *album_entry  = GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), "album_entry"));
        GtkEntry *genre_entry  = GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), "genre_entry"));
        const char *filepath = (const char*)g_object_get_data(G_OBJECT(dialog), "filepath");

        const char *title  = gtk_editable_get_text(GTK_EDITABLE(title_entry));
        const char *artist = gtk_editable_get_text(GTK_EDITABLE(artist_entry));
        const char *album  = gtk_editable_get_text(GTK_EDITABLE(album_entry));
        const char *genre  = gtk_editable_get_text(GTK_EDITABLE(genre_entry));

        if (write_tags_with_taglib(filepath, title, artist, album, genre)) {
            // Drop the cached metadata for this file so the next display
            // refresh re-reads what we just wrote, rather than the old
            // cached values or waiting on an mtime check that may not
            // notice (some filesystems have 1-second mtime resolution).
            {
                std::lock_guard<std::mutex> lock(g_queue_meta_mutex);
                g_queue_meta_cache.erase(filepath);
            }
            update_queue_display_with_filter(player, false);
            SDL_Log("Tags updated for: %s", filepath);
        } else {
            GtkWidget *error_dialog = gtk_message_dialog_new(
                GTK_WINDOW(dialog),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "Failed to save tags. The file format may not support tag editing, or it isn't writable."
            );
            g_signal_connect(error_dialog, "response", G_CALLBACK(queue_destroy_dialog_on_response), NULL);
            gtk_window_present(GTK_WINDOW(error_dialog));
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

void on_queue_edit_tags_item(GtkWidget *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }

    char *filepath = NULL;
    gtk_tree_model_get(model, &iter, COL_FILEPATH, &filepath, -1);
    if (!filepath || filepath[0] == '\0') {
        g_free(filepath);
        return;
    }

    if (!queue_file_supports_tag_editing(filepath)) {
        GtkWidget *info_dialog = gtk_message_dialog_new(
            GTK_WINDOW(player->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "Tag editing isn't supported for karaoke (.zip/.kfn/.kar) files."
        );
        g_signal_connect(info_dialog, "response", G_CALLBACK(queue_destroy_dialog_on_response), NULL);
        gtk_window_present(GTK_WINDOW(info_dialog));
        g_free(filepath);
        return;
    }

    // Pre-fill from whatever's already cached/displayed rather than
    // re-reading the file synchronously on the UI thread.
    bool needs_load = false;
    QueueMetaCacheEntry meta = get_cached_or_placeholder(filepath, &needs_load);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Edit Tags",
        GTK_WINDOW(player->window),
        GTK_DIALOG_MODAL,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Save", GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_start(content_area, 10);
    gtk_widget_set_margin_end(content_area, 10);
    gtk_widget_set_margin_top(content_area, 10);
    gtk_widget_set_margin_bottom(content_area, 10);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_box_append(GTK_BOX(content_area), grid);

    auto add_tag_row = [&](int row, const char *label_text, const char *value) -> GtkWidget* {
        GtkWidget *label = gtk_label_new(label_text);
        gtk_widget_set_halign(label, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

        GtkWidget *entry = gtk_entry_new();
        gtk_editable_set_text(GTK_EDITABLE(entry), value ? value : "");
        gtk_widget_set_hexpand(entry, TRUE);
        gtk_widget_set_size_request(entry, 260, -1);
        gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);
        return entry;
    };

    GtkWidget *title_entry  = add_tag_row(0, "Title:",  meta.loaded ? meta.title.c_str()  : "");
    GtkWidget *artist_entry = add_tag_row(1, "Artist:", meta.loaded ? meta.artist.c_str() : "");
    GtkWidget *album_entry  = add_tag_row(2, "Album:",  meta.loaded ? meta.album.c_str()  : "");
    GtkWidget *genre_entry  = add_tag_row(3, "Genre:",  meta.loaded ? meta.genre.c_str()  : "");

    g_object_set_data(G_OBJECT(dialog), "title_entry", title_entry);
    g_object_set_data(G_OBJECT(dialog), "artist_entry", artist_entry);
    g_object_set_data(G_OBJECT(dialog), "album_entry", album_entry);
    g_object_set_data(G_OBJECT(dialog), "genre_entry", genre_entry);
    g_object_set_data_full(G_OBJECT(dialog), "filepath", filepath, g_free);

    g_signal_connect(dialog, "response", G_CALLBACK(on_edit_tags_response), player);

    gtk_window_present(GTK_WINDOW(dialog));
}

// "button-press-event" -> GtkGestureClick's "pressed" signal. Both middle-
// click-to-delete and right-click context menu are handled here since both
// come through the same gesture (attached with "any button" in
// layout.cpp's create_queue_treeview()).
void on_queue_context_menu(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data) {
    (void)n_press;
    AudioPlayer *player = (AudioPlayer*)user_data;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    
    // Handle middle-click (button 2) - direct delete
    if (button == 2) {
        GtkTreePath *path;
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), 
                                         (gint)x, (gint)y, 
                                         &path, NULL, NULL, NULL)) {
            GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
            GtkTreeIter iter;
            if (!gtk_tree_model_get_iter(model, &iter, path)) {
                gtk_tree_path_free(path);
                return;
            }
            
            // Get the actual queue index, not the visible row index
            int index = -1;
            gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &index, -1);
            gtk_tree_path_free(path);
            
            if (index < 0 || index >= player->queue.count) {
                return;
            }
            
            SDL_Log("Removing item %d via middle-click", index);
            
            bool was_current_playing = (index == player->queue.current_index && player->is_playing);
            bool queue_will_be_empty = (player->queue.count <= 1);
            
            if (remove_from_queue(&player->queue, index)) {
                if (queue_will_be_empty) {
                    stop_playback(player);
                    player->is_loaded = false;
                    gtk_label_set_text(GTK_LABEL(player->file_label), "No file loaded");
                } else if (was_current_playing) {
                    stop_playback(player);
                    if (load_file_from_queue(player)) {
                        update_gui_state(player);
                        start_playback(player);
                    }
                    if (player->cdg_display) {
                        cdg_reset(player->cdg_display);
                        player->cdg_display->packet_count = 0;
                        player->has_cdg = false;
                    }
                }
                
                update_queue_display_with_filter(player, false);
                
                // Select the next item after deletion
                int next_index = (index < player->queue.count) ? index : index - 1;
                if (next_index >= 0 && player->queue_tree_view) {
                    GtkTreeModel *active_model = gtk_tree_view_get_model(GTK_TREE_VIEW(player->queue_tree_view));
                    GtkTreePath *next_path = nullptr;
                    if (active_model && find_tree_path_for_queue_index(active_model, next_index, &next_path)) {
                        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
                        gtk_tree_selection_select_path(selection, next_path);
                        gtk_tree_path_free(next_path);
                    }
                }
                
                update_gui_state(player);
            }
            
            return;
        }
    }
    
    // Handle right-click (button 3) - show context menu
    if (button == 3) {
        GtkTreePath *path;
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), 
                                         (gint)x, (gint)y, 
                                         &path, NULL, NULL, NULL)) {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
            gtk_tree_selection_select_path(selection, path);
            
            GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
            GtkTreeIter iter;
            if (!gtk_tree_model_get_iter(model, &iter, path)) {
                gtk_tree_path_free(path);
                return;
            }
            
            // Get the actual queue index, not the visible row index
            int index = -1;
            gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &index, -1);
            
            gtk_tree_path_free(path);
            
            if (index < 0 || index >= player->queue.count) {
                return;
            }
            
            // GtkMenu/GtkMenuItem are removed entirely in GTK4 - build a
            // small GtkPopover with plain buttons instead, positioned at the
            // click location.
            GtkWidget *popover = gtk_popover_new();
            gtk_widget_set_parent(popover, widget);
            GdkRectangle rect = {(int)x, (int)y, 1, 1};
            gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);

            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            gtk_widget_set_margin_start(box, 4);
            gtk_widget_set_margin_end(box, 4);
            gtk_widget_set_margin_top(box, 4);
            gtk_widget_set_margin_bottom(box, 4);
            gtk_popover_set_child(GTK_POPOVER(popover), box);

            GtkWidget *move_up_item = gtk_button_new_with_label("Move Up (Ctrl+↑)");
            gtk_button_set_has_frame(GTK_BUTTON(move_up_item), FALSE);
            gtk_widget_set_sensitive(move_up_item, index > 0);
            g_signal_connect(move_up_item, "clicked", G_CALLBACK(on_queue_move_up), player);
            g_signal_connect_swapped(move_up_item, "clicked", G_CALLBACK(gtk_popover_popdown), popover);
            gtk_box_append(GTK_BOX(box), move_up_item);

            GtkWidget *move_down_item = gtk_button_new_with_label("Move Down (Ctrl+↓)");
            gtk_button_set_has_frame(GTK_BUTTON(move_down_item), FALSE);
            gtk_widget_set_sensitive(move_down_item, index < player->queue.count - 1);
            g_signal_connect(move_down_item, "clicked", G_CALLBACK(on_queue_move_down), player);
            g_signal_connect_swapped(move_down_item, "clicked", G_CALLBACK(gtk_popover_popdown), popover);
            gtk_box_append(GTK_BOX(box), move_down_item);

            gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

            GtkWidget *rename_item = gtk_button_new_with_label("Rename File...");
            gtk_button_set_has_frame(GTK_BUTTON(rename_item), FALSE);
            g_signal_connect(rename_item, "clicked", G_CALLBACK(on_queue_rename_item), player);
            g_signal_connect_swapped(rename_item, "clicked", G_CALLBACK(gtk_popover_popdown), popover);
            gtk_box_append(GTK_BOX(box), rename_item);

            GtkWidget *edit_tags_item = gtk_button_new_with_label("Edit Tags...");
            gtk_button_set_has_frame(GTK_BUTTON(edit_tags_item), FALSE);
            gtk_widget_set_sensitive(edit_tags_item,
                index >= 0 && index < player->queue.count &&
                queue_file_supports_tag_editing(player->queue.files[index]));
            g_signal_connect(edit_tags_item, "clicked", G_CALLBACK(on_queue_edit_tags_item), player);
            g_signal_connect_swapped(edit_tags_item, "clicked", G_CALLBACK(gtk_popover_popdown), popover);
            gtk_box_append(GTK_BOX(box), edit_tags_item);

            gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

            GtkWidget *delete_item = gtk_button_new_with_label("Remove from Queue");
            gtk_button_set_has_frame(GTK_BUTTON(delete_item), FALSE);
            g_signal_connect(delete_item, "clicked", G_CALLBACK(on_queue_delete_item), player);
            g_signal_connect_swapped(delete_item, "clicked", G_CALLBACK(gtk_popover_popdown), popover);
            gtk_box_append(GTK_BOX(box), delete_item);

            // Detach the popover from its parent once closed, since it was
            // parented here manually rather than owned by a persistent widget.
            g_signal_connect(popover, "closed", G_CALLBACK(gtk_widget_unparent), NULL);

            gtk_popover_popup(GTK_POPOVER(popover));
            
            return;
        }
    }
}

void move_queue_item_up(AudioPlayer *player, int index) {
    if (index <= 0 || index >= player->queue.count) {
        return;
    }
    
    char *temp = player->queue.files[index];
    player->queue.files[index] = player->queue.files[index - 1];
    player->queue.files[index - 1] = temp;
    
    if (player->queue.current_index == index) {
        player->queue.current_index = index - 1;
    } else if (player->queue.current_index == index - 1) {
        player->queue.current_index = index;
    }
    
    update_queue_display_with_filter(player, false);
    
    if (player->queue_tree_view) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(index - 1, -1);
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
        gtk_tree_selection_select_path(selection, path);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(player->queue_tree_view), path, NULL, FALSE, 0.0, 0.0);
        gtk_tree_path_free(path);
    }
}

void move_queue_item_down(AudioPlayer *player, int index) {
    if (index < 0 || index >= player->queue.count - 1) {
        return;
    }
    
    char *temp = player->queue.files[index];
    player->queue.files[index] = player->queue.files[index + 1];
    player->queue.files[index + 1] = temp;
    
    if (player->queue.current_index == index) {
        player->queue.current_index = index + 1;
    } else if (player->queue.current_index == index + 1) {
        player->queue.current_index = index;
    }
    
    update_queue_display_with_filter(player, false);
    
    if (player->queue_tree_view) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(index + 1, -1);
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
        gtk_tree_selection_select_path(selection, path);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(player->queue_tree_view), path, NULL, FALSE, 0.0, 0.0);
        gtk_tree_path_free(path);
    }
}

void on_queue_move_up(GtkWidget *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
        gint *indices = gtk_tree_path_get_indices(path);
        int index = indices[0];
        gtk_tree_path_free(path);
        
        move_queue_item_up(player, index);
    }
}

void on_queue_move_down(GtkWidget *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
        gint *indices = gtk_tree_path_get_indices(path);
        int index = indices[0];
        gtk_tree_path_free(path);
        
        move_queue_item_down(player, index);
    }
}

// "key-press-event" -> GtkEventControllerKey's "key-pressed" signal.
gboolean on_queue_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    (void)keycode;
    AudioPlayer *player = (AudioPlayer*)user_data;
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return FALSE;
    }
    
    // Ctrl+Up/Down moves by raw tree-path position, which only lines up
    // with the queue order in the flat view - reordering doesn't have a
    // well-defined meaning while grouped (see set_queue_group_mode).
    if (player->queue_group_mode != QUEUE_GROUP_NONE) {
        return FALSE;
    }
    
    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
    gint *indices = gtk_tree_path_get_indices(path);
    int index = indices[0];
    gtk_tree_path_free(path);
    
    if (state & GDK_CONTROL_MASK) {
        if (keyval == GDK_KEY_Up) {
            move_queue_item_up(player, index);
            return TRUE;
        } else if (keyval == GDK_KEY_Down) {
            move_queue_item_down(player, index);
            return TRUE;
        }
    }
    
    return FALSE;
}

static gboolean apply_queue_filter_delayed(gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    // Clear the timeout ID
    player->queue_filter_timeout_id = 0;
    
    const char *filter_text = gtk_editable_get_text(GTK_EDITABLE(player->queue_search_entry));
    strncpy(player->queue_filter_text, filter_text, sizeof(player->queue_filter_text) - 1);
    player->queue_filter_text[sizeof(player->queue_filter_text) - 1] = '\0';
    
    SDL_Log("Applying queue filter: '%s'", player->queue_filter_text);
    
    // Refilter the tree view
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(player->queue_tree_view));
    if (model) {
        // If we have a filter, we need to rebuild with filtering
        // For now, let's just update the display
        update_queue_display_with_filter(player);
    }
    
    return G_SOURCE_REMOVE;
}

static void on_queue_search_changed(GtkEntry *entry, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    // Remove existing timeout if any
    if (player->queue_filter_timeout_id != 0) {
        g_source_remove(player->queue_filter_timeout_id);
    }
    
    // Set new timeout for 500ms
    player->queue_filter_timeout_id = g_timeout_add(500, apply_queue_filter_delayed, player);
}

// GTK4's GtkSearchEntry has a built-in clear icon and fires "stop-search"
// when it's clicked (or Escape is pressed) - replaces the old manual
// icon-press handling, which also assumed GtkSearchEntry was a GtkEntry
// subclass (it isn't anymore in GTK4).
static void on_queue_search_stop(GtkSearchEntry *entry, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    gtk_editable_set_text(GTK_EDITABLE(entry), "");
    player->queue_filter_text[0] = '\0';
    
    // Remove any pending timeout
    if (player->queue_filter_timeout_id != 0) {
        g_source_remove(player->queue_filter_timeout_id);
        player->queue_filter_timeout_id = 0;
    }
    
    // Full update to refresh all items with checkmarks
    update_queue_display_with_filter(player);
}

static void on_queue_group_dropdown_changed(GObject *dropdown, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    AudioPlayer *player = (AudioPlayer*)user_data;

    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));
    QueueGroupMode mode = QUEUE_GROUP_NONE;
    switch (selected) {
        case 1: mode = QUEUE_GROUP_ARTIST; break;
        case 2: mode = QUEUE_GROUP_ALBUM;  break;
        case 3: mode = QUEUE_GROUP_GENRE;  break;
        default: mode = QUEUE_GROUP_NONE;  break;
    }
    set_queue_group_mode(player, mode);
}

GtkWidget* create_queue_search_bar(AudioPlayer *player) {
    GtkWidget *bar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    GtkWidget *search_entry = gtk_search_entry_new();
    // NOTE(gtk4): GtkSearchEntry no longer inherits from GtkEntry in GTK4 -
    // GTK_ENTRY(search_entry) is an invalid cast now, so placeholder text
    // goes through the generic "placeholder-text" GObject property instead
    // (works for both GtkEntry and GtkSearchEntry).
    g_object_set(search_entry, "placeholder-text", "Filter queue...", NULL);
    
    // GtkSearchEntry has a built-in clear icon in GTK4 - no manual
    // gtk_entry_set_icon_*() calls needed (those were GtkEntry-only anyway).
    
    player->queue_search_entry = search_entry;
    player->queue_filter_timeout_id = 0;
    player->queue_filter_text[0] = '\0';
    
    // Connect signals
    g_signal_connect(search_entry, "changed", 
                     G_CALLBACK(on_queue_search_changed), player);
    g_signal_connect(search_entry, "stop-search",
                     G_CALLBACK(on_queue_search_stop), player);
    
    gtk_widget_set_hexpand(search_entry, TRUE);
    gtk_box_append(GTK_BOX(bar_box), search_entry);

    // "Group by" selector: None / Artist / Album / Genre. Index order here
    // must match the QUEUE_GROUP_* enum in audio_player.h.
    const char *group_options[] = {"None", "Artist", "Album", "Genre", NULL};
    GtkWidget *group_dropdown = gtk_drop_down_new_from_strings(group_options);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(group_dropdown), 0);
    player->queue_group_dropdown = group_dropdown;
    g_signal_connect(group_dropdown, "notify::selected",
                     G_CALLBACK(on_queue_group_dropdown_changed), player);
    gtk_box_append(GTK_BOX(bar_box), group_dropdown);

    return bar_box;
}

bool matches_filter(const char *text, const char *filter) {
    if (!filter || filter[0] == '\0') {
        return true;  // Empty filter matches everything
    }
    
    // Case-insensitive search
    char *text_lower = g_utf8_strdown(text, -1);
    char *filter_lower = g_utf8_strdown(filter, -1);
    
    bool matches = (strstr(text_lower, filter_lower) != NULL);
    
    g_free(text_lower);
    g_free(filter_lower);
    
    return matches;
}

// ---------------------------------------------------------------------------
// Grouped-by-artist queue view
//
// Selected via the "Group by" dropdown next to the queue filter. When
// active, the queue tree view's model is switched from the flat
// player->queue_store (GtkListStore) to player->queue_store_grouped
// (GtkTreeStore): one collapsible parent row per artist, tracks as
// children. player->queue.files itself is never reordered - grouping is
// purely a display transform, built fresh from the same per-file metadata
// cache the flat view uses. Drag-to-reorder only makes sense against the
// flat order, so it's disabled while grouped.
//
// next_song()/previous_song() (in zenamp_main.cpp) walk whichever model is
// currently attached to the tree view via get_queue_display_order() below,
// so playback follows the grouped order shown here once grouping is
// on, and falls back to plain queue order once it's off.
// ---------------------------------------------------------------------------

// Returns every row's queue index (player->queue.files[] index) in current
// display order - depth-first, so it naturally covers both the flat list
// (siblings only) and the grouped tree (each artist's children in turn).
// Group header rows (COL_QUEUE_INDEX == -1) are skipped. Visits every row
// in the model regardless of expand/collapse state, since collapsed groups
// should still take part in playback order.
std::vector<int> get_queue_display_order(AudioPlayer *player) {
    std::vector<int> order;
    if (!player || !player->queue_tree_view) return order;

    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(player->queue_tree_view));
    if (!model) return order;

    gtk_tree_model_foreach(model,
        [](GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) -> gboolean {
            (void)path;
            auto *out = (std::vector<int> *)data;
            int queue_index = -1;
            gtk_tree_model_get(model, iter, COL_QUEUE_INDEX, &queue_index, -1);
            if (queue_index >= 0) out->push_back(queue_index);
            return FALSE;  // keep going
        },
        &order);

    return order;
}

// Finds the tree path of the row whose COL_QUEUE_INDEX matches
// `queue_index` in `model` (flat or grouped). Caller must gtk_tree_path_free
// the result.
static bool find_tree_path_for_queue_index(GtkTreeModel *model, int queue_index, GtkTreePath **out_path) {
    struct Ctx { int target; GtkTreePath *found; } ctx{queue_index, nullptr};

    gtk_tree_model_foreach(model,
        [](GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) -> gboolean {
            auto *ctx = (Ctx *)data;
            int qi = -1;
            gtk_tree_model_get(model, iter, COL_QUEUE_INDEX, &qi, -1);
            if (qi == ctx->target) {
                ctx->found = gtk_tree_path_copy(path);
                return TRUE;  // stop
            }
            return FALSE;
        },
        &ctx);

    if (!ctx.found) return false;
    *out_path = ctx.found;
    return true;
}

// Makes sure the tree view is showing whichever store matches the current
// mode. Cheap no-op if it's already correct.
static void ensure_queue_tree_view_model(AudioPlayer *player) {
    if (!player->queue_tree_view) return;

    GtkTreeModel *want = (player->queue_group_mode != QUEUE_GROUP_NONE)
        ? GTK_TREE_MODEL(player->queue_store_grouped)
        : GTK_TREE_MODEL(player->queue_store);
    GtkTreeModel *have = gtk_tree_view_get_model(GTK_TREE_VIEW(player->queue_tree_view));

    if (have != want) {
        gtk_tree_view_set_model(GTK_TREE_VIEW(player->queue_tree_view), want);
    }
}

struct QueueGroupBucket {
    std::string label;
    std::string label_lower;
    std::vector<int> queue_indices;  // original queue.files order, filtered
};

// Pulls the grouping key out of a metadata entry for the given mode, with
// the same "Unknown X" / "(Loading…)" fallback convention used elsewhere
// in this file.
static std::string queue_group_key_for_meta(const QueueMetaCacheEntry &meta, QueueGroupMode mode) {
    if (!meta.loaded) return "(Loading…)";

    switch (mode) {
        case QUEUE_GROUP_ALBUM:
            return meta.album.empty() ? "Unknown Album" : meta.album;
        case QUEUE_GROUP_GENRE:
            return meta.genre.empty() ? "Unknown Genre" : meta.genre;
        case QUEUE_GROUP_ARTIST:
        default:
            return meta.artist.empty() ? "Unknown Artist" : meta.artist;
    }
}

static void update_queue_display_grouped(AudioPlayer *player, bool scroll_to_current) {
    if (!player->queue_store_grouped) return;

    gtk_tree_store_clear(player->queue_store_grouped);

    const char *filter = player->queue_filter_text;
    bool has_filter = (filter && filter[0] != '\0');
    QueueGroupMode mode = player->queue_group_mode;

    std::vector<QueueGroupBucket> buckets;
    std::unordered_map<std::string, size_t> bucket_for_key;
    std::vector<std::string> pending_paths;

    for (int i = 0; i < player->queue.count; i++) {
        const char *filepath = player->queue.files[i];

        bool needs_load = false;
        QueueMetaCacheEntry meta = get_cached_or_placeholder(filepath, &needs_load);
        if (needs_load) pending_paths.push_back(filepath);

        char *basename_c = g_path_get_basename(filepath);
        std::string basename = basename_c;
        g_free(basename_c);

        bool matches = true;
        if (has_filter) {
            matches = matches_filter(basename.c_str(), filter);
            if (!matches && meta.loaded) {
                matches = matches_filter(meta.title.c_str(), filter) ||
                          matches_filter(meta.artist.c_str(), filter) ||
                          matches_filter(meta.album.c_str(), filter) ||
                          matches_filter(meta.genre.c_str(), filter);
            }
        }
        if (!matches) continue;

        std::string group_key = queue_group_key_for_meta(meta, mode);
        std::string key_lower = group_key;
        for (auto &c : key_lower) c = tolower((unsigned char)c);

        auto it = bucket_for_key.find(key_lower);
        size_t bucket_idx;
        if (it == bucket_for_key.end()) {
            bucket_idx = buckets.size();
            buckets.push_back(QueueGroupBucket{group_key, key_lower, {}});
            bucket_for_key[key_lower] = bucket_idx;
        } else {
            bucket_idx = it->second;
        }
        buckets[bucket_idx].queue_indices.push_back(i);
    }

    // Alphabetical by group label (case-insensitive); files still awaiting
    // metadata sit in a "(Loading…)" group pinned last so they don't jump
    // around as extraction finishes in the background.
    std::stable_sort(buckets.begin(), buckets.end(), [](const QueueGroupBucket &a, const QueueGroupBucket &b) {
        bool a_loading = (a.label == "(Loading…)");
        bool b_loading = (b.label == "(Loading…)");
        if (a_loading != b_loading) return b_loading;
        return a.label_lower < b.label_lower;
    });

    for (const auto &bucket : buckets) {
        GtkTreeIter parent_iter;
        gtk_tree_store_append(player->queue_store_grouped, &parent_iter, NULL);

        char group_label[300];
        snprintf(group_label, sizeof(group_label), "%s (%zu)",
                 bucket.label.c_str(), bucket.queue_indices.size());

        gtk_tree_store_set(player->queue_store_grouped, &parent_iter,
            COL_FILEPATH, "",
            COL_PLAYING, "",
            COL_FILENAME, group_label,
            COL_TITLE, "",
            COL_ARTIST, "",
            COL_ALBUM, "",
            COL_GENRE, "",
            COL_DURATION, "",
            COL_CDGK, "",
            COL_QUEUE_INDEX, -1,
            -1);

        bool group_has_current = false;

        for (int i : bucket.queue_indices) {
            const char *filepath = player->queue.files[i];
            bool needs_load = false;
            QueueMetaCacheEntry meta = get_cached_or_placeholder(filepath, &needs_load);

            char *basename = g_path_get_basename(filepath);

            GtkTreeIter child_iter;
            gtk_tree_store_append(player->queue_store_grouped, &child_iter, &parent_iter);

            char duration_str[16] = "";
            if (meta.loaded) {
                if (meta.duration_seconds > 0) {
                    snprintf(duration_str, sizeof(duration_str), "%d:%02d",
                             meta.duration_seconds / 60, meta.duration_seconds % 60);
                }
            } else {
                strcpy(duration_str, "…");
            }

            const char *cdgk_indicator = meta.is_karaoke ? "✓" : "";
            const char *indicator = (i == player->queue.current_index) ? "▶" : "";
            if (indicator[0]) group_has_current = true;

            const char *accessibility_indicator = (meta.loaded && !meta.file_accessible) ? "⚠ " : "";
            char display_filename[512];
            snprintf(display_filename, sizeof(display_filename), "%s%s",
                     accessibility_indicator, basename);

            gtk_tree_store_set(player->queue_store_grouped, &child_iter,
                COL_FILEPATH, filepath,
                COL_PLAYING, indicator,
                COL_FILENAME, display_filename,
                COL_TITLE, meta.loaded ? meta.title.c_str() : "",
                COL_ARTIST, meta.loaded ? meta.artist.c_str() : "",
                COL_ALBUM, meta.loaded ? meta.album.c_str() : "",
                COL_GENRE, meta.loaded ? meta.genre.c_str() : "",
                COL_DURATION, duration_str,
                COL_CDGK, cdgk_indicator,
                COL_QUEUE_INDEX, i,
                -1);

            g_free(basename);
        }

        if (group_has_current && player->queue_tree_view) {
            GtkTreePath *ppath = gtk_tree_model_get_path(
                GTK_TREE_MODEL(player->queue_store_grouped), &parent_iter);
            gtk_tree_view_expand_row(GTK_TREE_VIEW(player->queue_tree_view), ppath, FALSE);
            gtk_tree_path_free(ppath);
        }
    }

    queue_metadata_loader_start(player, std::move(pending_paths));

    if (scroll_to_current && player->queue.current_index >= 0 && player->queue_tree_view) {
        GtkTreePath *path = nullptr;
        if (find_tree_path_for_queue_index(GTK_TREE_MODEL(player->queue_store_grouped),
                                            player->queue.current_index, &path)) {
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(player->queue_tree_view), path, NULL, TRUE, 0.5, 0.0);
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_path_free(path);
        }
    }
}

// Switches between the flat queue view and grouping by artist/album/genre.
// Disables drag-to-reorder while grouped, since manual reordering only has
// a well-defined meaning against the flat order.
void set_queue_group_mode(AudioPlayer *player, QueueGroupMode mode) {
    if (!player || player->queue_group_mode == mode) return;

    player->queue_group_mode = mode;

    if (player->queue_tree_view) {
        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(player->queue_tree_view), mode == QUEUE_GROUP_NONE);
    }

    update_queue_display_with_filter(player, true);
}

static void update_queue_display_flat(AudioPlayer *player, bool scroll_to_current) {
    // Save current scroll position before clearing
    int saved_queue_index = -1;
    int saved_tree_row = -1;  // Also save the visual row position
    
    if (!scroll_to_current && player->queue_tree_view) {
        // Get the first visible row in the current view
        GtkTreePath *start_path = NULL;
        GtkTreePath *end_path = NULL;
        
        if (gtk_tree_view_get_visible_range(GTK_TREE_VIEW(player->queue_tree_view), &start_path, &end_path)) {
            if (start_path) {
                GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(player->queue_tree_view));
                GtkTreeIter iter;
                if (gtk_tree_model_get_iter(model, &iter, start_path)) {
                    // Get the queue index of the first visible item
                    gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &saved_queue_index, -1);
                    
                    // Also save the tree row index as fallback
                    gint *indices = gtk_tree_path_get_indices(start_path);
                    if (indices) {
                        saved_tree_row = indices[0];
                    }
                }
            }
            if (start_path) gtk_tree_path_free(start_path);
            if (end_path) gtk_tree_path_free(end_path);
        }
    }

    if (player->queue_store) {
        gtk_list_store_clear(player->queue_store);
    }

    const char *filter = player->queue_filter_text;
    bool has_filter = (filter && filter[0] != '\0');

    int visible_count = 0;
    std::vector<std::string> pending_paths;  // files with no (fresh) cached metadata yet

    for (int i = 0; i < player->queue.count; i++) {
        const char *filepath = player->queue.files[i];

        bool needs_load = false;
        QueueMetaCacheEntry meta = get_cached_or_placeholder(filepath, &needs_load);
        if (needs_load) {
            pending_paths.push_back(filepath);
        }

        char *basename = g_path_get_basename(filepath);

        // Files still awaiting extraction only have a filename to match on;
        // once their metadata loads, the next refresh will re-filter them
        // against title/artist/album/genre too, so nothing stays hidden.
        bool matches = true;
        if (has_filter) {
            matches = matches_filter(basename, filter);
            if (!matches && meta.loaded) {
                matches = matches_filter(meta.title.c_str(), filter) ||
                          matches_filter(meta.artist.c_str(), filter) ||
                          matches_filter(meta.album.c_str(), filter) ||
                          matches_filter(meta.genre.c_str(), filter);
            }
        }

        if (matches) {
            GtkTreeIter iter;
            gtk_list_store_append(player->queue_store, &iter);

            char duration_str[16] = "";
            if (meta.loaded) {
                if (meta.duration_seconds > 0) {
                    snprintf(duration_str, sizeof(duration_str), "%d:%02d",
                             meta.duration_seconds / 60, meta.duration_seconds % 60);
                }
            } else {
                strcpy(duration_str, "…");
            }

            const char *cdgk_indicator = meta.is_karaoke ? "✓" : "";
            const char *indicator = (i == player->queue.current_index) ? "▶" : "";

            // Visual indicator for inaccessible files (only known once loaded)
            const char *accessibility_indicator = (meta.loaded && !meta.file_accessible) ? "⚠ " : "";
            char display_filename[512];
            snprintf(display_filename, sizeof(display_filename), "%s%s",
                     accessibility_indicator, basename);

            gtk_list_store_set(player->queue_store, &iter,
                COL_FILEPATH, filepath,
                COL_PLAYING, indicator,
                COL_FILENAME, display_filename,
                COL_TITLE, meta.loaded ? meta.title.c_str() : "",
                COL_ARTIST, meta.loaded ? meta.artist.c_str() : "",
                COL_ALBUM, meta.loaded ? meta.album.c_str() : "",
                COL_GENRE, meta.loaded ? meta.genre.c_str() : "",
                COL_DURATION, duration_str,
                COL_CDGK, cdgk_indicator,
                COL_QUEUE_INDEX, i,
                -1);

            visible_count++;
        }

        g_free(basename);
    }

    // Metadata/duration extraction for anything not (freshly) cached happens
    // off the main thread; queue_metadata_loader_start() is a no-op if a
    // loader is already in flight, so rapid-fire calls (e.g. filter typing)
    // don't spawn overlapping extraction threads.
    queue_metadata_loader_start(player, std::move(pending_paths));

    // Restore scroll position or scroll to current
    if (scroll_to_current && player->queue.current_index >= 0 && player->queue_tree_view) {
        // Scroll to currently playing item
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first(
            GTK_TREE_MODEL(player->queue_store), &iter);

        while (valid) {
            int queue_index = -1;
            gtk_tree_model_get(GTK_TREE_MODEL(player->queue_store), &iter,
                               COL_QUEUE_INDEX, &queue_index, -1);

            if (queue_index == player->queue.current_index) {
                GtkTreePath *path = gtk_tree_model_get_path(
                    GTK_TREE_MODEL(player->queue_store), &iter);
                gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(player->queue_tree_view),
                                             path, NULL, TRUE, 0.5, 0.0);
                GtkTreeSelection *selection = gtk_tree_view_get_selection(
                    GTK_TREE_VIEW(player->queue_tree_view));
                gtk_tree_selection_select_path(selection, path);
                gtk_tree_path_free(path);
                break;
            }

            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(player->queue_store), &iter);
        }
    } else if (!scroll_to_current && player->queue_tree_view) {
        // Try to restore to saved queue index first
        if (saved_queue_index >= 0) {
            GtkTreeIter iter;
            gboolean valid = gtk_tree_model_get_iter_first(
                GTK_TREE_MODEL(player->queue_store), &iter);
            int visible_row = 0;

            while (valid) {
                int queue_index = -1;
                gtk_tree_model_get(GTK_TREE_MODEL(player->queue_store), &iter,
                                   COL_QUEUE_INDEX, &queue_index, -1);

                if (queue_index == saved_queue_index) {
                    GtkTreePath *path = gtk_tree_model_get_path(
                        GTK_TREE_MODEL(player->queue_store), &iter);
                    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(player->queue_tree_view),
                                                 path, NULL, FALSE, 0.0, 0.0);
                    gtk_tree_path_free(path);
                    return;  // SUCCESS - found and scrolled
                }

                valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(player->queue_store), &iter);
                visible_row++;
            }
        }
        
        // Fallback: if we couldn't find the saved queue index, scroll to the saved tree row
        // This handles the case where the item was deleted
        if (saved_tree_row >= 0 && visible_count > 0) {
            GtkTreePath *fallback_path = gtk_tree_path_new_from_indices(
                saved_tree_row < visible_count ? saved_tree_row : visible_count - 1, -1);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(player->queue_tree_view),
                                         fallback_path, NULL, FALSE, 0.0, 0.0);
            gtk_tree_path_free(fallback_path);
        }
    }
}

// Public entry point used everywhere else in the app - dispatches to the
// flat or grouped renderer depending on the current view mode, and makes
// sure the tree view is showing the matching store.
void update_queue_display_with_filter(AudioPlayer *player, bool scroll_to_current) {
    ensure_queue_tree_view_model(player);

    if (player->queue_group_mode != QUEUE_GROUP_NONE) {
        update_queue_display_grouped(player, scroll_to_current);
    } else {
        update_queue_display_flat(player, scroll_to_current);
    }
}

// Cleanup function to call on exit
void cleanup_queue_filter(AudioPlayer *player) {
    if (player->queue_filter_timeout_id != 0) {
        g_source_remove(player->queue_filter_timeout_id);
        player->queue_filter_timeout_id = 0;
    }
}
