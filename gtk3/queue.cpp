#include "audio_player.h"
#include "miniz.h"
#include "kfn.h"
#include <glib.h>
#include <string.h>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <ctime>
#include <sys/stat.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Queue metadata cache + background loader
//
// update_queue_display_with_filter() used to extract tag metadata and decode
// duration for EVERY file in the queue, synchronously, on the GTK main
// thread, every time it ran (startup, filter keystrokes, add/remove, ...).
// For large queues (thousands of files) that meant thousands of file
// opens/decodes/zip-extractions blocking the UI thread at once - the app
// would freeze on startup and on every keystroke in the filter box.
//
// Fix: cache extracted metadata per filepath (invalidated by mtime), and
// only ever do the actual extraction work on a background thread. The
// display always renders immediately from whatever's in the cache; files
// not yet cached show a "(Loading...)" placeholder that gets refreshed as
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
    // Last time we actually confirmed (via a successful stat()) that this
    // file exists - NOT the same as "last used this session". Only touched
    // when the file is verified present, so a temporarily-unmounted
    // external/network drive's entries just stop advancing rather than
    // looking stale; they pick back up the next time the drive is back and
    // the file gets looked at again. See queue_cache_max_age_seconds below.
    time_t last_seen = 0;
    bool loaded = false;  // false = placeholder only, not yet extracted
};

// How long a cache entry can go without being confirmed present before
// save_queue_metadata_cache_to_disk() drops it instead of persisting it.
// Generous on purpose: a removable/network drive that's only plugged in
// occasionally should easily clear this bar, while files that are
// genuinely gone for good (deleted, drive retired) eventually age out on
// their own without needing any manual "clear cache" step.
static const time_t queue_cache_max_age_seconds = 60 * 60 * 24 * 365;  // ~1 year

static std::mutex g_queue_meta_mutex;
static std::unordered_map<std::string, QueueMetaCacheEntry> g_queue_meta_cache;
static std::atomic<bool> g_queue_meta_loader_running{false};

// Coalesces display-refresh requests from the background metadata loader.
// update_queue_display_with_filter() does a full O(queue size) rebuild of
// the tree store - it's not cheap. Firing one unconditionally every 200
// files is fine on a small queue but on a 30k-file import can extract
// metadata for 200 files faster than the main thread can finish one
// rebuild, so idle-callback requests would pile up faster than they drain.
// Gating on this flag means at most one refresh is ever in flight: the next
// request is a no-op until the previous refresh has actually run, so the
// redraw rate self-throttles to whatever the main thread can keep up with
// instead of to how fast the background thread can chew through files.
static std::atomic<bool> g_queue_display_refresh_pending{false};

static void queue_metadata_loader_request_refresh(AudioPlayer *player) {
    bool expected = false;
    if (!g_queue_display_refresh_pending.compare_exchange_strong(expected, true)) {
        return;  // a refresh is already queued/running - it'll pick up everything done so far
    }
    g_idle_add([](gpointer data) -> gboolean {
        AudioPlayer *p = (AudioPlayer *)data;
        if (p) update_queue_display_with_filter(p, false);
        g_queue_display_refresh_pending = false;
        return FALSE;
    }, player);
}

// Forward declarations - these are defined further down in this file, but
// extract_queue_item_metadata() (used by the background loader above the
// point they're defined) needs them.
static char *extract_kfn_metadata(const char *kfn_path);
static int get_kfn_duration(const char *kfn_path);
char* extract_audio_from_zip(const char *zip_path);

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
        result.last_seen = time(nullptr);  // confirmed present right now
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
            printf("Warning: ZIP file not accessible: %s\n", filepath);
        }
    } else if (ext && strcasecmp(ext, ".kfn") == 0) {
        metadata = extract_kfn_metadata(filepath);

        if (!metadata || strlen(metadata) == 0) {
            g_free(metadata);
            metadata = g_strdup("(File not accessible)");
            duration_seconds = 0;
            file_accessible = false;
            printf("Warning: KFN file not accessible: %s\n", filepath);
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
            printf("Warning: File not accessible: %s\n", filepath);
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
    bool exists_now = (stat(filepath, &st) == 0);
    if (exists_now) {
        // Confirmed present - keep this entry from aging out of the
        // persisted cache (see queue_cache_max_age_seconds) regardless of
        // whether its tags need re-reading below.
        it->second.last_seen = time(nullptr);
    }

    if (exists_now && st.st_mtime != it->second.mtime) {
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
    printf("Queue metadata loader: extracting metadata for %zu file(s)...\n", paths.size());

    int since_last_refresh = 0;
    size_t processed = 0;
    for (const auto &filepath : paths) {
        QueueMetaCacheEntry entry = extract_queue_item_metadata(filepath.c_str());

        {
            std::lock_guard<std::mutex> lock(g_queue_meta_mutex);
            g_queue_meta_cache[filepath] = entry;
        }

        processed++;
        if (processed % 1000 == 0 || processed == paths.size()) {
            int percent = (int)((processed * 100) / paths.size());
            printf("  Metadata extracted for %zu / %zu files... (%d%%)\n", processed, paths.size(), percent);
        }

        // Ask for a refresh every 200 files so progress is visible on very
        // large queues. queue_metadata_loader_request_refresh() coalesces
        // these - if the main thread hasn't finished the previous redraw
        // yet, this is a no-op instead of queueing up another expensive
        // full rebuild on top of it.
        if (++since_last_refresh >= 200) {
            since_last_refresh = 0;
            queue_metadata_loader_request_refresh(player);
        }
    }

    printf("Queue metadata loader: finished.\n");
    g_queue_meta_loader_running = false;

    // Final refresh to pick up the last batch, and to pick up any files
    // that were added to the queue (and so became "pending") while this
    // loader was already running - that refresh will spawn a fresh loader
    // for them since the running flag is now clear.
    queue_metadata_loader_request_refresh(player);
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

static bool get_queue_metadata_cache_path(char *path, size_t path_size) {
#ifdef _WIN32
    char app_data[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, app_data) != S_OK) {
        return false;
    }
    char config_dir[512];
    snprintf(config_dir, sizeof(config_dir), "%s\\Zenamp", app_data);
    CreateDirectoryA(config_dir, NULL);
    snprintf(path, path_size, "%s\\cache.txt", config_dir);
#else
    const char *home = getenv("HOME");
    if (!home) {
        return false;
    }
    char config_dir[512];
    snprintf(config_dir, sizeof(config_dir), "%s/.zenamp", home);
    mkdir(config_dir, 0755);
    snprintf(path, path_size, "%s/cache.txt", config_dir);
#endif
    return true;
}

// Persists the metadata cache (title/artist/album/genre/duration/karaoke/
// accessibility, keyed by filepath and validated by mtime) to a simple
// tab-separated text file, so the *next* launch can skip re-extracting tags
// and re-decoding durations for files it's already seen. One line per
// file: filepath, title, artist, album, genre, duration_seconds,
// is_karaoke, file_accessible, mtime, last_seen. Tabs/newlines inside
// string fields are collapsed to spaces so the format stays a simple,
// dependency-free split on '\t'.
//
// Entries that haven't been confirmed present (last_seen) in over
// queue_cache_max_age_seconds are quietly dropped instead of persisted -
// an entry only ages out after a full year of the file never once being
// seen again, so a removable/network drive that's merely unplugged at the
// moment easily survives, while files that are genuinely gone for good
// eventually stop taking up space without anyone having to clean up.
void save_queue_metadata_cache_to_disk() {
    char path[1024];
    if (!get_queue_metadata_cache_path(path, sizeof(path))) {
        return;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        printf("Failed to open metadata cache for writing: %s\n", path);
        return;
    }

    fprintf(f, "# Zenamp metadata cache - auto-generated, do not edit by hand\n");

    auto sanitize = [](std::string s) {
        for (auto &c : s) {
            if (c == '\t' || c == '\n' || c == '\r') c = ' ';
        }
        return s;
    };

    time_t now = time(nullptr);
    int written = 0;
    int aged_out = 0;
    {
        std::lock_guard<std::mutex> lock(g_queue_meta_mutex);
        for (const auto &kv : g_queue_meta_cache) {
            const QueueMetaCacheEntry &e = kv.second;
            if (!e.loaded) continue;  // nothing useful to persist for a placeholder

            if (e.last_seen != 0 && (now - e.last_seen) > queue_cache_max_age_seconds) {
                aged_out++;
                continue;
            }

            fprintf(f, "%s\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%ld\t%ld\n",
                    sanitize(kv.first).c_str(),
                    sanitize(e.title).c_str(),
                    sanitize(e.artist).c_str(),
                    sanitize(e.album).c_str(),
                    sanitize(e.genre).c_str(),
                    e.duration_seconds,
                    e.is_karaoke ? 1 : 0,
                    e.file_accessible ? 1 : 0,
                    (long)e.mtime,
                    (long)e.last_seen);
            written++;
        }
    }

    fclose(f);
    printf("Saved %d metadata cache entries to: %s (%d aged out after 1+ year unseen)\n",
           written, path, aged_out);
}

// Loads a previously-saved cache back into memory. Entries are still
// mtime-checked the normal way (see get_cached_or_placeholder()) the first
// time each file is looked up, so a file that changed on disk since the
// cache was written gets transparently re-extracted in the background
// rather than showing stale tags.
void load_queue_metadata_cache_from_disk() {
    char path[1024];
    if (!get_queue_metadata_cache_path(path, sizeof(path))) {
        return;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("No metadata cache file found: %s\n", path);
        return;
    }

    char line[4096];
    int loaded_count = 0;

    std::lock_guard<std::mutex> lock(g_queue_meta_mutex);
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        // Tab-separated: filepath, title, artist, album, genre,
        // duration_seconds, is_karaoke, file_accessible, mtime, last_seen.
        char *fields[10] = {nullptr};
        int field_count = 0;
        char *cursor = line;
        fields[field_count++] = cursor;
        while (field_count < 10 && (cursor = strchr(cursor, '\t')) != nullptr) {
            *cursor = '\0';
            cursor++;
            fields[field_count++] = cursor;
        }
        if (field_count != 9 && field_count != 10) continue;  // malformed/truncated line - skip it

        QueueMetaCacheEntry entry;
        entry.title = fields[1];
        entry.artist = fields[2];
        entry.album = fields[3];
        entry.genre = fields[4];
        entry.duration_seconds = atoi(fields[5]);
        entry.is_karaoke = atoi(fields[6]) != 0;
        entry.file_accessible = atoi(fields[7]) != 0;
        entry.mtime = (time_t)atol(fields[8]);
        // Older cache files without a last_seen column: treat as "seen
        // right now" rather than 0/never, so pre-existing entries get a
        // full fresh year before they'd be eligible to age out.
        entry.last_seen = (field_count == 10) ? (time_t)atol(fields[9]) : time(nullptr);
        entry.loaded = true;

        g_queue_meta_cache[fields[0]] = std::move(entry);
        loaded_count++;
    }

    fclose(f);
    printf("Loaded %d metadata cache entries from: %s\n", loaded_count, path);
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
 
// Global variable to track drag source row
static GtkTreeRowReference *drag_source_ref = NULL;
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
    
    printf("Model row deleted at index: %d\n", pending_delete_index);
    
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
    
    printf("Model row inserted at index: %d (was at %d)\n", insert_index, pending_delete_index);
    
    if (pending_delete_index >= 0 && pending_move_file) {
        // Perform the actual queue reorder
        if (reorder_queue_item(&player->queue, pending_delete_index, insert_index)) {
            printf("Queue reordered: %d -> %d\n", pending_delete_index, insert_index);
            
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
    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(tree_view), TRUE);
    
    printf("Queue tree view set to reorderable\n");
}

void on_queue_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
        drag_source_ref = gtk_tree_row_reference_new(model, path);
        
        gint *indices = gtk_tree_path_get_indices(path);
        int source_index = indices[0];
        
        // Create drag icon
        char *basename = g_path_get_basename(player->queue.files[source_index]);
        
        // Get the column values for a nicer drag icon
        gchar *title = NULL, *artist = NULL;
        gtk_tree_model_get(model, &iter, 
                          COL_TITLE, &title,
                          COL_ARTIST, &artist,
                          -1);
        
        char drag_text[512];
        if (title && title[0]) {
            if (artist && artist[0]) {
                snprintf(drag_text, sizeof(drag_text), "♪ %s - %s", artist, title);
            } else {
                snprintf(drag_text, sizeof(drag_text), "♪ %s", title);
            }
        } else {
            snprintf(drag_text, sizeof(drag_text), "♪ %s", basename);
        }
        
        GtkWidget *drag_icon = gtk_label_new(drag_text);
        gtk_widget_show(drag_icon);
        gtk_drag_set_icon_widget(context, drag_icon, 0, 0);
        
        g_free(basename);
        g_free(title);
        g_free(artist);
        gtk_tree_path_free(path);
        
        printf("Drag begin: source index %d\n", source_index);
    }
}

void on_queue_drag_data_get(GtkWidget *widget, GdkDragContext *context,
                            GtkSelectionData *selection_data, guint target_type,
                            guint time, gpointer user_data) {
    (void)widget;
    (void)context;
    (void)time;
    (void)user_data;
    
    if (target_type == TARGET_STRING && drag_source_ref) {
        GtkTreePath *path = gtk_tree_row_reference_get_path(drag_source_ref);
        if (path) {
            gint *indices = gtk_tree_path_get_indices(path);
            char index_str[16];
            snprintf(index_str, sizeof(index_str), "%d", indices[0]);
            gtk_selection_data_set_text(selection_data, index_str, -1);
            printf("Drag data get: sending index %d\n", indices[0]);
            gtk_tree_path_free(path);
        }
    }
}

void on_queue_drag_data_received(GtkWidget *widget, GdkDragContext *context,
                                 gint x, gint y, GtkSelectionData *selection_data,
                                 guint target_type, guint time, gpointer user_data) {
    (void)x;
    (void)y;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    if (target_type == TARGET_STRING) {
        const gchar *data = (const gchar*)gtk_selection_data_get_text(selection_data);
        if (data) {
            int source_index = atoi(data);
            
            // Get drop position
            GtkTreePath *dest_path = NULL;
            GtkTreeViewDropPosition pos;
            
            gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(widget), &dest_path, &pos);
            
            if (dest_path) {
                gint *indices = gtk_tree_path_get_indices(dest_path);
                int dest_index = indices[0];
                
                // Adjust destination based on drop position
                if (pos == GTK_TREE_VIEW_DROP_AFTER || 
                    pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER) {
                    dest_index++;
                }
                
                // Adjust if dropping after source (since source will be removed first)
                if (dest_index > source_index) {
                    dest_index--;
                }
                
                printf("Drag data received: moving from %d to %d\n", source_index, dest_index);
                
                // Perform the reorder
                if (reorder_queue_item(&player->queue, source_index, dest_index)) {
                    update_queue_display_with_filter(player, false);
                    update_gui_state(player);
                    printf("Queue reordered successfully\n");
                }
                
                gtk_tree_path_free(dest_path);
            }
        }
    }
    
    gtk_drag_finish(context, TRUE, FALSE, time);
}

void on_queue_drag_end(GtkWidget *widget, GdkDragContext *context, gpointer user_data) {
    (void)widget;
    (void)context;
    (void)user_data;
    
    // Clean up the row reference
    if (drag_source_ref) {
        gtk_tree_row_reference_free(drag_source_ref);
        drag_source_ref = NULL;
    }
    
    printf("Drag end\n");
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
    
    if (queue_index < 0 || queue_index >= player->queue.count) {
        return;
    }
    
    printf("Queue row activated: original queue index %d\n", queue_index);
    
    // Get the filepath from the model to verify what file is actually being clicked
    char *filepath = NULL;
    gtk_tree_model_get(model, &iter, COL_FILEPATH, &filepath, -1);
    
    if (!filepath) {
        return;
    }
    
    // Check if already playing this exact file
    if (queue_index == player->queue.current_index && player->is_playing) {
        printf("Already playing this song\n");
        g_free(filepath);
        return;
    }
    
    // Only set current_index after we've verified it matches the filepath
    printf("Setting current_index to %d for file: %s\n", queue_index, filepath);
    
    stop_playback(player);
    player->queue.current_index = queue_index;
    
    if (load_file_from_queue(player)) {
        update_queue_display_minimal(player);  // Preserves karaoke checkmarks
        update_gui_state(player);
        start_playback(player);
        printf("Started playing: %s\n", get_current_queue_file(&player->queue));
        char *metadata = extract_metadata(get_current_queue_file(&player->queue));
        char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
        parse_metadata(metadata, title, artist, album, genre);
        if (!ends_with_zip(get_current_queue_file(&player->queue))) {
            show_track_info_overlay(player->visualizer, title, artist, album,
                get_file_duration(player->queue.files[player->queue.current_index]));
        }
        //printf("\n\n\nMy Queue %s %i\n\n\n", get_current_queue_file(&player->queue), !ends_with_zip(get_current_queue_file(&player->queue)));
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

void on_queue_delete_item(GtkMenuItem *menuitem, gpointer user_data) {
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
        
        printf("Removing item %d from queue\n", index);
        
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
                        printf("Warning: Could not rename .cdg file from %s to %s\n", 
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
static void on_rename_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    if (response_id == GTK_RESPONSE_OK) {
        GtkEntry *entry = GTK_ENTRY(g_object_get_data(G_OBJECT(dialog), "name_entry"));
        const char *old_path = (const char*)g_object_get_data(G_OBJECT(dialog), "old_path");
        
        const char *new_filename = gtk_entry_get_text(entry);
        
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
                printf("File renamed successfully\n");
            } else {
                GtkWidget *error_dialog = gtk_message_dialog_new(
                    GTK_WINDOW(dialog),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_OK,
                    "Failed to rename file. Check that the file exists and you have permission."
                );
                gtk_dialog_run(GTK_DIALOG(error_dialog));
                gtk_widget_destroy(error_dialog);
            }
        }
    }
    
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

void on_queue_rename_item(GtkMenuItem *menuitem, gpointer user_data) {
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
    gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 5);
    
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), basename);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);  // Select all text
    gtk_box_pack_start(GTK_BOX(content_area), entry, FALSE, FALSE, 5);
    
    // Store data in dialog for handler
    g_object_set_data(G_OBJECT(dialog), "name_entry", entry);
    g_object_set_data(G_OBJECT(dialog), "old_path", filepath);
    
    gtk_widget_show_all(content_area);
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_rename_response), player);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    
    g_free(filepath);
    g_free(basename);
}

gboolean on_queue_context_menu(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    // Handle middle-click (button 2) - direct delete
    if (event->type == GDK_BUTTON_PRESS && event->button == 2) {
        GtkTreePath *path;
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), 
                                         (gint)event->x, (gint)event->y, 
                                         &path, NULL, NULL, NULL)) {
            GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
            GtkTreeIter iter;
            if (!gtk_tree_model_get_iter(model, &iter, path)) {
                gtk_tree_path_free(path);
                return FALSE;
            }
            
            // Get the actual queue index, not the visible row index
            int index = -1;
            gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &index, -1);
            gtk_tree_path_free(path);
            
            if (index < 0 || index >= player->queue.count) {
                return FALSE;
            }
            
            printf("Removing item %d via middle-click\n", index);
            
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
                    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
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
            
            return TRUE;
        }
    }
    
    // Handle right-click (button 3) - show context menu
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkTreePath *path;
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), 
                                         (gint)event->x, (gint)event->y, 
                                         &path, NULL, NULL, NULL)) {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
            gtk_tree_selection_select_path(selection, path);
            
            GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
            GtkTreeIter iter;
            if (!gtk_tree_model_get_iter(model, &iter, path)) {
                gtk_tree_path_free(path);
                return FALSE;
            }
            
            // Get the actual queue index, not the visible row index
            int index = -1;
            gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &index, -1);
            
            gtk_tree_path_free(path);
            
            if (index < 0 || index >= player->queue.count) {
                return FALSE;
            }
            
            GtkWidget *menu = gtk_menu_new();
            
            GtkWidget *move_up_item = gtk_menu_item_new_with_label("Move Up (Ctrl+↑)");
            g_signal_connect(move_up_item, "activate", G_CALLBACK(on_queue_move_up), player);
            gtk_widget_set_sensitive(move_up_item, index > 0);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), move_up_item);
            
            GtkWidget *move_down_item = gtk_menu_item_new_with_label("Move Down (Ctrl+↓)");
            g_signal_connect(move_down_item, "activate", G_CALLBACK(on_queue_move_down), player);
            gtk_widget_set_sensitive(move_down_item, index < player->queue.count - 1);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), move_down_item);
            
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
            
            GtkWidget *rename_item = gtk_menu_item_new_with_label("Rename File...");
            g_signal_connect(rename_item, "activate", G_CALLBACK(on_queue_rename_item), player);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), rename_item);
            
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
            
            GtkWidget *delete_item = gtk_menu_item_new_with_label("Remove from Queue");
            g_signal_connect(delete_item, "activate", G_CALLBACK(on_queue_delete_item), player);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);
            
            gtk_widget_show_all(menu);
            gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
            
            return TRUE;
        }
    }
    
    return FALSE;
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

void on_queue_move_up(GtkMenuItem *menuitem, gpointer user_data) {
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

void on_queue_move_down(GtkMenuItem *menuitem, gpointer user_data) {
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

gboolean on_queue_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return FALSE;
    }
    
    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
    gint *indices = gtk_tree_path_get_indices(path);
    int index = indices[0];
    gtk_tree_path_free(path);
    
    if (event->state & GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_Up) {
            move_queue_item_up(player, index);
            return TRUE;
        } else if (event->keyval == GDK_KEY_Down) {
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
    
    const char *filter_text = gtk_entry_get_text(GTK_ENTRY(player->queue_search_entry));
    strncpy(player->queue_filter_text, filter_text, sizeof(player->queue_filter_text) - 1);
    player->queue_filter_text[sizeof(player->queue_filter_text) - 1] = '\0';
    
    printf("Applying queue filter: '%s'\n", player->queue_filter_text);
    
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

static void on_queue_search_icon_press(GtkEntry *entry, GtkEntryIconPosition icon_pos, 
                                       GdkEvent *event, gpointer user_data) {
    (void)event;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    if (icon_pos == GTK_ENTRY_ICON_SECONDARY) {
        // Clear button clicked
        gtk_entry_set_text(entry, "");
        player->queue_filter_text[0] = '\0';
        
        // Remove any pending timeout
        if (player->queue_filter_timeout_id != 0) {
            g_source_remove(player->queue_filter_timeout_id);
            player->queue_filter_timeout_id = 0;
        }
        
        // Full update to refresh all items with checkmarks
        update_queue_display_with_filter(player);
    }
}

GtkWidget* create_queue_search_bar(AudioPlayer *player) {
    GtkWidget *search_entry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Filter queue...");
    
    // Add clear icon
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(search_entry), 
                                     GTK_ENTRY_ICON_SECONDARY, 
                                     "edit-clear-symbolic");
    gtk_entry_set_icon_tooltip_text(GTK_ENTRY(search_entry),
                                   GTK_ENTRY_ICON_SECONDARY,
                                   "Clear filter");
    
    player->queue_search_entry = search_entry;
    player->queue_filter_timeout_id = 0;
    player->queue_filter_text[0] = '\0';
    
    // Connect signals
    g_signal_connect(search_entry, "changed", 
                     G_CALLBACK(on_queue_search_changed), player);
    g_signal_connect(search_entry, "icon-press",
                     G_CALLBACK(on_queue_search_icon_press), player);
    
    return search_entry;
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

void update_queue_display_with_filter(AudioPlayer *player, bool scroll_to_current) {
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
                COL_TITLE, meta.loaded ? meta.title.c_str() : "(Loading…)",
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


// Cleanup function to call on exit
void cleanup_queue_filter(AudioPlayer *player) {
    if (player->queue_filter_timeout_id != 0) {
        g_source_remove(player->queue_filter_timeout_id);
        player->queue_filter_timeout_id = 0;
    }
}
