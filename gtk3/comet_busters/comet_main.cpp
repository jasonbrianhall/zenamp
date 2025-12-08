#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "cometbuster.h"
#include "visualization.h"
#include "audio_wad.h"

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
    
} CometGUI;

// Forward declarations
gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data);
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

gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    
    gui->visualizer.width = width;
    gui->visualizer.height = height;
    
    // Draw black background
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    
    // Draw the game
    draw_comet_buster(&gui->visualizer, cr);
    
    return FALSE;
}

void update_status_text(CometGUI *gui) {
    char status[256];
    CometBusterGame *game = &gui->visualizer.comet_buster;
    
    if (game->game_over) {
        snprintf(status, sizeof(status), 
                "GAME OVER | Score: %d | Wave: %d | Lives: %d | Press R to Restart",
                game->score, game->current_wave, game->ship_lives);
    } else {
        snprintf(status, sizeof(status),
                "Score: %d | Wave: %d | Lives: %d | Multiplier: %.1fx | Energy: %.0f/%.0f",
                game->score, game->current_wave, game->ship_lives,
                game->score_multiplier, game->energy_amount, game->max_energy);
    }
    
    gtk_label_set_text(GTK_LABEL(gui->status_label), status);
}

gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    
    if (event->button == 1) {
        // Left mouse button
        gui->visualizer.mouse_left_pressed = true;
    } else if (event->button == 3) {
        // Right mouse button
        gui->visualizer.mouse_right_pressed = true;
    } else if (event->button == 2) {
        // Middle mouse button
        gui->visualizer.mouse_middle_pressed = true;
    }
    
    return TRUE;
}

gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    
    if (event->button == 1) {
        gui->visualizer.mouse_left_pressed = false;
    } else if (event->button == 3) {
        gui->visualizer.mouse_right_pressed = false;
    } else if (event->button == 2) {
        gui->visualizer.mouse_middle_pressed = false;
    }
    
    return TRUE;
}

gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    int new_x = (int)event->x;
    int new_y = (int)event->y;
    
    // Check if mouse actually moved (not just within same pixel)
    if (new_x != gui->visualizer.mouse_x || new_y != gui->visualizer.mouse_y) {
        gui->visualizer.mouse_just_moved = true;
        gui->visualizer.mouse_movement_timer = 0.0;  // Reset timer on movement
    }
    
    gui->visualizer.mouse_x = new_x;
    gui->visualizer.mouse_y = new_y;
    return TRUE;
}

gboolean game_update_timer(gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    
    // Update at approximately 60 FPS (16.67 ms per frame)
    double dt = 0.01667;  // 16.67 ms
    
    // Update mouse movement timer
    gui->visualizer.mouse_movement_timer += dt;
    if (gui->visualizer.mouse_movement_timer > 0.5) {
        // Mouse hasn't moved in 0.5 seconds, switch back to keyboard if keyboard was last used
        gui->visualizer.mouse_just_moved = false;
    }
    
    // Update the game
    update_comet_buster(&gui->visualizer, dt);
    
    // Update status text
    update_status_text(gui);
    
    // Trigger redraw
    gtk_widget_queue_draw(gui->drawing_area);
    
    gui->total_time += dt;
    gui->frame_count++;
    
    return G_SOURCE_CONTINUE;
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    CometBusterGame *game = &gui->visualizer.comet_buster;
    
    // Handle arcade-style controls
    // Rotation: A=left, D=right
    if (event->keyval == GDK_KEY_a || event->keyval == GDK_KEY_A) {
        gui->visualizer.key_a_pressed = true;
        return TRUE;
    } else if (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D) {
        gui->visualizer.key_d_pressed = true;
        return TRUE;
    }
    // Thrust: W=forward, S=backward
    else if (event->keyval == GDK_KEY_w || event->keyval == GDK_KEY_W) {
        gui->visualizer.key_w_pressed = true;
        return TRUE;
    } else if (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S) {
        gui->visualizer.key_s_pressed = true;
        return TRUE;
    }
    // Boost: X or SPACE
    else if (event->keyval == GDK_KEY_x || event->keyval == GDK_KEY_X) {
        gui->visualizer.key_x_pressed = true;
        return TRUE;
    } else if (event->keyval == GDK_KEY_space || event->keyval == GDK_KEY_KP_Space) {
        gui->visualizer.key_space_pressed = true;
        return TRUE;
    }
    // Fire: CTRL (left or right)
    else if (event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R) {
        gui->visualizer.key_ctrl_pressed = true;
        return TRUE;
    }
    // Other game controls
    else if (event->keyval == GDK_KEY_r || event->keyval == GDK_KEY_R) {
        // Restart game
        comet_buster_reset_game(game);
        update_status_text(gui);
        gtk_widget_queue_draw(gui->drawing_area);
        return TRUE;
    } else if (event->keyval == GDK_KEY_F11) {
        // Toggle fullscreen
        on_toggle_fullscreen(widget, data);
        return TRUE;
    } else if (event->keyval == GDK_KEY_Escape) {
        gtk_main_quit();
        return TRUE;
    }
    
    return FALSE;
}

void on_new_game(GtkWidget *widget, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    comet_buster_reset_game(&gui->visualizer.comet_buster);
    update_status_text(gui);
    gtk_widget_queue_draw(gui->drawing_area);
}

void on_about(GtkWidget *widget, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    
    GtkWidget *about_dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about_dialog), "CometBuster");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about_dialog), "1.0");
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about_dialog), "Copyright 2025 Jason Brian Hall");
    
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about_dialog), "https://github.com/jasonbrianhall/zenamp");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(about_dialog), "ZenAmp on GitHub");
    
    const gchar *authors[] = {
        "Jason Brian Hall",
        NULL
    };
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about_dialog), authors);
    
    const gchar *credits[] = {
        "GTK+ 3.0 - GUI Framework",
        "Cairo - Graphics Rendering",
        "GNU C++ - Compiler",
        NULL
    };
    gtk_about_dialog_add_credit_section(GTK_ABOUT_DIALOG(about_dialog), 
                                       "Technology", credits);
    
    gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about_dialog),
        "CometBuster - A fast-paced arcade game featuring asteroid destruction,\n"
        "enemy waves, and real-time gameplay.\n\n"
        "Part of the ZenAmp visualization project.\n\n"
        "MIT License\n"
        "===========\n\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n\n"
        "The above copyright notice and this permission notice shall be included in all\n"
        "copies or substantial portions of the Software.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE.");
    
    gtk_window_set_transient_for(GTK_WINDOW(about_dialog), GTK_WINDOW(gui->window));
    gtk_window_set_modal(GTK_WINDOW(about_dialog), TRUE);
    gtk_dialog_run(GTK_DIALOG(about_dialog));
    gtk_widget_destroy(about_dialog);
}

void on_game_controls(GtkWidget *widget, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    
    GtkWidget *controls_dialog = gtk_dialog_new_with_buttons(
        "Game Controls",
        GTK_WINDOW(gui->window),
        GTK_DIALOG_MODAL,
        "Close",
        GTK_RESPONSE_OK,
        NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(controls_dialog), 500, 400);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(controls_dialog));
    
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer,
        "COMETBUSTER GAME CONTROLS\n"
        "=========================\n\n"
        "AIMING & SHOOTING:\n"
        "  Mouse Position     - Aim ship (move mouse to rotate)\n"
        "  Left Click         - Fire bullets at aimed direction\n"
        "  Middle Click       - Omnidirectional fire (all 8 directions)\n\n"
        "MOVEMENT & CONTROLS:\n"
        "  A                  - Turn ship left\n"
        "  D                  - Turn ship right\n"
        "  W                  - Forward thrust\n"
        "  S                  - Backward thrust\n"
        "  CTRL               - Fire weapon\n"
        "  X or SPACE         - Boost thrusters (uses energy)\n"
        "  Energy Bar         - Shows current/max energy (regenerates)\n\n"
        "GAME CONTROLS:\n"
        "  R                  - Restart game\n"
        "  F11                - Toggle fullscreen mode\n"
        "  ESC                - Quit application\n\n"
        "GAMEPLAY:\n"
        "  Score              - Points from destroying comets\n"
        "  Multiplier         - Increases with consecutive hits\n"
        "  Wave               - Enemy wave number\n"
        "  Lives              - Remaining ship lives\n"
        "  Energy             - Fuel for boost thrusters\n\n"
        "GAME MECHANICS:\n"
        "  Destroy comets to earn points\n"
        "  Avoid collisions with comets and enemy bullets\n"
        "  Use boost strategically to dodge attacks\n"
        "  Achieve higher waves for increased difficulty\n"
        "  Build multiplier for more points",
        -1);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_box_pack_start(GTK_BOX(content_area), scrolled_window, TRUE, TRUE, 10);
    
    gtk_widget_show_all(controls_dialog);
    gtk_dialog_run(GTK_DIALOG(controls_dialog));
    gtk_widget_destroy(controls_dialog);
}

void on_toggle_fullscreen(GtkWidget *widget, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    
    if (gui->is_fullscreen) {
        // Exit fullscreen - return to maximized windowed mode
        gtk_window_unfullscreen(GTK_WINDOW(gui->window));
        gui->is_fullscreen = false;
        // Maximize the window again
        gtk_window_maximize(GTK_WINDOW(gui->window));
        // Show menu bar
        gtk_widget_show(gui->menu_bar);
    } else {
        // Enter fullscreen
        gtk_window_fullscreen(GTK_WINDOW(gui->window));
        gui->is_fullscreen = true;
        // Hide menu bar in fullscreen
        gtk_widget_hide(gui->menu_bar);
    }
}

gboolean on_key_release(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    CometGUI *gui = (CometGUI*)data;
    
    // Handle arcade-style control key releases
    if (event->keyval == GDK_KEY_a || event->keyval == GDK_KEY_A) {
        gui->visualizer.key_a_pressed = false;
        return TRUE;
    } else if (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D) {
        gui->visualizer.key_d_pressed = false;
        return TRUE;
    } else if (event->keyval == GDK_KEY_w || event->keyval == GDK_KEY_W) {
        gui->visualizer.key_w_pressed = false;
        return TRUE;
    } else if (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S) {
        gui->visualizer.key_s_pressed = false;
        return TRUE;
    } else if (event->keyval == GDK_KEY_x || event->keyval == GDK_KEY_X) {
        gui->visualizer.key_x_pressed = false;
        return TRUE;
    } else if (event->keyval == GDK_KEY_space || event->keyval == GDK_KEY_KP_Space) {
        gui->visualizer.key_space_pressed = false;
        return TRUE;
    } else if (event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R) {
        gui->visualizer.key_ctrl_pressed = false;
        return TRUE;
    }
    
    return FALSE;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    
    gtk_init(&argc, &argv);
    
    CometGUI gui;
    memset(&gui, 0, sizeof(gui));
    
    gui.frame_count = 0;
    gui.total_time = 0.0;
    gui.is_fullscreen = false;
    
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
    gui.visualizer.key_x_pressed = false;
    gui.visualizer.key_space_pressed = false;
    gui.visualizer.key_ctrl_pressed = false;
    
    // Initialize the game
    init_comet_buster_system(&gui.visualizer);
    
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
    
    // Load background music tracks
#ifdef ExternalSound
    audio_play_music(&gui.audio, "music/track1.mp3", false);   // Load track 1
    audio_play_music(&gui.audio, "music/track2.mp3", false);   // Load track 2
    audio_play_music(&gui.audio, "music/track3.mp3", false);   // Load track 3
    audio_play_music(&gui.audio, "music/track4.mp3", false);   // Load track 4
    audio_play_music(&gui.audio, "music/track5.mp3", false);   // Load track 5
    
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
    
    // File menu
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_item = gtk_menu_item_new_with_label("File");
    
    GtkWidget *new_game_item = gtk_menu_item_new_with_label("New Game");
    g_signal_connect(new_game_item, "activate", G_CALLBACK(on_new_game), &gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), new_game_item);
    
    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator);
    
    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit_item, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_item);
    
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
