#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include "equalizer.h"
#include "audio_player.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern AudioPlayer *player;

Equalizer* equalizer_new(int sample_rate) {
    Equalizer *eq = (Equalizer*)malloc(sizeof(Equalizer));
    if (!eq) return NULL;
    
    memset(eq, 0, sizeof(Equalizer));
    eq->sample_rate = sample_rate;
    eq->enabled = true;
    
    // Initialize with neutral settings (0 dB gain)
    eq->bass_gain_db = 0.0;
    eq->mid_gain_db = 0.0;
    eq->treble_gain_db = 0.0;
    
    // Set up frequency bands
    // Bass: 100 Hz, Mid: 1000 Hz, Treble: 8000 Hz
    calculate_biquad_coefficients(&eq->bands[0], 100.0, 0.0, 0.7, sample_rate);
    calculate_biquad_coefficients(&eq->bands[1], 1000.0, 0.0, 0.7, sample_rate);
    calculate_biquad_coefficients(&eq->bands[2], 8000.0, 0.0, 0.7, sample_rate);
    
    return eq;
}

void equalizer_free(Equalizer *eq) {
    if (eq) {
        free(eq);
    }
}

void equalizer_set_enabled(Equalizer *eq, bool enabled) {
    if (eq) {
        eq->enabled = enabled;
    }
}

void equalizer_set_bass(Equalizer *eq, double gain_db) {
    if (!eq) return;
    
    // Clamp gain to reasonable range
    if (gain_db < -12.0) gain_db = -12.0;
    if (gain_db > 12.0) gain_db = 12.0;
    
    eq->bass_gain_db = gain_db;
    calculate_biquad_coefficients(&eq->bands[0], 100.0, gain_db, 0.7, eq->sample_rate);
}

void equalizer_set_mid(Equalizer *eq, double gain_db) {
    if (!eq) return;
    
    // Clamp gain to reasonable range
    if (gain_db < -12.0) gain_db = -12.0;
    if (gain_db > 12.0) gain_db = 12.0;
    
    eq->mid_gain_db = gain_db;
    calculate_biquad_coefficients(&eq->bands[1], 1000.0, gain_db, 0.7, eq->sample_rate);
}

void equalizer_set_treble(Equalizer *eq, double gain_db) {
    if (!eq) return;
    
    // Clamp gain to reasonable range
    if (gain_db < -12.0) gain_db = -12.0;
    if (gain_db > 12.0) gain_db = 12.0;
    
    eq->treble_gain_db = gain_db;
    calculate_biquad_coefficients(&eq->bands[2], 8000.0, gain_db, 0.7, eq->sample_rate);
}

void equalizer_reset(Equalizer *eq) {
    if (!eq) return;
    
    // Reset all filter states
    for (int i = 0; i < EQ_BANDS; i++) {
        memset(eq->bands[i].x, 0, sizeof(eq->bands[i].x));
        memset(eq->bands[i].y, 0, sizeof(eq->bands[i].y));
    }
}

int16_t equalizer_process_sample(Equalizer *eq, int16_t input) {
    if (!eq || !eq->enabled) return input;
    
    double sample = (double)input / 32768.0;  // Convert to float
    double output = sample;
    
    // Apply each band filter
    for (int i = 0; i < EQ_BANDS; i++) {
        output = biquad_filter(&eq->bands[i], output);
    }
    
    // Convert back to int16_t with clipping
    output *= 32768.0;
    if (output > 32767.0) output = 32767.0;
    if (output < -32768.0) output = -32768.0;
    
    return (int16_t)output;
}

void equalizer_process_buffer(Equalizer *eq, int16_t *buffer, size_t length) {
    if (!eq || !eq->enabled || !buffer) return;
    
    for (size_t i = 0; i < length; i++) {
        buffer[i] = equalizer_process_sample(eq, buffer[i]);
    }
}

void calculate_biquad_coefficients(EQBand *band, double frequency, double gain_db, double q, int sample_rate) {
    if (!band) return;
    
    double w = 2.0 * M_PI * frequency / sample_rate;
    double cos_w = cos(w);
    double sin_w = sin(w);
    double A = pow(10.0, gain_db / 40.0);  // Square root of linear gain
    double alpha = sin_w / (2.0 * q);
    
    // Peaking EQ coefficients
    double b0 = 1.0 + alpha * A;
    double b1 = -2.0 * cos_w;
    double b2 = 1.0 - alpha * A;
    double a0 = 1.0 + alpha / A;
    double a1 = -2.0 * cos_w;
    double a2 = 1.0 - alpha / A;
    
    // Normalize coefficients
    band->b[0] = b0 / a0;
    band->b[1] = b1 / a0;
    band->b[2] = b2 / a0;
    band->a[0] = 1.0;
    band->a[1] = a1 / a0;
    band->a[2] = a2 / a0;
    
    band->frequency = frequency;
    band->q_factor = q;
    band->gain = db_to_linear(gain_db);
}

double db_to_linear(double db) {
    return pow(10.0, db / 20.0);
}

double biquad_filter(EQBand *band, double input) {
    if (!band) return input;
    
    // Biquad filter implementation (Direct Form II)
    double output = band->b[0] * input + band->b[1] * band->x[0] + band->b[2] * band->x[1]
                   - band->a[1] * band->y[0] - band->a[2] * band->y[1];
    
    // Shift delay line
    band->x[1] = band->x[0];
    band->x[0] = input;
    band->y[1] = band->y[0];
    band->y[0] = output;
    
    return output;
}

void on_eq_enabled_toggled(GtkToggleButton *button, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    bool enabled = gtk_toggle_button_get_active(button);
    equalizer_set_enabled(player->equalizer, enabled);
    
    // Enable/disable the EQ controls
    gtk_widget_set_sensitive(player->bass_scale, enabled);
    gtk_widget_set_sensitive(player->mid_scale, enabled);
    gtk_widget_set_sensitive(player->treble_scale, enabled);
    gtk_widget_set_sensitive(player->eq_reset_button, enabled);
}

void on_bass_changed(GtkRange *range, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    double value = gtk_range_get_value(range);
    equalizer_set_bass(player->equalizer, value);
}

void on_mid_changed(GtkRange *range, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    double value = gtk_range_get_value(range);
    equalizer_set_mid(player->equalizer, value);
}

void on_treble_changed(GtkRange *range, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    double value = gtk_range_get_value(range);
    equalizer_set_treble(player->equalizer, value);
}

void on_eq_reset_clicked(GtkButton *button, gpointer user_data) {
    AudioPlayer *player = (AudioPlayer*)user_data;
    
    // Reset all sliders to 0
    gtk_range_set_value(GTK_RANGE(player->bass_scale), 0.0);
    gtk_range_set_value(GTK_RANGE(player->mid_scale), 0.0);
    gtk_range_set_value(GTK_RANGE(player->treble_scale), 0.0);
    
    // Reset equalizer
    equalizer_set_bass(player->equalizer, 0.0);
    equalizer_set_mid(player->equalizer, 0.0);
    equalizer_set_treble(player->equalizer, 0.0);
    equalizer_reset(player->equalizer);
}

// Function to create equalizer controls:
GtkWidget* create_equalizer_controls(AudioPlayer *player) {
    player->eq_frame = gtk_frame_new(NULL);

    GtkWidget *eq_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    // GtkFrame has a single dedicated child slot in GTK4 (gtk_frame_set_child)
    // rather than going through the removed generic GtkContainer API.
    gtk_frame_set_child(GTK_FRAME(player->eq_frame), eq_vbox);
    // gtk_container_set_border_width() is gone - GTK4 widgets carry their own margins.
    gtk_widget_set_margin_start(eq_vbox, 5);
    gtk_widget_set_margin_end(eq_vbox, 5);
    gtk_widget_set_margin_top(eq_vbox, 5);
    gtk_widget_set_margin_bottom(eq_vbox, 5);

    // GTK4 removed GdkScreen - monitor geometry comes from GdkMonitor now,
    // already reported in logical (scale-adjusted) pixels.
    GdkSurface *win_surface = gtk_native_get_surface(GTK_NATIVE(player->window));
    GdkDisplay *display = gtk_widget_get_display(player->window);
    GdkMonitor *monitor = win_surface ? gdk_display_get_monitor_at_surface(display, win_surface) : NULL;
    GdkRectangle monitor_geom = {0, 0, 1920, 1080};
    if (monitor) {
        gdk_monitor_get_geometry(monitor, &monitor_geom);
    }
    int screen_width = monitor_geom.width;
    int screen_height = monitor_geom.height;
    int scale = gtk_widget_get_scale_factor(player->window);
    (void)scale; // no longer needed for manual division - GdkMonitor geometry is already logical
    
    int slider_width, slider_height;
    bool use_horizontal;
    bool reset_button_on_side=true;

    use_horizontal = false;
    
    if (screen_width <= 800 || screen_height <= 600) {
        slider_width = 100;
        slider_height = 60;
        reset_button_on_side = true;
        SDL_Log("EQ: Using very small screen layout with side reset button");
    } else if (screen_width < 1200 || screen_height < 800) {
        slider_width = 110;
        slider_height = 80;
        SDL_Log("EQ: Using small-medium screen layout with side reset button");
        reset_button_on_side = true;
    } else {
        slider_width = scale_size(150, screen_width, 1920);
        slider_height = scale_size(150, screen_height, 1080);
        slider_width = fmax(slider_width, 120);
        slider_height = fmax(slider_height, 120);
        reset_button_on_side = false;
        SDL_Log("EQ: Using large screen layout with bottom reset button");
    }

    SDL_Log("EQ sliders: %dx%d, horizontal=%s, reset_on_side=%s", 
           slider_width, slider_height, use_horizontal ? "yes" : "no",
           reset_button_on_side ? "yes" : "no");
    if (reset_button_on_side) {
        GtkWidget *eq_controls_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_widget_set_vexpand(eq_controls_box, TRUE);
        gtk_box_append(GTK_BOX(eq_vbox), eq_controls_box);

        GtkOrientation orientation = GTK_ORIENTATION_HORIZONTAL;
        int label_width = 60;

        GtkWidget *bass_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        GtkWidget *bass_label = gtk_label_new("Bass");
        gtk_widget_set_size_request(bass_label, label_width, -1);
        gtk_label_set_xalign(GTK_LABEL(bass_label), 1.0);

        player->bass_scale = gtk_scale_new_with_range(orientation, -12.0, 12.0, 0.5);
        gtk_range_set_value(GTK_RANGE(player->bass_scale), 0.0);
        gtk_scale_set_draw_value(GTK_SCALE(player->bass_scale), TRUE);
        gtk_scale_set_value_pos(GTK_SCALE(player->bass_scale), GTK_POS_BOTTOM);
        gtk_widget_set_size_request(player->bass_scale, slider_width, slider_height);
        gtk_range_set_inverted(GTK_RANGE(player->bass_scale), FALSE);
        gtk_widget_set_can_focus(player->bass_scale, TRUE);
        gtk_widget_set_hexpand(player->bass_scale, TRUE);
        g_signal_connect(player->bass_scale, "value-changed", G_CALLBACK(on_bass_changed), player);

        gtk_box_append(GTK_BOX(bass_hbox), bass_label);
        gtk_box_append(GTK_BOX(bass_hbox), player->bass_scale);
        gtk_widget_set_vexpand(bass_hbox, TRUE);
        gtk_box_append(GTK_BOX(eq_controls_box), bass_hbox);

        GtkWidget *mid_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        GtkWidget *mid_label = gtk_label_new("Mid");
        gtk_widget_set_size_request(mid_label, label_width, -1);
        gtk_label_set_xalign(GTK_LABEL(mid_label), 1.0);

        player->mid_scale = gtk_scale_new_with_range(orientation, -12.0, 12.0, 0.5);
        gtk_range_set_value(GTK_RANGE(player->mid_scale), 0.0);
        gtk_scale_set_draw_value(GTK_SCALE(player->mid_scale), TRUE);
        gtk_scale_set_value_pos(GTK_SCALE(player->mid_scale), GTK_POS_BOTTOM);
        gtk_widget_set_size_request(player->mid_scale, slider_width, slider_height);
        gtk_range_set_inverted(GTK_RANGE(player->mid_scale), FALSE);
        gtk_widget_set_can_focus(player->mid_scale, TRUE);
        gtk_widget_set_hexpand(player->mid_scale, TRUE);
        g_signal_connect(player->mid_scale, "value-changed", G_CALLBACK(on_mid_changed), player);

        gtk_box_append(GTK_BOX(mid_hbox), mid_label);
        gtk_box_append(GTK_BOX(mid_hbox), player->mid_scale);
        gtk_widget_set_vexpand(mid_hbox, TRUE);
        gtk_box_append(GTK_BOX(eq_controls_box), mid_hbox);

        GtkWidget *treble_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        GtkWidget *treble_label = gtk_label_new("Treble");
        gtk_widget_set_size_request(treble_label, label_width, -1);
        gtk_label_set_xalign(GTK_LABEL(treble_label), 1.0);

        player->treble_scale = gtk_scale_new_with_range(orientation, -12.0, 12.0, 0.5);
        gtk_range_set_value(GTK_RANGE(player->treble_scale), 0.0);
        gtk_scale_set_draw_value(GTK_SCALE(player->treble_scale), TRUE);
        gtk_scale_set_value_pos(GTK_SCALE(player->treble_scale), GTK_POS_BOTTOM);
        gtk_widget_set_size_request(player->treble_scale, slider_width, slider_height);
        gtk_range_set_inverted(GTK_RANGE(player->treble_scale), FALSE);
        gtk_widget_set_can_focus(player->treble_scale, TRUE);
        gtk_widget_set_hexpand(player->treble_scale, TRUE);
        g_signal_connect(player->treble_scale, "value-changed", G_CALLBACK(on_treble_changed), player);

        gtk_box_append(GTK_BOX(treble_hbox), treble_label);
        gtk_box_append(GTK_BOX(treble_hbox), player->treble_scale);
        gtk_widget_set_vexpand(treble_hbox, TRUE);
        gtk_box_append(GTK_BOX(eq_controls_box), treble_hbox);

        player->eq_reset_button = gtk_button_new_with_label("Reset Equalizer");
        gtk_widget_set_can_focus(player->eq_reset_button, TRUE);
        g_signal_connect(player->eq_reset_button, "clicked", G_CALLBACK(on_eq_reset_clicked), player);
        gtk_box_append(GTK_BOX(eq_vbox), player->eq_reset_button);
    } else {
        GtkWidget *eq_controls_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_widget_set_vexpand(eq_controls_box, TRUE);
        gtk_box_append(GTK_BOX(eq_vbox), eq_controls_box);

        GtkWidget *bass_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *bass_label = gtk_label_new("Bass");
        
        GtkOrientation orientation = GTK_ORIENTATION_VERTICAL;
        player->bass_scale = gtk_scale_new_with_range(orientation, -12.0, 12.0, 0.5);
        
        gtk_range_set_value(GTK_RANGE(player->bass_scale), 0.0);
        gtk_scale_set_draw_value(GTK_SCALE(player->bass_scale), TRUE);
        gtk_scale_set_value_pos(GTK_SCALE(player->bass_scale), GTK_POS_BOTTOM);
        gtk_widget_set_size_request(player->bass_scale, slider_width, slider_height);
        gtk_range_set_inverted(GTK_RANGE(player->bass_scale), TRUE);
        gtk_widget_set_can_focus(player->bass_scale, TRUE);
        g_signal_connect(player->bass_scale, "value-changed", G_CALLBACK(on_bass_changed), player);
        gtk_box_append(GTK_BOX(bass_vbox), bass_label);
        gtk_box_append(GTK_BOX(bass_vbox), player->bass_scale);
        gtk_widget_set_hexpand(bass_vbox, TRUE);
        gtk_box_append(GTK_BOX(eq_controls_box), bass_vbox);

        GtkWidget *mid_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *mid_label = gtk_label_new("Mid");
        player->mid_scale = gtk_scale_new_with_range(orientation, -12.0, 12.0, 0.5);
        
        gtk_range_set_value(GTK_RANGE(player->mid_scale), 0.0);
        gtk_scale_set_draw_value(GTK_SCALE(player->mid_scale), TRUE);
        gtk_scale_set_value_pos(GTK_SCALE(player->mid_scale), GTK_POS_BOTTOM);
        gtk_widget_set_size_request(player->mid_scale, slider_width, slider_height);
        gtk_range_set_inverted(GTK_RANGE(player->mid_scale), TRUE);
        gtk_widget_set_can_focus(player->mid_scale, TRUE);
        g_signal_connect(player->mid_scale, "value-changed", G_CALLBACK(on_mid_changed), player);
        gtk_box_append(GTK_BOX(mid_vbox), mid_label);
        gtk_box_append(GTK_BOX(mid_vbox), player->mid_scale);
        gtk_widget_set_hexpand(mid_vbox, TRUE);
        gtk_box_append(GTK_BOX(eq_controls_box), mid_vbox);

        GtkWidget *treble_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *treble_label = gtk_label_new("Treble");
        player->treble_scale = gtk_scale_new_with_range(orientation, -12.0, 12.0, 0.5);
        
        gtk_range_set_value(GTK_RANGE(player->treble_scale), 0.0);
        gtk_scale_set_draw_value(GTK_SCALE(player->treble_scale), TRUE);
        gtk_scale_set_value_pos(GTK_SCALE(player->treble_scale), GTK_POS_BOTTOM);
        gtk_widget_set_size_request(player->treble_scale, slider_width, slider_height);
        gtk_range_set_inverted(GTK_RANGE(player->treble_scale), TRUE);
        gtk_widget_set_can_focus(player->treble_scale, TRUE);
        g_signal_connect(player->treble_scale, "value-changed", G_CALLBACK(on_treble_changed), player);
        gtk_box_append(GTK_BOX(treble_vbox), treble_label);
        gtk_box_append(GTK_BOX(treble_vbox), player->treble_scale);
        gtk_widget_set_hexpand(treble_vbox, TRUE);
        gtk_box_append(GTK_BOX(eq_controls_box), treble_vbox);

        player->eq_reset_button = gtk_button_new_with_label("Reset Equalizer");
        gtk_widget_set_can_focus(player->eq_reset_button, TRUE);
        g_signal_connect(player->eq_reset_button, "clicked", G_CALLBACK(on_eq_reset_clicked), player);
        gtk_box_append(GTK_BOX(eq_vbox), player->eq_reset_button);
    }

    return player->eq_frame;
}





