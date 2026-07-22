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
#include "audio_player.h"
#include "karafun.h"

static gboolean is_text_input_widget(GtkWidget *widget) {
    return GTK_IS_ENTRY(widget) || 
           GTK_IS_TEXT_VIEW(widget) || 
           GTK_IS_SEARCH_ENTRY(widget) ||
           GTK_IS_SPIN_BUTTON(widget);
}

gboolean on_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    GtkWidget *focused_widget = gtk_window_get_focus(GTK_WINDOW(player->window));
    
    // If a text input widget has focus, don't handle global shortcuts at all
    if (focused_widget && is_text_input_widget(focused_widget)) {
        return FALSE; // Let the text input handle all keys
    }
    
    gboolean ctrl_pressed = (event->state & GDK_CONTROL_MASK) != 0;
    gboolean shift_pressed = (event->state & GDK_SHIFT_MASK) != 0;
    
    // Check if queue has focus for special handling
    gboolean queue_focused = gtk_widget_has_focus(player->queue_tree_view);
    
    switch (event->keyval) {
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            // Enter: Play selected queue item if queue has focus
            if (queue_focused) {
                GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
                GtkTreeModel *model;
                GtkTreeIter iter;
                
                if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
                    int selected_index;
                    gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &selected_index, -1);
                    
                    if (selected_index == player->queue.current_index && player->is_playing) {
                        printf("Already playing this song\n");
                        return TRUE;
                    }
                    
                    stop_playback(player);
                    player->queue.current_index = selected_index;
                    
                    if (load_file_from_queue(player)) {
                        update_queue_display_with_filter(player);
                        update_gui_state(player);
                        start_playback(player);
                        printf("Started playing: %s\n", get_current_queue_file(&player->queue));
                        char *metadata = extract_metadata(get_current_queue_file(&player->queue));
                        char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
                        parse_metadata(metadata, title, artist, album, genre);
        
                        show_track_info_overlay(player->visualizer, title, artist, album,
                               get_file_duration(player->queue.files[player->queue.current_index]));
                        g_free(metadata);
                    }
                }
                return TRUE;
            }
            break;
        case GDK_KEY_F10:
            {
                gboolean is_visible = gtk_widget_get_visible(player->layout.queue_vbox);
                
                if (is_visible) {
                    gtk_widget_hide(player->layout.queue_vbox);
                    if (player->layout.toggle_queue_menu_item) {
                        g_signal_handlers_block_by_func(
                            player->layout.toggle_queue_menu_item,
                            (void*)on_toggle_queue_panel, player);
                        gtk_check_menu_item_set_active(
                            GTK_CHECK_MENU_ITEM(player->layout.toggle_queue_menu_item), 
                            FALSE);
                        g_signal_handlers_unblock_by_func(
                            player->layout.toggle_queue_menu_item,
                            (void*)on_toggle_queue_panel, player);
                    }
                    printf("Queue panel hidden (F10)\n");
                } else {
                    gtk_widget_show(player->layout.queue_vbox);
                    if (player->layout.toggle_queue_menu_item) {
                        g_signal_handlers_block_by_func(
                            player->layout.toggle_queue_menu_item,
                            (void*)on_toggle_queue_panel, player);
                        gtk_check_menu_item_set_active(
                            GTK_CHECK_MENU_ITEM(player->layout.toggle_queue_menu_item), 
                            TRUE);
                        g_signal_handlers_unblock_by_func(
                            player->layout.toggle_queue_menu_item,
                            (void*)on_toggle_queue_panel, player);
                    }
                    printf("Queue panel shown (F10)\n");
                }
            }
            return TRUE;            
        case GDK_KEY_x:
        case GDK_KEY_X:
            // X: Remove selected item if queue has focus
            if (queue_focused) {
                GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
                GtkTreeModel *model;
                GtkTreeIter iter;
                
                if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
                    int selected_index;
                    gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &selected_index, -1);
                    printf("Removing item %d from queue via keyboard\n", selected_index);
                    
                    bool was_current_playing = (selected_index == player->queue.current_index && player->is_playing);
                    bool queue_will_be_empty = (player->queue.count <= 1);
                    
                    if (remove_from_queue(&player->queue, selected_index)) {
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
                        }
                        update_queue_display_with_filter(player);
                        update_gui_state(player);
                    }
                }
                return TRUE;
            }
            break;
            
        case GDK_KEY_F9:
                toggle_vis_fullscreen(player);
                return TRUE;
        case GDK_KEY_F:
        case GDK_KEY_f:
        {
            gboolean alt_pressed = (event->state & GDK_MOD1_MASK) != 0;
            if (!alt_pressed) {
                toggle_vis_fullscreen(player);
                return TRUE;
            }
        }
        break;
            
        case GDK_KEY_q:
        case GDK_KEY_Q:
            if (ctrl_pressed) {
                gtk_main_quit();
                return TRUE;
            }
            // Q: Next visualization
            if (player->visualizer) {
                visualizer_prev_mode(player->visualizer);
                printf("Switched to prev visualization\n");
            }
            return TRUE;
            
        case GDK_KEY_a:
        case GDK_KEY_A:
            if (ctrl_pressed) {
                on_add_to_queue_clicked(NULL, player);
                return TRUE;
            }
            // A: Previous visualization
            if (player->visualizer) {
                visualizer_next_mode(player->visualizer);
                printf("Switched to next visualization\n");
            }
            return TRUE;
            
        case GDK_KEY_v:
        case GDK_KEY_V:
            // V: Toggle Karafun vocal track (no-op unless a .kfn is loaded)
            karafun_toggle_vocal_and_reload(player);
            return TRUE;
            
        case GDK_KEY_b:
        case GDK_KEY_B:
            // B: Toggle Karafun backing track (no-op unless a .kfn is loaded)
            karafun_toggle_backing_and_reload(player);
            return TRUE;
            
        case GDK_KEY_space:
            if (queue_focused) {
                return FALSE;
            }
            
            if (player->is_playing) {
                toggle_pause(player);
            } else if (player->is_loaded) {
                start_playback(player);
            }
            update_gui_state(player);
            return TRUE;
            
        case GDK_KEY_Delete:
        case GDK_KEY_d:
        case GDK_KEY_D:
            // Try to delete the selected queue item, or current if none selected
            if (player->queue.count > 0) {
                int index_to_delete = player->queue.current_index;
                
                // Try to get selected item from queue tree view
                GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(player->queue_tree_view));
                GtkTreeModel *model;
                GtkTreeIter iter;
                
                if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
                    gtk_tree_model_get(model, &iter, COL_QUEUE_INDEX, &index_to_delete, -1);
                    printf("Removing selected queue item (index %d) via keyboard\n", index_to_delete);
                } else {
                    printf("Removing current song (index %d) via keyboard\n", index_to_delete);
                }
                
                bool was_current_playing = (index_to_delete == player->queue.current_index && player->is_playing);
                bool queue_will_be_empty = (player->queue.count <= 1);
                
                if (remove_from_queue(&player->queue, index_to_delete)) {
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
                    }
                    update_queue_display_with_filter(player, false);
                    
                    // Select the next item after deletion
                    int next_index = (index_to_delete < player->queue.count) ? index_to_delete : index_to_delete - 1;
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
            }
            return TRUE;
            
        case GDK_KEY_N:
        case GDK_KEY_n:
            if (!ctrl_pressed) {
                next_song_filtered(player);
                return TRUE;
            }
            break;

        case GDK_KEY_P:
        case GDK_KEY_p:
            if (!ctrl_pressed) {
                previous_song_filtered(player);
                return TRUE;
            }
            break;
            
        case GDK_KEY_S:
        case GDK_KEY_s:
            if (!ctrl_pressed) {
                stop_playback(player);
                update_gui_state(player);
                return TRUE;
            }
            break;
        case GDK_KEY_R:
        case GDK_KEY_r:
            if (!ctrl_pressed) {
                player->queue.repeat_queue = !player->queue.repeat_queue;
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(player->repeat_queue_button), 
                                           player->queue.repeat_queue);
                printf("Queue repeat: %s\n", player->queue.repeat_queue ? "ON" : "OFF");
                return TRUE;
            }
            break;
            
        case GDK_KEY_Left:
        case GDK_KEY_comma:
        case GDK_KEY_less:
            rewind_5_seconds(player);
            return TRUE;
            
        case GDK_KEY_Right:
        case GDK_KEY_period:
        case GDK_KEY_greater:
            fast_forward_5_seconds(player);
            return TRUE;
            
        case GDK_KEY_Home:
            if (player->is_loaded) {
                seek_to_position(player, 0.0);
                gtk_range_set_value(GTK_RANGE(player->progress_scale), 0.0);
            }
            return TRUE;
            
        case GDK_KEY_End:
            if (player->is_loaded && player->queue.count > 1) {
                next_song_filtered(player);
            }
            return TRUE;
            
        case GDK_KEY_Up:
            {
                if (queue_focused) {
                    return FALSE;
                }
                double current = gtk_range_get_value(GTK_RANGE(player->volume_scale));
                double new_vol = current + 0.1;
                if (new_vol > 5.0) new_vol = 5.0;
                gtk_range_set_value(GTK_RANGE(player->volume_scale), new_vol);
                return TRUE;
            }
            break;
            
        case GDK_KEY_Down:
            {
                if (queue_focused) {
                    return FALSE;
                }
                double current = gtk_range_get_value(GTK_RANGE(player->volume_scale));
                double new_vol = current - 0.1;
                if (new_vol < 0.0) new_vol = 0.0;
                gtk_range_set_value(GTK_RANGE(player->volume_scale), new_vol);
                return TRUE;
            }
            break;
            
        case GDK_KEY_o:
            if (ctrl_pressed) {
                on_menu_open(NULL, player);
                return TRUE;
            }
            break;
        case GDK_KEY_C:
        case GDK_KEY_c:
            if (ctrl_pressed) {
                on_clear_queue_clicked(NULL, player);
                return TRUE;
            }
            break;
            
        case GDK_KEY_F1:
            show_keyboard_help(player);
            return TRUE;
            
        case GDK_KEY_F11:
            toggle_fullscreen(player);
            return TRUE;

        case GDK_KEY_0:
        case GDK_KEY_1:
        case GDK_KEY_2:
        case GDK_KEY_3:
        case GDK_KEY_4:
        case GDK_KEY_5:
        case GDK_KEY_6:
        case GDK_KEY_7:
        case GDK_KEY_8:
        case GDK_KEY_9:
            {
                int queue_pos = event->keyval - GDK_KEY_1;
                if (queue_pos < player->queue.count) {
                    stop_playback(player);
                    player->queue.current_index = queue_pos;
                    if (load_file_from_queue(player)) {
                        update_queue_display_with_filter(player);
                        update_gui_state(player);
                        start_playback(player);
                    }
                }
            }
            return TRUE;
    }
    
    return FALSE;
}
void toggle_fullscreen(AudioPlayer *player) {
    GdkWindow *gdk_window = gtk_widget_get_window(player->window);
    if (!gdk_window) {
        printf("Cannot toggle fullscreen: window not realized\n");
        return;
    }
    
    GdkWindowState state = gdk_window_get_state(gdk_window);
    
    if (state & GDK_WINDOW_STATE_FULLSCREEN) {
        gtk_window_unfullscreen(GTK_WINDOW(player->window));
        printf("Exiting fullscreen mode\n");
    } else {
        gtk_window_fullscreen(GTK_WINDOW(player->window));
        printf("Entering fullscreen mode\n");
    }
}

gboolean on_vis_fullscreen_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    gboolean ctrl_pressed = (event->state & GDK_CONTROL_MASK) != 0;
    
    switch (event->keyval) {
        case GDK_KEY_F9:
        case GDK_KEY_Escape:
        case GDK_KEY_F:
        case GDK_KEY_f:
            toggle_vis_fullscreen(player);
            return TRUE;
            
        case GDK_KEY_q:
        case GDK_KEY_Q:
            if (player->visualizer) {
                visualizer_prev_mode(player->visualizer);
                printf("Switched to previous visualization in fullscreen\n");
            }
            return TRUE;
            
        case GDK_KEY_a:
        case GDK_KEY_A:
            if (player->visualizer) {
                visualizer_next_mode(player->visualizer);
                printf("Switched to next visualization in fullscreen\n");
            }
            return TRUE;
            
        case GDK_KEY_v:
        case GDK_KEY_V:
            karafun_toggle_vocal_and_reload(player);
            return TRUE;
            
        case GDK_KEY_b:
        case GDK_KEY_B:
            karafun_toggle_backing_and_reload(player);
            return TRUE;
            
        case GDK_KEY_space:
            if (player->is_playing) {
                toggle_pause(player);
            } else if (player->is_loaded) {
                start_playback(player);
            }
            update_gui_state(player);
            return TRUE;
        case GDK_KEY_N:
        case GDK_KEY_n:
            if (!ctrl_pressed) {
                next_song_filtered(player);
                return TRUE;
            }
            break;
            
        case GDK_KEY_P:
        case GDK_KEY_p:
            if (!ctrl_pressed) {
                previous_song_filtered(player);
                return TRUE;
            }
            break;
        case GDK_KEY_S:
        case GDK_KEY_s:
            if (!ctrl_pressed) {
                stop_playback(player);
                update_gui_state(player);
                return TRUE;
            }
            break;
        case GDK_KEY_R:
        case GDK_KEY_r:
            if (!ctrl_pressed) {
                player->queue.repeat_queue = !player->queue.repeat_queue;
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(player->repeat_queue_button), 
                                           player->queue.repeat_queue);
                printf("Queue repeat: %s\n", player->queue.repeat_queue ? "ON" : "OFF");
                return TRUE;
            }
            break;
            
        case GDK_KEY_Left:
        case GDK_KEY_comma:
        case GDK_KEY_less:
            rewind_5_seconds(player);
            return TRUE;
            
        case GDK_KEY_Right:
        case GDK_KEY_period:
        case GDK_KEY_greater:
            fast_forward_5_seconds(player);
            return TRUE;
            
        case GDK_KEY_Home:
            if (player->is_loaded) {
                seek_to_position(player, 0.0);
                gtk_range_set_value(GTK_RANGE(player->progress_scale), 0.0);
            }
            return TRUE;
            
        case GDK_KEY_End:
            if (player->is_loaded && player->queue.count > 1) {
                next_song_filtered(player);
            }
            return TRUE;
            
        case GDK_KEY_Up:
            {
                double current = gtk_range_get_value(GTK_RANGE(player->volume_scale));
                double new_vol = current + 0.1;
                if (new_vol > 5.0) new_vol = 5.0;
                gtk_range_set_value(GTK_RANGE(player->volume_scale), new_vol);
                return TRUE;
            }
            
        case GDK_KEY_Down:
            {
                double current = gtk_range_get_value(GTK_RANGE(player->volume_scale));
                double new_vol = current - 0.1;
                if (new_vol < 0.0) new_vol = 0.0;
                gtk_range_set_value(GTK_RANGE(player->volume_scale), new_vol);
                return TRUE;
            }
            
        case GDK_KEY_F1:
            show_keyboard_help(player);
            return TRUE;
            
        case GDK_KEY_F11:
            toggle_fullscreen(player);
            return TRUE;

        case GDK_KEY_0:
        case GDK_KEY_1:
        case GDK_KEY_2:
        case GDK_KEY_3:
        case GDK_KEY_4:
        case GDK_KEY_5:
        case GDK_KEY_6:
        case GDK_KEY_7:
        case GDK_KEY_8:
        case GDK_KEY_9:
            {
                int queue_pos = event->keyval - GDK_KEY_1;
                if (queue_pos < player->queue.count) {
                    stop_playback(player);
                    player->queue.current_index = queue_pos;
                    if (load_file_from_queue(player)) {
                        update_queue_display_with_filter(player);
                        update_gui_state(player);
                        start_playback(player);
                        printf("Started playing: %s\n", get_current_queue_file(&player->queue));
                        char *metadata = extract_metadata(get_current_queue_file(&player->queue));
                        char title[256] = "", artist[256] = "", album[256] = "", genre[256] = "";
                        parse_metadata(metadata, title, artist, album, genre);
        
                        show_track_info_overlay(player->visualizer, title, artist, album,
                               get_file_duration(player->queue.files[player->queue.current_index]));
                        g_free(metadata);
                        
                    }
                }
            }
            return TRUE;
    }
    
    return FALSE;
}

void show_keyboard_help(AudioPlayer *player) {
    GdkScreen *screen = gtk_widget_get_screen(player->window);
    int screen_height = gdk_screen_get_height(screen);
    bool use_compact_dialog = (screen_height <= 700);
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Keyboard Shortcuts",
                                                    GTK_WINDOW(player->window),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "_Close", GTK_RESPONSE_CLOSE,
                                                    NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), use_compact_dialog ? 10 : 15);
    
    if (use_compact_dialog) {
        GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(scrolled, 400, 400);
        
        GtkWidget *label = gtk_label_new(
            "Shortcuts:\n\n"
            "Space - Play/Pause    S - Stop\n"
            "N - Next    P - Previous\n"
            "Q - Next Vis    A - Previous Vis\n"
            "< - Rewind 5s    > - Forward 5s\n"
            "↑ - Volume up    ↓ - Volume down\n"
            "Home - Beginning    End - Next song\n\n"
            "D/Del - Remove current song\n"
            "R - Toggle repeat    1-9 - Jump to #\n\n"
            "Ctrl+O - Open    Ctrl+A - Add queue\n"
            "Ctrl+C - Clear    Ctrl+Q - Quit\n"
            "F1  - This help    F11 - Fullscreen\n"
            "F9  - Visualization Fullscreen"
            "F10 - Toggle Queue"

        );
        
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled), label);
        gtk_container_add(GTK_CONTAINER(content_area), scrolled);
        
    } else {
        GtkWidget *label = gtk_label_new(
            "Keyboard Shortcuts:\n\n"
            "Playback Control:\n"
            "  Space\t\t- Play/Pause toggle\n"
            "  S\t\t- Stop\n"
            "  N\t\t- Next song\n"
            "  P\t\t- Previous song\n"
            "  Q\t\t- Next Visualization\n"
            "  A\t\t- Previous Visualization\n"
            "  , / < ←\t- Rewind 5 seconds\n"
            "  . / > →\t- Fast forward 5 seconds\n"
            "  Home\t- Go to beginning\n"
            "  End\t\t- Skip to next song\n\n"
            "Queue Management:\n"
            "  D / Delete\t- Remove current song from queue\n"
            "  R\t\t- Toggle repeat mode\n"
            "  1-9\t\t- Jump to queue position\n\n"
            "Volume:\n"
            "  ↑\t\t- Volume up\n"
            "  ↓\t\t- Volume down\n\n"
            "File Operations:\n"
            "  Ctrl+O\t- Open file\n"
            "  Ctrl+A\t- Add to queue\n"
            "  Ctrl+C\t- Clear queue\n"
            "  Ctrl+Q\t- Quit\n\n"
            "Display:\n"
            "  F1\t\t- Show this help\n"
            "  F9\t\t- Toggle Visualization Fullscreen"
            "  F10\t\t- Toggle Queue"
            "  F11\t\t- Toggle fullscreen\n"
            
        );
        
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
        gtk_container_add(GTK_CONTAINER(content_area), label);
    }
    
    if (use_compact_dialog) {
        gtk_window_set_default_size(GTK_WINDOW(dialog), 420, 450);
        gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);
    } else {
        gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    }
    
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void setup_keyboard_shortcuts(AudioPlayer *player) {
    gtk_widget_set_can_focus(player->window, TRUE);
    gtk_widget_grab_focus(player->window);
    
    g_signal_connect(player->window, "key-press-event", 
                     G_CALLBACK(on_key_press_event), player);
}

void on_shortcuts_menu_clicked(GtkMenuItem *menuitem, gpointer user_data) {
    (void)menuitem;
    show_keyboard_help((AudioPlayer*)user_data);
}
