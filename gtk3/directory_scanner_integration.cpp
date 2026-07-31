// Directory Scanner Integration for Zenamp main.cpp
// Add this to the top includes: #include "directory_scanner.hpp"
// Then add these functions and declarations to your main.cpp

#include "directory_scanner.hpp"
#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <thread>
#include <mutex>
#include <atomic>

// Forward declarations - from your audio_player.h/main.cpp
extern AudioPlayer *player;
extern void add_to_queue(Queue *queue, const char *filepath);
extern void update_queue_display_with_filter(AudioPlayer *player);
extern void update_gui_state(AudioPlayer *player);
extern bool load_file_from_queue(AudioPlayer *player);
extern void start_playback(AudioPlayer *player);

// ============================================================================
// Scan Progress Dialog State
// ============================================================================

struct ScanProgressState {
    GtkWidget *dialog;
    GtkWidget *label;
    GtkWidget *counter_label;
    GtkWidget *file_tree;
    GtkListStore *file_store;
    std::atomic<bool> cancel_requested{false};
    std::atomic<int> total_files{0};
};

static ScanProgressState *g_scan_state = nullptr;

// ============================================================================
// Idle callback for thread-safe UI updates
// ============================================================================

struct UpdateUIData {
    std::string current_file;
    int total_scanned;
};

static gboolean on_scan_progress_update(gpointer user_data) {
    UpdateUIData *data = static_cast<UpdateUIData *>(user_data);
    
    if (!g_scan_state || !g_scan_state->dialog) {
        delete data;
        return FALSE;
    }

    // Extract just the filename for display
    size_t last_slash = data->current_file.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos)
        ? data->current_file.substr(last_slash + 1)
        : data->current_file;

    gtk_label_set_text(GTK_LABEL(g_scan_state->label), filename.c_str());

    char counter_text[64];
    snprintf(counter_text, sizeof(counter_text), "Found: %d files", data->total_scanned);
    gtk_label_set_text(GTK_LABEL(g_scan_state->counter_label), counter_text);

    // Add file to tree view
    GtkTreeIter iter;
    gtk_list_store_append(g_scan_state->file_store, &iter);
    gtk_list_store_set(g_scan_state->file_store, &iter,
                       0, filename.c_str(),
                       -1);

    g_scan_state->total_files = data->total_scanned;

    delete data;
    return FALSE;
}

// ============================================================================
// Progress callback for scanner (runs in background thread)
// ============================================================================

static void scan_progress_callback(const std::string& current_file, int total_scanned) {
    if (!g_scan_state) return;

    if (g_scan_state->cancel_requested) {
        return; // Signal scanner to stop
    }

    // Queue UI update on main thread
    UpdateUIData *data = new UpdateUIData{current_file, total_scanned};
    g_idle_add(on_scan_progress_update, data);

    // Give main thread a chance to process
    g_usleep(1000);
}

// ============================================================================
// Background scan thread
// ============================================================================

struct ScanThreadData {
    std::string directory;
    bool recursive;
    std::vector<std::string> results;
};

static gpointer scan_thread_func(gpointer user_data) {
    ScanThreadData *data = static_cast<ScanThreadData *>(user_data);

    printf("Scan thread started for: %s\n", data->directory.c_str());

    // Perform the scan with progress callback
    data->results = DirectoryScanner::scanDirectory(
        data->directory,
        data->recursive,
        scan_progress_callback
    );

    printf("Scan thread complete: found %zu files\n", data->results.size());

    // Queue completion callback on main thread
    g_idle_add([](gpointer ud) -> gboolean {
        ScanThreadData *scan_data = static_cast<ScanThreadData *>(ud);

        if (g_scan_state && g_scan_state->dialog) {
            gtk_widget_destroy(g_scan_state->dialog);
            g_scan_state->dialog = nullptr;
        }

        // Add files to queue
        if (scan_data->results.size() > 0) {
            int added_count = 0;
            for (const auto& file : scan_data->results) {
                add_to_queue(&player->queue, file.c_str());
                added_count++;
            }

            printf("Added %d files to queue\n", added_count);

            // Update UI
            update_queue_display_with_filter(player);
            update_gui_state(player);

            // Show completion dialog
            char msg[256];
            snprintf(msg, sizeof(msg), "Successfully imported %zu music files", scan_data->results.size());
            GtkWidget *completion_dialog = gtk_message_dialog_new(
                GTK_WINDOW(player->window),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                msg
            );
            gtk_dialog_run(GTK_DIALOG(completion_dialog));
            gtk_widget_destroy(completion_dialog);
        } else {
            GtkWidget *msg_dialog = gtk_message_dialog_new(
                GTK_WINDOW(player->window),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_INFO,
                GTK_BUTTONS_OK,
                "No music files found in directory"
            );
            gtk_dialog_run(GTK_DIALOG(msg_dialog));
            gtk_widget_destroy(msg_dialog);
        }

        delete scan_data;
        if (g_scan_state) {
            delete g_scan_state;
            g_scan_state = nullptr;
        }

        return FALSE;
    }, scan_data);

    return nullptr;
}

// ============================================================================
// Scan progress dialog handlers
// ============================================================================

static gint on_scan_progress_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    if (g_scan_state) {
        g_scan_state->cancel_requested = true;
    }
    return TRUE; // Don't close immediately
}

static void on_scan_cancel_clicked(GtkButton *button, gpointer user_data) {
    if (g_scan_state) {
        g_scan_state->cancel_requested = true;
    }
}

// ============================================================================
// Create and show progress dialog
// ============================================================================

static void create_and_show_scan_dialog(const std::string& directory, bool recursive) {
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
    gtk_container_set_border_width(GTK_CONTAINER(content), 15);

    // Title label
    g_scan_state->label = gtk_label_new("Initializing scan...");
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(g_scan_state->label), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_pack_start(GTK_BOX(content), g_scan_state->label, FALSE, FALSE, 0);

    // Counter label
    g_scan_state->counter_label = gtk_label_new("Found: 0 files");
    gtk_box_pack_start(GTK_BOX(content), g_scan_state->counter_label, FALSE, FALSE, 0);

    // Progress bar
    GtkWidget *progress = gtk_progress_bar_new();
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress));
    gtk_box_pack_start(GTK_BOX(content), progress, FALSE, FALSE, 0);

    // File list with scrollbar
    GtkWidget *scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

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

    gtk_container_add(GTK_CONTAINER(scrolled), g_scan_state->file_tree);
    gtk_box_pack_start(GTK_BOX(content), scrolled, TRUE, TRUE, 0);

    gtk_widget_show_all(content);

    g_signal_connect(g_scan_state->dialog, "delete-event",
                     G_CALLBACK(on_scan_progress_delete), nullptr);
    g_signal_connect(g_scan_state->dialog, "response",
                     G_CALLBACK(on_scan_cancel_clicked), nullptr);

    gtk_widget_show(g_scan_state->dialog);

    // Start scan in background thread
    ScanThreadData *scan_data = new ScanThreadData{directory, recursive, {}};
    std::thread scan_thread(scan_thread_func, scan_data);
    scan_thread.detach();

    // Pulse progress bar while scanning
    g_timeout_add(100, [](gpointer data) -> gboolean {
        if (g_scan_state && g_scan_state->dialog && GTK_IS_WIDGET(g_scan_state->dialog)) {
            // Find the progress bar child
            GList *children = gtk_container_get_children(GTK_CONTAINER(
                gtk_dialog_get_content_area(GTK_DIALOG(g_scan_state->dialog))
            ));

            for (GList *l = children; l; l = l->next) {
                if (GTK_IS_PROGRESS_BAR(l->data)) {
                    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(l->data));
                    break;
                }
            }
            g_list_free(children);
            return TRUE;
        }
        return FALSE;
    }, nullptr);
}

// ============================================================================
// File chooser dialog
// ============================================================================

static void on_import_directory_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        gchar *folder_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        if (folder_path) {
            printf("Starting import from: %s\n", folder_path);
            create_and_show_scan_dialog(folder_path, true);
            g_free(folder_path);
        }
    }

    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void on_import_directory_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Import Music Directory",
        GTK_WINDOW(player->window),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Import", GTK_RESPONSE_ACCEPT,
        nullptr
    );

    // Set to home directory by default
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_get_home_dir());

    g_signal_connect(dialog, "response", G_CALLBACK(on_import_directory_response), nullptr);
    gtk_widget_show_all(dialog);
}

// ============================================================================
// Add to File Menu (call this during menu creation in main)
// ============================================================================

static void add_import_directory_menu(GtkWidget *file_menu) {
    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator);

    GtkWidget *import_item = gtk_menu_item_new_with_label("Import Directory...");
    g_signal_connect(import_item, "activate",
                     G_CALLBACK(on_import_directory_clicked), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), import_item);

    gtk_widget_show(separator);
    gtk_widget_show(import_item);
}

// ============================================================================
// Public API for direct use (no dialog)
// ============================================================================

void import_music_directory_direct(const std::string& directory_path, bool recursive) {
    if (!player) return;

    printf("Starting direct directory import: %s\n", directory_path.c_str());
    create_and_show_scan_dialog(directory_path, recursive);
}
