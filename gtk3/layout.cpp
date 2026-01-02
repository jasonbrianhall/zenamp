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
#include "visualization.h"
#include "midiplayer.h"
#include "dbopl_wrapper.h"
#include "wav_converter.h"
#include "audioconverter.h"
#include "convertoggtowav.h"
#include "convertopustowav.h"
#include "audio_player.h"
#include "vfs.h"
#include "icon.h"
#include "aiff.h"
#include "equalizer.h"

extern IconAnimationState *g_icon_animation;


// Helper functions for layout management
static void calculate_layout_config(LayoutManager *layout) {
    // Get screen info
    GdkScreen *screen = gdk_screen_get_default();
    int screen_width = gdk_screen_get_width(screen);
    int screen_height = gdk_screen_get_height(screen);
    
    // Determine if we should use compact layout
    //layout->config.is_compact = (screen_width <= 1024);
    layout->config.is_compact = false;
    // Adaptive base sizes based on screen resolution category
    int base_window_width, base_window_height, base_player_width;
    int base_vis_width, base_vis_height, base_queue_width, base_queue_height;
    
    if (screen_width <= 800 || screen_height <= 600) {
        // Very small screens
        base_window_width = 750;
        base_window_height = 550;
        base_player_width = 350;
        base_vis_width = 200;
        base_vis_height = 80;
        base_queue_width = 200;
        base_queue_height = 300;
    } else if (screen_width < 1200 || screen_height < 900) {
        // Medium screens
        base_window_width = 800;
        base_window_height = 600;
        base_player_width = 400;
        base_vis_width = 260;
        base_vis_height = 120;
        base_queue_width = 200;
        base_queue_height = 350;
    } else {
        // Large screens
        base_window_width = 900;
        base_window_height = 700;
        base_player_width = 500;
        base_vis_width = 400;
        base_vis_height = 200;
        base_queue_width = 300;
        base_queue_height = 400;
    }
    
    // Use appropriate reference resolution
    int ref_width = (screen_width < 1200) ? 1024 : 1920;
    int ref_height = (screen_height < 900) ? 768 : 1080;
    
    // Calculate sizes
    layout->config.window_width = scale_size(base_window_width, screen_width, ref_width);
    layout->config.window_height = scale_size(base_window_height, screen_height, ref_height);
    layout->config.player_width = scale_size(base_player_width, screen_width, ref_width);
    layout->config.vis_width = scale_size(base_vis_width, screen_width, ref_width);
    layout->config.vis_height = scale_size(base_vis_height, screen_height, ref_height);
    layout->config.queue_width = scale_size(base_queue_width, screen_width, ref_width);
    layout->config.queue_height = scale_size(base_queue_height, screen_height, ref_height);
    layout->config.icon_size = scale_size(64, screen_width, 1920);
    
    // Apply DPI scaling if needed
    int scale = 1; // We'll get this from the window later
    if (scale > 1) {
        layout->config.window_width /= scale;
        layout->config.window_height /= scale;
        layout->config.player_width /= scale;
        layout->config.vis_width /= scale;
        layout->config.vis_height /= scale;
        layout->config.queue_width /= scale;
        layout->config.queue_height /= scale;
    }
    
    // Apply minimums
    if (screen_width <= 800) {
        layout->config.window_width = screen_width;
        layout->config.window_height = screen_height;
        layout->config.vis_width = fmax(layout->config.vis_width, 180);
        layout->config.vis_height = fmax(layout->config.vis_height, 60);
        layout->config.queue_width = fmax(layout->config.queue_width, 180);
        layout->config.queue_height = fmax(layout->config.queue_height, 250);
    } else if (screen_width <= 1024) {
        layout->config.window_width = fmax(layout->config.window_width, 800);
        layout->config.window_height = fmax(layout->config.window_height, 600);
        layout->config.player_width = fmax(layout->config.player_width, 400);
        layout->config.vis_width = fmax(layout->config.vis_width, 220);
        layout->config.vis_height = fmax(layout->config.vis_height, 100);
        layout->config.queue_width = fmax(layout->config.queue_width, 250);
        layout->config.queue_height = fmax(layout->config.queue_height, 300);
    } else {
        layout->config.window_width = fmax(layout->config.window_width, 800);
        layout->config.window_height = fmax(layout->config.window_height, 600);
        layout->config.player_width = fmax(layout->config.player_width, 400);
        layout->config.vis_width = fmax(layout->config.vis_width, 300);
        layout->config.vis_height = fmax(layout->config.vis_height, 150);
        layout->config.queue_width = fmax(layout->config.queue_width, 250);
        layout->config.queue_height = fmax(layout->config.queue_height, 300);
    }
    
    // Icon size bounds
    layout->config.icon_size = fmax(layout->config.icon_size, 32);
    layout->config.icon_size = fmin(layout->config.icon_size, 96);
}

static void create_menu_bar(AudioPlayer *player) {
    GtkWidget *menubar = gtk_menu_bar_new();
    
    // File menu
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_item = gtk_menu_item_new_with_mnemonic("_File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_item);
    
    GtkWidget *open_item = gtk_menu_item_new_with_mnemonic("_Open File (Add & Play)");
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_item);
    g_signal_connect(open_item, "activate", G_CALLBACK(on_menu_open), player);

    GtkWidget *playlist_separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), playlist_separator);

    GtkWidget *load_playlist_item = gtk_menu_item_new_with_mnemonic("_Load Playlist...");
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), load_playlist_item);
    g_signal_connect(load_playlist_item, "activate", G_CALLBACK(on_menu_load_playlist), player);

    GtkWidget *save_playlist_item = gtk_menu_item_new_with_mnemonic("_Save Playlist...");
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_playlist_item);
    g_signal_connect(save_playlist_item, "activate", G_CALLBACK(on_menu_save_playlist), player);

    // ADD RECENT PLAYLISTS SUBMENU
    GtkWidget *recent_separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), recent_separator);
    
    GtkWidget *recent_playlist_item = gtk_menu_item_new_with_mnemonic("_Recent Playlists");
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), recent_playlist_item);
    
    // Create recent files submenu
    GtkWidget *recent_submenu = gtk_recent_chooser_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(recent_playlist_item), recent_submenu);
    
    // Filter to show only playlist files
    GtkRecentFilter *playlist_filter = gtk_recent_filter_new();
    gtk_recent_filter_set_name(playlist_filter, "Playlists");
    gtk_recent_filter_add_mime_type(playlist_filter, "audio/x-mpegurl");
    gtk_recent_filter_add_mime_type(playlist_filter, "audio/mpegurl");
    gtk_recent_filter_add_pattern(playlist_filter, "*.m3u");
    gtk_recent_filter_add_pattern(playlist_filter, "*.m3u8");
    gtk_recent_chooser_add_filter(GTK_RECENT_CHOOSER(recent_submenu), playlist_filter);
    gtk_recent_chooser_set_filter(GTK_RECENT_CHOOSER(recent_submenu), playlist_filter);
    
    // Set other properties
    gtk_recent_chooser_set_limit(GTK_RECENT_CHOOSER(recent_submenu), 10);
    gtk_recent_chooser_set_sort_type(GTK_RECENT_CHOOSER(recent_submenu), GTK_RECENT_SORT_MRU);
    gtk_recent_chooser_set_show_not_found(GTK_RECENT_CHOOSER(recent_submenu), FALSE);
    
    // Connect the activation signal
    g_signal_connect(recent_submenu, "item-activated", 
                     G_CALLBACK(on_recent_playlist_activated), player);

    GtkWidget *add_to_queue_playlist_item = gtk_menu_item_new_with_mnemonic("_Add to Queue... (CTRL+A)");
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), add_to_queue_playlist_item);
    g_signal_connect(add_to_queue_playlist_item, "activate", G_CALLBACK(on_add_to_queue_clicked), player);

    GtkWidget *clear_queue_playlist_item = gtk_menu_item_new_with_mnemonic("_Clear Queue... (CTRL+C)");
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), clear_queue_playlist_item);
    g_signal_connect(clear_queue_playlist_item, "activate", G_CALLBACK(on_clear_queue_clicked), player);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    
    GtkWidget *quit_item = gtk_menu_item_new_with_mnemonic("_Quit (CTRL+Q)");
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);
    g_signal_connect(quit_item, "activate", G_CALLBACK(on_menu_quit), player);
    
    // View menu
    GtkWidget *view_menu = gtk_menu_new();
    GtkWidget *view_item = gtk_menu_item_new_with_mnemonic("_View");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), view_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_item);

    // Toggle Queue Panel
    GtkWidget *toggle_queue_item = gtk_menu_item_new_with_mnemonic("_Toggle Queue/Equalizer Panel (F10)");
    //gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(toggle_queue_item), TRUE);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), toggle_queue_item);
    g_object_set_data(G_OBJECT(toggle_queue_item), "player", player);
    g_signal_connect(toggle_queue_item, "activate", 
                     G_CALLBACK(on_toggle_queue_panel), player);

    GtkWidget *view_separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), view_separator);

    // Toggle Fullscreen Visualization
    GtkWidget *toggle_fullscreen_item = gtk_menu_item_new_with_mnemonic("_Fullscreen Visualization (F9)");
    //gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(toggle_fullscreen_item), FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), toggle_fullscreen_item);
    g_object_set_data(G_OBJECT(toggle_fullscreen_item), "player", player);
    g_signal_connect(toggle_fullscreen_item, "activate", 
                     G_CALLBACK(on_toggle_fullscreen_visualization), player);

    // Store references for later updates
    player->layout.toggle_queue_menu_item = toggle_queue_item;
    player->layout.toggle_fullscreen_menu_item = toggle_fullscreen_item;

    // Help menu
    GtkWidget *help_menu = gtk_menu_new();
    GtkWidget *help_item = gtk_menu_item_new_with_mnemonic("_Help");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_item);

    GtkWidget *shortcuts_item = gtk_menu_item_new_with_mnemonic("_Keyboard Shortcuts");
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), shortcuts_item);
    g_signal_connect(shortcuts_item, "activate", G_CALLBACK(on_shortcuts_menu_clicked), player);    

    GtkWidget *about_item = gtk_menu_item_new_with_mnemonic("_About");
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_item);
    g_signal_connect(about_item, "activate", G_CALLBACK(on_menu_about), player);
    
    gtk_box_pack_start(GTK_BOX(player->layout.player_vbox), menubar, FALSE, FALSE, 0);
}

static void create_visualization_section(AudioPlayer *player) {
    // Initialize visualizer
    player->visualizer = visualizer_new();
    
    // Visualization section
    GtkWidget *vis_frame = gtk_frame_new("Visualization (Toggle FS with F9 or F)");
    gtk_box_pack_start(GTK_BOX(player->layout.content_vbox), vis_frame, TRUE, TRUE, 0);
    
    GtkWidget *vis_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(vis_frame), vis_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vis_vbox), 5);
    
    // Set visualizer size based on layout config
    gtk_widget_set_size_request(player->visualizer->drawing_area, 
                                player->layout.config.vis_width, 
                                player->layout.config.vis_height);
    
    // Create event box to handle double-click events
    GtkWidget *vis_event_box = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(vis_event_box), FALSE);
    gtk_event_box_set_above_child(GTK_EVENT_BOX(vis_event_box), FALSE);
    
    // Add tooltip for keyboard shortcuts
    gtk_widget_set_tooltip_text(vis_event_box, "Double-click or F/F9: Fullscreen | Q: Next | A: Previous");
    
    // Add visualization drawing area to event box
    gtk_container_add(GTK_CONTAINER(vis_event_box), player->visualizer->drawing_area);
    
    gtk_widget_add_events(vis_event_box, 
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_2BUTTON_PRESS | GDK_POINTER_MOTION_MASK |
        GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    gtk_widget_add_events(player->visualizer->drawing_area, 
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_2BUTTON_PRESS | GDK_POINTER_MOTION_MASK |
        GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

    g_signal_connect(vis_event_box, "button-press-event", 
                    G_CALLBACK(on_visualizer_button_press), player);
    g_signal_connect(player->visualizer->drawing_area, "button-press-event", 
                    G_CALLBACK(on_visualizer_button_press), player);
    g_signal_connect(player->visualizer->drawing_area, "button-release-event",
                    G_CALLBACK(on_visualizer_button_release), player);
    g_signal_connect(player->visualizer->drawing_area, "motion-notify-event",
                    G_CALLBACK(on_visualizer_motion), player);
    g_signal_connect(player->visualizer->drawing_area, "scroll-event",
                    G_CALLBACK(on_visualizer_scroll), player);
    g_signal_connect(player->visualizer->drawing_area, "enter-notify-event",
                    G_CALLBACK(on_visualizer_enter), player);
    g_signal_connect(player->visualizer->drawing_area, "leave-notify-event",
                    G_CALLBACK(on_visualizer_leave), player);
    
    // Add event box (containing drawing area) to the layout
    gtk_box_pack_start(GTK_BOX(vis_vbox), vis_event_box, TRUE, TRUE, 0);
    
    // Add visualization controls
    player->vis_controls = create_visualization_controls(player->visualizer);
    gtk_box_pack_start(GTK_BOX(vis_vbox), player->vis_controls, FALSE, FALSE, 0);
    
    printf("Double-click handler added to visualizer (toggles fullscreen)\n");
}

static void create_player_controls(AudioPlayer *player) {
    player->file_label = gtk_label_new("No file loaded");
    gtk_box_pack_start(GTK_BOX(player->layout.content_vbox), player->file_label, FALSE, FALSE, 0);
    
    player->progress_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 0.1);
    gtk_scale_set_draw_value(GTK_SCALE(player->progress_scale), FALSE);
    gtk_widget_set_sensitive(player->progress_scale, FALSE);
    gtk_widget_set_can_focus(player->progress_scale, TRUE);
    gtk_widget_set_tooltip_text(player->progress_scale, "Use ←/→ arrow keys or </> to seek");
    g_signal_connect(player->progress_scale, "value-changed", G_CALLBACK(on_progress_scale_value_changed), player);
    gtk_box_pack_start(GTK_BOX(player->layout.content_vbox), player->progress_scale, FALSE, FALSE, 0);
    
    player->time_label = gtk_label_new("00:00 / 00:00");
    gtk_box_pack_start(GTK_BOX(player->layout.content_vbox), player->time_label, FALSE, FALSE, 0);
    
    player->layout.nav_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(player->layout.nav_button_box), TRUE);
    gtk_box_pack_start(GTK_BOX(player->layout.content_vbox), player->layout.nav_button_box, FALSE, FALSE, 0);
    
    player->prev_button = gtk_button_new_with_label("|◄");
    player->rewind_button = gtk_button_new_with_label("◄◄ 5s");
    player->play_button = gtk_button_new_with_label("▶");
    player->pause_button = gtk_button_new_with_label("⏸");
    player->stop_button = gtk_button_new_with_label("⏹");
    player->fast_forward_button = gtk_button_new_with_label("5s ►►");
    player->next_button = gtk_button_new_with_label("▶|");
    
    gtk_widget_set_can_focus(player->prev_button, TRUE);
    gtk_widget_set_can_focus(player->rewind_button, TRUE);
    gtk_widget_set_can_focus(player->play_button, TRUE);
    gtk_widget_set_can_focus(player->pause_button, TRUE);
    gtk_widget_set_can_focus(player->stop_button, TRUE);
    gtk_widget_set_can_focus(player->fast_forward_button, TRUE);
    gtk_widget_set_can_focus(player->next_button, TRUE);
    
    gtk_widget_set_tooltip_text(player->prev_button, "Previous Track (P)");
    gtk_widget_set_tooltip_text(player->rewind_button, "Rewind 5s (< or ,)");
    gtk_widget_set_tooltip_text(player->play_button, "Play (Space)");
    gtk_widget_set_tooltip_text(player->pause_button, "Pause (Space)");
    gtk_widget_set_tooltip_text(player->stop_button, "Stop (S)");
    gtk_widget_set_tooltip_text(player->fast_forward_button, "Forward 5s (> or .)");
    gtk_widget_set_tooltip_text(player->next_button, "Next Track (N)");
    
    gtk_box_pack_start(GTK_BOX(player->layout.nav_button_box), player->prev_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.nav_button_box), player->rewind_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.nav_button_box), player->play_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.nav_button_box), player->pause_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.nav_button_box), player->stop_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.nav_button_box), player->fast_forward_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.nav_button_box), player->next_button, TRUE, TRUE, 0);
    
    player->layout.volume_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(player->layout.content_vbox), player->layout.volume_box, FALSE, FALSE, 0);

    GtkWidget *volume_label = gtk_label_new("Volume:");
    player->volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 5.0, 0.1);
    gtk_range_set_value(GTK_RANGE(player->volume_scale), (double)globalVolume / 100.0);
    gtk_widget_set_tooltip_text(player->volume_scale, "App volume (↑/↓ arrows) - independent of system volume");
    gtk_widget_set_can_focus(player->volume_scale, TRUE);

    GtkWidget *speed_label = gtk_label_new("Speed:");
    player->speed_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.1, 4.0, 0.5);
    gtk_range_set_value(GTK_RANGE(player->speed_scale), 1.0);
    gtk_scale_set_digits(GTK_SCALE(player->speed_scale), 1);
    gtk_widget_set_tooltip_text(player->speed_scale, "Playback speed (0.1x to 4.0x)");
    gtk_widget_set_can_focus(player->speed_scale, TRUE);

    gtk_widget_set_size_request(player->volume_scale, 200, -1);
    gtk_widget_set_size_request(player->speed_scale, 120, -1);

    gtk_box_pack_start(GTK_BOX(player->layout.volume_box), volume_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.volume_box), player->volume_scale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.volume_box), speed_label, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(player->layout.volume_box), player->speed_scale, FALSE, FALSE, 0);
}

static void create_queue_controls_compact(AudioPlayer *player) {
    printf("Creating compact queue controls layout\n");
    
    player->layout.compact.bottom_controls_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(player->layout.content_vbox), player->layout.compact.bottom_controls_hbox, FALSE, FALSE, 0);
    
    player->add_to_queue_button = gtk_button_new_with_label("Add");
    player->clear_queue_button = gtk_button_new_with_label("Clear");
    player->repeat_queue_button = gtk_check_button_new_with_label("Repeat");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(player->repeat_queue_button), TRUE);
    
    gtk_widget_set_can_focus(player->add_to_queue_button, TRUE);
    gtk_widget_set_can_focus(player->clear_queue_button, TRUE);
    gtk_widget_set_can_focus(player->repeat_queue_button, TRUE);
    
    gtk_widget_set_tooltip_text(player->add_to_queue_button, "Add to Queue (Ctrl+A)");
    gtk_widget_set_tooltip_text(player->clear_queue_button, "Clear Queue (Ctrl+C)");
    gtk_widget_set_tooltip_text(player->repeat_queue_button, "Toggle Repeat (R)");
    
    gtk_widget_set_size_request(player->add_to_queue_button, 80, 30);
    gtk_widget_set_size_request(player->clear_queue_button, 80, 30);
    
    gtk_box_pack_start(GTK_BOX(player->layout.compact.bottom_controls_hbox), player->add_to_queue_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.compact.bottom_controls_hbox), player->clear_queue_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.compact.bottom_controls_hbox), player->repeat_queue_button, TRUE, TRUE, 0);
}


static void create_queue_controls_regular(AudioPlayer *player) {
    printf("Creating regular queue controls layout\n");
    
    player->layout.regular.queue_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(player->layout.content_vbox), player->layout.regular.queue_button_box, FALSE, FALSE, 0);
    
    player->add_to_queue_button = gtk_button_new_with_label("Add to Queue");
    player->clear_queue_button = gtk_button_new_with_label("Clear Queue");
    player->repeat_queue_button = gtk_check_button_new_with_label("Repeat Queue");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(player->repeat_queue_button), TRUE);
    
    gtk_widget_set_can_focus(player->add_to_queue_button, TRUE);
    gtk_widget_set_can_focus(player->clear_queue_button, TRUE);
    gtk_widget_set_can_focus(player->repeat_queue_button, TRUE);
    
    gtk_widget_set_tooltip_text(player->add_to_queue_button, "Add to Queue (Ctrl+A)");
    gtk_widget_set_tooltip_text(player->clear_queue_button, "Clear Queue (Ctrl+C)");
    gtk_widget_set_tooltip_text(player->repeat_queue_button, "Toggle Repeat (R)");
    
    gtk_box_pack_start(GTK_BOX(player->layout.regular.queue_button_box), player->add_to_queue_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.regular.queue_button_box), player->clear_queue_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->layout.regular.queue_button_box), player->repeat_queue_button, TRUE, TRUE, 0);
}


static void create_icon_section(AudioPlayer *player) {
    player->layout.bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_end(GTK_BOX(player->layout.content_vbox), player->layout.bottom_box, FALSE, FALSE, 0);

    // ======== ANIMATED ICON WITH CLICK SUPPORT ========
    
    // Load icon and create image widget
    GdkPixbuf *small_icon = load_icon_from_base64();
    if (small_icon) {
        GdkPixbuf *scaled_icon = gdk_pixbuf_scale_simple(
            small_icon, 
            player->layout.config.icon_size, 
            player->layout.config.icon_size, 
            GDK_INTERP_BILINEAR
        );
        
        if (scaled_icon) {
            // Create image widget for animation
            GtkWidget *icon_image = gtk_image_new_from_pixbuf(scaled_icon);
            
            // Initialize animation state
            g_icon_animation = init_icon_animation(GTK_IMAGE(icon_image));
            
            if (g_icon_animation) {
                // Create event box to receive click events
                // (GtkImage doesn't have its own window, so we need event box)
                GtkWidget *icon_event_box = gtk_event_box_new();
                gtk_container_add(GTK_CONTAINER(icon_event_box), icon_image);
                
                // CRITICAL FIX: Enable button press events on the event box
                // This is what was missing!
                gtk_widget_add_events(icon_event_box, GDK_BUTTON_PRESS_MASK);
                
                // Connect click handler
                g_signal_connect(icon_event_box, "button-press-event",
                                G_CALLBACK(on_icon_button_press), player);
                
                // Make event box respond to clicks
                gtk_event_box_set_above_child(GTK_EVENT_BOX(icon_event_box), TRUE);
                
                // Add to layout
                gtk_box_pack_start(GTK_BOX(player->layout.bottom_box), 
                                  icon_event_box, FALSE, FALSE, 0);
                
                printf("✓ Animated icon initialized (click to play animation)\n");
            } else {
                // Fallback if animation initialization fails
                GtkWidget *icon_image_fallback = gtk_image_new_from_pixbuf(scaled_icon);
                gtk_box_pack_start(GTK_BOX(player->layout.bottom_box), 
                                  icon_image_fallback, FALSE, FALSE, 0);
                
                g_warning("Animation initialization failed, using static icon");
            }
            
            g_object_unref(scaled_icon);
        }
        g_object_unref(small_icon);
    }

    // ======== METADATA LABEL (unchanged) ========
    
    // Metadata label beside the icon
    player->metadata_label = gtk_label_new("No track loaded");
    gtk_label_set_use_markup(GTK_LABEL(player->metadata_label), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(player->metadata_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(player->metadata_label), 0.0);
    gtk_label_set_selectable(GTK_LABEL(player->metadata_label), TRUE);
    gtk_widget_set_margin_start(player->metadata_label, 10);
    gtk_box_pack_start(GTK_BOX(player->layout.bottom_box), player->metadata_label, TRUE, TRUE, 0);
}

static void create_queue_display(AudioPlayer *player) {
    player->layout.queue_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    
    // Set a SMALLER default width (what it starts at)
    // Full width would be ~950px, but start at config.queue_width for compact view
    gtk_widget_set_size_request(player->layout.queue_vbox, 
                               player->layout.config.queue_width, -1);
    
    // Pack with TRUE, TRUE so it can expand larger if user resizes
    gtk_box_pack_end(GTK_BOX(player->layout.main_hbox), player->layout.queue_vbox, TRUE, TRUE, 0);

    GtkWidget *queue_label = gtk_label_new("Queue:");
    gtk_widget_set_halign(queue_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(player->layout.queue_vbox), queue_label, FALSE, FALSE, 0);

    // THE SEARCH BAR HERE
    GtkWidget *search_bar = create_queue_search_bar(player);
    gtk_box_pack_start(GTK_BOX(player->layout.queue_vbox), search_bar, FALSE, FALSE, 0);

    player->queue_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(player->queue_scrolled_window), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    int adjusted_queue_height = player->layout.config.queue_height - 150;
    gtk_widget_set_size_request(player->queue_scrolled_window, 
                               player->layout.config.queue_width, 
                               adjusted_queue_height);

    // Create the tree view with columns
    create_queue_treeview(player);

    gtk_box_pack_start(GTK_BOX(player->layout.queue_vbox), player->queue_scrolled_window, TRUE, TRUE, 0);
    
    gtk_box_pack_end(GTK_BOX(player->layout.queue_vbox), player->layout.shared_equalizer, FALSE, FALSE, 0);
}

void create_queue_treeview(AudioPlayer *player) {
    // Create list store with 9 columns now (added COL_QUEUE_INDEX)
    player->queue_store = gtk_list_store_new(NUM_COLS,
        G_TYPE_STRING,  // COL_FILEPATH
        G_TYPE_STRING,  // COL_PLAYING
        G_TYPE_STRING,  // COL_FILENAME
        G_TYPE_STRING,  // COL_TITLE
        G_TYPE_STRING,  // COL_ARTIST
        G_TYPE_STRING,  // COL_ALBUM
        G_TYPE_STRING,  // COL_GENRE
        G_TYPE_STRING,  // COL_DURATION
        G_TYPE_STRING,  // GTK3
        G_TYPE_INT);    // COL_QUEUE_INDEX - NEW!
    
    // Create tree view
    GtkWidget *tree_view = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(player->queue_store));
    
    player->queue_tree_view = tree_view;
    
    // Create columns
    add_column(tree_view, "", COL_PLAYING, 30, FALSE);
    add_column(tree_view, "Filename", COL_FILENAME, 200, TRUE);
    add_column(tree_view, "Title", COL_TITLE, 180, TRUE);
    add_column(tree_view, "Artist", COL_ARTIST, 150, TRUE);
    add_column(tree_view, "Album", COL_ALBUM, 150, TRUE);
    add_column(tree_view, "Genre", COL_GENRE, 100, TRUE);
    add_column(tree_view, "Time", COL_DURATION, 60, TRUE);
    add_column(tree_view, "CD+G", COL_CDGK, 50, TRUE);
    // Note: COL_QUEUE_INDEX is not displayed as a column, it's just stored in the model
    
    // Enable sorting
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree_view), TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree_view), COL_FILENAME);
    
    // Connect click handler for double-click
    g_signal_connect(tree_view, "row-activated",
                     G_CALLBACK(on_queue_row_activated), player);
    
    // Connect right-click handler for context menu
    g_signal_connect(tree_view, "button-press-event",
                     G_CALLBACK(on_queue_context_menu), player);
    g_signal_connect(tree_view, "key-press-event",
                     G_CALLBACK(on_queue_key_press), player);
    
    // Add to scrolled window
    gtk_container_add(GTK_CONTAINER(player->queue_scrolled_window), tree_view);
    
    // Setup drag-and-drop (must be after tree view is created)
    setup_queue_drag_and_drop(player);
}

static void connect_widget_signals(AudioPlayer *player) {
    g_signal_connect(player->window, "delete-event", G_CALLBACK(on_window_delete_event), player);
    g_signal_connect(player->window, "destroy", G_CALLBACK(on_window_destroy), player);
    g_signal_connect(player->play_button, "clicked", G_CALLBACK(on_play_clicked), player);
    g_signal_connect(player->pause_button, "clicked", G_CALLBACK(on_pause_clicked), player);
    g_signal_connect(player->stop_button, "clicked", G_CALLBACK(on_stop_clicked), player);
    g_signal_connect(player->rewind_button, "clicked", G_CALLBACK(on_rewind_clicked), player);
    g_signal_connect(player->fast_forward_button, "clicked", G_CALLBACK(on_fast_forward_clicked), player);
    g_signal_connect(player->next_button, "clicked", G_CALLBACK(on_next_clicked), player);
    g_signal_connect(player->prev_button, "clicked", G_CALLBACK(on_previous_clicked), player);
    g_signal_connect(player->volume_scale, "value-changed", G_CALLBACK(on_volume_changed), player);
    g_signal_connect(player->speed_scale, "value-changed", G_CALLBACK(on_speed_changed), player);
    g_signal_connect(player->add_to_queue_button, "clicked", G_CALLBACK(on_add_to_queue_clicked), player);
    g_signal_connect(player->clear_queue_button, "clicked", G_CALLBACK(on_clear_queue_clicked), player);
    g_signal_connect(player->repeat_queue_button, "toggled", G_CALLBACK(on_repeat_queue_toggled), player);
    setup_keyboard_shortcuts(player);
}

static void hide_unused_layout(AudioPlayer *player) {
    if (player->layout.config.is_compact) {
        // Hide regular layout widgets
        if (player->layout.regular.queue_button_box) {
            gtk_widget_hide(player->layout.regular.queue_button_box);
        }
        // No need to hide eq_below_controls since equalizer is now in queue_vbox
    } else {
        // Hide compact layout widgets
        if (player->layout.compact.bottom_controls_hbox) {
            gtk_widget_hide(player->layout.compact.bottom_controls_hbox);
        }
    }
}

// Function to switch layouts at runtime (for future use)
void switch_layout(AudioPlayer *player, bool to_compact) {
    if (player->layout.config.is_compact == to_compact) {
        return; // Already in the desired layout
    }
    
    player->layout.config.is_compact = to_compact;
    
    if (to_compact) {
        // Switch to compact
        gtk_widget_hide(player->layout.regular.queue_button_box);
        gtk_widget_show_all(player->layout.compact.bottom_controls_hbox);
    } else {
        // Switch to regular
        gtk_widget_hide(player->layout.compact.bottom_controls_hbox);
        gtk_widget_show_all(player->layout.regular.queue_button_box);
    }
    // Equalizer stays visible in queue_vbox for both layouts
}

void create_main_window(AudioPlayer *player) {
#ifdef _WIN32
    // Mark for Windows single instance detection
    // The actual property will be set after gtk_widget_show_all() in main()
    static bool zenamp_window_marker = true;
    printf("Windows single instance marker prepared\n");
#endif
    
    // Calculate layout configuration first
    calculate_layout_config(&player->layout);
    
    // Create main window
    player->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(player->window), "Zenamp Audio Player");
    gtk_window_set_default_size(GTK_WINDOW(player->window), 
                               player->layout.config.window_width, 
                               player->layout.config.window_height);
    gtk_container_set_border_width(GTK_CONTAINER(player->window), 10);
    
    set_window_icon_from_base64(GTK_WINDOW(player->window));
    
    // Connect realize signal to handle DPI scaling
    g_signal_connect(player->window, "realize", G_CALLBACK(on_window_realize), player);
    g_signal_connect(player->window, "configure-event", G_CALLBACK(on_window_resize), player);

    // Main layout structure
    player->layout.main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(player->window), player->layout.main_hbox);
    
    // Player controls vbox (left side) - use TRUE, TRUE to allow expansion
    player->layout.player_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(player->layout.player_vbox, player->layout.config.player_width, -1);
    gtk_box_pack_start(GTK_BOX(player->layout.main_hbox), player->layout.player_vbox, TRUE, TRUE, 0);
    
    // Create menu bar
    create_menu_bar(player);
    
    // Content area for left side
    player->layout.content_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(player->layout.content_vbox), 10);
    gtk_box_pack_start(GTK_BOX(player->layout.player_vbox), player->layout.content_vbox, TRUE, TRUE, 0);
    
    // Create sections in new order:
    // Left side: visualization at top, controls below
    create_visualization_section(player);
    create_player_controls(player);

    // Create shared equalizer widget FIRST (before queue display)
    player->layout.shared_equalizer = create_equalizer_controls(player);
    //create_shared_equalizer(player);
    
    // Create both layout variants for queue controls (but only show the active one)
    //create_queue_controls_compact(player);
    create_queue_controls_regular(player);
    
    create_icon_section(player);
    
    // Right side: queue at top, equalizer at bottom (handled in create_queue_display)
    create_queue_display(player);
    
    create_tray_icon(player);
    gtk_status_icon_set_visible(player->tray_icon, TRUE); // Hidden initially
    
    g_signal_connect(player->window, "window-state-event", 
                     G_CALLBACK(on_window_state_event), player);
    
    // Connect all signals
    connect_widget_signals(player);
    
    printf("Created main window with %s layout (screen-based decision)\n", 
           player->layout.config.is_compact ? "compact" : "regular");
}

void create_shared_equalizer(AudioPlayer *player) {
    if (!player->layout.shared_equalizer) {
        printf("Creating shared equalizer widget\n");
        player->layout.shared_equalizer = create_equalizer_controls(player);
    }
}

void add_to_recent_files(const char* filepath, const char* mime_type) {
    GtkRecentManager *recent_manager = gtk_recent_manager_get_default();
    gchar *uri = g_filename_to_uri(filepath, NULL, NULL);
    
    if (uri) {
        GtkRecentData recent_data;
        memset(&recent_data, 0, sizeof(recent_data));
        
        recent_data.display_name = g_path_get_basename(filepath);
        recent_data.description = "Audio playlist";
        recent_data.mime_type = (gchar*)mime_type;
        recent_data.app_name = "Zenamp";
        recent_data.app_exec = "zenamp %f";
        recent_data.groups = NULL;
        recent_data.is_private = FALSE;
        
        gtk_recent_manager_add_full(recent_manager, uri, &recent_data);
        
        g_free(recent_data.display_name);
        g_free(uri);
        
        printf("Added to recent files: %s\n", filepath);
    }
}

// Callback for recent playlist items
void on_recent_playlist_activated(GtkRecentChooser *chooser, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    gchar *uri = gtk_recent_chooser_get_current_uri(chooser);
    
    if (uri) {
        gchar *filename = g_filename_from_uri(uri, NULL, NULL);
        if (filename) {
            printf("Loading recent playlist: %s\n", filename);
            
            if (load_m3u_playlist(player, filename)) {
                update_queue_display_with_filter(player);
                update_gui_state(player);
                
                // Start playing if queue has files
                if (player->queue.count > 0 && load_file_from_queue(player)) {
                    update_gui_state(player);
                }
            }
            
            g_free(filename);
        }
        g_free(uri);
    }
}

static void create_metadata_section(AudioPlayer *player) {
    GtkWidget *metadata_frame = gtk_frame_new("Track Information");
    gtk_box_pack_start(GTK_BOX(player->layout.content_vbox), metadata_frame, FALSE, FALSE, 0);
    
    player->metadata_label = gtk_label_new("No track information");
    gtk_label_set_line_wrap(GTK_LABEL(player->metadata_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(player->metadata_label), 0.0);
    gtk_label_set_selectable(GTK_LABEL(player->metadata_label), TRUE); // Allow copying
    gtk_widget_set_margin_start(player->metadata_label, 5);
    gtk_widget_set_margin_end(player->metadata_label, 5);
    gtk_widget_set_margin_top(player->metadata_label, 5);
    gtk_widget_set_margin_bottom(player->metadata_label, 5);
    gtk_container_add(GTK_CONTAINER(metadata_frame), player->metadata_label);
}

static void on_tray_show_window(GtkMenuItem *item, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    gtk_window_deiconify(GTK_WINDOW(player->window));
    gtk_window_present(GTK_WINDOW(player->window));
}

void create_tray_icon(AudioPlayer *player) {
    // Create tray icon
    GdkPixbuf *icon_pixbuf = load_icon_from_base64();
    
    if (icon_pixbuf) {
        GdkPixbuf *tray_icon_pixbuf = gdk_pixbuf_scale_simple(icon_pixbuf, 
                                                               22, 22, 
                                                               GDK_INTERP_BILINEAR);
        player->tray_icon = gtk_status_icon_new_from_pixbuf(tray_icon_pixbuf);
        g_object_unref(tray_icon_pixbuf);
        g_object_unref(icon_pixbuf);
        printf("Tray icon created from base64 icon\n");
    } else {
        player->tray_icon = gtk_status_icon_new_from_icon_name("multimedia-audio-player");
        printf("Tray icon created from icon name\n");
    }
    
    gtk_status_icon_set_tooltip_text(player->tray_icon, "Zenamp");
    
    // Check if embedded
    if (gtk_status_icon_is_embedded(player->tray_icon)) {
        printf("✓ Tray icon is embedded in system tray\n");
    } else {
        printf("✗ WARNING: Tray icon is NOT embedded (system tray may not be available)\n");
    }
    
    // Connect signals
    g_signal_connect(player->tray_icon, "activate", 
                     G_CALLBACK(on_tray_icon_activate), player);
    g_signal_connect(player->tray_icon, "popup-menu", 
                     G_CALLBACK(on_tray_icon_popup_menu), player);
    
    // Create popup menu
    player->tray_menu = gtk_menu_new();
    
    GtkWidget *play_item = gtk_menu_item_new_with_label("▶ Play");
    GtkWidget *pause_item = gtk_menu_item_new_with_label("⏸ Pause");
    GtkWidget *stop_item = gtk_menu_item_new_with_label("⏹ Stop");
    GtkWidget *prev_item = gtk_menu_item_new_with_label("|◄ Previous");
    GtkWidget *next_item = gtk_menu_item_new_with_label("►| Next");
    GtkWidget *sep = gtk_separator_menu_item_new();
    GtkWidget *show_item = gtk_menu_item_new_with_label("Show Window");
    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit (CTRL+Q)");
    
    g_signal_connect(play_item, "activate", G_CALLBACK(on_play_clicked), player);
    g_signal_connect(pause_item, "activate", G_CALLBACK(on_pause_clicked), player);
    g_signal_connect(stop_item, "activate", G_CALLBACK(on_stop_clicked), player);
    g_signal_connect(prev_item, "activate", G_CALLBACK(on_previous_clicked), player);
    g_signal_connect(next_item, "activate", G_CALLBACK(on_next_clicked), player);
    g_signal_connect(show_item, "activate", G_CALLBACK(on_tray_show_window), player);
    g_signal_connect(quit_item, "activate", G_CALLBACK(on_menu_quit), player);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), play_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), pause_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), stop_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), prev_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), next_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), sep);
    gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), show_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), quit_item);
    
    gtk_widget_show_all(player->tray_menu);
}

void on_tray_icon_activate(GtkStatusIcon *status_icon, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    // Toggle window visibility on left-click
    if (gtk_widget_get_visible(player->window)) {
        gtk_widget_hide(player->window);
    } else {
        gtk_window_deiconify(GTK_WINDOW(player->window));
        gtk_window_present(GTK_WINDOW(player->window));
    }
}

void on_tray_icon_popup_menu(GtkStatusIcon *status_icon, guint button, 
                              guint activate_time, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    gtk_menu_popup(GTK_MENU(player->tray_menu), NULL, NULL, 
                   gtk_status_icon_position_menu, status_icon, 
                   button, activate_time);
}

// Add window state event handler
gboolean on_window_state_event(GtkWidget *widget, GdkEventWindowState *event, 
                                gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    if (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) {
        // Window was minimized
        if (player->tray_icon) {
            gtk_status_icon_set_visible(player->tray_icon, TRUE);
        }
    } else {
        // Window was restored
        if (player->tray_icon) {
            gtk_status_icon_set_visible(player->tray_icon, FALSE);
        }
    }
    
    return FALSE;
}

void on_toggle_queue_panel(GtkCheckMenuItem *check_item, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer *)user_data;
    
    gboolean show_queue = gtk_check_menu_item_get_active(check_item);
    
    if (show_queue) {
        gtk_widget_show(player->layout.queue_vbox);
        printf("Queue panel shown\n");
    } else {
        gtk_widget_hide(player->layout.queue_vbox);
        printf("Queue panel hidden\n");
    }
}

void on_toggle_fullscreen_visualization(GtkCheckMenuItem *check_item, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer *)user_data;
    
    gboolean fullscreen = gtk_check_menu_item_get_active(check_item);
    
    // This function is declared in main.cpp, so we just call it
    extern void toggle_vis_fullscreen(AudioPlayer *player);
    
    toggle_vis_fullscreen(player);
}

gboolean on_visualizer_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    
    // Update mouse position
    vis->mouse_x = (int)event->x;
    vis->mouse_y = (int)event->y;
    vis->mouse_press_time = g_get_monotonic_time() / 1000000.0;
    
    // Handle double-click for fullscreen toggle (only enter fullscreen, don't exit)
    if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
        printf("Visualizer double-clicked\n");
        
        // Check if visualizer is already in fullscreen mode
        extern bool is_visualizer_fullscreen();
        if (is_visualizer_fullscreen()) {
            // Already fullscreen, don't toggle to avoid interfering with games
            printf("Visualizer already in fullscreen - ignoring double-click\n");
            return TRUE;
        }
        
        // Not in fullscreen, so enable it
        printf("Entering fullscreen mode\n");
        extern void toggle_vis_fullscreen(AudioPlayer *player);
        toggle_vis_fullscreen(player);
        return TRUE;
    }
    
    // Track button state
    switch (event->button) {
        case 1:
            vis->mouse_left_pressed = TRUE;
            break;
        case 2:
            vis->mouse_middle_pressed = TRUE;
            break;
        case 3:
            vis->mouse_right_pressed = TRUE;
            break;
    }
    
    return FALSE;
}

gboolean on_visualizer_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;

    switch (event->button) {
        case 1:
            vis->mouse_left_pressed = FALSE;
            break;
        case 2:
            vis->mouse_middle_pressed = FALSE;
            break;
        case 3:
            vis->mouse_right_pressed = FALSE;
            break;
    }
    
    return FALSE;
}

gboolean on_visualizer_motion(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    
    // Store previous position
    vis->mouse_last_x = vis->mouse_x;
    vis->mouse_last_y = vis->mouse_y;
    
    // Update current position
    vis->mouse_x = (int)event->x;
    vis->mouse_y = (int)event->y;
    
    // Calculate velocity
    double dt = 0.016666;
    vis->mouse_velocity_x = (vis->mouse_x - vis->mouse_last_x) / dt;
    vis->mouse_velocity_y = (vis->mouse_y - vis->mouse_last_y) / dt;
    
    // Calculate distance from center
    double center_x = vis->width / 2.0;
    double center_y = vis->height / 2.0;
    double dx = vis->mouse_x - center_x;
    double dy = vis->mouse_y - center_y;
    vis->mouse_distance_from_center = sqrt(dx*dx + dy*dy);
    
    return FALSE;
}

gboolean on_visualizer_enter(GtkWidget *widget, GdkEventCrossing *event, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    vis->mouse_over = TRUE;
    return FALSE;
}

gboolean on_visualizer_leave(GtkWidget *widget, GdkEventCrossing *event, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    vis->mouse_over = FALSE;
    vis->mouse_left_pressed = FALSE;
    vis->mouse_right_pressed = FALSE;
    vis->mouse_middle_pressed = FALSE;
    vis->mouse_velocity_x = 0;
    vis->mouse_velocity_y = 0;
    return FALSE;
}

gboolean on_visualizer_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    
    // Update mouse position from scroll event
    vis->mouse_x = (int)event->x;
    vis->mouse_y = (int)event->y;
    
    // Determine scroll direction
    // GDK_SCROLL_UP or GDK_SCROLL_SMOOTH with positive direction
    switch (event->direction) {
        case GDK_SCROLL_UP:
            vis->scroll_direction = 1;
            break;
        case GDK_SCROLL_DOWN:
            vis->scroll_direction = -1;
            break;
        case GDK_SCROLL_LEFT:
            // Could be used for different purposes
            vis->scroll_direction = 0;
            break;
        case GDK_SCROLL_RIGHT:
            // Could be used for different purposes
            vis->scroll_direction = 0;
            break;
        case GDK_SCROLL_SMOOTH:
            // Handle smooth scrolling (trackpad, etc.)
            if (event->delta_y > 0) {
                vis->scroll_direction = -1;
            } else if (event->delta_y < 0) {
                vis->scroll_direction = 1;
            } else {
                vis->scroll_direction = 0;
            }
            break;
    }
    
    return FALSE;
}
