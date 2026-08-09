#ifndef THEMES_H
#define THEMES_H

#include "audio_player.h"

typedef struct {
    const char* name;
    double bg_r, bg_g, bg_b;        // Background color
    double fg_r, fg_g, fg_b;        // Foreground/primary color
    double accent_r, accent_g, accent_b;  // Accent color
} Theme;

// 30 predefined themes
static const Theme themes[] = {
    // Classic themes
    {"Default Green", 0.1, 0.1, 0.1, 0.0, 0.8, 0.0, 0.0, 1.0, 0.5},
    {"Antarctic", 0.05, 0.1, 0.2, 0.7, 0.9, 1.0, 0.3, 0.8, 1.0},
    {"Classic Blue", 0.0, 0.0, 0.2, 0.3, 0.5, 1.0, 0.6, 0.8, 1.0},
    {"Deep Purple", 0.1, 0.0, 0.2, 0.6, 0.3, 1.0, 0.8, 0.5, 1.0},
    {"Blood Red", 0.2, 0.0, 0.0, 1.0, 0.2, 0.2, 1.0, 0.6, 0.6},
    
    // Nature themes
    {"Forest", 0.05, 0.15, 0.05, 0.2, 0.8, 0.2, 0.4, 1.0, 0.4},
    {"Ocean Deep", 0.0, 0.1, 0.2, 0.0, 0.6, 0.8, 0.2, 0.8, 1.0},
    {"Sunset", 0.2, 0.1, 0.0, 1.0, 0.6, 0.2, 1.0, 0.8, 0.4},
    {"Desert Sand", 0.2, 0.15, 0.1, 0.9, 0.7, 0.4, 1.0, 0.9, 0.6},
    {"Cherry Blossom", 0.15, 0.1, 0.15, 1.0, 0.7, 0.8, 1.0, 0.4, 0.7},
    
    // Neon/Cyberpunk themes
    {"Neon Pink", 0.05, 0.0, 0.1, 1.0, 0.0, 0.8, 1.0, 0.4, 1.0},
    {"Cyber Blue", 0.0, 0.05, 0.15, 0.0, 0.8, 1.0, 0.4, 1.0, 1.0},
    {"Matrix Green", 0.0, 0.1, 0.0, 0.0, 1.0, 0.2, 0.4, 1.0, 0.6},
    {"Neon Orange", 0.1, 0.05, 0.0, 1.0, 0.5, 0.0, 1.0, 0.8, 0.2},
    {"Electric Violet", 0.1, 0.0, 0.2, 0.8, 0.2, 1.0, 1.0, 0.6, 1.0},
    
    // Monochrome themes
    {"Pure Black", 0.0, 0.0, 0.0, 0.7, 0.7, 0.7, 1.0, 1.0, 1.0},
    {"Charcoal", 0.1, 0.1, 0.1, 0.6, 0.6, 0.6, 0.9, 0.9, 0.9},
    {"Silver", 0.2, 0.2, 0.2, 0.8, 0.8, 0.8, 1.0, 1.0, 1.0},
    {"Inverse", 0.9, 0.9, 0.9, 0.2, 0.2, 0.2, 0.0, 0.0, 0.0},
    {"High Contrast", 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.5, 0.5, 0.5},
    
    // Retro themes
    {"80s Vaporwave", 0.1, 0.0, 0.2, 1.0, 0.2, 0.8, 0.0, 1.0, 1.0},
    {"70s Brown", 0.15, 0.1, 0.05, 0.8, 0.6, 0.3, 1.0, 0.8, 0.4},
    {"60s Psychedelic", 0.2, 0.0, 0.2, 1.0, 0.6, 0.0, 1.0, 1.0, 0.0},
    {"Sepia", 0.15, 0.12, 0.08, 0.8, 0.7, 0.5, 1.0, 0.9, 0.7},
    {"Amber Terminal", 0.0, 0.0, 0.0, 1.0, 0.75, 0.0, 1.0, 0.9, 0.3},
    
    // Seasonal themes
    {"Spring Green", 0.05, 0.15, 0.05, 0.4, 1.0, 0.4, 0.7, 1.0, 0.7},
    {"Summer Sky", 0.0, 0.2, 0.4, 0.6, 0.9, 1.0, 1.0, 1.0, 0.8},
    {"Autumn Leaves", 0.15, 0.1, 0.0, 1.0, 0.6, 0.0, 1.0, 0.8, 0.2},
    {"Winter Ice", 0.1, 0.15, 0.2, 0.8, 0.9, 1.0, 0.9, 1.0, 1.0},
    {"Midnight", 0.0, 0.0, 0.1, 0.3, 0.3, 0.8, 0.6, 0.6, 1.0}
};

#define THEME_COUNT (sizeof(themes) / sizeof(Theme))

// Function declarations - Fixed: removed 'struct' keyword
void apply_theme(AudioPlayer *player, int theme_index);
void cycle_theme(AudioPlayer *player);
void cycle_theme_backwards(AudioPlayer *player);
int get_current_theme_index(AudioPlayer *player);
const char* get_theme_name(int theme_index);
const char* get_current_theme_name(AudioPlayer *player);
void reset_theme_to_default(AudioPlayer *player);
void save_current_theme(AudioPlayer *player, const char* config_file);
int load_saved_theme(const char* config_file);

// Callback functions for UI integration
void on_penguin_clicked(GtkButton *button, gpointer user_data);
void on_theme_menu_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_theme_combo_changed(GtkComboBox *combo, gpointer user_data);

// UI creation functions
GtkWidget* create_theme_menu(AudioPlayer *player);
GtkWidget* create_theme_combo(AudioPlayer *player);

// Utility functions
void apply_random_theme(AudioPlayer *player);
void apply_theme_to_widgets(AudioPlayer *player, const Theme *theme);

void on_prev_theme_menu_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_random_theme_menu_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_next_theme_menu_activate(GtkMenuItem *menuitem, gpointer user_data);


#endif // THEMES_H
