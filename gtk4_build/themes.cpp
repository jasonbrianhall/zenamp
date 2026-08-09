// themes.cpp
#include "themes.h"
#include "audio_player.h"
#include <stdio.h>
#include <string.h>

// Add to AudioPlayer struct:
// int current_theme_index;

void apply_theme(AudioPlayer *player, int theme_index) {
    if (!player || !player->visualizer || theme_index < 0 || theme_index >= THEME_COUNT) {
        return;
    }
    
    const Theme *theme = &themes[theme_index];
    
    // Apply to visualizer
    player->visualizer->bg_r = theme->bg_r;
    player->visualizer->bg_g = theme->bg_g;
    player->visualizer->bg_b = theme->bg_b;
    
    player->visualizer->fg_r = theme->fg_r;
    player->visualizer->fg_g = theme->fg_g;
    player->visualizer->fg_b = theme->fg_b;
    
    player->visualizer->accent_r = theme->accent_r;
    player->visualizer->accent_g = theme->accent_g;
    player->visualizer->accent_b = theme->accent_b;
    
    // Store current theme
    player->current_theme_index = theme_index;
    
    // Apply to main window if desired
    apply_theme_to_widgets(player, theme);
    
    // Force redraw
    if (player->visualizer->drawing_area) {
        gtk_widget_queue_draw(player->visualizer->drawing_area);
    }
    
    printf("Applied theme: %s\n", theme->name);
}

void apply_theme_to_widgets(AudioPlayer *player, const Theme *theme) {
    // Create colors for GTK widgets
    GdkRGBA bg_color = {theme->bg_r * 0.5 + 0.1, theme->bg_g * 0.5 + 0.1, theme->bg_b * 0.5 + 0.1, 1.0};
    GdkRGBA text_color = {theme->fg_r, theme->fg_g, theme->fg_b, 1.0};
    GdkRGBA accent_color = {theme->accent_r * 0.7, theme->accent_g * 0.7, theme->accent_b * 0.7, 1.0};
    
    // Apply background to main window
    gtk_widget_override_background_color(player->window, GTK_STATE_FLAG_NORMAL, &bg_color);
    
    // Apply text colors
    gtk_widget_override_color(player->file_label, GTK_STATE_FLAG_NORMAL, &text_color);
    gtk_widget_override_color(player->time_label, GTK_STATE_FLAG_NORMAL, &text_color);
    
    // Apply button colors (subtle theme application)
    gtk_widget_override_background_color(player->play_button, GTK_STATE_FLAG_NORMAL, &accent_color);
    gtk_widget_override_background_color(player->pause_button, GTK_STATE_FLAG_NORMAL, &accent_color);
    gtk_widget_override_background_color(player->stop_button, GTK_STATE_FLAG_NORMAL, &accent_color);
    gtk_widget_override_background_color(player->next_button, GTK_STATE_FLAG_NORMAL, &accent_color);
    gtk_widget_override_background_color(player->prev_button, GTK_STATE_FLAG_NORMAL, &accent_color);
    gtk_widget_override_background_color(player->rewind_button, GTK_STATE_FLAG_NORMAL, &accent_color);
    gtk_widget_override_background_color(player->fast_forward_button, GTK_STATE_FLAG_NORMAL, &accent_color);
}

void cycle_theme(AudioPlayer *player) {
    if (!player) return;
    
    int next_theme = (player->current_theme_index + 1) % THEME_COUNT;
    apply_theme(player, next_theme);
}

void cycle_theme_backwards(AudioPlayer *player) {
    if (!player) return;
    
    int prev_theme = player->current_theme_index - 1;
    if (prev_theme < 0) prev_theme = THEME_COUNT - 1;
    apply_theme(player, prev_theme);
}

int get_current_theme_index(AudioPlayer *player) {
    return player ? player->current_theme_index : 0;
}

const char* get_theme_name(int theme_index) {
    if (theme_index < 0 || theme_index >= THEME_COUNT) {
        return "Unknown";
    }
    return themes[theme_index].name;
}

const char* get_current_theme_name(AudioPlayer *player) {
    if (!player) return "Unknown";
    return get_theme_name(player->current_theme_index);
}

void reset_theme_to_default(AudioPlayer *player) {
    if (!player) return;
    
    // Reset all widget colors to system defaults
    gtk_widget_override_background_color(player->window, GTK_STATE_FLAG_NORMAL, NULL);
    gtk_widget_override_color(player->file_label, GTK_STATE_FLAG_NORMAL, NULL);
    gtk_widget_override_color(player->time_label, GTK_STATE_FLAG_NORMAL, NULL);
    gtk_widget_override_background_color(player->play_button, GTK_STATE_FLAG_NORMAL, NULL);
    gtk_widget_override_background_color(player->pause_button, GTK_STATE_FLAG_NORMAL, NULL);
    gtk_widget_override_background_color(player->stop_button, GTK_STATE_FLAG_NORMAL, NULL);
    gtk_widget_override_background_color(player->next_button, GTK_STATE_FLAG_NORMAL, NULL);
    gtk_widget_override_background_color(player->prev_button, GTK_STATE_FLAG_NORMAL, NULL);
    gtk_widget_override_background_color(player->rewind_button, GTK_STATE_FLAG_NORMAL, NULL);
    gtk_widget_override_background_color(player->fast_forward_button, GTK_STATE_FLAG_NORMAL, NULL);
    
    // Apply default theme (index 0)
    apply_theme(player, 0);
}

void save_current_theme(AudioPlayer *player, const char* config_file) {
    if (!player || !config_file) return;
    
    FILE *file = fopen(config_file, "w");
    if (file) {
        fprintf(file, "theme_index=%d\n", player->current_theme_index);
        fclose(file);
        printf("Saved theme: %s\n", get_current_theme_name(player));
    }
}

int load_saved_theme(const char* config_file) {
    if (!config_file) return 0;
    
    FILE *file = fopen(config_file, "r");
    if (file) {
        int theme_index = 0;
        if (fscanf(file, "theme_index=%d", &theme_index) == 1) {
            fclose(file);
            if (theme_index >= 0 && theme_index < THEME_COUNT) {
                printf("Loaded saved theme: %s\n", themes[theme_index].name);
                return theme_index;
            }
        }
        fclose(file);
    }
    return 0; // Default theme
}

// Callback functions for UI integration
void on_penguin_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    AudioPlayer *player = (AudioPlayer*)user_data;
    cycle_theme(player);
}

void on_theme_menu_activate(GtkMenuItem *menuitem, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    // Get theme index from menu item data
    int theme_index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), "theme_index"));
    apply_theme(player, theme_index);
}

// Create a theme selection menu
GtkWidget* create_theme_menu(AudioPlayer *player) {
    GtkWidget *theme_menu = gtk_menu_new();
    
    for (int i = 0; i < THEME_COUNT; i++) {
        GtkWidget *menu_item = gtk_menu_item_new_with_label(themes[i].name);
        g_object_set_data(G_OBJECT(menu_item), "theme_index", GINT_TO_POINTER(i));
        g_signal_connect(menu_item, "activate", G_CALLBACK(on_theme_menu_activate), player);
        gtk_menu_shell_append(GTK_MENU_SHELL(theme_menu), menu_item);
    }
    
    return theme_menu;
}

// Create theme selection dropdown for settings
GtkWidget* create_theme_combo(AudioPlayer *player) {
    GtkWidget *combo = gtk_combo_box_text_new();
    
    for (int i = 0; i < THEME_COUNT; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), themes[i].name);
    }
    
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), player->current_theme_index);
    
    g_signal_connect(combo, "changed", G_CALLBACK(on_theme_combo_changed), player);
    
    return combo;
}

void on_theme_combo_changed(GtkComboBox *combo, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    int selected = gtk_combo_box_get_active(combo);
    if (selected >= 0 && selected < THEME_COUNT) {
        apply_theme(player, selected);
    }
}

// Random theme selection
void apply_random_theme(AudioPlayer *player) {
    if (!player) return;
    
    int random_index = rand() % THEME_COUNT;
    apply_theme(player, random_index);
}
