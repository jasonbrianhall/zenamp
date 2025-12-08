#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "cometbuster.h"
#include "visualization.h"
#include "audio_wad.h"

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

typedef struct {
    GtkWidget *window;
    GtkWidget *drawing_area;
    GtkWidget *status_label;
    GtkWidget *menu_bar;
    
    Visualizer visualizer;
    AudioManager audio;          // Audio system
    
    int frame_count;
    double total_time;
    guint update_timer_id;
    
    bool is_fullscreen;
    bool game_paused;            // Track if game is paused
    
    // Volume control dialog
    GtkWidget *volume_dialog;
    GtkWidget *music_volume_scale;
    GtkWidget *sfx_volume_scale;
    GtkWidget *music_volume_label;
    GtkWidget *sfx_volume_label;
    
    // High score entry dialog
    GtkWidget *high_score_dialog;
    GtkWidget *high_score_name_entry;
    bool high_score_dialog_shown;
    
    int music_volume;
    int sfx_volume;
    
} CometGUI;

// ============================================================
// VOLUME SETTINGS PERSISTENCE FUNCTIONS
// ============================================================

/**
 * Get the settings directory path for the current platform
 * Returns a statically allocated string
 */
static const char* settings_get_dir(void) {
    static char settings_dir[512] = {0};
    static bool initialized = false;
    
    if (initialized) return settings_dir;
    
#ifdef _WIN32
    // Windows: %APPDATA%\CometBuster\
    char appdata_path[MAX_PATH];
    
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata_path))) {
        snprintf(settings_dir, sizeof(settings_dir), "%s\\CometBuster", appdata_path);
    } else {
        // Fallback to user home directory
        const char *home = getenv("USERPROFILE");
        if (home) {
            snprintf(settings_dir, sizeof(settings_dir), "%s\\CometBuster", home);
        } else {
            strcpy(settings_dir, ".\\CometBuster");
        }
    }
#else
    // Linux/Unix: ~/.cometbuster/
    const char *home = getenv("HOME");
    if (home) {
        snprintf(settings_dir, sizeof(settings_dir), "%s/.cometbuster", home);
    } else {
        strcpy(settings_dir, "./.cometbuster");
    }
#endif
    
    initialized = true;
    return settings_dir;
}

/**
 * Get the full path to the settings file
 */
static const char* settings_get_path(void) {
    static char settings_path[512] = {0};
    static bool initialized = false;
    
    if (initialized) return settings_path;
    
    snprintf(settings_path, sizeof(settings_path), "%s/volumesettings", settings_get_dir());
    
    initialized = true;
    return settings_path;
}

/**
 * Create the settings directory if it doesn't exist
 */
static bool settings_ensure_dir(void) {
#ifdef _WIN32
    const char *dir = settings_get_dir();
    if (CreateDirectoryA(dir, NULL)) {
        fprintf(stdout, "[SETTINGS] Created directory: %s\n", dir);
        return true;
    }
    
    DWORD attribs = GetFileAttributesA(dir);
    if (attribs != INVALID_FILE_ATTRIBUTES && (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    
    fprintf(stderr, "[SETTINGS] Failed to create directory: %s\n", dir);
    return false;
#else
    const char *dir = settings_get_dir();
    if (mkdir(dir, 0755) == 0) {
        fprintf(stdout, "[SETTINGS] Created directory: %s\n", dir);
        return true;
    }
    
    struct stat st;
    if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    
    fprintf(stderr, "[SETTINGS] Failed to create directory: %s\n", dir);
    return false;
#endif
}

/**
 * Load volume settings from file
 */
static bool settings_load_volumes(int *music_volume, int *sfx_volume) {
    if (!music_volume || !sfx_volume) return false;
    
    const char *path = settings_get_path();
    FILE *fp = fopen(path, "r");
    
    if (!fp) {
        fprintf(stdout, "[SETTINGS] No existing settings file found: %s\n", path);
        return false;
    }
    
    int music_vol = -1;
    int sfx_vol = -1;
    int result = fscanf(fp, "%d %d", &music_vol, &sfx_vol);
    fclose(fp);
    
    if (result != 2) {
        fprintf(stderr, "[SETTINGS] Invalid settings file format\n");
        return false;
    }
    
    if (music_vol < 0 || music_vol > 128 || sfx_vol < 0 || sfx_vol > 128) {
        fprintf(stderr, "[SETTINGS] Invalid volume values in settings\n");
        return false;
    }
    
    *music_volume = music_vol;
    *sfx_volume = sfx_vol;
    
    fprintf(stdout, "[SETTINGS] Loaded volumes: Music=%d, SFX=%d\n", music_vol, sfx_vol);
    return true;
}

/**
 * Save volume settings to file
 */
static bool settings_save_volumes(int music_volume, int sfx_volume) {
    if (!settings_ensure_dir()) {
        fprintf(stderr, "[SETTINGS] Failed to ensure settings directory exists\n");
        return false;
    }
    
    const char *path = settings_get_path();
    FILE *fp = fopen(path, "w");
    
    if (!fp) {
        fprintf(stderr, "[SETTINGS] Failed to open settings file for writing: %s\n", path);
        return false;
    }
    
    fprintf(fp, "%d %d\n", music_volume, sfx_volume);
    fclose(fp);
    
    fprintf(stdout, "[SETTINGS] Saved volumes: Music=%d, SFX=%d to %s\n", 
            music_volume, sfx_volume, path);
    return true;
}

// ============================================================
// END VOLUME SETTINGS PERSISTENCE FUNCTIONS
// ============================================================

// ============================================================
// HIGH SCORE PERSISTENCE FUNCTIONS
// ============================================================

/**
 * Get the high scores file path
 */
static const char* high_scores_get_path(void) {
    static char scores_path[512] = {0};
    static bool initialized = false;
    
    if (initialized) return scores_path;
    
#ifdef _WIN32
    char appdata_path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata_path))) {
        snprintf(scores_path, sizeof(scores_path), "%s\\CometBuster\\highscores.txt", appdata_path);
    } else {
        const char *home = getenv("USERPROFILE");
        if (home) {
            snprintf(scores_path, sizeof(scores_path), "%s\\CometBuster\\highscores.txt", home);
        } else {
            strcpy(scores_path, ".\\CometBuster\\highscores.txt");
        }
    }
#else
    const char *home = getenv("HOME");
    if (home) {
        snprintf(scores_path, sizeof(scores_path), "%s/.cometbuster/highscores.txt", home);
    } else {
        strcpy(scores_path, "./.cometbuster/highscores.txt");
    }
#endif
    
    initialized = true;
    return scores_path;
}

/**
 * Create the high scores directory if it doesn't exist
 */
static bool high_scores_ensure_dir(void) {
#ifdef _WIN32
    char dir[512];
    strcpy(dir, high_scores_get_path());
    char *last_slash = strrchr(dir, '\\');
    if (last_slash) *last_slash = '\0';
    
    if (CreateDirectoryA(dir, NULL)) {
        return true;
    }
    
    DWORD attribs = GetFileAttributesA(dir);
    if (attribs != INVALID_FILE_ATTRIBUTES && (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    return false;
#else
    char dir[512];
    strcpy(dir, high_scores_get_path());
    char *last_slash = strrchr(dir, '/');
    if (last_slash) *last_slash = '\0';
    
    if (mkdir(dir, 0755) == 0) {
        return true;
    }
    
    struct stat st;
    if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return false;
#endif
}

/**
 * Load high scores from disk (text format)
 */
static void high_scores_load(CometBusterGame *game) {
    if (!game) return;
    
    game->high_score_count = 0;
    for (int i = 0; i < MAX_HIGH_SCORES; i++) {
        game->high_scores[i].score = 0;
        game->high_scores[i].wave = 0;
        game->high_scores[i].timestamp = 0;
        game->high_scores[i].player_name[0] = '\0';
    }
    
    const char *path = high_scores_get_path();
    FILE *fp = fopen(path, "r");
    
    if (!fp) {
        fprintf(stdout, "[HIGH SCORES] No existing high scores file\n");
        return;
    }
    
    while (game->high_score_count < MAX_HIGH_SCORES) {
        int score, wave;
        time_t timestamp;
        char name[32];
        
        if (fscanf(fp, "%d %d %ld %31s\n", &score, &wave, &timestamp, name) != 4) {
            break;
        }
        
        HighScore *hs = &game->high_scores[game->high_score_count];
        hs->score = score;
        hs->wave = wave;
        hs->timestamp = timestamp;
        strncpy(hs->player_name, name, sizeof(hs->player_name) - 1);
        hs->player_name[sizeof(hs->player_name) - 1] = '\0';
        
        game->high_score_count++;
    }
    
    fclose(fp);
    fprintf(stdout, "[HIGH SCORES] Loaded %d high scores\n", game->high_score_count);
}

/**
 * Save high scores to disk (text format)
 */
static void high_scores_save(CometBusterGame *game) {
    if (!game) return;
    
    if (!high_scores_ensure_dir()) {
        fprintf(stderr, "[HIGH SCORES] Failed to create directory\n");
        return;
    }
    
    const char *path = high_scores_get_path();
    FILE *fp = fopen(path, "w");
    
    if (!fp) {
        fprintf(stderr, "[HIGH SCORES] Failed to open file for writing\n");
        return;
    }
    
    for (int i = 0; i < game->high_score_count; i++) {
        HighScore *hs = &game->high_scores[i];
        fprintf(fp, "%d %d %ld %s\n", hs->score, hs->wave, hs->timestamp, hs->player_name);
    }
    
    fclose(fp);
    fprintf(stdout, "[HIGH SCORES] Saved %d high scores\n", game->high_score_count);
}

/**
 * Add a high score (maintains sorted order)
 */
static void high_scores_add(CometBusterGame *game, int score, int wave, const char *name) {
    if (!game || !name) return;
    
    int insert_pos = game->high_score_count;
    for (int i = 0; i < game->high_score_count; i++) {
        if (score > game->high_scores[i].score) {
            insert_pos = i;
            break;
        }
    }
    
    if (insert_pos >= MAX_HIGH_SCORES) {
        return;
    }
    
    if (game->high_score_count >= MAX_HIGH_SCORES) {
        for (int i = MAX_HIGH_SCORES - 1; i > insert_pos; i--) {
            game->high_scores[i] = game->high_scores[i - 1];
        }
    } else {
        for (int i = game->high_score_count; i > insert_pos; i--) {
            game->high_scores[i] = game->high_scores[i - 1];
        }
        game->high_score_count++;
    }
    
    HighScore *hs = &game->high_scores[insert_pos];
    hs->score = score;
    hs->wave = wave;
    hs->timestamp = time(NULL);
    strncpy(hs->player_name, name, sizeof(hs->player_name) - 1);
    hs->player_name[sizeof(hs->player_name) - 1] = '\0';
}

// ============================================================
// END HIGH SCORE PERSISTENCE FUNCTIONS
// ============================================================

// ============================================================
// HIGH SCORE ENTRY DIALOG FUNCTIONS
// ============================================================

/**
 * Show high score entry dialog when game ends and score is in top 10
 */
void on_high_score_dialog_submit(GtkWidget *widget, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return;
    
    const char *player_name = gtk_entry_get_text(GTK_ENTRY(gui->high_score_name_entry));
    
    // Only add score if player actually entered a name (not empty, not just placeholder)
    if (player_name && strlen(player_name) > 0) {
        high_scores_add(&gui->visualizer.comet_buster, 
                        gui->visualizer.comet_buster.score,
                        gui->visualizer.comet_buster.current_wave,
                        player_name);
        high_scores_save(&gui->visualizer.comet_buster);
        fprintf(stdout, "[HIGH SCORE] Added score for %s: %d (Wave %d)\n", 
                player_name, gui->visualizer.comet_buster.score, gui->visualizer.comet_buster.current_wave);
    }
    
    if (gui->high_score_dialog) {
        gtk_widget_destroy(gui->high_score_dialog);
        gui->high_score_dialog = NULL;
    }
    
    gui->game_paused = false;
}

gboolean on_high_score_dialog_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (gui) {
        gui->high_score_dialog = NULL;
        // Resume game (show game over screen)
        gui->game_paused = false;
    }
    return FALSE;
}

gboolean on_high_score_name_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return FALSE;
    
    // Submit on Enter key
    if (event->keyval == GDK_KEY_Return) {
        on_high_score_dialog_submit(NULL, gui);
        return TRUE;
    }
    
    return FALSE;
}

void on_show_high_score_entry(CometGUI *gui) {
    if (!gui) return;
    
    // If dialog already exists, bring it to front
    if (gui->high_score_dialog) {
        gtk_window_present(GTK_WINDOW(gui->high_score_dialog));
        return;
    }
    
    // Create high score entry dialog
    gui->high_score_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gui->high_score_dialog), "New High Score!");
    gtk_window_set_type_hint(GTK_WINDOW(gui->high_score_dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_default_size(GTK_WINDOW(gui->high_score_dialog), 400, 300);
    gtk_window_set_modal(GTK_WINDOW(gui->high_score_dialog), TRUE);
    
    g_signal_connect(gui->high_score_dialog, "delete-event", 
                    G_CALLBACK(on_high_score_dialog_delete), gui);
    
    // Create main vbox
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(gui->high_score_dialog), vbox);
    
    // Title
    GtkWidget *title_label = gtk_label_new("Congratulations!");
    PangoAttrList *attrs = pango_attr_list_new();
    PangoAttribute *weight = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    weight->start_index = 0;
    weight->end_index = -1;
    pango_attr_list_insert(attrs, weight);
    gtk_label_set_attributes(GTK_LABEL(title_label), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);
    
    // Score info
    GtkWidget *score_label = gtk_label_new("");
    char score_text[256];
    snprintf(score_text, sizeof(score_text), 
             "You achieved a HIGH SCORE!\n\nScore: %d\nWave Reached: %d",
             gui->visualizer.comet_buster.score,
             gui->visualizer.comet_buster.current_wave);
    gtk_label_set_text(GTK_LABEL(score_label), score_text);
    gtk_label_set_xalign(GTK_LABEL(score_label), 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), score_label, FALSE, FALSE, 0);
    
    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);
    
    // Name entry
    GtkWidget *name_label = gtk_label_new("Enter your name:");
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), name_label, FALSE, FALSE, 0);
    
    gui->high_score_name_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(gui->high_score_name_entry), 31);
    //gtk_entry_set_placeholder_text(GTK_ENTRY(gui->high_score_name_entry), "Player");
    g_signal_connect(gui->high_score_name_entry, "key-press-event",
                    G_CALLBACK(on_high_score_name_key_press), gui);
    gtk_box_pack_start(GTK_BOX(vbox), gui->high_score_name_entry, FALSE, FALSE, 0);
    
    // Button box
    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_CENTER);
    gtk_box_set_spacing(GTK_BOX(button_box), 10);
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);
    
    // Submit button
    GtkWidget *submit_button = gtk_button_new_with_label("Submit");
    g_signal_connect(submit_button, "clicked",
                    G_CALLBACK(on_high_score_dialog_submit), gui);
    gtk_container_add(GTK_CONTAINER(button_box), submit_button);
    
    // Skip button
    GtkWidget *skip_button = gtk_button_new_with_label("Skip");
    g_signal_connect(skip_button, "clicked",
                    G_CALLBACK(on_high_score_dialog_delete), gui);
    gtk_container_add(GTK_CONTAINER(button_box), skip_button);
    
    gtk_widget_show_all(gui->high_score_dialog);
    gtk_widget_grab_focus(gui->high_score_name_entry);
}

// ============================================================
// END HIGH SCORE ENTRY DIALOG FUNCTIONS
// ============================================================

// ============================================================
// HIGH SCORE VIEW DIALOG FUNCTIONS
// ============================================================

void on_view_high_scores(GtkWidget *widget, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return;
    
    // Create high score view dialog
    GtkWidget *dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(dialog), "High Scores");
    gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    
    g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_widget_destroy), NULL);
    
    // Create main vbox
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    gtk_container_add(GTK_CONTAINER(dialog), vbox);
    
    // Title
    GtkWidget *title_label = gtk_label_new("HIGH SCORES");
    PangoAttrList *attrs = pango_attr_list_new();
    PangoAttribute *weight = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    weight->start_index = 0;
    weight->end_index = -1;
    pango_attr_list_insert(attrs, weight);
    gtk_label_set_attributes(GTK_LABEL(title_label), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);
    
    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);
    
    // Scrolled window for high scores list
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    
    // Create text view for high scores
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_NONE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    
    // Get text buffer and add high scores
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(buffer, &iter);
    
    // Always reload high scores from disk
    high_scores_load(&gui->visualizer.comet_buster);
    
    // Format and add high scores to text buffer
    char header[100];
    snprintf(header, sizeof(header), "%-6s %-20s %-8s %-6s\n", 
             "Rank", "Player Name", "Score", "Wave");
    gtk_text_buffer_insert(buffer, &iter, header, -1);
    
    // Add separator line
    gtk_text_buffer_insert(buffer, &iter, 
                          "─────────────────────────────────────────\n", -1);
    
    // Add high scores
    if (gui->visualizer.comet_buster.high_score_count == 0) {
        gtk_text_buffer_insert(buffer, &iter, 
                              "No high scores yet. Get playing!\n", -1);
    } else {
        for (int i = 0; i < gui->visualizer.comet_buster.high_score_count; i++) {
            HighScore *score = &gui->visualizer.comet_buster.high_scores[i];
            char score_line[100];
            snprintf(score_line, sizeof(score_line), "#%-5d %-20s %-8d %-6d\n",
                     i + 1, score->player_name, score->score, score->wave);
            gtk_text_buffer_insert(buffer, &iter, score_line, -1);
        }
    }
    
    // Close button
    GtkWidget *close_button = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(close_button, "clicked", 
                            G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_box_pack_start(GTK_BOX(vbox), close_button, FALSE, FALSE, 0);
    
    gtk_widget_show_all(dialog);
}

// ============================================================
// END HIGH SCORE VIEW DIALOG FUNCTIONS
// ============================================================

gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data);
gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data);
gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer data);
gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data);
gboolean on_key_release(GtkWidget *widget, GdkEventKey *event, gpointer data);
void update_status_text(CometGUI *gui);
gboolean game_update_timer(gpointer data);
void on_about(GtkWidget *widget, gpointer data);
void on_game_controls(GtkWidget *widget, gpointer data);
void on_toggle_fullscreen(GtkWidget *widget, gpointer data);
void on_volume_dialog_open(GtkWidget *widget, gpointer data);
void on_music_volume_changed(GtkRange *range, gpointer data);
void on_sfx_volume_changed(GtkRange *range, gpointer data);
gboolean on_volume_dialog_delete(GtkWidget *widget, GdkEvent *event, gpointer data);
void update_volume_labels(CometGUI *gui);

// Volume control dialog functions
void update_volume_labels(CometGUI *gui) {
    if (!gui) return;
    
    char music_label[64];
    char sfx_label[64];
    
    // Display as percentage (0-100)
    int music_percent = (gui->music_volume * 100) / 128;
    int sfx_percent = (gui->sfx_volume * 100) / 128;
    
    snprintf(music_label, sizeof(music_label), "Music Volume: %d%%", music_percent);
    snprintf(sfx_label, sizeof(sfx_label), "Sound Effects Volume: %d%%", sfx_percent);
    
    gtk_label_set_text(GTK_LABEL(gui->music_volume_label), music_label);
    gtk_label_set_text(GTK_LABEL(gui->sfx_volume_label), sfx_label);
}

void on_music_volume_changed(GtkRange *range, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return;
    
    gui->music_volume = (int)gtk_range_get_value(range);
    
    // Update music volume using the new audio_wad function
    audio_set_music_volume(&gui->audio, gui->music_volume);
    
    update_volume_labels(gui);
    
    // Save settings to file
    settings_save_volumes(gui->music_volume, gui->sfx_volume);
}

void on_sfx_volume_changed(GtkRange *range, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return;
    
    gui->sfx_volume = (int)gtk_range_get_value(range);
    
    // Update SFX volume using the new audio_wad function
    audio_set_sfx_volume(&gui->audio, gui->sfx_volume);
    
    update_volume_labels(gui);
    
    // Save settings to file
    settings_save_volumes(gui->music_volume, gui->sfx_volume);
}

gboolean on_volume_dialog_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (gui) {
        gui->volume_dialog = NULL;
        // Resume the game
        gui->game_paused = false;
        fprintf(stdout, "[*] Game Resumed\n");
    }
    return FALSE;  // Allow window to close
}

void on_volume_dialog_open(GtkWidget *widget, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return;
    
    // If dialog already exists, bring it to front
    if (gui->volume_dialog) {
        gtk_window_present(GTK_WINDOW(gui->volume_dialog));
        return;
    }
    
    // Pause the game
    gui->game_paused = true;
    fprintf(stdout, "[*] Game Paused (Volume Dialog Open)\n");
    
    // Create volume control dialog window
    gui->volume_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gui->volume_dialog), "Volume Control");
    gtk_window_set_type_hint(GTK_WINDOW(gui->volume_dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_default_size(GTK_WINDOW(gui->volume_dialog), 400, 250);
    gtk_window_set_modal(GTK_WINDOW(gui->volume_dialog), FALSE);
    
    g_signal_connect(gui->volume_dialog, "delete-event", 
                    G_CALLBACK(on_volume_dialog_delete), gui);
    
    // Create main vbox
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    gtk_container_add(GTK_CONTAINER(gui->volume_dialog), vbox);
    
    // Title label
    GtkWidget *title_label = gtk_label_new("Audio Settings");
    PangoAttrList *attrs = pango_attr_list_new();
    PangoAttribute *weight = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    weight->start_index = 0;
    weight->end_index = -1;
    pango_attr_list_insert(attrs, weight);
    gtk_label_set_attributes(GTK_LABEL(title_label), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 0);
    
    // Separator
    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), separator1, FALSE, FALSE, 0);
    
    // Music volume section
    GtkWidget *music_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), music_vbox, FALSE, FALSE, 0);
    
    gui->music_volume_label = gtk_label_new("Music Volume: 100%");
    gtk_label_set_xalign(GTK_LABEL(gui->music_volume_label), 0.0);
    gtk_box_pack_start(GTK_BOX(music_vbox), gui->music_volume_label, FALSE, FALSE, 0);
    
    gui->music_volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 128, 1);
    gtk_scale_set_draw_value(GTK_SCALE(gui->music_volume_scale), FALSE);
    gtk_range_set_value(GTK_RANGE(gui->music_volume_scale), gui->music_volume);
    g_signal_connect(gui->music_volume_scale, "value-changed",
                    G_CALLBACK(on_music_volume_changed), gui);
    gtk_box_pack_start(GTK_BOX(music_vbox), gui->music_volume_scale, FALSE, FALSE, 0);
    
    // SFX volume section
    GtkWidget *sfx_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), sfx_vbox, FALSE, FALSE, 0);
    
    gui->sfx_volume_label = gtk_label_new("Sound Effects Volume: 100%");
    gtk_label_set_xalign(GTK_LABEL(gui->sfx_volume_label), 0.0);
    gtk_box_pack_start(GTK_BOX(sfx_vbox), gui->sfx_volume_label, FALSE, FALSE, 0);
    
    gui->sfx_volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 128, 1);
    gtk_scale_set_draw_value(GTK_SCALE(gui->sfx_volume_scale), FALSE);
    gtk_range_set_value(GTK_RANGE(gui->sfx_volume_scale), gui->sfx_volume);
    g_signal_connect(gui->sfx_volume_scale, "value-changed",
                    G_CALLBACK(on_sfx_volume_changed), gui);
    gtk_box_pack_start(GTK_BOX(sfx_vbox), gui->sfx_volume_scale, FALSE, FALSE, 0);
    
    // Separator
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), separator2, FALSE, FALSE, 0);
    
    // Info label
    GtkWidget *info_label = gtk_label_new("Move sliders to adjust volume\nChanges apply immediately");
    gtk_label_set_xalign(GTK_LABEL(info_label), 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);
    
    gtk_widget_show_all(gui->volume_dialog);
    update_volume_labels(gui);
}

void on_new_game(GtkWidget *widget, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return;
    
    // Close high score dialog if open
    if (gui->high_score_dialog) {
        gtk_widget_destroy(gui->high_score_dialog);
        gui->high_score_dialog = NULL;
    }
    
    // Reset high score dialog flag
    gui->high_score_dialog_shown = false;
    gui->game_paused = false;
    
    // Reset game state
    init_comet_buster_system(&gui->visualizer);
    fprintf(stdout, "[GAME] New Game Started\n");
}

void on_about(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "CometBuster v1.0.0\n\n"
        "A classic arcade-style space shooter\n"
        "Defend against incoming comets!\n\n"
        "Controls:\n"
        "A/D - Move Left/Right\n"
        "W/S - Move Up/Down\n"
        "Z/X - Rotate\n"
        "SPACE - Fire\n"
        "CTRL - Boost\n"
        "V - Volume Settings\n"
        "F11 - Fullscreen"
    );
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void on_game_controls(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "Game Controls:\n\n"
        "A/D - Move Left/Right\n"
        "W/S - Move Up/Down\n"
        "Z/X - Rotate Ship\n"
        "SPACE - Fire Weapons\n"
        "CTRL - Boost Speed\n"
        "V - Open Volume Settings\n"
        "F11 - Toggle Fullscreen\n"
        "ESC - Pause/Resume Game"
    );
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void on_toggle_fullscreen(GtkWidget *widget, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return;
    
    gui->is_fullscreen = !gui->is_fullscreen;
    
    if (gui->is_fullscreen) {
        gtk_window_fullscreen(GTK_WINDOW(gui->window));
        gtk_widget_hide(gui->menu_bar);
        fprintf(stdout, "[UI] Fullscreen ON\n");
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(gui->window));
        gtk_widget_show(gui->menu_bar);
        fprintf(stdout, "[UI] Fullscreen OFF\n");
    }
}

void update_status_text(CometGUI *gui) {
    if (!gui || !gui->status_label) return;
    
    char status[256];
    snprintf(status, sizeof(status),
             "Score: %d | FPS: %.1f",
             gui->visualizer.comet_buster.score,
             gui->frame_count > 0 ? (1000.0 / (gui->total_time / gui->frame_count)) : 0);
    
    gtk_label_set_text(GTK_LABEL(gui->status_label), status);
}

gboolean game_update_timer(gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return TRUE;
    
    if (!gui->game_paused) {
        // Update game
        update_comet_buster(&gui->visualizer, 1.0 / 60.0);
        
        // Check if game just ended and it's a high score
        if (gui->visualizer.comet_buster.game_over && !gui->high_score_dialog_shown) {
            gui->high_score_dialog_shown = true;
            
            // Check if this is a high score
            if (comet_buster_is_high_score(&gui->visualizer.comet_buster, 
                                           gui->visualizer.comet_buster.score)) {
                fprintf(stdout, "[HIGH SCORE] New high score detected: %d\n", 
                        gui->visualizer.comet_buster.score);
                // Pause game and show high score dialog
                gui->game_paused = true;
                on_show_high_score_entry(gui);
            }
        }
        
        // Update frame counter for FPS
        gui->frame_count++;
        gui->total_time += 16.67;  // Approximately 60 FPS
        
        // Update status every 60 frames to reduce label updates
        if (gui->frame_count % 60 == 0) {
            update_status_text(gui);
        }
        
        // Redraw
        gtk_widget_queue_draw(gui->drawing_area);
    }
    
    return TRUE;  // Continue timer
}

gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return FALSE;
    
    // Get widget dimensions
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    
    // Update visualizer dimensions
    gui->visualizer.width = allocation.width;
    gui->visualizer.height = allocation.height;
    
    // Draw game
    draw_comet_buster(&gui->visualizer, cr);
    
    return FALSE;
}

gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return FALSE;
    
    gui->visualizer.mouse_x = event->x;
    gui->visualizer.mouse_y = event->y;
    
    if (event->button == 1) gui->visualizer.mouse_left_pressed = true;
    if (event->button == 2) gui->visualizer.mouse_middle_pressed = true;
    if (event->button == 3) {
        gui->visualizer.mouse_right_pressed = true;
        
        // Right-click to restart game if game is over
        if (gui->visualizer.comet_buster.game_over) {
            fprintf(stdout, "[GAME] Restarting game via right-click...\n");
            
            // Reset the game
            comet_buster_reset_game(&gui->visualizer.comet_buster);
        }
    }
    
    return FALSE;
}

gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return FALSE;
    
    if (event->button == 1) gui->visualizer.mouse_left_pressed = false;
    if (event->button == 2) gui->visualizer.mouse_middle_pressed = false;
    if (event->button == 3) gui->visualizer.mouse_right_pressed = false;
    
    return FALSE;
}

gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return FALSE;
    
    gui->visualizer.last_mouse_x = gui->visualizer.mouse_x;
    gui->visualizer.last_mouse_y = gui->visualizer.mouse_y;
    gui->visualizer.mouse_x = event->x;
    gui->visualizer.mouse_y = event->y;
    gui->visualizer.mouse_just_moved = true;
    gui->visualizer.mouse_movement_timer = 0.5;
    
    return FALSE;
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return FALSE;
    
    switch (event->keyval) {
        case GDK_KEY_a:
        case GDK_KEY_A:
            gui->visualizer.key_a_pressed = true;
            break;
        case GDK_KEY_d:
        case GDK_KEY_D:
            gui->visualizer.key_d_pressed = true;
            break;
        case GDK_KEY_w:
        case GDK_KEY_W:
            gui->visualizer.key_w_pressed = true;
            break;
        case GDK_KEY_s:
        case GDK_KEY_S:
            gui->visualizer.key_s_pressed = true;
            break;
        case GDK_KEY_z:
        case GDK_KEY_Z:
            gui->visualizer.key_z_pressed = true;
            break;
        case GDK_KEY_x:
        case GDK_KEY_X:
            gui->visualizer.key_x_pressed = true;
            break;
        case GDK_KEY_space:
            gui->visualizer.key_space_pressed = true;
            break;
        case GDK_KEY_Control_L:
        case GDK_KEY_Control_R:
            gui->visualizer.key_ctrl_pressed = true;
            break;
        case GDK_KEY_v:
        case GDK_KEY_V:
            on_volume_dialog_open(NULL, gui);
            break;
        case GDK_KEY_F11:
            on_toggle_fullscreen(NULL, gui);
            return TRUE;
            break;
        case GDK_KEY_Escape:
            gui->game_paused = !gui->game_paused;
            fprintf(stdout, "%s\n", gui->game_paused ? "[*] Game Paused" : "[*] Game Resumed");
            break;
    }
    
    return FALSE;
}

gboolean on_key_release(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    if (!gui) return FALSE;
    
    switch (event->keyval) {
        case GDK_KEY_a:
        case GDK_KEY_A:
            gui->visualizer.key_a_pressed = false;
            break;
        case GDK_KEY_d:
        case GDK_KEY_D:
            gui->visualizer.key_d_pressed = false;
            break;
        case GDK_KEY_w:
        case GDK_KEY_W:
            gui->visualizer.key_w_pressed = false;
            break;
        case GDK_KEY_s:
        case GDK_KEY_S:
            gui->visualizer.key_s_pressed = false;
            break;
        case GDK_KEY_z:
        case GDK_KEY_Z:
            gui->visualizer.key_z_pressed = false;
            break;
        case GDK_KEY_x:
        case GDK_KEY_X:
            gui->visualizer.key_x_pressed = false;
            break;
        case GDK_KEY_space:
            gui->visualizer.key_space_pressed = false;
            break;
        case GDK_KEY_Control_L:
        case GDK_KEY_Control_R:
            gui->visualizer.key_ctrl_pressed = false;
            break;
    }
    
    return FALSE;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    CometGUI gui;
    memset(&gui, 0, sizeof(CometGUI));
    
    gui.volume_dialog = NULL;
    gui.music_volume_scale = NULL;
    gui.sfx_volume_scale = NULL;
    gui.music_volume_label = NULL;
    gui.sfx_volume_label = NULL;
    gui.music_volume = 100;  // Default
    gui.sfx_volume = 100;    // Default
    
    // High score dialog
    gui.high_score_dialog = NULL;
    gui.high_score_name_entry = NULL;
    gui.high_score_dialog_shown = false;
    
    // Initialize visualizer - will be set dynamically by on_draw
    gui.visualizer.width = 640;  // Default, will be updated in on_draw
    gui.visualizer.height = 480; // Default, will be updated in on_draw
    gui.visualizer.volume_level = 0.5;
    gui.visualizer.mouse_x = 400;
    gui.visualizer.mouse_y = 300;
    gui.visualizer.last_mouse_x = 400;
    gui.visualizer.last_mouse_y = 300;
    gui.visualizer.mouse_movement_timer = 0.0;
    gui.visualizer.mouse_just_moved = false;
    gui.visualizer.mouse_left_pressed = false;
    gui.visualizer.mouse_right_pressed = false;
    gui.visualizer.mouse_middle_pressed = false;
    
    // Initialize arcade-style keyboard input
    gui.visualizer.key_a_pressed = false;
    gui.visualizer.key_d_pressed = false;
    gui.visualizer.key_w_pressed = false;
    gui.visualizer.key_s_pressed = false;
    gui.visualizer.key_z_pressed = false;
    gui.visualizer.key_x_pressed = false;
    gui.visualizer.key_space_pressed = false;
    gui.visualizer.key_ctrl_pressed = false;
    
    // Initialize the game
    init_comet_buster_system(&gui.visualizer);
    
    // Load high scores from disk
    high_scores_load(&gui.visualizer.comet_buster);
    
    // Initialize audio system
    memset(&gui.audio, 0, sizeof(AudioManager));
    if (!audio_init(&gui.audio)) {
        fprintf(stderr, "Warning: Audio initialization failed, continuing without sound\n");
    }
    
    // Try to load WAD file with sounds
    if (!audio_load_wad(&gui.audio, "cometbuster.wad")) {
        fprintf(stderr, "Warning: Could not load cometbuster.wad, sounds will be silent\n");
    }
    
    // Copy audio system to visualizer so game code can access it
    gui.visualizer.audio = gui.audio;
    
    // Load saved volume settings from file
    if (settings_load_volumes(&gui.music_volume, &gui.sfx_volume)) {
        fprintf(stdout, "[AUDIO] Loaded saved volume settings\n");
    } else {
        fprintf(stdout, "[AUDIO] Using default volume settings\n");
    }
    
    // Apply loaded settings to audio system
    audio_set_music_volume(&gui.audio, gui.music_volume);
    audio_set_sfx_volume(&gui.audio, gui.sfx_volume);
    
    // Load background music tracks
#ifdef ExternalSound
    audio_play_music(&gui.audio, "music/track1.mp3", false);   // Load track 1
    audio_play_music(&gui.audio, "music/track2.mp3", false);   // Load track 2
    audio_play_music(&gui.audio, "music/track3.mp3", false);   // Load track 3
    audio_play_music(&gui.audio, "music/track4.mp3", false);   // Load track 4
    audio_play_music(&gui.audio, "music/track5.mp3", false);   // Load track 5
    audio_play_music(&gui.audio, "music/track6.mp3", false);   // Load track 6
    
    // Play a random track
    audio_play_random_music(&gui.audio);
#endif
    
    // Create window
    gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gui.window), "CometBuster");
    
    // Get screen dimensions using GdkDisplay (non-deprecated)
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gint screen_width = gdk_screen_get_width(screen);
    gint screen_height = gdk_screen_get_height(screen);
    
    // Set window size to 90% of screen
    gint window_width = (gint)(screen_width * 0.9);
    gint window_height = (gint)(screen_height * 0.9);
    gtk_window_set_default_size(GTK_WINDOW(gui.window), window_width, window_height);
    
    // Allow window to be resizable and maximizable
    gtk_window_set_resizable(GTK_WINDOW(gui.window), TRUE);
    gtk_window_maximize(GTK_WINDOW(gui.window));
    g_signal_connect(gui.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(gui.window, "key-press-event", G_CALLBACK(on_key_press), &gui);
    g_signal_connect(gui.window, "key-release-event", G_CALLBACK(on_key_release), &gui);
    
    // Create main vbox
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(gui.window), main_vbox);
    
    // Create menu bar
    GtkWidget *menu_bar = gtk_menu_bar_new();
    gui.menu_bar = menu_bar;  // Save reference for show/hide
    
    // Game menu
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_item = gtk_menu_item_new_with_label("Game");
    
    GtkWidget *new_game_item = gtk_menu_item_new_with_label("New Game");
    g_signal_connect(new_game_item, "activate", G_CALLBACK(on_new_game), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), new_game_item);
    
    GtkWidget *view_scores_item = gtk_menu_item_new_with_label("View High Scores");
    g_signal_connect(view_scores_item, "activate", G_CALLBACK(on_view_high_scores), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), view_scores_item);
    
    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator);
    
    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit_item, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_item);
    
    // Audio menu
    GtkWidget *audio_menu = gtk_menu_new();
    GtkWidget *audio_item = gtk_menu_item_new_with_label("Audio");
    
    GtkWidget *volume_settings_item = gtk_menu_item_new_with_label("Volume Settings (V)");
    g_signal_connect(volume_settings_item, "activate", G_CALLBACK(on_volume_dialog_open), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(audio_menu), volume_settings_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(audio_item), audio_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), audio_item);
    
    // Help menu
    GtkWidget *help_menu = gtk_menu_new();
    GtkWidget *help_item = gtk_menu_item_new_with_label("Help");
    
    GtkWidget *game_controls_item = gtk_menu_item_new_with_label("Game Controls");
    g_signal_connect(game_controls_item, "activate", G_CALLBACK(on_game_controls), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), game_controls_item);
    
    GtkWidget *fullscreen_item = gtk_menu_item_new_with_label("Toggle Fullscreen (F11)");
    g_signal_connect(fullscreen_item, "activate", G_CALLBACK(on_toggle_fullscreen), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), fullscreen_item);
    
    GtkWidget *help_separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), help_separator);
    
    GtkWidget *about_item = gtk_menu_item_new_with_label("About CometBuster");
    g_signal_connect(about_item, "activate", G_CALLBACK(on_about), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), help_item);
    
    gtk_box_pack_start(GTK_BOX(main_vbox), menu_bar, FALSE, FALSE, 0);
    
    // Create vbox for game content
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_vbox), vbox, TRUE, TRUE, 0);
    
    // Status label
    gui.status_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox), gui.status_label, FALSE, FALSE, 5);
    update_status_text(&gui);
    
    // Drawing area - scales with window
    gui.drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(gui.drawing_area, -1, -1);  // Dynamic sizing
    gtk_widget_set_can_focus(gui.drawing_area, TRUE);  // Allow focus
    gtk_widget_add_events(gui.drawing_area, 
                         GDK_BUTTON_PRESS_MASK | 
                         GDK_BUTTON_RELEASE_MASK | 
                         GDK_POINTER_MOTION_MASK |
                         GDK_KEY_PRESS_MASK |
                         GDK_KEY_RELEASE_MASK);
    g_signal_connect(gui.drawing_area, "draw", G_CALLBACK(on_draw), &gui);
    g_signal_connect(gui.drawing_area, "button-press-event", G_CALLBACK(on_button_press), &gui);
    g_signal_connect(gui.drawing_area, "button-release-event", G_CALLBACK(on_button_release), &gui);
    g_signal_connect(gui.drawing_area, "motion-notify-event", G_CALLBACK(on_motion_notify), &gui);
    g_signal_connect(gui.drawing_area, "key-press-event", G_CALLBACK(on_key_press), &gui);
    g_signal_connect(gui.drawing_area, "key-release-event", G_CALLBACK(on_key_release), &gui);
    gtk_box_pack_start(GTK_BOX(vbox), gui.drawing_area, TRUE, TRUE, 0);
    
    gtk_widget_show_all(gui.window);
    
    // Grab keyboard focus on drawing area so key events go there
    gtk_widget_grab_focus(gui.drawing_area);
    // Start game update timer (approximately 60 FPS)
    gui.update_timer_id = g_timeout_add(17, game_update_timer, &gui);  // ~60 FPS
    
    gtk_main();
    
    // Cleanup
    g_source_remove(gui.update_timer_id);
    comet_buster_cleanup(&gui.visualizer.comet_buster);
    audio_cleanup(&gui.audio);
    
    return 0;
}
