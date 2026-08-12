#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

// Add to audio_player.h:
#ifndef _WIN32
#include <gio/gio.h>
#define ZENAMP_DBUS_NAME "com.zenamp.AudioPlayer"
#define ZENAMP_DBUS_PATH "/com/zenamp/AudioPlayer"
#else
#include <windows.h>
#define ZENAMP_MUTEX_NAME "Global\\ZenampSingleInstance"
#endif

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
#include <vector>
#include "midiplayer.h"
#include "dbopl_wrapper.h"
#include "wav_converter.h"
#include "audioconverter.h"
#include "convertoggtowav.h"
#include "convertflactowav.h"
#include "equalizer.h"
#include "visualization.h"
#include "cdg.h"
#include "zip_support.h"

typedef struct {
    char *filepath;
    int16_t *data;
    size_t length;
    int sample_rate;
    int channels;
    int bits_per_sample;
    double song_duration;
    time_t last_access;
    size_t memory_size;  // in bytes
} CachedAudioBuffer;

typedef struct {
    CachedAudioBuffer **buffers;
    int count;
    int capacity;
    size_t total_memory;  // Track total memory used
    size_t max_memory;    // Maximum memory to use (e.g., 500 MB)
} AudioBufferCache;

typedef struct {
    char *filepath;
    char *title;
    char *artist;
    char *album;
    char *genre;
    int duration_seconds;
} QueueItem;

// Column enumeration for the tree model
enum {
    COL_FILEPATH = 0,
    COL_PLAYING,      // "▶" indicator
    COL_FILENAME,
    COL_TITLE,
    COL_ARTIST,
    COL_ALBUM,
    COL_GENRE,
    COL_DURATION,
    COL_CDGK,
    COL_QUEUE_INDEX,
    NUM_COLS
};

// Which field the queue's collapsible group view is currently grouped by.
// QUEUE_GROUP_NONE means the flat, ungrouped queue_store is shown.
enum QueueGroupMode {
    QUEUE_GROUP_NONE = 0,
    QUEUE_GROUP_ARTIST,
    QUEUE_GROUP_ALBUM,
    QUEUE_GROUP_GENRE
};

// Structure to hold audio metadata
struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string year;
    std::string genre;
    std::string comment;
    int track_number = 0;
    int duration_seconds = 0;
    int bitrate = 0;
    std::map<std::string, std::string> custom_tags;
};


typedef struct {
    bool is_compact;
    int window_width;
    int window_height;
    int player_width;
    int vis_width;
    int vis_height;
    int queue_width;
    int queue_height;
    int icon_size;
} LayoutConfig;

// Compact layout widgets
typedef struct {
    GtkWidget *bottom_controls_hbox;
    GtkWidget *queue_controls_vbox;
} CompactLayout;

// Regular layout widgets  
typedef struct {
    GtkWidget *queue_button_box;
    GtkWidget *eq_below_controls;
} RegularLayout;

// Layout container
typedef struct {
    LayoutConfig config;
    CompactLayout compact;
    RegularLayout regular;
    
    // Common widgets that exist in both layouts
    GtkWidget *main_hbox;
    GtkWidget *player_vbox;
    GtkWidget *content_vbox;
    GtkWidget *queue_vbox;
    GtkWidget *nav_button_box;
    GtkWidget *volume_box;
    GtkWidget *bottom_box;
    GtkWidget *shared_equalizer;
    GtkWidget *toggle_queue_menu_item;
    GtkWidget *toggle_fullscreen_menu_item;    
} LayoutManager;


// Conversion Cache Entries.
typedef struct {
    char *original_path;
    char *virtual_filename;
    time_t modification_time;
    off_t file_size;
} ConversionCacheEntry;

// Cache File Conversion
typedef struct {
    ConversionCacheEntry *entries;
    int count;
    int capacity;
} ConversionCache;

// Audio buffer structure
typedef struct {
    int16_t *data;
    size_t length;
    size_t position;
} AudioBuffer;

// Play queue structure
typedef struct {
    char **files;           // Array of file paths
    int count;              // Number of files in queue
    int capacity;           // Allocated capacity
    int current_index;      // Currently playing file index
    bool repeat_queue;      // Whether to repeat the entire queue
} PlayQueue;

// Main audio player structure
typedef struct {
    GtkWidget *window;
    GtkWidget *play_button;
    GtkWidget *pause_button;
    GtkWidget *stop_button;
    GtkWidget *rewind_button;
    GtkWidget *fast_forward_button;
    GtkWidget *progress_scale;
    GtkWidget *time_label;
    GtkWidget *volume_scale;
    GtkWidget *speed_scale;
    GtkWidget *file_label;
    
    // Queue widgets
    GtkWidget *queue_scrolled_window;
    GtkWidget *queue_listbox;
    GtkWidget *add_to_queue_button;
    GtkWidget *clear_queue_button;
    GtkWidget *repeat_queue_button;
    GtkWidget *next_button;
    GtkWidget *prev_button;
    GtkListStore *queue_store;
    GtkWidget *queue_tree_view;

    // Collapsible "Group by" view: a second store with the same column
    // layout as queue_store, holding one collapsible parent row per
    // artist/album/genre (per queue_group_mode) with tracks as children.
    // The tree view's model is switched between this and queue_store
    // depending on queue_group_mode; player->queue.files itself is never
    // reordered by grouping. See set_queue_group_mode() and
    // get_queue_display_order() in queue.cpp.
    GtkTreeStore *queue_store_grouped = nullptr;
    QueueGroupMode queue_group_mode = QUEUE_GROUP_NONE;
    GtkWidget *queue_group_dropdown = nullptr;

    // "Karaoke only" checkbox next to the Group by dropdown - when on, both
    // the flat and grouped queue views (and next/previous navigation) only
    // show/visit tracks with karaoke content (.kfn/.zip/.kar, or an audio
    // file with a matching .cdg). See queue_karaoke_only_filter usage in
    // queue.cpp's update_queue_display_flat()/update_queue_display_grouped().
    bool queue_karaoke_only_filter = false;
    GtkWidget *queue_karaoke_filter_check = nullptr;
    
    PlayQueue queue;
    ConversionCache conversion_cache;

    GtkWidget *queue_search_entry;
    guint queue_filter_timeout_id;
    char queue_filter_text[256];
    
    bool is_loaded;
    bool is_playing;
    bool is_paused;
    bool seeking;
    char current_file[1024];
    char temp_wav_file[1024];
    double song_duration;
    guint update_timer_id;
    
    AudioBuffer audio_buffer;
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    pthread_mutex_t audio_mutex;
    
    // Audio format info for seeking calculations
    int sample_rate;
    int channels;
    int bits_per_sample;
    double playback_speed;
    double speed_accumulator;  // For fractional sample stepping
    
    Visualizer *visualizer;
    GtkWidget *vis_controls;
    
    Equalizer *equalizer;
    
    // Equalizer GUI controls
    GtkWidget *eq_frame;
    GtkWidget *eq_enable_check;
    GtkWidget *bass_scale;
    GtkWidget *mid_scale;
    GtkWidget *treble_scale;
    GtkWidget *eq_reset_button;

    // CD+G
    CDGDisplay *cdg_display;
    bool has_cdg;
    KaraokeZipContents karaoke_temp_files;
    bool is_loading_cdg_from_zip;

    // Metadata
    AudioMetadata current_metadata;
    GtkWidget *metadata_label;

    LayoutManager layout;
    
    // GTK4 removed GtkStatusIcon entirely with no built-in replacement (its
    // call site, create_tray_icon(), is already commented out in layout.cpp).
    // Left as void* so the struct still compiles; see the note by
    // create_tray_icon() below for the real decision this needs.
    void *tray_icon;
    GtkWidget *tray_menu;
    bool minimized_to_tray;
    
    AudioBufferCache audio_cache; 

#ifndef _WIN32
    guint dbus_owner_id;
    GDBusConnection *dbus_connection;
#else
    HANDLE single_instance_mutex;
    HANDLE pipe_handle;
#endif

    
} AudioPlayer;

typedef struct {
    double volume;
    double speed;
    // Equalizer settings
    bool eq_enabled;
    float bass_gain;
    float mid_gain;
    float treble_gain;
    // Visualization settings
    int vis_type;
    float vis_sensitivity;
} PlayerSettings;



// External variables from midiplayer
extern double playTime;
extern bool isPlaying;
extern bool paused;
extern int globalVolume;
extern double playwait;

// External functions from midiplayer
extern void processEvents(void);

// Global player instance
extern AudioPlayer *player;

// Queue management functions
void init_queue(PlayQueue *queue);
void clear_queue(PlayQueue *queue);
bool add_to_queue(PlayQueue *queue, const char *filename);
const char* get_current_queue_file(PlayQueue *queue);
bool advance_queue(PlayQueue *queue);
bool previous_queue(PlayQueue *queue);
void update_queue_display(AudioPlayer *player);
void add_column(GtkWidget *tree_view, const char *title, int col_id, int width, gboolean sortable);
void on_queue_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);
void parse_metadata(const char *metadata_str, char *title, char *artist, char *album, char *genre);
int get_file_duration(const char *filepath);
void create_queue_treeview(AudioPlayer *player);
// NOTE(gtk4): the four functions below are declared here but not defined in
// any file shared so far - they're presumably in queue.cpp per the Makefile.
// Their signatures are updated to GTK4's controller model; the actual .cpp
// definitions (and the g_signal_connect call sites wiring them up, wherever
// those live) need to switch from "button-press-event"/"key-press-event" to
// a GtkGestureClick/GtkEventControllerKey added via gtk_widget_add_controller().
void on_queue_context_menu(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
void move_queue_item_up(AudioPlayer *player, int index);
void move_queue_item_down(AudioPlayer *player, int index);
void on_queue_move_up(GtkWidget *menuitem, gpointer user_data);
void on_queue_move_down(GtkWidget *menuitem, gpointer user_data);
gboolean on_queue_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
// NOTE(gtk4): GTK3's drag-and-drop model (GtkTargetEntry/GdkDragContext/
// GtkSelectionData/"drag-data-get" etc. signals) is gone entirely in GTK4,
// replaced by GtkDragSource (dragging out) and GtkDropTarget (accepting
// drops), which exchange GValue-typed data through a GdkContentProvider
// instead of raw selection buffers. There's no signature-level equivalent
// for several of the old callbacks - this is a real rewrite, not a rename:
//   - on_queue_drag_begin/on_queue_drag_end  -> GtkDragSource "drag-begin"/"drag-end"
//   - on_queue_drag_data_get                 -> GtkDragSource "prepare" (returns a GdkContentProvider*)
//   - on_queue_drag_data_received + drag_drop -> GtkDropTarget "drop" (one signal replaces both)
//   - on_drag_motion                         -> GtkDropTarget "motion" (optional, for drop-position feedback)
// setup_queue_drag_and_drop() needs to build a GtkDragSource + GtkDropTarget
// pair and attach them with gtk_widget_add_controller() instead of
// gtk_drag_source_set()/gtk_drag_dest_set(). This almost certainly lives in
// queue.cpp, which hasn't been shared yet - flagging so the reorder logic
// gets a real rewrite rather than a guess when that file comes in.
void setup_queue_drag_and_drop(AudioPlayer *player);
void on_queue_drag_begin(GtkDragSource *source, GdkDrag *drag, gpointer user_data);
GdkContentProvider* on_queue_drag_data_get(GtkDragSource *source, gdouble x, gdouble y, gpointer user_data);
gboolean on_queue_drag_data_received(GtkDropTarget *target, const GValue *value, gdouble x, gdouble y, gpointer user_data);
void on_queue_drag_end(GtkDragSource *source, GdkDrag *drag, gboolean delete_data, gpointer user_data);
bool reorder_queue_item(PlayQueue *queue, int from_index, int to_index);
void on_queue_model_row_deleted(GtkTreeModel *model, GtkTreePath *path, gpointer user_data);
void on_queue_model_row_inserted(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
void cleanup_queue_filter(AudioPlayer *player);
GtkWidget* create_queue_search_bar(AudioPlayer *player);
// Opens a small dialog to edit title/artist/album/genre tags (via TagLib)
// for the currently selected queue row. No-ops for karaoke wrapper formats
// (.zip/.kfn/.kar), which don't carry ordinary audio tags.
void on_queue_edit_tags_item(GtkWidget *menuitem, gpointer user_data);
void update_queue_display_with_filter(AudioPlayer *player, bool scroll_to_current = true);
void update_queue_display_minimal(AudioPlayer *player);
void update_queue_display_debounced(AudioPlayer *player);
// Queue group-by view (see queue_store_grouped above). set_queue_group_mode()
// switches the tree view's model and rebuilds the display; get_queue_display_order()
// returns queue.files[] indices in current on-screen order (flat or grouped),
// depth-first, used by next_song()/previous_song() so playback follows whichever
// order is showing.
void set_queue_group_mode(AudioPlayer *player, QueueGroupMode mode);
std::vector<int> get_queue_display_order(AudioPlayer *player);
bool matches_filter(const char *text, const char *filter);
// Cheap cache-only lookup (no synchronous extraction) of whether filepath is
// known to be karaoke content. Returns false for anything not yet cached -
// safe to call from the main thread (e.g. from next_song()/previous_song()'s
// filtered-navigation path) since it never blocks on file I/O.
bool queue_file_is_karaoke_cached(const char *filepath);
bool filename_exists_in_queue(PlayQueue *queue, const char *filepath);
// NOTE(gtk4): GtkCheckMenuItem is gone along with the rest of the old menu
// widgets - a GTK4 menu bar built from GMenu/GAction would represent these as
// a stateful GSimpleAction instead (callback (GSimpleAction*, GVariant*,
// gpointer), toggled via g_simple_action_set_state()). Loosened to GtkWidget*
// here just to compile; real fix belongs with the menu-bar rebuild.
void on_toggle_queue_panel(GtkWidget *check_item, gpointer user_data);
int find_file_in_queue(PlayQueue *queue, const char *filepath);
void on_toggle_fullscreen_visualization(GtkWidget *check_item, gpointer user_data);
void on_shortcuts_menu_clicked(GtkWidget *menuitem, gpointer user_data);

// Settings
bool save_player_settings(AudioPlayer *player);
bool load_player_settings(AudioPlayer *player);
bool get_settings_path(char *path, size_t path_size);
// Persists/restores the per-file metadata cache (tags, duration, karaoke
// detection) used by the queue display, so relaunching doesn't have to
// re-extract everything before the queue shows real info. `path` is a
// full file path (see get_metadata_cache_path()), not a directory.
bool save_queue_metadata_cache(const char *path);
bool load_queue_metadata_cache(const char *path);
bool get_metadata_cache_path(char *path, size_t path_size);

// Audio functions
void audio_callback(void* userdata, Uint8* stream, int len);
bool init_audio(AudioPlayer *player, int sample_rate = SAMPLE_RATE, int channels = AUDIO_CHANNELS);

// Tray Icon
// NOTE(gtk4): GtkStatusIcon is removed entirely (not deprecated - gone, no
// compat shim). There is no built-in GTK4 tray icon API; the closest options
// are a third-party library (libayatana-appindicator or similar) or the
// freedesktop StatusNotifierItem D-Bus protocol implemented by hand. Since
// create_tray_icon()'s only call site in layout.cpp is already commented
// out, this whole feature looks currently disabled - worth deciding whether
// to actually reimplement tray support or just remove it, rather than
// carrying dead code against a removed type. Signatures below are loosened
// to void* purely so the struct/header still compile either way.
void on_tray_icon_activate(void *status_icon, gpointer user_data);
void on_tray_icon_popup_menu(void *status_icon, guint button, guint activate_time, gpointer user_data);
// "window-state-event" is also gone in GTK4; iconified/maximized/fullscreen
// state comes from GdkToplevel now (gdk_toplevel_get_state(), as already used
// in zenamp_main.cpp's timer callback), typically observed via GtkWindow's
// "notify::maximized"/"notify::fullscreened" property signals rather than a
// single window-state signal.
gboolean on_window_state_event(GtkWidget *widget, gpointer user_data);
void create_tray_icon(AudioPlayer *player);

// File conversion functions
bool convert_midi_to_wav(AudioPlayer *player, const char* filename);
bool convert_mp3_to_wav(AudioPlayer *player, const char* filename);
#ifdef _WIN32
bool convertM4aToWav(const char* m4a_filename, const char* wav_filename);
bool convertWmaToWavInMemory(const std::vector<uint8_t>& wma_data, std::vector<uint8_t>& wav_data);
bool convert_audio_to_wav_internal(AudioPlayer *player, const char* filename);
#endif
bool convert_m4a_to_wav(AudioPlayer *player, const char* filename);
bool convert_wma_to_wav(AudioPlayer *player, const char* filename);
bool convert_audio_to_wav(AudioPlayer *player, const char* filename);
bool convert_ogg_to_wav(AudioPlayer *player, const char* filename);
bool convert_flac_to_wav(AudioPlayer *player, const char* filename);
bool load_wav_file(AudioPlayer *player, const char* wav_path);
bool load_file(AudioPlayer *player, const char *filename);
bool load_file_from_queue(AudioPlayer *player);
int scale_size(int base_size, int screen_dimension, int base_dimension);

// Playback control functions
void seek_to_position(AudioPlayer *player, double position_seconds);
void start_playback(AudioPlayer *player);
void toggle_pause(AudioPlayer *player);
void stop_playback(AudioPlayer *player);
void rewind_5_seconds(AudioPlayer *player);
void fast_forward_5_seconds(AudioPlayer *player);
void next_song(AudioPlayer *player);
void previous_song(AudioPlayer *player);
void update_gui_state(AudioPlayer *player);
void next_song_filtered(AudioPlayer *player);
void previous_song_filtered(AudioPlayer *player);

// GUI callback functions
void on_progress_scale_value_changed(GtkRange *range, gpointer user_data);
void on_add_to_queue_clicked(GtkButton *button, gpointer user_data);
void on_clear_queue_clicked(GtkButton *button, gpointer user_data);
void on_repeat_queue_toggled(GtkCheckButton *button, gpointer user_data);
void on_menu_open(GtkWidget *menuitem, gpointer user_data);
void on_menu_quit(GtkWidget *menuitem, gpointer user_data);
void on_menu_about(GtkWidget *menuitem, gpointer user_data);
void on_play_clicked(GtkButton *button, gpointer user_data);
void on_pause_clicked(GtkButton *button, gpointer user_data);
void on_stop_clicked(GtkButton *button, gpointer user_data);
void on_rewind_clicked(GtkButton *button, gpointer user_data);
void on_fast_forward_clicked(GtkButton *button, gpointer user_data);
void on_next_clicked(GtkButton *button, gpointer user_data);
void on_previous_clicked(GtkButton *button, gpointer user_data);
void on_volume_changed(GtkRange *range, gpointer user_data);
void on_window_destroy(GtkWidget *widget, gpointer user_data);
// "delete-event" -> "close-request" in GTK4: (GtkWindow*, gpointer) -> gboolean.
gboolean on_window_delete_event(GtkWindow *window, gpointer user_data);
bool remove_from_queue(PlayQueue *queue, int index);
void on_queue_item_clicked(GtkListBox *listbox, GtkListBoxRow *row, gpointer user_data);
double get_scale_factor(GtkWidget *widget);
void on_speed_changed(GtkRange *range, gpointer user_data);

// Caching
void init_conversion_cache(ConversionCache *cache);
void cleanup_conversion_cache(ConversionCache *cache);
const char* get_cached_conversion(ConversionCache *cache, const char* original_path);
void add_to_conversion_cache(ConversionCache *cache, const char* original_path, const char* virtual_filename);
bool is_file_modified(const char* filepath, time_t cached_time, off_t cached_size);

// GUI creation function
void create_main_window(AudioPlayer *player);


// Windows Specific 
#ifdef _WIN32
bool open_windows_file_dialog(char* filename, size_t filename_size, bool multiple = false);
#endif

// keyboard stuff
// NOTE(gtk4): defined in keyboard.cpp (not yet shared) - "key-press-event"
// is gone, so wherever this gets connected needs a GtkEventControllerKey
// added via gtk_widget_add_controller() instead, hooked to "key-pressed".
gboolean on_key_press_event(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
void show_keyboard_help(AudioPlayer *player);
void add_keyboard_shortcuts_menu(AudioPlayer *player, GtkWidget *help_menu);
void setup_keyboard_shortcuts(AudioPlayer *player);

bool load_m3u_playlist(AudioPlayer *player, const char *m3u_path);
bool save_m3u_playlist(AudioPlayer *player, const char *m3u_path);
bool is_m3u_file(const char *filename);
void on_menu_load_playlist(GtkWidget *menuitem, gpointer user_data);
void on_menu_save_playlist(GtkWidget *menuitem, gpointer user_data);
void toggle_fullscreen(AudioPlayer *player);

GtkWidget* create_equalizer_controls(AudioPlayer *player);

// NOTE(gtk4): same GTK3->GTK4 drag-and-drop story as setup_queue_drag_and_drop()
// above, but this second set (no "queue_" prefix) looks like it's for
// accepting file drops onto the main window from outside the app (e.g. an
// audio file dragged in from a file manager), rather than reordering the
// queue internally. TARGET_STRING/target_list/n_targets are gone - GTK4
// expresses acceptable drop types via GdkContentFormats (e.g.
// gdk_content_formats_new_for_gtype(GDK_TYPE_FILE_LIST) for file drops)
// passed to gtk_drop_target_new(), not a GtkTargetEntry table. Likely also
// belongs in queue.cpp or wherever the window's drop target gets set up -
// flagging for a real rewrite rather than guessing the exact content type.
void on_drag_begin(GtkDragSource *source, GdkDrag *drag, gpointer user_data);
GdkContentProvider* on_drag_data_get(GtkDragSource *source, gdouble x, gdouble y, gpointer user_data);
GdkDragAction on_drag_motion(GtkDropTarget *target, gdouble x, gdouble y, gpointer user_data);

gboolean on_drag_data_received(GtkDropTarget *target, const GValue *value, gdouble x, gdouble y, gpointer user_data);

// "button-press-event" -> GtkGestureClick's "pressed" signal in GTK4 (void, not gboolean).
void on_queue_button_press(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);

gboolean on_drag_drop(GtkDropTarget *target, const GValue *value, gdouble x, gdouble y, gpointer user_data);

void on_queue_item_button_press(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
void on_window_resize(GtkWidget *widget, gpointer user_data);
void on_window_realize(GtkWidget *widget, gpointer user_data);
void create_shared_equalizer(AudioPlayer *player);
void toggle_vis_fullscreen(AudioPlayer *player);
void cleanup_vis_fullscreen();
// "key-press-event" -> GtkEventControllerKey's "key-pressed"; "delete-event"
// -> GtkWindow's "close-request". Matches the call sites already wired up in
// zenamp_main.cpp's toggle_vis_fullscreen().
gboolean on_vis_fullscreen_key_press(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
gboolean on_vis_fullscreen_delete_event(GtkWindow *window, gpointer user_data);
// void, not gboolean - matches GtkGestureClick's actual "pressed" signal
// (same correction as the sibling handlers in visualization.h).
void on_visualizer_button_press(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);

bool is_visualizer_fullscreen();

void add_to_recent_files(const char* filepath, const char* mime_type);
// NOTE(gtk4): GtkRecentChooser (and GtkRecentChooserMenu/Widget/Dialog) are
// all removed entirely in GTK4 - there's no ready-made "recent files" widget
// left. GtkRecentManager itself is still available for the underlying data
// (gtk_recent_manager_get_default(), g_list of GtkRecentInfo), so the recent-
// files submenu in layout.cpp will need to be built by hand from that list
// instead of a GtkRecentChooserMenu, with plain click handling on whatever
// widget represents each entry - hence the generic GtkWidget* here.
void on_recent_playlist_activated(GtkWidget *widget, gpointer user_data);
bool save_last_playlist_path(const char *playlist_path);
bool save_current_queue_on_exit(AudioPlayer *player);

char* extract_metadata(const char *filepath);

bool generate_karaoke_zip_from_lrc(const std::string& lrc_path, std::string& out_zip_path);

void init_audio_cache(AudioBufferCache *cache, size_t max_memory_mb);
CachedAudioBuffer* find_in_cache(AudioBufferCache *cache, const char *filepath);
void add_to_cache(AudioBufferCache *cache, const char *filepath, 
                  int16_t *data, size_t length, int sample_rate, 
                  int channels, int bits_per_sample, double song_duration);
void cleanup_audio_cache(AudioBufferCache *cache);

#endif // AUDIO_PLAYER_H
