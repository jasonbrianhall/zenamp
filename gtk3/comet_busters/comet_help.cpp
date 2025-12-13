#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

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
 * Helper function to open URL in default browser
 */
static void open_url(const char *url) {
    char command[512];
#ifdef _WIN32
    snprintf(command, sizeof(command), "start %s", url);
#elif __APPLE__
    snprintf(command, sizeof(command), "open %s", url);
#else
    snprintf(command, sizeof(command), "xdg-open %s", url);
#endif
    system(command);
}

/**
 * Callback for opening URLs
 */
static void on_link_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    const char *url = (const char *)user_data;
    open_url(url);
}

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
        "CometBuster - About & Help",
        GTK_WINDOW(parent_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE,
        NULL
    );
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1000, 750);
    
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
    
    GtkWidget *about_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(about_box), 20);
    
    GtkWidget *title_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_label), 
        "<span size='xx-large' weight='bold'>🎮 CometBuster</span>\n"
        "<span size='large' foreground='#666666'>Build #Development</span>");
    gtk_widget_set_halign(title_label, GTK_ALIGN_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(title_label), TRUE);
    gtk_box_pack_start(GTK_BOX(about_box), title_label, FALSE, FALSE, 0);
    
    GtkWidget *tagline_label = gtk_label_new(
        "A fast-paced arcade shooter with audio-reactive gameplay");
    gtk_label_set_line_wrap(GTK_LABEL(tagline_label), TRUE);
    gtk_widget_set_halign(tagline_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(about_box), tagline_label, FALSE, FALSE, 0);
    
    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(about_box), sep1, FALSE, FALSE, 0);
    
    GtkWidget *gameplay_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(gameplay_header), 
        "<span weight='bold' size='large'>🚀 Gameplay Features</span>");
    gtk_widget_set_halign(gameplay_header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(about_box), gameplay_header, FALSE, FALSE, 0);
    
    GtkWidget *gameplay_label = gtk_label_new(
        "• Destroy waves of comets and enemy ships while dodging attacks\n"
        "• Audio-reactive gameplay that syncs with your music\n"
        "• Progressive difficulty levels with escalating challenges\n"
        "• Epic boss battles with unique attack patterns\n"
        "• Score tracking and persistent high score table\n"
        "• Power-ups, special weapons, and tactical abilities\n"
        "• Stunning particle effects and explosion animations");
    gtk_label_set_justify(GTK_LABEL(gameplay_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(gameplay_label), TRUE);
    gtk_widget_set_margin_start(gameplay_label, 15);
    gtk_box_pack_start(GTK_BOX(about_box), gameplay_label, FALSE, FALSE, 0);
    
    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(about_box), sep2, FALSE, FALSE, 0);
    
    GtkWidget *features_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(features_header), 
        "<span weight='bold' size='large'>⚙️ Technical Features</span>");
    gtk_widget_set_halign(features_header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(about_box), features_header, FALSE, FALSE, 0);
    
    GtkWidget *features_label = gtk_label_new(
        "• Full-screen and windowed rendering modes\n"
        "• Gamepad and keyboard support\n"
        "• Seamless audio integration with visualization\n"
        "• Smooth 60 FPS gameplay performance\n"
        "• Cross-platform compatibility (Windows, Linux, macOS)");
    gtk_label_set_justify(GTK_LABEL(features_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(features_label), TRUE);
    gtk_widget_set_margin_start(features_label, 15);
    gtk_box_pack_start(GTK_BOX(about_box), features_label, FALSE, FALSE, 0);
    
    GtkWidget *sep3 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(about_box), sep3, FALSE, FALSE, 0);
    
    GtkWidget *author_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(author_label), 
        "<span weight='bold'>👨‍💻 Author:</span> Jason Hall");
    gtk_widget_set_halign(author_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(about_box), author_label, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(about_scrolled), about_box);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), about_scrolled, 
                            gtk_label_new("About"));
    
    // ========== CONTROLS TAB ==========
    GtkWidget *controls_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(controls_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(controls_scrolled, TRUE);
    gtk_widget_set_vexpand(controls_scrolled, TRUE);
    
    GtkWidget *controls_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(controls_box), 20);
    
    GtkWidget *kb_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(kb_header), 
        "<span weight='bold' size='large'>⌨️ Keyboard Controls</span>");
    gtk_widget_set_halign(kb_header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(controls_box), kb_header, FALSE, FALSE, 0);
    
    GtkWidget *kb_label = gtk_label_new(
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
        "V - Volume settings");
    gtk_label_set_justify(GTK_LABEL(kb_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(kb_label), TRUE);
    gtk_widget_set_margin_start(kb_label, 15);
    gtk_box_pack_start(GTK_BOX(controls_box), kb_label, FALSE, FALSE, 0);
    
    GtkWidget *sep_c1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controls_box), sep_c1, FALSE, FALSE, 0);
    
    GtkWidget *mouse_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(mouse_header), 
        "<span weight='bold' size='large'>🖱️ Mouse Controls</span>");
    gtk_widget_set_halign(mouse_header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(controls_box), mouse_header, FALSE, FALSE, 0);
    
    GtkWidget *mouse_label = gtk_label_new(
        "Left Click - Fire at cursor\n"
        "Right Click - Advanced thrust\n"
        "Middle Click - Omnidirectional fire\n"
        "Cursor Position - Ship follows mouse");
    gtk_label_set_justify(GTK_LABEL(mouse_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(mouse_label), TRUE);
    gtk_widget_set_margin_start(mouse_label, 15);
    gtk_box_pack_start(GTK_BOX(controls_box), mouse_label, FALSE, FALSE, 0);
    
    GtkWidget *sep_c2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controls_box), sep_c2, FALSE, FALSE, 0);
    
    GtkWidget *gamepad_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(gamepad_header), 
        "<span weight='bold' size='large'>🎮 Gamepad Controls</span>");
    gtk_widget_set_halign(gamepad_header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(controls_box), gamepad_header, FALSE, FALSE, 0);
    
    GtkWidget *gamepad_label = gtk_label_new(
        "Left Stick - Forward/Backward\n"
        "Right Stick - Turn left/right\n"
        "A Button - Fire\n"
        "X Button - Boost\n"
        "B Button - Alternative action\n"
        "RT Trigger - Special fire\n"
        "D-Pad - Menu navigation");
    gtk_label_set_justify(GTK_LABEL(gamepad_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(gamepad_label), TRUE);
    gtk_widget_set_margin_start(gamepad_label, 15);
    gtk_box_pack_start(GTK_BOX(controls_box), gamepad_label, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(controls_scrolled), controls_box);
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
        "SOFTWARE.");
    gtk_label_set_justify(GTK_LABEL(license_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(license_label), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(license_label), 20);
    gtk_container_add(GTK_CONTAINER(license_scrolled), license_label);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), license_scrolled, 
                            gtk_label_new("License"));
    
    // ========== PRIVACY TAB ==========
    GtkWidget *privacy_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(privacy_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(privacy_scrolled, TRUE);
    gtk_widget_set_vexpand(privacy_scrolled, TRUE);
    
    GtkWidget *privacy_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(privacy_box), 20);
    
    GtkWidget *privacy_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(privacy_title), 
        "<span weight='bold' size='large'>🔒 Privacy Policy</span>");
    gtk_widget_set_halign(privacy_title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(privacy_box), privacy_title, FALSE, FALSE, 0);
    
    GtkWidget *privacy_date = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(privacy_date), 
        "<span foreground='#666666' size='small'>Last updated: December 2025</span>");
    gtk_widget_set_halign(privacy_date, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(privacy_box), privacy_date, FALSE, FALSE, 0);
    
    GtkWidget *privacy_sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(privacy_box), privacy_sep1, FALSE, FALSE, 0);
    
    GtkWidget *privacy_label = gtk_label_new(
        "CometBuster is a local game that does not collect, store, or transmit any "
        "personal information to external servers.\n\n"
        "Local File Access:\n"
        "• The application only accesses audio files and resources that you explicitly provide\n"
        "• High scores and game settings are stored locally on your device only\n"
        "• No file information or game data is shared with third parties\n"
        "• All processing happens locally on your device\n\n"
        "Data Storage:\n"
        "• High scores and game progress are stored locally in your user directory\n"
        "• User preferences (volume, display settings) are saved locally\n"
        "• No personal data is transmitted over the internet\n\n"
        "Audio Integration:\n"
        "• Audio visualization uses only audio playing on your local system\n"
        "• No audio data is recorded or transmitted");
    gtk_label_set_justify(GTK_LABEL(privacy_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(privacy_label), TRUE);
    gtk_box_pack_start(GTK_BOX(privacy_box), privacy_label, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(privacy_scrolled), privacy_box);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), privacy_scrolled, 
                            gtk_label_new("Privacy"));
    
    // ========== SUPPORT TAB ==========
    GtkWidget *support_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(support_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(support_scrolled, TRUE);
    gtk_widget_set_vexpand(support_scrolled, TRUE);
    
    GtkWidget *support_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_set_border_width(GTK_CONTAINER(support_box), 30);
    
    GtkWidget *support_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(support_title), 
        "<span weight='bold' size='xx-large'>☕ Support CometBuster</span>");
    gtk_widget_set_halign(support_title, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(support_box), support_title, FALSE, FALSE, 0);
    
    GtkWidget *support_text = gtk_label_new(
        "If you enjoy playing CometBuster and would like to support its development, "
        "consider buying the developer a coffee!");
    gtk_label_set_line_wrap(GTK_LABEL(support_text), TRUE);
    gtk_widget_set_halign(support_text, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(support_box), support_text, FALSE, FALSE, 0);
    
    GtkWidget *support_button = gtk_button_new_with_label("☕ Buy Me a Coffee");
    gtk_widget_set_halign(support_button, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(support_button, 250, 50);
    g_signal_connect(support_button, "clicked", 
                    G_CALLBACK(on_link_clicked), 
                    (gpointer)"https://buymeacoffee.com/jasonbrianhall");
    gtk_box_pack_start(GTK_BOX(support_box), support_button, FALSE, FALSE, 0);
    
    GtkWidget *support_note = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(support_note), 
        "<span foreground='#666666' size='small'>"
        "This is an independent donation platform.\n"
        "This project is not affiliated with or endorsed by any company."
        "</span>");
    gtk_widget_set_halign(support_note, GTK_ALIGN_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(support_note), TRUE);
    gtk_box_pack_start(GTK_BOX(support_box), support_note, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(support_scrolled), support_box);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), support_scrolled, 
                            gtk_label_new("Support"));
    
    // ========== CONTRIBUTING TAB ==========
    GtkWidget *contrib_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(contrib_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(contrib_scrolled, TRUE);
    gtk_widget_set_vexpand(contrib_scrolled, TRUE);
    
    GtkWidget *contrib_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(contrib_box), 20);
    
    GtkWidget *contrib_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(contrib_title), 
        "<span weight='bold' size='large'>🤝 Contributing to CometBuster</span>");
    gtk_widget_set_halign(contrib_title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(contrib_box), contrib_title, FALSE, FALSE, 0);
    
    GtkWidget *contrib_intro = gtk_label_new("This is a community project and contributions are welcome!");
    gtk_label_set_line_wrap(GTK_LABEL(contrib_intro), TRUE);
    gtk_box_pack_start(GTK_BOX(contrib_box), contrib_intro, FALSE, FALSE, 0);
    
    GtkWidget *contrib_sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(contrib_box), contrib_sep1, FALSE, FALSE, 0);
    
    GtkWidget *ways_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(ways_header), 
        "<span weight='bold'>Ways You Can Contribute:</span>");
    gtk_widget_set_halign(ways_header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(contrib_box), ways_header, FALSE, FALSE, 0);
    
    GtkWidget *ways_label = gtk_label_new(
        "🐛 Bug Fixes\nFound a bug? Submit a fix with description and reproduction steps.\n\n"
        "🎮 Gameplay Improvements\nEnhance game mechanics, balance, difficulty curves, or level design.\n\n"
        "🎨 Visual Enhancements\nImprove graphics, particle effects, UI, or create new designs.\n\n"
        "🔊 Audio Integration\nHelp improve audio visualization and reactive gameplay features.\n\n"
        "📚 Documentation\nImprove README, add examples, create tutorials, or enhance code comments.\n\n"
        "🌐 Translation\nHelp translate CometBuster to other languages.");
    gtk_label_set_justify(GTK_LABEL(ways_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(ways_label), TRUE);
    gtk_widget_set_margin_start(ways_label, 15);
    gtk_box_pack_start(GTK_BOX(contrib_box), ways_label, FALSE, FALSE, 0);
    
    GtkWidget *contrib_sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(contrib_box), contrib_sep2, FALSE, FALSE, 0);
    
    GtkWidget *report_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(report_header), 
        "<span weight='bold'>Reporting Issues:</span>");
    gtk_widget_set_halign(report_header, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(contrib_box), report_header, FALSE, FALSE, 0);
    
    GtkWidget *report_label = gtk_label_new(
        "1. Check existing issues\n"
        "2. Provide system info (OS, compiler, libraries)\n"
        "3. Include detailed reproduction steps\n"
        "4. Provide error messages or console output");
    gtk_label_set_justify(GTK_LABEL(report_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(report_label), TRUE);
    gtk_widget_set_margin_start(report_label, 15);
    gtk_box_pack_start(GTK_BOX(contrib_box), report_label, FALSE, FALSE, 0);
    
    GtkWidget *contrib_sep3 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(contrib_box), contrib_sep3, FALSE, FALSE, 0);
    
    GtkWidget *repo_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(repo_label), 
        "<span weight='bold'>Website:</span>");
    gtk_widget_set_halign(repo_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(contrib_box), repo_label, FALSE, FALSE, 0);
    
    GtkWidget *repo_button = gtk_button_new_with_label("🔗 https://jasonbrianhall.github.io");
    gtk_widget_set_halign(repo_button, GTK_ALIGN_START);
    g_signal_connect(repo_button, "clicked", 
                    G_CALLBACK(on_link_clicked), 
                    (gpointer)"https://jasonbrianhall.github.io/");
    gtk_box_pack_start(GTK_BOX(contrib_box), repo_button, FALSE, FALSE, 0);
    
    GtkWidget *contrib_sep4 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(contrib_box), contrib_sep4, FALSE, FALSE, 0);
    
    GtkWidget *thanks_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(thanks_label), 
        "<span weight='bold' foreground='#2E7D32'>Thank you for making CometBuster better!</span>");
    gtk_widget_set_halign(thanks_label, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(contrib_box), thanks_label, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(contrib_scrolled), contrib_box);
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
