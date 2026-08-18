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
#include <vector>
#include <algorithm>
#include <utility>
#include <ctime>
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
extern void on_menu_import_directory(GtkWidget *menuitem, gpointer user_data);

// GTK4 removed the old GtkMenuBar/GtkMenu/GtkMenuItem widgets entirely, along
// with the GdkEventBox/GdkEventButton/GdkEventMotion/etc. event model. These
// small helpers replace the menu widgets with GtkMenuButton+GtkPopover
// dropdowns (a common, low-risk GTK4 porting pattern that avoids adopting the
// full GMenu/GAction system everywhere), so the rest of this file's
// structure and callback signatures can stay close to the original.
static GtkWidget* menu_popover_new() {
    GtkWidget *popover = gtk_popover_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(box, 4);
    gtk_widget_set_margin_end(box, 4);
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);
    gtk_popover_set_child(GTK_POPOVER(popover), box);
    g_object_set_data(G_OBJECT(popover), "menu-box", box);
    return popover;
}

static GtkWidget* menu_popover_box(GtkWidget *popover) {
    return GTK_WIDGET(g_object_get_data(G_OBJECT(popover), "menu-box"));
}

// A clickable row that closes its popover after activating - the GTK4
// equivalent of a GtkMenuItem's "activate" signal.
static GtkWidget* menu_add_item(GtkWidget *popover, const char *label, GCallback callback, gpointer user_data) {
    GtkWidget *box = menu_popover_box(popover);
    GtkWidget *button = gtk_button_new_with_mnemonic(label);
    gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
    GtkWidget *child = gtk_button_get_child(GTK_BUTTON(button));
    if (GTK_IS_LABEL(child)) {
        gtk_label_set_xalign(GTK_LABEL(child), 0.0);
    }
    gtk_box_append(GTK_BOX(box), button);
    if (callback) {
        g_signal_connect(button, "clicked", callback, user_data);
    }
    g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_popover_popdown), popover);
    return button;
}

static GtkWidget* menu_add_separator(GtkWidget *popover) {
    GtkWidget *box = menu_popover_box(popover);
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(box), sep);
    return sep;
}

// A checkable row - the GTK4 equivalent of a GtkCheckMenuItem. Deliberately
// doesn't close the popover on click, since toggling a checkbox is often
// followed by checking other options.
static GtkWidget* menu_add_check_item(GtkWidget *popover, const char *label, GCallback callback, gpointer user_data) {
    GtkWidget *box = menu_popover_box(popover);
    GtkWidget *check = gtk_check_button_new_with_mnemonic(label);
    gtk_box_append(GTK_BOX(box), check);
    if (callback) {
        g_signal_connect(check, "toggled", callback, user_data);
    }
    return check;
}

// A row that opens a nested popover (submenu), e.g. "Recent Playlists".
static GtkWidget* menu_add_submenu(GtkWidget *popover, const char *label, GtkWidget *submenu_popover) {
    GtkWidget *box = menu_popover_box(popover);
    GtkWidget *button = gtk_menu_button_new();
    gtk_menu_button_set_label(GTK_MENU_BUTTON(button), label);
    gtk_menu_button_set_use_underline(GTK_MENU_BUTTON(button), TRUE);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), submenu_popover);
    // NOTE(gtk4): GtkMenuButton is its own widget in GTK4, not a GtkButton
    // subclass - gtk_button_set_has_frame(GTK_BUTTON(button), ...) here was
    // an invalid cast that silently did nothing, so the frame never came
    // off. Use GtkMenuButton's own has-frame accessor instead.
    gtk_menu_button_set_has_frame(GTK_MENU_BUTTON(button), FALSE);
    gtk_widget_set_halign(button, GTK_ALIGN_FILL);
    gtk_box_append(GTK_BOX(box), button);
    return button;
}

static GtkWidget* menu_button_new(const char *label, GtkWidget *popover) {
    GtkWidget *button = gtk_menu_button_new();
    gtk_menu_button_set_label(GTK_MENU_BUTTON(button), label);
    gtk_menu_button_set_use_underline(GTK_MENU_BUTTON(button), TRUE);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
    // See note above - this needs the GtkMenuButton-specific setter, not a
    // GTK_BUTTON() cast, or the frame never actually comes off.
    gtk_menu_button_set_has_frame(GTK_MENU_BUTTON(button), FALSE);
    return button;
}


// Helper functions for layout management
static void calculate_layout_config(LayoutManager *layout) {
    // Get screen info. GTK4 removed GdkScreen - monitor geometry comes from
    // GdkMonitor now. No window exists yet at this point in startup, so this
    // grabs the first monitor off the default display via GListModel
    // (gdk_display_get_monitor(display, n) was also removed).
    GdkDisplay *display = gdk_display_get_default();
    GdkRectangle monitor_geom = {0, 0, 1920, 1080};
    if (display) {
        GListModel *monitors = gdk_display_get_monitors(display);
        if (monitors && g_list_model_get_n_items(monitors) > 0) {
            GdkMonitor *monitor = GDK_MONITOR(g_list_model_get_item(monitors, 0));
            if (monitor) {
                gdk_monitor_get_geometry(monitor, &monitor_geom);
                g_object_unref(monitor);
            }
        }
    }
    int screen_width = monitor_geom.width;
    int screen_height = monitor_geom.height;
    
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
    layout->config.icon_size = scale_size(100, screen_width, 1920);
    
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
    layout->config.icon_size = fmax(layout->config.icon_size, 64);
    layout->config.icon_size = fmin(layout->config.icon_size, 512);
}

// GtkRecentChooserMenu is gone in GTK4 along with the rest of GtkRecentChooser
// - this rebuilds the same "Recent Playlists" list by hand from
// GtkRecentManager, filtered to playlist files and sorted most-recent-first.
static void populate_recent_playlists_popover(GtkWidget *submenu_popover, AudioPlayer *player) {
    GtkWidget *box = menu_popover_box(submenu_popover);

    // Clear any previous entries (rebuilt each time the submenu opens would
    // be nicer, but populating once at menu-build time matches the original
    // behavior closely enough here).
    GtkWidget *child = gtk_widget_get_first_child(box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(box), child);
        child = next;
    }

    GtkRecentManager *recent_manager = gtk_recent_manager_get_default();
    GList *items = gtk_recent_manager_get_items(recent_manager);

    // Filter to playlist files and collect with their modified time for MRU sort.
    std::vector<std::pair<time_t, GtkRecentInfo*>> playlists;
    for (GList *l = items; l; l = l->next) {
        GtkRecentInfo *info = (GtkRecentInfo*)l->data;
        const char *mime = gtk_recent_info_get_mime_type(info);
        const char *uri = gtk_recent_info_get_uri(info);
        bool is_playlist = (mime && (strcmp(mime, "audio/x-mpegurl") == 0 || strcmp(mime, "audio/mpegurl") == 0)) ||
                            (uri && (g_str_has_suffix(uri, ".m3u") || g_str_has_suffix(uri, ".m3u8")));
        if (is_playlist) {
            playlists.push_back(std::make_pair((time_t)gtk_recent_info_get_modified(info), gtk_recent_info_ref(info)));
        }
    }
    g_list_free_full(items, (GDestroyNotify)gtk_recent_info_unref);

    std::sort(playlists.begin(), playlists.end(),
              [](const auto &a, const auto &b) { return a.first > b.first; });

    int limit = 10;
    for (size_t i = 0; i < playlists.size() && (int)i < limit; i++) {
        GtkRecentInfo *info = playlists[i].second;
        const char *display_name = gtk_recent_info_get_display_name(info);
        const char *uri = gtk_recent_info_get_uri(info);

        GtkWidget *item = menu_add_item(submenu_popover, display_name ? display_name : uri,
                                        G_CALLBACK(on_recent_playlist_activated), player);
        g_object_set_data_full(G_OBJECT(item), "recent-uri", g_strdup(uri), g_free);
    }

    for (auto &p : playlists) {
        gtk_recent_info_unref(p.second);
    }

    if (playlists.empty()) {
        GtkWidget *empty_item = gtk_label_new("(No recent playlists)");
        gtk_widget_set_sensitive(empty_item, FALSE);
        gtk_box_append(GTK_BOX(box), empty_item);
    }
}

static void create_menu_bar(AudioPlayer *player) {
    // GtkMenuBar/GtkMenu/GtkMenuItem are all removed entirely in GTK4 - this
    // menu bar is built from GtkMenuButton+GtkPopover dropdowns instead (see
    // the menu_* helpers above).
    GtkWidget *menubar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    // Themed as a menu bar (background/spacing/hover states matching the
    // rest of the app's chrome) rather than a bare row of buttons.
    gtk_widget_add_css_class(menubar, "menubar");

    // File menu
    GtkWidget *file_popover = menu_popover_new();

    menu_add_item(file_popover, "_Open File (Add & Play)", G_CALLBACK(on_menu_open), player);
    menu_add_item(file_popover, "_Import Directory...", G_CALLBACK(on_menu_import_directory), player);
    menu_add_separator(file_popover);
    menu_add_item(file_popover, "_Load Playlist...", G_CALLBACK(on_menu_load_playlist), player);
    menu_add_item(file_popover, "_Save Playlist...", G_CALLBACK(on_menu_save_playlist), player);
    menu_add_separator(file_popover);

    GtkWidget *recent_popover = menu_popover_new();
    populate_recent_playlists_popover(recent_popover, player);
    menu_add_submenu(file_popover, "_Recent Playlists", recent_popover);

    menu_add_item(file_popover, "_Add to Queue... (CTRL+A)", G_CALLBACK(on_add_to_queue_clicked), player);
    menu_add_item(file_popover, "_Clear Queue... (CTRL+C)", G_CALLBACK(on_clear_queue_clicked), player);
    menu_add_separator(file_popover);
    menu_add_item(file_popover, "_Quit (CTRL+Q)", G_CALLBACK(on_menu_quit), player);

    GtkWidget *file_button = menu_button_new("_File", file_popover);
    gtk_box_append(GTK_BOX(menubar), file_button);

    // View menu
    GtkWidget *view_popover = menu_popover_new();

    GtkWidget *toggle_queue_item = menu_add_check_item(view_popover,
        "_Toggle Queue/Equalizer Panel (F10)", G_CALLBACK(on_toggle_queue_panel), player);

    menu_add_separator(view_popover);

    GtkWidget *toggle_fullscreen_item = menu_add_check_item(view_popover,
        "_Fullscreen Visualization (F9)", G_CALLBACK(on_toggle_fullscreen_visualization), player);

    // Store references for later updates
    player->layout.toggle_queue_menu_item = toggle_queue_item;
    player->layout.toggle_fullscreen_menu_item = toggle_fullscreen_item;

    GtkWidget *view_button = menu_button_new("_View", view_popover);
    gtk_box_append(GTK_BOX(menubar), view_button);

    // Help menu
    GtkWidget *help_popover = menu_popover_new();

    menu_add_item(help_popover, "_Keyboard Shortcuts", G_CALLBACK(on_shortcuts_menu_clicked), player);
    menu_add_item(help_popover, "_About", G_CALLBACK(on_menu_about), player);

    GtkWidget *help_button = menu_button_new("_Help", help_popover);
    gtk_box_append(GTK_BOX(menubar), help_button);

    gtk_box_append(GTK_BOX(player->layout.player_vbox), menubar);
}

static void create_visualization_section(AudioPlayer *player) {
    // Initialize visualizer
    player->visualizer = visualizer_new();
    
    // Visualization section
    GtkWidget *vis_frame = gtk_frame_new("Visualization (Toggle FS with F9 or F)");
    gtk_widget_set_vexpand(vis_frame, TRUE);
    gtk_box_append(GTK_BOX(player->layout.content_vbox), vis_frame);
    
    GtkWidget *vis_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_frame_set_child(GTK_FRAME(vis_frame), vis_vbox);
    gtk_widget_set_margin_start(vis_vbox, 5);
    gtk_widget_set_margin_end(vis_vbox, 5);
    gtk_widget_set_margin_top(vis_vbox, 5);
    gtk_widget_set_margin_bottom(vis_vbox, 5);
    
    // Set visualizer size based on layout config
    gtk_widget_set_size_request(player->visualizer->drawing_area, 
                                player->layout.config.vis_width, 
                                player->layout.config.vis_height);
    
    // GtkEventBox is removed entirely in GTK4 - every widget can receive
    // input directly now, so mouse/scroll handling attaches straight to the
    // drawing area via event controllers instead of wrapping it in a
    // separate event-catching widget.
    gtk_widget_set_tooltip_text(player->visualizer->drawing_area,
        "Double-click or F9: Fullscreen; Escape or F9 Exits Fullscreen | Q: Next | A: Previous");

    // "button-press-event"/"button-release-event" -> GtkGestureClick's
    // "pressed"/"released" (n_press replaces the old GDK_2BUTTON_PRESS check
    // for double-clicks).
    GtkGesture *click_gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), 0); // any button
    g_signal_connect(click_gesture, "pressed", G_CALLBACK(on_visualizer_button_press), player);
    g_signal_connect(click_gesture, "released", G_CALLBACK(on_visualizer_button_release), player);
    gtk_widget_add_controller(player->visualizer->drawing_area, GTK_EVENT_CONTROLLER(click_gesture));

    // "motion-notify-event"/"enter-notify-event"/"leave-notify-event" -> GtkEventControllerMotion.
    GtkEventController *motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "enter", G_CALLBACK(on_visualizer_enter), player);
    g_signal_connect(motion_controller, "leave", G_CALLBACK(on_visualizer_leave), player);
    g_signal_connect(motion_controller, "motion", G_CALLBACK(on_visualizer_motion), player);
    gtk_widget_add_controller(player->visualizer->drawing_area, motion_controller);

    // "scroll-event" -> GtkEventControllerScroll.
    GtkEventController *scroll_controller = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(scroll_controller, "scroll", G_CALLBACK(on_visualizer_scroll), player);
    gtk_widget_add_controller(player->visualizer->drawing_area, scroll_controller);
    
    // Add drawing area directly to the layout (no event box wrapper needed).
    gtk_widget_set_vexpand(player->visualizer->drawing_area, TRUE);
    gtk_box_append(GTK_BOX(vis_vbox), player->visualizer->drawing_area);
    
    // Add visualization controls
    player->vis_controls = create_visualization_controls(player->visualizer);
    gtk_box_append(GTK_BOX(vis_vbox), player->vis_controls);
    
    SDL_Log("Double-click handler added to visualizer (toggles fullscreen)");
}

static void create_player_controls(AudioPlayer *player) {
    player->file_label = gtk_label_new("No file loaded");
    gtk_box_append(GTK_BOX(player->layout.content_vbox), player->file_label);
    
    player->progress_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 0.1);
    gtk_scale_set_draw_value(GTK_SCALE(player->progress_scale), FALSE);
    gtk_widget_set_sensitive(player->progress_scale, FALSE);
    gtk_widget_set_can_focus(player->progress_scale, TRUE);
    gtk_widget_set_tooltip_text(player->progress_scale, "Use ←/→ arrow keys or </> to seek");
    g_signal_connect(player->progress_scale, "value-changed", G_CALLBACK(on_progress_scale_value_changed), player);
    gtk_box_append(GTK_BOX(player->layout.content_vbox), player->progress_scale);
    
    player->time_label = gtk_label_new("00:00 / 00:00");
    gtk_box_append(GTK_BOX(player->layout.content_vbox), player->time_label);
    
    player->layout.nav_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_set_homogeneous(GTK_BOX(player->layout.nav_button_box), TRUE);
    gtk_box_append(GTK_BOX(player->layout.content_vbox), player->layout.nav_button_box);
    
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
    
    // These all had expand=TRUE in the horizontal nav_button_box, which is
    // already gtk_box_set_homogeneous(TRUE) above - hexpand keeps them
    // sharing the available width evenly like the original pack_start did.
    gtk_widget_set_hexpand(player->prev_button, TRUE);
    gtk_widget_set_hexpand(player->rewind_button, TRUE);
    gtk_widget_set_hexpand(player->play_button, TRUE);
    gtk_widget_set_hexpand(player->pause_button, TRUE);
    gtk_widget_set_hexpand(player->stop_button, TRUE);
    gtk_widget_set_hexpand(player->fast_forward_button, TRUE);
    gtk_widget_set_hexpand(player->next_button, TRUE);

    gtk_box_append(GTK_BOX(player->layout.nav_button_box), player->prev_button);
    gtk_box_append(GTK_BOX(player->layout.nav_button_box), player->rewind_button);
    gtk_box_append(GTK_BOX(player->layout.nav_button_box), player->play_button);
    gtk_box_append(GTK_BOX(player->layout.nav_button_box), player->pause_button);
    gtk_box_append(GTK_BOX(player->layout.nav_button_box), player->stop_button);
    gtk_box_append(GTK_BOX(player->layout.nav_button_box), player->fast_forward_button);
    gtk_box_append(GTK_BOX(player->layout.nav_button_box), player->next_button);
    
    player->layout.volume_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(player->layout.content_vbox), player->layout.volume_box);

    GtkWidget *volume_label = gtk_label_new("Volume:");
    player->volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 5.0, 0.1);
    gtk_range_set_value(GTK_RANGE(player->volume_scale), (double)globalVolume / 100.0);
    // NOTE(gtk4): GtkScale's draw-value defaults to TRUE in GTK3 but FALSE in
    // GTK4, so these silently stopped showing their numeric value in the
    // port (the equalizer's bass/mid/treble scales already set this
    // explicitly, which is why those still show a number). Without it
    // there's no way to see the current value while dragging, which makes it
    // hard to tell exactly when speed is back at 1.0.
    gtk_scale_set_draw_value(GTK_SCALE(player->volume_scale), TRUE);
    gtk_scale_set_digits(GTK_SCALE(player->volume_scale), 1);
    gtk_widget_set_tooltip_text(player->volume_scale, "App volume (↑/↓ arrows) - independent of system volume");
    gtk_widget_set_can_focus(player->volume_scale, TRUE);
    gtk_widget_set_hexpand(player->volume_scale, TRUE);

    GtkWidget *speed_label = gtk_label_new("Speed:");
    // Had a 5px pack_start padding - GTK4 has no per-child padding argument,
    // so that becomes a start margin on the widget itself.
    gtk_widget_set_margin_start(speed_label, 5);
    player->speed_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.1, 4.0, 0.5);
    gtk_range_set_value(GTK_RANGE(player->speed_scale), 1.0);
    gtk_scale_set_draw_value(GTK_SCALE(player->speed_scale), TRUE);
    gtk_scale_set_digits(GTK_SCALE(player->speed_scale), 1);
    gtk_widget_set_tooltip_text(player->speed_scale, "Playback speed (0.1x to 4.0x)");
    gtk_widget_set_can_focus(player->speed_scale, TRUE);

    gtk_widget_set_size_request(player->volume_scale, 200, -1);
    gtk_widget_set_size_request(player->speed_scale, 120, -1);

    gtk_box_append(GTK_BOX(player->layout.volume_box), volume_label);
    gtk_box_append(GTK_BOX(player->layout.volume_box), player->volume_scale);
    gtk_box_append(GTK_BOX(player->layout.volume_box), speed_label);
    gtk_box_append(GTK_BOX(player->layout.volume_box), player->speed_scale);
}

static void create_queue_controls_compact(AudioPlayer *player) {
    SDL_Log("Creating compact queue controls layout");
    
    player->layout.compact.bottom_controls_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(player->layout.content_vbox), player->layout.compact.bottom_controls_hbox);
    
    player->add_to_queue_button = gtk_button_new_with_label("Add");
    player->clear_queue_button = gtk_button_new_with_label("Clear");
    player->repeat_queue_button = gtk_check_button_new_with_label("Repeat");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(player->repeat_queue_button), TRUE);
    
    gtk_widget_set_can_focus(player->add_to_queue_button, TRUE);
    gtk_widget_set_can_focus(player->clear_queue_button, TRUE);
    gtk_widget_set_can_focus(player->repeat_queue_button, TRUE);
    
    gtk_widget_set_tooltip_text(player->add_to_queue_button, "Add to Queue (Ctrl+A)");
    gtk_widget_set_tooltip_text(player->clear_queue_button, "Clear Queue (Ctrl+C)");
    gtk_widget_set_tooltip_text(player->repeat_queue_button, "Toggle Repeat (R)");
    
    gtk_widget_set_size_request(player->add_to_queue_button, 80, 30);
    gtk_widget_set_size_request(player->clear_queue_button, 80, 30);
    
    gtk_widget_set_hexpand(player->add_to_queue_button, TRUE);
    gtk_widget_set_hexpand(player->clear_queue_button, TRUE);
    gtk_widget_set_hexpand(player->repeat_queue_button, TRUE);
    gtk_box_append(GTK_BOX(player->layout.compact.bottom_controls_hbox), player->add_to_queue_button);
    gtk_box_append(GTK_BOX(player->layout.compact.bottom_controls_hbox), player->clear_queue_button);
    gtk_box_append(GTK_BOX(player->layout.compact.bottom_controls_hbox), player->repeat_queue_button);
}


static void create_queue_controls_regular(AudioPlayer *player) {
    SDL_Log("Creating regular queue controls layout");
    
    player->layout.regular.queue_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(player->layout.content_vbox), player->layout.regular.queue_button_box);
    
    player->add_to_queue_button = gtk_button_new_with_label("Add to Queue");
    player->clear_queue_button = gtk_button_new_with_label("Clear Queue");
    player->repeat_queue_button = gtk_check_button_new_with_label("Repeat Queue");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(player->repeat_queue_button), TRUE);
    
    gtk_widget_set_can_focus(player->add_to_queue_button, TRUE);
    gtk_widget_set_can_focus(player->clear_queue_button, TRUE);
    gtk_widget_set_can_focus(player->repeat_queue_button, TRUE);
    
    gtk_widget_set_tooltip_text(player->add_to_queue_button, "Add to Queue (Ctrl+A)");
    gtk_widget_set_tooltip_text(player->clear_queue_button, "Clear Queue (Ctrl+C)");
    gtk_widget_set_tooltip_text(player->repeat_queue_button, "Toggle Repeat (R)");
    
    gtk_widget_set_hexpand(player->add_to_queue_button, TRUE);
    gtk_widget_set_hexpand(player->clear_queue_button, TRUE);
    gtk_widget_set_hexpand(player->repeat_queue_button, TRUE);
    gtk_box_append(GTK_BOX(player->layout.regular.queue_button_box), player->add_to_queue_button);
    gtk_box_append(GTK_BOX(player->layout.regular.queue_button_box), player->clear_queue_button);
    gtk_box_append(GTK_BOX(player->layout.regular.queue_button_box), player->repeat_queue_button);
}


static void create_icon_section(AudioPlayer *player) {
    player->layout.bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(player->layout.content_vbox), player->layout.bottom_box);

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
            // GtkImage's own size negotiation isn't reliable here - testing
            // showed it measuring a 200x200 pixbuf as a 16x16 natural size
            // (likely a theme/CSS icon-size default), so pixel_size must be
            // set explicitly to force the actual display size.
            gtk_image_set_pixel_size(GTK_IMAGE(icon_image), player->layout.config.icon_size);
            
            // Initialize animation state (scales every frame, including the
            // first, to player->layout.config.icon_size - see icon.cpp)
            g_icon_animation = init_icon_animation(GTK_IMAGE(icon_image), player->layout.config.icon_size);
            
            if (g_icon_animation) {
                // GtkEventBox is removed entirely in GTK4 - every widget can
                // receive input directly now (the old "GtkImage doesn't have
                // its own window" limitation doesn't apply anymore), so the
                // click gesture attaches straight to icon_image.
                GtkGesture *click_gesture = gtk_gesture_click_new();
                g_signal_connect(click_gesture, "pressed",
                                G_CALLBACK(on_icon_button_press), player);
                gtk_widget_add_controller(icon_image, GTK_EVENT_CONTROLLER(click_gesture));
                
                // Add to layout
                gtk_box_append(GTK_BOX(player->layout.bottom_box), icon_image);
                
                SDL_Log("✓ Animated icon initialized (click to play animation)");
            } else {
                // Fallback if animation initialization fails
                GtkWidget *icon_image_fallback = gtk_image_new_from_pixbuf(scaled_icon);
                gtk_image_set_pixel_size(GTK_IMAGE(icon_image_fallback), player->layout.config.icon_size);
                gtk_box_append(GTK_BOX(player->layout.bottom_box), icon_image_fallback);
                
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
    gtk_label_set_wrap(GTK_LABEL(player->metadata_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(player->metadata_label), 0.0);
    gtk_label_set_selectable(GTK_LABEL(player->metadata_label), TRUE);
    gtk_widget_set_margin_start(player->metadata_label, 10);
    gtk_widget_set_hexpand(player->metadata_label, TRUE);
    gtk_box_append(GTK_BOX(player->layout.bottom_box), player->metadata_label);
}

static void create_queue_display(AudioPlayer *player) {
    player->layout.queue_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    
    // Set a SMALLER default width (what it starts at)
    // Full width would be ~950px, but start at config.queue_width for compact view
    gtk_widget_set_size_request(player->layout.queue_vbox, 
                               player->layout.config.queue_width, -1);
    
    // Was pack_end with expand=TRUE; main_hbox only has one end-packed child
    // here, so a plain append (after player_vbox, which is already appended
    // earlier) lands in the same visual position.
    gtk_widget_set_hexpand(player->layout.queue_vbox, TRUE);
    gtk_box_append(GTK_BOX(player->layout.main_hbox), player->layout.queue_vbox);

    GtkWidget *queue_label = gtk_label_new("Queue:");
    gtk_widget_set_halign(queue_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(player->layout.queue_vbox), queue_label);

    // THE SEARCH BAR HERE
    GtkWidget *search_bar = create_queue_search_bar(player);
    gtk_box_append(GTK_BOX(player->layout.queue_vbox), search_bar);

    player->queue_scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(player->queue_scrolled_window), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    int adjusted_queue_height = player->layout.config.queue_height - 150;
    gtk_widget_set_size_request(player->queue_scrolled_window, 
                               player->layout.config.queue_width, 
                               adjusted_queue_height);

    // Create the tree view with columns
    create_queue_treeview(player);

    gtk_widget_set_vexpand(player->queue_scrolled_window, TRUE);
    gtk_box_append(GTK_BOX(player->layout.queue_vbox), player->queue_scrolled_window);
    
    // Only end-packed child in queue_vbox, so append after the scrolled
    // window lands in the same visual position as the original pack_end.
    gtk_box_append(GTK_BOX(player->layout.queue_vbox), player->layout.shared_equalizer);
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

    // Second store, same column layout, used for the "Group by" (artist/album/genre)
    // view: one collapsible parent row per artist, tracks as its children.
    // Toggling between the two is just a matter of switching the tree
    // view's model (see set_queue_group_mode() in queue.cpp) - the
    // flat player->queue_store above is left untouched either way.
    player->queue_store_grouped = gtk_tree_store_new(NUM_COLS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_INT);
    
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
    add_column(tree_view, "Karaoke", COL_CDGK, 50, TRUE);
    // Note: COL_QUEUE_INDEX is not displayed as a column, it's just stored in the model
    
    // Enable sorting
    // NOTE: enable_search is intentionally OFF. GtkTreeView's built-in
    // interactive search intercepts printable key presses (letters/digits)
    // whenever this tree view has keyboard focus, opening its own search
    // popup and swallowing the event before it can bubble up to the
    // window-level key controller in keyboard.cpp - this is what was
    // eating the 'Q'/'A' visualization-switch shortcuts (and any other
    // single-letter shortcut) whenever the queue list had focus. The app
    // already has its own dedicated filter entry above the queue for
    // searching, so the built-in one is redundant as well as conflicting.
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree_view), FALSE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree_view), COL_FILENAME);

    // Put the expand/collapse arrow next to the filename column rather
    // than the tiny "now playing" indicator column - only matters once
    // the grouped-by-artist tree store is showing, but harmless either way.
    GtkTreeViewColumn *filename_column = gtk_tree_view_get_column(GTK_TREE_VIEW(tree_view), 1);
    if (filename_column) {
        gtk_tree_view_set_expander_column(GTK_TREE_VIEW(tree_view), filename_column);
    }
    
    // Connect click handler for double-click
    g_signal_connect(tree_view, "row-activated",
                     G_CALLBACK(on_queue_row_activated), player);
    
    // "button-press-event"/"key-press-event" are gone in GTK4 - right-click
    // context menu and key handling now come from controllers attached
    // directly to the tree view.
    GtkGesture *context_gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context_gesture), 0); // any button (on_queue_context_menu checks which)
    g_signal_connect(context_gesture, "pressed", G_CALLBACK(on_queue_context_menu), player);
    gtk_widget_add_controller(tree_view, GTK_EVENT_CONTROLLER(context_gesture));

    GtkEventController *key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_queue_key_press), player);
    gtk_widget_add_controller(tree_view, key_controller);
    
    // Add to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(player->queue_scrolled_window), tree_view);
    
    // Setup drag-and-drop (must be after tree view is created)
    setup_queue_drag_and_drop(player);
}

static void connect_widget_signals(AudioPlayer *player) {
    // "delete-event" -> "close-request" in GTK4, matching
    // on_window_delete_event's updated (GtkWindow*, gpointer) signature.
    g_signal_connect(player->window, "close-request", G_CALLBACK(on_window_delete_event), player);
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
        // gtk_widget_show_all() is gone in GTK4; its descendants were never
        // individually hidden (only this container as a whole), so a plain
        // set_visible is equivalent here.
        gtk_widget_set_visible(player->layout.compact.bottom_controls_hbox, TRUE);
    } else {
        // Switch to regular
        gtk_widget_hide(player->layout.compact.bottom_controls_hbox);
        gtk_widget_set_visible(player->layout.regular.queue_button_box, TRUE);
    }
    // Equalizer stays visible in queue_vbox for both layouts
}

// "configure-event" is removed entirely in GTK4. The closest reliable
// replacement for tracking a top-level window's live size is watching
// GtkWindow's "notify::default-width"/"notify::default-height" properties,
// whose signal signature differs from on_window_resize's existing
// (GtkWidget*, gpointer) - this adapts between the two.
static void on_window_resize_notify(GObject *object, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    on_window_resize(GTK_WIDGET(object), user_data);
}

void create_main_window(AudioPlayer *player) {
#ifdef _WIN32
    // Mark for Windows single instance detection
    // The actual property will be set after gtk_widget_show_all() in main()
    static bool zenamp_window_marker = true;
    SDL_Log("Windows single instance marker prepared");
#endif
    
    // Calculate layout configuration first
    calculate_layout_config(&player->layout);
    
    // Create main window. gtk_window_new() takes no arguments in GTK4
    // (GTK_WINDOW_TOPLEVEL is gone - GtkWindow is always a toplevel now).
    player->window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(player->window), "Zenamp Audio Player");
    gtk_window_set_default_size(GTK_WINDOW(player->window), 
                               player->layout.config.window_width, 
                               player->layout.config.window_height);
    
    set_window_icon_from_base64(GTK_WINDOW(player->window));
    
    // Connect realize signal to handle DPI scaling.
    // "configure-event" is gone - see on_window_resize_notify() above.
    g_signal_connect(player->window, "realize", G_CALLBACK(on_window_realize), player);
    g_signal_connect(player->window, "notify::default-width", G_CALLBACK(on_window_resize_notify), player);
    g_signal_connect(player->window, "notify::default-height", G_CALLBACK(on_window_resize_notify), player);

    // Main layout structure
    player->layout.main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    // gtk_container_set_border_width() on the window itself is gone - GTK4
    // has no generic gtk_container_add() either, so the window takes its
    // single child via gtk_window_set_child(), and the old border width
    // becomes a margin on that child.
    gtk_widget_set_margin_start(player->layout.main_hbox, 10);
    gtk_widget_set_margin_end(player->layout.main_hbox, 10);
    gtk_widget_set_margin_top(player->layout.main_hbox, 10);
    gtk_widget_set_margin_bottom(player->layout.main_hbox, 10);
    gtk_window_set_child(GTK_WINDOW(player->window), player->layout.main_hbox);
    
    // Player controls vbox (left side) - use hexpand to allow expansion
    player->layout.player_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(player->layout.player_vbox, player->layout.config.player_width, -1);
    gtk_widget_set_hexpand(player->layout.player_vbox, TRUE);
    gtk_box_append(GTK_BOX(player->layout.main_hbox), player->layout.player_vbox);
    
    // Create menu bar
    create_menu_bar(player);
    
    // Content area for left side
    player->layout.content_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(player->layout.content_vbox, 10);
    gtk_widget_set_margin_end(player->layout.content_vbox, 10);
    gtk_widget_set_margin_top(player->layout.content_vbox, 10);
    gtk_widget_set_margin_bottom(player->layout.content_vbox, 10);
    gtk_widget_set_vexpand(player->layout.content_vbox, TRUE);
    gtk_box_append(GTK_BOX(player->layout.player_vbox), player->layout.content_vbox);
    
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
    
    // NOTE(gtk4): this was commented out during the port. It builds the
    // animated click-to-dance icon AND player->metadata_label (the label
    // that sits beside it) - the original app never called the separate
    // create_metadata_section() below; this is the one real call site.
    // Losing this call silently dropped the dancing icon feature entirely,
    // and left player->metadata_label NULL for the whole app's life
    // (AudioPlayer is zero-initialized via g_malloc0), which is why every
    // gtk_label_set_markup(GTK_LABEL(player->metadata_label), ...) call
    // elsewhere was hitting "assertion 'GTK_IS_LABEL (self)' failed".
    create_icon_section(player);
    
    // Right side: queue at top, equalizer at bottom (handled in create_queue_display)
    create_queue_display(player);
    
    // Tray icon feature is disabled (GtkStatusIcon is removed entirely in
    // GTK4 with no built-in replacement) - see create_tray_icon()'s comment
    // for details. "window-state-event" (which only existed to drive the
    // tray icon's show/hide) is gone in GTK4 too, so that connection is
    // dropped along with it rather than wired to a no-op.
    //create_tray_icon(player);
    
    // Connect all signals
    connect_widget_signals(player);
    
    SDL_Log("Created main window with %s layout (screen-based decision)", 
           player->layout.config.is_compact ? "compact" : "regular");
}

void create_shared_equalizer(AudioPlayer *player) {
    if (!player->layout.shared_equalizer) {
        SDL_Log("Creating shared equalizer widget");
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
        // ISO C++ forbids binding string literals to non-const char* -
        // GtkRecentData's fields are gchar* (mutable) even though this data
        // is only ever read by GTK, so these need an explicit const_cast.
        recent_data.description = const_cast<gchar*>("Audio playlist");
        recent_data.mime_type = (gchar*)mime_type;
        recent_data.app_name = const_cast<gchar*>("Zenamp");
        recent_data.app_exec = const_cast<gchar*>("zenamp %f");
        recent_data.groups = NULL;
        recent_data.is_private = FALSE;
        
        gtk_recent_manager_add_full(recent_manager, uri, &recent_data);
        
        g_free(recent_data.display_name);
        g_free(uri);
        
        SDL_Log("Added to recent files: %s", filepath);
    }
}

// Callback for recent playlist items. GtkRecentChooser is gone in GTK4, so
// there's no gtk_recent_chooser_get_current_uri() to call - the URI was
// stashed directly on the clicked widget when the "Recent Playlists" submenu
// was built (see populate_recent_playlists_popover() above).
void on_recent_playlist_activated(GtkWidget *widget, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    const char *uri = (const char*)g_object_get_data(G_OBJECT(widget), "recent-uri");
    
    if (uri) {
        gchar *filename = g_filename_from_uri(uri, NULL, NULL);
        if (filename) {
            SDL_Log("Loading recent playlist: %s", filename);
            
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
    }
}

static void create_metadata_section(AudioPlayer *player) {
    GtkWidget *metadata_frame = gtk_frame_new("Track Information");
    gtk_box_append(GTK_BOX(player->layout.content_vbox), metadata_frame);
    
    player->metadata_label = gtk_label_new("No track information");
    gtk_label_set_wrap(GTK_LABEL(player->metadata_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(player->metadata_label), 0.0);
    gtk_label_set_selectable(GTK_LABEL(player->metadata_label), TRUE); // Allow copying
    gtk_widget_set_margin_start(player->metadata_label, 5);
    gtk_widget_set_margin_end(player->metadata_label, 5);
    gtk_widget_set_margin_top(player->metadata_label, 5);
    gtk_widget_set_margin_bottom(player->metadata_label, 5);
    gtk_frame_set_child(GTK_FRAME(metadata_frame), player->metadata_label);
}

// ============================================================================
// TRAY ICON - DISABLED under GTK4
// ============================================================================
// GtkStatusIcon (and everything built on it below) is removed entirely in
// GTK4 with no built-in replacement - unlike most of this port, there's no
// direct API swap available. create_tray_icon()'s only call site was already
// commented out before this port, so the feature looks intentionally
// inactive; these are stubbed to keep the build green rather than guessing
// at a third-party replacement (e.g. libayatana-appindicator, or a hand-
// rolled freedesktop StatusNotifierItem D-Bus service). The original GTK3
// implementation is preserved below in case tray support gets revisited:
//
// static void on_tray_show_window(GtkMenuItem *item, gpointer user_data) {
//     AudioPlayer *player = (AudioPlayer*)user_data;
//     gtk_window_deiconify(GTK_WINDOW(player->window));
//     gtk_window_present(GTK_WINDOW(player->window));
// }
//
// void create_tray_icon(AudioPlayer *player) {
//     GdkPixbuf *icon_pixbuf = load_icon_from_base64();
//     if (icon_pixbuf) {
//         GdkPixbuf *tray_icon_pixbuf = gdk_pixbuf_scale_simple(icon_pixbuf,
//                                                                22, 22,
//                                                                GDK_INTERP_BILINEAR);
//         player->tray_icon = gtk_status_icon_new_from_pixbuf(tray_icon_pixbuf);
//         g_object_unref(tray_icon_pixbuf);
//         g_object_unref(icon_pixbuf);
//     } else {
//         player->tray_icon = gtk_status_icon_new_from_icon_name("multimedia-audio-player");
//     }
//     gtk_status_icon_set_tooltip_text(player->tray_icon, "Zenamp");
//     g_signal_connect(player->tray_icon, "activate",
//                      G_CALLBACK(on_tray_icon_activate), player);
//     g_signal_connect(player->tray_icon, "popup-menu",
//                      G_CALLBACK(on_tray_icon_popup_menu), player);
//     player->tray_menu = gtk_menu_new();
//     GtkWidget *play_item = gtk_menu_item_new_with_label("▶ Play");
//     GtkWidget *pause_item = gtk_menu_item_new_with_label("⏸ Pause");
//     GtkWidget *stop_item = gtk_menu_item_new_with_label("⏹ Stop");
//     GtkWidget *prev_item = gtk_menu_item_new_with_label("|◄ Previous");
//     GtkWidget *next_item = gtk_menu_item_new_with_label("►| Next");
//     GtkWidget *sep = gtk_separator_menu_item_new();
//     GtkWidget *show_item = gtk_menu_item_new_with_label("Show Window");
//     GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit (CTRL+Q)");
//     g_signal_connect(play_item, "activate", G_CALLBACK(on_play_clicked), player);
//     g_signal_connect(pause_item, "activate", G_CALLBACK(on_pause_clicked), player);
//     g_signal_connect(stop_item, "activate", G_CALLBACK(on_stop_clicked), player);
//     g_signal_connect(prev_item, "activate", G_CALLBACK(on_previous_clicked), player);
//     g_signal_connect(next_item, "activate", G_CALLBACK(on_next_clicked), player);
//     g_signal_connect(show_item, "activate", G_CALLBACK(on_tray_show_window), player);
//     g_signal_connect(quit_item, "activate", G_CALLBACK(on_menu_quit), player);
//     gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), play_item);
//     gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), pause_item);
//     gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), stop_item);
//     gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), prev_item);
//     gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), next_item);
//     gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), sep);
//     gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), show_item);
//     gtk_menu_shell_append(GTK_MENU_SHELL(player->tray_menu), quit_item);
//     gtk_widget_show_all(player->tray_menu);
// }
//
// void on_tray_icon_activate(GtkStatusIcon *status_icon, gpointer user_data) {
//     AudioPlayer *player = (AudioPlayer*)user_data;
//     if (gtk_widget_get_visible(player->window)) {
//         gtk_widget_hide(player->window);
//     } else {
//         gtk_window_deiconify(GTK_WINDOW(player->window));
//         gtk_window_present(GTK_WINDOW(player->window));
//     }
// }
//
// void on_tray_icon_popup_menu(GtkStatusIcon *status_icon, guint button,
//                               guint activate_time, gpointer user_data) {
//     AudioPlayer *player = (AudioPlayer*)user_data;
//     gtk_menu_popup(GTK_MENU(player->tray_menu), NULL, NULL,
//                    gtk_status_icon_position_menu, status_icon,
//                    button, activate_time);
// }

void create_tray_icon(AudioPlayer *player) {
    (void)player;
    // Intentionally empty - see comment block above.
}

void on_tray_icon_activate(void *status_icon, gpointer user_data) {
    (void)status_icon;
    (void)user_data;
}

void on_tray_icon_popup_menu(void *status_icon, guint button, 
                              guint activate_time, gpointer user_data) {
    (void)status_icon;
    (void)button;
    (void)activate_time;
    (void)user_data;
}

// "window-state-event" is also gone in GTK4, and this handler only ever
// existed to drive the (now-disabled) tray icon's visibility - stubbed
// alongside it rather than wired to a real GdkToplevelState check.
gboolean on_window_state_event(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    (void)user_data;
    return FALSE;
}

void on_toggle_queue_panel(GtkWidget *check_item, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer *)user_data;
    
    // menu_add_check_item() builds these as GtkCheckButton (GtkCheckMenuItem
    // is gone in GTK4).
    gboolean show_queue = gtk_check_button_get_active(GTK_CHECK_BUTTON(check_item));
    
    if (show_queue) {
        gtk_widget_show(player->layout.queue_vbox);
        SDL_Log("Queue panel shown");
    } else {
        gtk_widget_hide(player->layout.queue_vbox);
        SDL_Log("Queue panel hidden");
    }
}

void on_toggle_fullscreen_visualization(GtkWidget *check_item, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer *)user_data;
    (void)check_item;
    
    // This function is declared in main.cpp, so we just call it
    extern void toggle_vis_fullscreen(AudioPlayer *player);
    
    toggle_vis_fullscreen(player);
}

// "button-press-event" -> GtkGestureClick's "pressed" signal. n_press
// replaces the old GDK_2BUTTON_PRESS check (n_press == 2 means double-click),
// and the button number comes from the gesture itself rather than the event.
void on_visualizer_button_press(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    
    // Update mouse position
    vis->mouse_x = (int)x;
    vis->mouse_y = (int)y;
    vis->mouse_press_time = g_get_monotonic_time() / 1000000.0;
    
    // Handle double-click for fullscreen toggle (only enter fullscreen, don't exit)
    if (n_press == 2 && button == 1) {
        SDL_Log("Visualizer double-clicked");
        
        // Check if visualizer is already in fullscreen mode
        extern bool is_visualizer_fullscreen();
        if (is_visualizer_fullscreen()) {
            // Already fullscreen, don't toggle to avoid interfering with games
            SDL_Log("Visualizer already in fullscreen - ignoring double-click");
            return;
        }
        
        // Not in fullscreen, so enable it
        SDL_Log("Entering fullscreen mode");
        extern void toggle_vis_fullscreen(AudioPlayer *player);
        toggle_vis_fullscreen(player);
        return;
    }
    
    // Track button state
    switch (button) {
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
}

// "button-release-event" -> GtkGestureClick's "released" signal.
void on_visualizer_button_release(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data) {
    (void)n_press;
    (void)x;
    (void)y;
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));

    switch (button) {
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
}

// "motion-notify-event" -> GtkEventControllerMotion's "motion" signal.
void on_visualizer_motion(GtkEventControllerMotion *controller, gdouble x, gdouble y, gpointer user_data) {
    (void)controller;
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    
    // Store previous position
    vis->mouse_last_x = vis->mouse_x;
    vis->mouse_last_y = vis->mouse_y;
    
    // Update current position
    vis->mouse_x = (int)x;
    vis->mouse_y = (int)y;
    
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
}

// "enter-notify-event" -> GtkEventControllerMotion's "enter" signal.
void on_visualizer_enter(GtkEventControllerMotion *controller, gdouble x, gdouble y, gpointer user_data) {
    (void)controller;
    (void)x;
    (void)y;
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    vis->mouse_over = TRUE;
}

// "leave-notify-event" -> GtkEventControllerMotion's "leave" signal (no
// coordinates on leave, unlike "enter"/"motion").
void on_visualizer_leave(GtkEventControllerMotion *controller, gpointer user_data) {
    (void)controller;
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    vis->mouse_over = FALSE;
    vis->mouse_left_pressed = FALSE;
    vis->mouse_right_pressed = FALSE;
    vis->mouse_middle_pressed = FALSE;
    vis->mouse_velocity_x = 0;
    vis->mouse_velocity_y = 0;
}

// "scroll-event" -> GtkEventControllerScroll's "scroll" signal. GTK4 reports
// delta values directly instead of a GDK_SCROLL_UP/DOWN/LEFT/RIGHT/SMOOTH
// enum, which folds the old discrete-vs-smooth-scroll distinction into one
// case. No pointer coordinates come with this signal (unlike the old
// GdkEventScroll), so mouse_x/mouse_y just aren't touched here - the most
// recent "motion" update already covers that.
gboolean on_visualizer_scroll(GtkEventControllerScroll *controller, gdouble dx, gdouble dy, gpointer user_data) {
    (void)controller;
    (void)dx;
    AudioPlayer *player = (AudioPlayer*)user_data;
    Visualizer *vis = player->visualizer;
    
    if (dy > 0) {
        vis->scroll_direction = -1;
    } else if (dy < 0) {
        vis->scroll_direction = 1;
    } else {
        vis->scroll_direction = 0;
    }
    
    return TRUE;
}
