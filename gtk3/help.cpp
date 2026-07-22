#ifndef HELPMENU_H
#define HELPMENU_H

#include "audio_player.h"
#include "icon.h"

// Define fallback version if not provided by Makefile
#ifndef VERSION
#define VERSION "Development"
#endif

void on_menu_about(GtkMenuItem *menuitem, gpointer user_data) {
    (void)menuitem;
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    // Get screen resolution to adapt dialog size
    GdkScreen *screen = gtk_widget_get_screen(player->window);
    int screen_width = gdk_screen_get_width(screen);
    int screen_height = gdk_screen_get_height(screen);
    bool use_compact_dialog = (screen_width <= 1024 || screen_height <= 700);
    
    // Create main dialog window
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "About Zenamp",
        GTK_WINDOW(player->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE,
        NULL
    );
    
    // Set dialog properties based on screen size
    if (use_compact_dialog) {
        gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);
        gtk_widget_set_size_request(dialog, 480, 450); // Fit in 800x600
    } else {
        gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
        gtk_widget_set_size_request(dialog, 600, 500);
    }
    
    // Create notebook for tabs
    GtkWidget *notebook = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(notebook), use_compact_dialog ? 5 : 10);
    
    // Get content area and add notebook
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content_area), notebook);
    
    // === ABOUT TAB ===
    // Create scrolled window for the About tab content
    GtkWidget *about_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(about_scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *about_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, use_compact_dialog ? 8 : 10);
    gtk_container_set_border_width(GTK_CONTAINER(about_vbox), use_compact_dialog ? 10 : 20);
    
    // Logo - smaller for compact layout
    GdkPixbuf *logo = load_icon_from_base64();
    if (logo) {
        if (use_compact_dialog) {
            // Scale down logo for small screens
            GdkPixbuf *small_logo = gdk_pixbuf_scale_simple(logo, 48, 48, GDK_INTERP_BILINEAR);
            GtkWidget *logo_image = gtk_image_new_from_pixbuf(small_logo);
            gtk_box_pack_start(GTK_BOX(about_vbox), logo_image, FALSE, FALSE, 0);
            g_object_unref(small_logo);
        } else {
            GtkWidget *logo_image = gtk_image_new_from_pixbuf(logo);
            gtk_box_pack_start(GTK_BOX(about_vbox), logo_image, FALSE, FALSE, 0);
        }
        g_object_unref(logo);
    }
    
    // Program name and version - smaller text for compact layout
    GtkWidget *title_label = gtk_label_new(NULL);
    char version_text[256];
    
    if (use_compact_dialog) {
        snprintf(version_text, sizeof(version_text),
            "<span size='x-large' weight='bold'>Zenamp</span>\n"
            "<span size='medium'>Build #%s</span>", VERSION);
    } else {
        snprintf(version_text, sizeof(version_text),
            "<span size='xx-large' weight='bold'>Zenamp</span>\n"
            "<span size='large'>Build #%s</span>", VERSION);
    }
    
    gtk_label_set_markup(GTK_LABEL(title_label), version_text);
    gtk_label_set_justify(GTK_LABEL(title_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(about_vbox), title_label, FALSE, FALSE, 0);
    
    // Description - more compact for small screens
    const char *description = use_compact_dialog ?
        "Multi-format music player with GTK interface\n\n"
        "Supported Formats:\n"
        "• MIDI (.mid, .midi) - OPL3 synthesis\n"
        "• WAV (.wav) - Uncompressed audio\n"
        "• MP3 (.mp3) - Classic compressed format\n"
        "• OGG (.ogg) - Open-source alternative\n"
        "• FLAC (.flac) - Lossless compression\n"
        "• OPUS (.opus) - Modern codec\n"
        "• AIFF (.aiff) - Apple audio format\n"
        "• CDG (.zip) - Karaoke Files\n\n"      
        "Other formats"      

        "Features:\n"
        "• Playlist queue with repeat mode\n"
        "• Audio visualizer\n"
        "• 3-band equalizer\n"
        "• Progress seeking\n"
        "• 5-second skip controls\n"
        "• Previous/next navigation\n"
        "• Volume control\n"
        "• GTK interface"
        :
        "A multi-format music player with GTK interface\n\n"
        "Supported Formats:\n"
        "• MIDI (.mid, .midi) - Nostalgic bleeps and bloops with OPL3 synthesis\n"
        "• WAV (.wav) - Uncompressed audio goodness, chunky but delicious\n"
        "• MP3 (.mp3) - The classic compressed format that conquered the world\n"
        "• OGG (.ogg) - Open-source alternative that sounds great\n"
        "• FLAC (.flac) - Lossless perfection for the audiophiles\n"
        "• OPUS (.opus) - Modern codec that's small but mighty\n"
        "• AIFF (.aiff) - Apple's answer to WAV, crisp and clean\n\n"
        "• CDG (.zip) - Karaoke Files\n\n"      
        "Other formats supported by either FFMPEG (Linux) or Windows Native"      
        "Features:\n"
        "• Playlist queue with repeat mode\n"
        "• Audio visualizer for eye candy while you listen\n"
        "• Equalizer to fine-tune your sound\n"
        "• Drag progress slider to seek\n"
        "• << and >> buttons for 5-second rewind/fast-forward\n"
        "• |< and >| buttons for previous/next song\n"
        "• Volume control\n"
        "• GTK interface\n";
    
    GtkWidget *desc_label = gtk_label_new(description);
    gtk_label_set_justify(GTK_LABEL(desc_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(desc_label), TRUE);
    gtk_box_pack_start(GTK_BOX(about_vbox), desc_label, TRUE, TRUE, 0);
    
    // Author and website
    GtkWidget *author_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(author_label), 
        "<b>Author:</b> Jason Hall\n"
        "<b>Website:</b> <a href=\"https://jasonbrianhall.github.io/zenamp\">"
        "GitHub Repository</a>");
    gtk_label_set_justify(GTK_LABEL(author_label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(about_vbox), author_label, FALSE, FALSE, 0);
    
    // Add the vbox to the scrolled window
    if (use_compact_dialog) {
        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(about_scrolled), about_vbox);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), about_scrolled, 
                                gtk_label_new("About"));
    } else {
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), about_vbox, 
                                gtk_label_new("About"));
    }
    
    // === KEYBOARD SHORTCUTS TAB ===
    GtkWidget *shortcuts_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(shortcuts_scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *shortcuts_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, use_compact_dialog ? 12 : 15);
    gtk_container_set_border_width(GTK_CONTAINER(shortcuts_vbox), use_compact_dialog ? 15 : 25);

    GtkWidget *shortcuts_heading = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(shortcuts_heading),
        "<span size='x-large' weight='bold'>Keyboard Shortcuts</span>");
    gtk_label_set_justify(GTK_LABEL(shortcuts_heading), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(shortcuts_vbox), shortcuts_heading, FALSE, FALSE, 0);

    GtkWidget *shortcuts_general_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(shortcuts_general_label),
        "<b>Visualization</b>\n"
        "<tt>F9 or F</tt>   Toggle fullscreen visualization\n"
        "<tt>Q</tt>         Next visualization type\n"
        "<tt>A</tt>         Previous visualization type");
    gtk_label_set_justify(GTK_LABEL(shortcuts_general_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(shortcuts_general_label), TRUE);
    gtk_widget_set_halign(shortcuts_general_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(shortcuts_vbox), shortcuts_general_label, FALSE, FALSE, 0);

    GtkWidget *shortcuts_karafun_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(shortcuts_karafun_label),
        "<b>Karaoke (Karafun .kfn files)</b>\n"
        "<tt>V</tt>         Toggle vocal track on/off\n"
        "<tt>B</tt>         Toggle backing track on/off\n\n"
        "Muting vocals lets you sing along without the original vocalist; "
        "muting the backing track isolates the vocals for practice. At least "
        "one track always stays on — toggling one back in automatically "
        "un-mutes the other if both would otherwise go silent. These only "
        "do anything while a Karafun (.kfn) file is loaded.");
    gtk_label_set_justify(GTK_LABEL(shortcuts_karafun_label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap(GTK_LABEL(shortcuts_karafun_label), TRUE);
    gtk_widget_set_halign(shortcuts_karafun_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(shortcuts_vbox), shortcuts_karafun_label, FALSE, FALSE, 0);

    GtkWidget *shortcuts_spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(shortcuts_vbox), shortcuts_spacer, TRUE, TRUE, 0);

    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(shortcuts_scrolled), shortcuts_vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), shortcuts_scrolled,
                            gtk_label_new("Shortcuts"));

    // === LICENSE TAB ===
    GtkWidget *license_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(license_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    if (use_compact_dialog) {
        gtk_widget_set_size_request(license_scrolled, 450, 350);
    } else {
        gtk_widget_set_size_request(license_scrolled, 550, 400);
    }
    
    GtkWidget *license_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(license_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(license_textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(license_textview), GTK_WRAP_WORD);
    
    GtkTextBuffer *license_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(license_textview));
    const char *mit_license = 
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
        "SOFTWARE.";
    
    gtk_text_buffer_set_text(license_buffer, mit_license, -1);
    gtk_container_add(GTK_CONTAINER(license_scrolled), license_textview);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), license_scrolled, 
                            gtk_label_new("License"));
    
    // === PRIVACY POLICY TAB ===
    GtkWidget *privacy_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(privacy_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    if (use_compact_dialog) {
        gtk_widget_set_size_request(privacy_scrolled, 450, 350);
    } else {
        gtk_widget_set_size_request(privacy_scrolled, 550, 400);
    }
    
    GtkWidget *privacy_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(privacy_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(privacy_textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(privacy_textview), GTK_WRAP_WORD);
    
    GtkTextBuffer *privacy_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(privacy_textview));
    const char *privacy_policy = 
        "Privacy Policy for Zenamp\n\n"
        "Last updated: September 11, 2025\n\n"
        "Information We Collect:\n"
        "Zenamp is a local music player that does not collect, store, or transmit any "
        "personal information to external servers.\n\n"
        "Local File Access:\n"
        "- The application only accesses audio files that you explicitly choose to open\n"
        "- No file information is shared with third parties\n"
        "- All processing happens locally on your device\n\n"
        "Data Storage:\n"
        "- The application may store user preferences (volume settings, equalizer settings) "
        "locally on your device\n"
        "- No personal data is transmitted over the internet\n\n"
        "Contact:\n"
        "For questions about this privacy policy, contact jasonbrianhall@yahoo.com";
    
    gtk_text_buffer_set_text(privacy_buffer, privacy_policy, -1);
    gtk_container_add(GTK_CONTAINER(privacy_scrolled), privacy_textview);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), privacy_scrolled, 
                            gtk_label_new("Privacy Policy"));
    
    // === SUPPORT TAB ===
    GtkWidget *support_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(support_scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    
    GtkWidget *support_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, use_compact_dialog ? 12 : 15);
    gtk_container_set_border_width(GTK_CONTAINER(support_vbox), use_compact_dialog ? 15 : 25);
    
    // Support heading
    GtkWidget *support_heading = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(support_heading), 
        "<span size='x-large' weight='bold'>Support Zenamp</span>");
    gtk_label_set_justify(GTK_LABEL(support_heading), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(support_vbox), support_heading, FALSE, FALSE, 0);
    
    // Support message
    GtkWidget *support_message = gtk_label_new(
        "If you enjoy using Zenamp and would like to support "
        "its development, consider buying the developer a coffee!");
    gtk_label_set_justify(GTK_LABEL(support_message), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(support_message), TRUE);
    gtk_box_pack_start(GTK_BOX(support_vbox), support_message, FALSE, FALSE, 0);
    
    // Coffee emoji
    GtkWidget *coffee_emoji = gtk_label_new("☕");
    GtkWidget *emoji_align = gtk_alignment_new(0.5, 0.5, 0, 0);
    gtk_container_add(GTK_CONTAINER(emoji_align), coffee_emoji);
    gtk_box_pack_start(GTK_BOX(support_vbox), emoji_align, FALSE, FALSE, 0);
    
    // Buy Me a Coffee link
    GtkWidget *bmac_link = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(bmac_link),
        "<b>Buy Me a Coffee</b>\n\n"
        "<a href=\"https://buymeacoffee.com/jasonbrianhall\">"
        "https://buymeacoffee.com/jasonbrianhall</a>");
    gtk_label_set_justify(GTK_LABEL(bmac_link), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(bmac_link), TRUE);
    gtk_box_pack_start(GTK_BOX(support_vbox), bmac_link, FALSE, FALSE, 0);
    
    // Disclaimer
    GtkWidget *disclaimer = gtk_label_new(
        "This is an independent donation platform.\n"
        "Microsoft is not affiliated with or responsible for this donation link.");
    gtk_label_set_justify(GTK_LABEL(disclaimer), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(disclaimer), TRUE);
    GtkWidget *disclaimer_event = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(disclaimer_event), disclaimer);
    gtk_box_pack_start(GTK_BOX(support_vbox), disclaimer_event, FALSE, FALSE, use_compact_dialog ? 10 : 15);
    
    // Add spacer to push content to top
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(support_vbox), spacer, TRUE, TRUE, 0);
    
    // Add vbox to scrolled window
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(support_scrolled), support_vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), support_scrolled, 
                            gtk_label_new("Support"));
    
    // === CONTRIBUTING TAB ===
    GtkWidget *contrib_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(contrib_scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    if (use_compact_dialog) {
        gtk_widget_set_size_request(contrib_scrolled, 450, 350);
    } else {
        gtk_widget_set_size_request(contrib_scrolled, 550, 400);
    }
    
    GtkWidget *contrib_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(contrib_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(contrib_textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(contrib_textview), GTK_WRAP_WORD);
    
    GtkTextBuffer *contrib_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(contrib_textview));
    const char *contributing_text = 
        "Contributing to Zenamp\n\n"
        "This is a community project and contributions are welcome!\n\n"
        "Ways You Can Contribute:\n\n"
        "🐛 Bug Fixes\n"
        "Found a bug? Submit a pull request with a fix. Please include:\n"
        "- Description of the bug\n"
        "- Steps to reproduce\n"
        "- System information (OS, GTK version)\n\n"
        "🎨 New Visualizations\n"
        "Create amazing new visualizations! This is a great way to contribute.\n"
        "Documentation and examples are available in the source code.\n\n"
        "🎵 Format Support\n"
        "Help improve audio format support for various file types.\n\n"
        "📚 Documentation\n"
        "Improve README, add examples, or create tutorials.\n\n"
        "🌍 Translation\n"
        "Help translate Zenamp to other languages.\n\n"
        "Reporting Issues:\n\n"
        "1. Check existing issues on GitHub\n"
        "2. Provide system info (OS, GTK version, audio backend)\n"
        "3. Include console output if available\n"
        "4. Describe steps to reproduce\n\n"
        "Repository:\n"
        "https://github.com/jasonbrianhall/zenamp\n\n"
        "Thank you for making Zenamp better! ⭐";
    
    gtk_text_buffer_set_text(contrib_buffer, contributing_text, -1);
    gtk_container_add(GTK_CONTAINER(contrib_scrolled), contrib_textview);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), contrib_scrolled, 
                            gtk_label_new("Contributing"));
    
    // Show all widgets
    gtk_widget_show_all(dialog);
    
    // Run dialog and clean up
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

#endif
