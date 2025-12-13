#include <gtk/gtk.h>
#include <string.h>

// Define fallback version if not provided by Makefile
#ifndef VERSION
#define VERSION "Development"
#endif

/**
 * Data structure for passing game state to help dialog callback
 */
typedef struct {
    GtkWidget *window;
    bool *game_paused;
} CometHelpUserData;

/**
 * Display the CometBuster About/Help dialog with multiple information tabs
 */
void on_menu_about_comet(GtkMenuItem *menuitem, gpointer user_data) {
    (void)menuitem;
    CometHelpUserData *help_data = (CometHelpUserData*)user_data;
    GtkWidget *parent_window = help_data->window;
    bool *game_paused = help_data->game_paused;
    
    // Pause the game before showing help dialog
    bool was_paused = *game_paused;
    *game_paused = true;
    
    // Create main dialog window
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "About CometBuster",
        GTK_WINDOW(parent_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE,
        NULL
    );
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 900, 700);
    
    // Create notebook for tabs
    GtkWidget *notebook = gtk_notebook_new();
    gtk_widget_set_hexpand(notebook, TRUE);
    gtk_widget_set_vexpand(notebook, TRUE);
    
    // Get content area and add notebook
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content_area), notebook);
    
    // ========== ABOUT TAB ==========
    GtkWidget *about_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(about_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(about_scrolled, TRUE);
    gtk_widget_set_vexpand(about_scrolled, TRUE);
    
    GtkWidget *about_label = gtk_label_new(
        "CometBuster\n"
        "Build #Development\n\n"
        "A fast-paced arcade shooter with audio-reactive gameplay\n\n"
        "Gameplay:\n"
        "• Destroy waves of comets and enemy ships while dodging attacks\n"
        "• Audio-reactive gameplay that syncs with your music\n"
        "• Progressive difficulty levels with escalating challenges\n"
        "• Epic boss battles with unique attack patterns\n"
        "• Score tracking and persistent high score table\n"
        "• Power-ups, special weapons, and tactical abilities\n"
        "• Stunning particle effects and explosion animations\n\n"
        "Features:\n"
        "• Full-screen and windowed rendering modes\n"
        "• Gamepad and keyboard support\n"
        "• Seamless audio integration with visualization\n"
        "• Smooth 60 FPS gameplay performance\n"
        "• Cross-platform compatibility (Windows, Linux, macOS)\n\n"
        "Author: Jason Hall\n"
        "Website: https://github.com/jasonbrianhall/cometbuster"
    );
    gtk_label_set_justify(GTK_LABEL(about_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(about_label), TRUE);
    gtk_container_add(GTK_CONTAINER(about_scrolled), about_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), about_scrolled, 
                            gtk_label_new("About"));
    
    // ========== CONTROLS TAB ==========
    GtkWidget *controls_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(controls_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(controls_scrolled, TRUE);
    gtk_widget_set_vexpand(controls_scrolled, TRUE);
    
    GtkWidget *controls_label = gtk_label_new(
        "Keyboard Controls:\n"
        "W - Forward thrust\n"
        "A - Turn left\n"
        "D - Turn right\n"
        "S - Backward thrust\n"
        "SPACE - Boost\n"
        "X - Quick boost\n"
        "CTRL - Fire forward\n"
        "Z - Omnidirectional fire\n"
        "ESC/P - Pause/Resume\n"
        "F11 - Toggle fullscreen\n"
        "V - Volume settings\n\n"
        "Mouse Controls:\n"
        "Left Click - Fire at cursor\n"
        "Right Click - Advanced thrust\n"
        "Middle Click - Omnidirectional fire\n"
        "Cursor Position - Ship follows mouse\n\n"
        "Gamepad Controls:\n"
        "Left Stick - Forward/Backward\n"
        "Right Stick - Turn left/right\n"
        "A Button - Fire\n"
        "X Button - Boost\n"
        "B Button - Alternative action\n"
        "RT Trigger - Special fire\n"
        "D-Pad - Menu navigation"
    );
    gtk_label_set_justify(GTK_LABEL(controls_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(controls_label), TRUE);
    gtk_container_add(GTK_CONTAINER(controls_scrolled), controls_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), controls_scrolled, 
                            gtk_label_new("Controls"));
    
    // ========== LICENSE TAB ==========
    GtkWidget *license_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(license_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(license_scrolled, TRUE);
    gtk_widget_set_vexpand(license_scrolled, TRUE);
    
    GtkWidget *license_label = gtk_label_new(
        "MIT License\n\n"
        "Copyright (c) 2025 Jason Hall\n\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy "
        "of this software and associated documentation files (the \"Software\"), to deal "
        "in the Software without restriction, including without limitation the rights "
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
        "copies of the Software, and to permit persons to whom the Software is "
        "furnished to do so, subject to the following conditions:\n\n"
        "The above copyright notice and this permission notice shall be included in all "
        "copies or substantial portions of the Software.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
        "SOFTWARE."
    );
    gtk_label_set_justify(GTK_LABEL(license_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(license_label), TRUE);
    gtk_container_add(GTK_CONTAINER(license_scrolled), license_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), license_scrolled, 
                            gtk_label_new("License"));
    
    // ========== PRIVACY TAB ==========
    GtkWidget *privacy_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(privacy_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(privacy_scrolled, TRUE);
    gtk_widget_set_vexpand(privacy_scrolled, TRUE);
    
    GtkWidget *privacy_label = gtk_label_new(
        "Privacy Policy for CometBuster\n\n"
        "Last updated: December 2025\n\n"
        "Information We Collect:\n"
        "CometBuster is a local game that does not collect, store, or transmit any "
        "personal information to external servers.\n\n"
        "Local File Access:\n"
        "- The application only accesses audio files and resources that you explicitly provide\n"
        "- High scores and game settings are stored locally on your device only\n"
        "- No file information or game data is shared with third parties\n"
        "- All processing happens locally on your device\n\n"
        "Data Storage:\n"
        "- High scores and game progress are stored locally in your user directory\n"
        "- User preferences (volume, display settings) are saved locally\n"
        "- No personal data is transmitted over the internet\n\n"
        "Audio Integration:\n"
        "- Audio visualization uses only audio playing on your local system\n"
        "- No audio data is recorded or transmitted\n\n"
        "Contact:\n"
        "For questions about this privacy policy, visit the GitHub repository"
    );
    gtk_label_set_justify(GTK_LABEL(privacy_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(privacy_label), TRUE);
    gtk_container_add(GTK_CONTAINER(privacy_scrolled), privacy_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), privacy_scrolled, 
                            gtk_label_new("Privacy"));
    
    // ========== SUPPORT TAB ==========
    GtkWidget *support_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(support_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(support_scrolled, TRUE);
    gtk_widget_set_vexpand(support_scrolled, TRUE);
    
    GtkWidget *support_label = gtk_label_new(
        "Support CometBuster\n\n"
        "If you enjoy playing CometBuster and would like to support its development, "
        "consider buying the developer a coffee!\n\n"
        "☕\n\n"
        "Buy Me a Coffee:\n"
        "https://buymeacoffee.com/jasonbrianhall\n\n"
        "This is an independent donation platform.\n"
        "This project is not affiliated with or endorsed by any company."
    );
    gtk_label_set_justify(GTK_LABEL(support_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(support_label), TRUE);
    gtk_container_add(GTK_CONTAINER(support_scrolled), support_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), support_scrolled, 
                            gtk_label_new("Support"));
    
    // ========== CONTRIBUTING TAB ==========
    GtkWidget *contrib_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(contrib_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(contrib_scrolled, TRUE);
    gtk_widget_set_vexpand(contrib_scrolled, TRUE);
    
    GtkWidget *contrib_label = gtk_label_new(
        "Contributing to CometBuster\n\n"
        "This is a community project and contributions are welcome!\n\n"
        "Ways You Can Contribute:\n\n"
        "Bug Fixes\n"
        "Found a bug? Submit a pull request with a fix. Please include:\n"
        "- Description of the bug\n"
        "- Steps to reproduce\n"
        "- System information (OS, compiler version)\n\n"
        "Gameplay Improvements\n"
        "Enhance game mechanics, balance, difficulty curves, or level design.\n"
        "Discussion of ideas is encouraged!\n\n"
        "Visual Enhancements\n"
        "Improve graphics, particle effects, UI, or create new ship/comet designs.\n\n"
        "Audio Integration\n"
        "Help improve audio visualization and reactive gameplay features.\n\n"
        "Documentation\n"
        "Improve README, add examples, create tutorials, or enhance code comments.\n\n"
        "Translation\n"
        "Help translate CometBuster to other languages.\n\n"
        "Reporting Issues:\n"
        "1. Check existing issues on GitHub\n"
        "2. Provide system info (OS, compiler, libraries)\n"
        "3. Include detailed reproduction steps\n"
        "4. Provide error messages or console output\n\n"
        "Repository:\n"
        "https://github.com/jasonbrianhall/cometbuster\n\n"
        "Thank you for making CometBuster better!"
    );
    gtk_label_set_justify(GTK_LABEL(contrib_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(contrib_label), TRUE);
    gtk_container_add(GTK_CONTAINER(contrib_scrolled), contrib_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), contrib_scrolled, 
                            gtk_label_new("Contributing"));
    
    // Show all widgets
    gtk_widget_show_all(dialog);
    
    // Run dialog and clean up
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    // Resume game if it wasn't paused before
    if (!was_paused) {
        *game_paused = false;
    }
}
