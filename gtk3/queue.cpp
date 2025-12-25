#include "audio_player.h"
#include "miniz.h"
#include <glib.h>
#include <string.h>
#include <filesystem>
   
namespace fs = std::filesystem;

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
        update_queue_display_with_filter(player);  // Changed from update_queue_display
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

        const char *ext = strrchr(player->queue.files[i], '.');
        if (ext && strcasecmp(ext, ".zip") == 0) {
            char *extracted_path = extract_audio_from_zip(player->queue.files[i]);
            if (extracted_path) {
                metadata = extract_metadata(extracted_path);
                g_free(extracted_path);
            } else {
                metadata = g_strdup("No metadata available");
            }
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
        
        g_free(metadata);
        
        char *basename = g_path_get_basename(player->queue.files[i]);
        
        //const char *ext = strrchr(player->queue.files[i], '.');
        const char *cdgk_indicator = "";
        if (ext && strcasecmp(ext, ".zip") == 0) {
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
        
        // Immediately update display to show all items
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

    for (int i = 0; i < player->queue.count; i++) {
        const char *filepath = player->queue.files[i];
        const char *ext = strrchr(filepath, '.');

        char *metadata = NULL;
        int duration_seconds = 0;

        if (ext && strcasecmp(ext, ".zip") == 0) {
            char *extracted_path = extract_audio_from_zip(filepath);
            if (extracted_path) {
                metadata = extract_metadata(extracted_path);
                duration_seconds = get_file_duration(extracted_path);
                unlink(extracted_path);
                g_free(extracted_path);
            } else {
                metadata = g_strdup("No metadata available");
            }
        } else {
            metadata = extract_metadata(filepath);
            duration_seconds = get_file_duration(filepath);
        }

        char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
        parse_metadata(metadata, title, artist, album, genre);
        g_free(metadata);

        char *basename = g_path_get_basename(filepath);

        bool matches = true;
        if (has_filter) {
            matches = matches_filter(basename, filter) ||
                      matches_filter(title, filter) ||
                      matches_filter(artist, filter) ||
                      matches_filter(album, filter) ||
                      matches_filter(genre, filter);
        }

        if (matches) {
            GtkTreeIter iter;
            gtk_list_store_append(player->queue_store, &iter);

            char duration_str[16];
            if (duration_seconds > 0) {
                snprintf(duration_str, sizeof(duration_str), "%d:%02d",
                         duration_seconds / 60, duration_seconds % 60);
            } else {
                strcpy(duration_str, "");
            }

            const char *cdgk_indicator = (ext && strcasecmp(ext, ".zip") == 0) ? "✓" : "";
            const char *indicator = (i == player->queue.current_index) ? "▶" : "";

            gtk_list_store_set(player->queue_store, &iter,
                COL_FILEPATH, filepath,
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

            visible_count++;
        }

        g_free(basename);
    }

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
