#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
#include "visualization.h"
#include "audio_player.h"

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <shlobj.h>
#endif

#include <time.h>

Visualizer* visualizer_new(void) {
    Visualizer *vis = g_malloc0(sizeof(Visualizer));
    srand(time(NULL));
    // Initialize arrays
    vis->audio_samples = g_malloc0(VIS_SAMPLES * sizeof(double));
    vis->frequency_bands = g_malloc0(VIS_FREQUENCY_BARS * sizeof(double));
    vis->peak_data = g_malloc0(VIS_FREQUENCY_BARS * sizeof(double));
    vis->band_values = g_malloc0(VIS_FREQUENCY_BARS * sizeof(double));
    
    // Initialize history
    for (int i = 0; i < VIS_HISTORY_SIZE; i++) {
        vis->history[i] = g_malloc0(VIS_FREQUENCY_BARS * sizeof(double));
    }
    vis->history_index = 0;
    
    // Initialize simple frequency band analysis
    init_frequency_bands(vis);

    // Fractal
    init_mandelbrot_system(vis);
    
    // Create drawing area with DPI awareness
    vis->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(vis->drawing_area, 400, 200);
    
    // Make drawing area DPI aware
    g_signal_connect(vis->drawing_area, "realize", G_CALLBACK(on_visualizer_realize), vis);
    
    // Default settings
    vis->type = VIS_WAVEFORM;
    
    // Try to load last visualization type
    VisualizationType last_vis_type;
    if (load_last_visualization(&last_vis_type)) {
        vis->type = last_vis_type;
        printf("Restored last visualization type: %d\n", last_vis_type);
    }    
    
    vis->showing_error=false;
    vis->error_display_time=0.0;
    vis->sensitivity = 1.0;
    vis->decay_rate = 0.95;
    vis->enabled = TRUE;
    vis->volume_level = 0.0;
    
    // Default color scheme (dark theme)
    vis->bg_r = 0.1; vis->bg_g = 0.1; vis->bg_b = 0.1;
    vis->fg_r = 0.0; vis->fg_g = 0.8; vis->fg_b = 0.0;
    vis->accent_r = 0.0; vis->accent_g = 1.0; vis->accent_b = 0.5;
    
    vis->rotation = 0.0;
    vis->time_offset = 0.0;

    vis->cdg_display = NULL;
    vis->cdg_surface = NULL;
    vis->cdg_last_packet = -1;
    
    // Connect signals
    g_signal_connect(vis->drawing_area, "draw", G_CALLBACK(on_visualizer_draw), vis);
    g_signal_connect(vis->drawing_area, "configure-event", G_CALLBACK(on_visualizer_configure), vis);
    
    // Start animation timer for 30 FPS (33.33ms per frame)
    vis->timer_id = g_timeout_add(33, visualizer_timer_callback, vis);
    init_fireworks_system(vis);
    init_dna_system(vis);
    init_dna2_system(vis);
    init_sudoku_system(vis);
    init_ripple_system(vis);
    init_bouncy_ball_system(vis);
    init_clock_system(vis);
    init_analog_clock_system(vis);
    init_robot_chaser_system(vis);
    init_radial_wave_system(vis);
    init_blockstack_system(vis);
    init_hanoi_system(vis);
    init_beat_chess_system(vis);
    init_beat_checkers_system(vis);
    init_maze3d_system(vis);
    init_minesweeper(vis);
    pong_init(vis);
    init_comet_buster_system(vis);
    
    vis->track_info_display_time = 0.0;
    vis->track_info_fade_alpha = 1.0;
    memset(vis->track_info_title, 0, sizeof(vis->track_info_title));
    memset(vis->track_info_artist, 0, sizeof(vis->track_info_artist));
    memset(vis->track_info_album, 0, sizeof(vis->track_info_album));
    vis->track_info_duration = 0;

    init_bouncing_circle_system(vis);
    
    // Auto-cleanup when widget is destroyed
    g_object_set_data_full(G_OBJECT(vis->drawing_area), "visualizer", vis, (GDestroyNotify)visualizer_free);
    
    return vis;
}

void visualizer_free(Visualizer *vis) {
    if (!vis) return;
    
    printf("Freeing Visualizer\n");

    if (player->visualizer) {
        save_last_visualization(player->visualizer->type);
    }

    if (vis->timer_id > 0) {
        g_source_remove(vis->timer_id);
    }
    
    g_free(vis->audio_samples);
    g_free(vis->frequency_bands);
    g_free(vis->peak_data);
    g_free(vis->band_values);
    
    for (int i = 0; i < VIS_FREQUENCY_BARS; i++) {
        if (vis->band_filters[i]) {
            g_free(vis->band_filters[i]);
        }
    }
    
    for (int i = 0; i < VIS_HISTORY_SIZE; i++) {
        g_free(vis->history[i]);
    }
    
    if (vis->surface) {
        cairo_surface_destroy(vis->surface);
    }
    
    if (vis->sudoku_solver) {
        delete vis->sudoku_solver;
        vis->sudoku_solver = NULL;
    }
    
    if (vis->background_solver) {
        delete vis->background_solver;
        vis->background_solver = NULL;
    }

    if (vis->background_generator) {
        delete vis->background_generator;
        vis->background_generator = NULL;
    }


    if (vis->puzzle_generator) {
        delete vis->puzzle_generator;
        vis->puzzle_generator = NULL;
    }

    if (vis->cdg_surface) {
        cairo_surface_destroy(vis->cdg_surface);
    }

    chess_cleanup_thinking_state(&vis->beat_chess.thinking_state);
    checkers_cleanup_thinking_state(&vis->beat_checkers.thinking_state);
    g_free(vis);
}

// Check if a visualization type is an interactive game
static gboolean is_interactive_game(VisualizationType type) {
    switch (type) {
        // Interactive games that should never stop and maintain 30 FPS
        case VIS_BEAT_CHESS:
        case VIS_BEAT_CHECKERS:
        case VIS_BOUNCY_BALLS:
        case VIS_BUBBLES:
        case VIS_COMET_BUSTER:
        case VIS_FIREWORKS:
        case VIS_MATRIX:
        case VIS_MINESWEEPER:
        case VIS_PONG:
        case VIS_RABBITHARE:
        case VIS_RIPPLES:
            return TRUE;
        default:
            return FALSE;
    }
}

void visualizer_set_type(Visualizer *vis, VisualizationType type) {
    if (vis) {
        vis->type = type;
        
        // Find the combo box widget using the global player reference
        if (player && player->vis_controls) {
            // Search for the combo box by name
            GList *children = gtk_container_get_children(GTK_CONTAINER(player->vis_controls));
            for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
                GtkWidget *widget = GTK_WIDGET(iter->data);
                const char *name = gtk_widget_get_name(widget);
                
                if (name && strcmp(name, "vis_type_combo") == 0) {
                    // Found it! Update the combo box
                    g_signal_handlers_block_by_func(widget, 
                                                   (gpointer)on_vis_type_changed, 
                                                   vis);
                    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), type);
                    g_signal_handlers_unblock_by_func(widget, 
                                                     (gpointer)on_vis_type_changed, 
                                                     vis);
                    break;
                }
            }
            g_list_free(children);
        }
        
        gtk_widget_queue_draw(vis->drawing_area);
    }
}

void visualizer_update_audio_data(Visualizer *vis, int16_t *samples, size_t sample_count, int channels) {
    if (!vis || !vis->enabled || !samples || sample_count == 0) return;
    
    // Convert and downsample audio data
    size_t step = sample_count / VIS_SAMPLES;
    if (step == 0) step = 1;
    
    double rms_sum = 0.0;
    
    for (int i = 0; i < VIS_SAMPLES; i++) {
        double sum = 0.0;
        int count = 0;
        
        // Average multiple samples if needed
        for (size_t j = 0; j < step && (i * step + j) < sample_count; j++) {
            if (channels == 1) {
                sum += samples[i * step + j];
            } else {
                // Average stereo channels
                size_t idx = (i * step + j) * 2;
                if (idx + 1 < sample_count * channels) {
                    sum += (samples[idx] + samples[idx + 1]) / 2.0;
                }
            }
            count++;
        }
        
        if (count > 0) {
            vis->audio_samples[i] = (sum / count) / 32768.0 * vis->sensitivity;
            rms_sum += vis->audio_samples[i] * vis->audio_samples[i];
        } else {
            vis->audio_samples[i] = 0.0;
        }
    }
    
    // Calculate overall volume level (RMS)
    vis->volume_level = sqrt(rms_sum / VIS_SAMPLES);
    
    // Process simple frequency analysis
    process_audio_simple(vis);
}

void visualizer_set_enabled(Visualizer *vis, gboolean enabled) {
    if (vis) {
        vis->enabled = enabled;
        if (!enabled) {
            // Clear visualization data
            memset(vis->frequency_bands, 0, VIS_FREQUENCY_BARS * sizeof(double));
            memset(vis->peak_data, 0, VIS_FREQUENCY_BARS * sizeof(double));
            vis->volume_level = 0.0;
            gtk_widget_queue_draw(vis->drawing_area);
        }
    }
}

void init_frequency_bands(Visualizer *vis) {
    // Create simple frequency band filters using moving averages
    // This is a basic approximation without FFT
    for (int band = 0; band < VIS_FREQUENCY_BARS; band++) {
        vis->band_filters[band] = g_malloc0(VIS_SAMPLES * sizeof(double));
        
        // Create simple band-pass filter coefficients
        // Lower bands = longer averaging windows (bass)
        // Higher bands = shorter averaging windows (treble)
        int window_size = VIS_SAMPLES / (band + 2);
        if (window_size < 2) window_size = 2;
        if (window_size > VIS_SAMPLES / 4) window_size = VIS_SAMPLES / 4;
        
        for (int i = 0; i < window_size; i++) {
            vis->band_filters[band][i] = 1.0 / window_size;
        }
    }
}

void process_audio_simple(Visualizer *vis) {
    // Simple frequency band analysis using filtering
    for (int band = 0; band < VIS_FREQUENCY_BARS; band++) {
        double band_energy = 0.0;
        
        // Split the audio samples into frequency-like bands
        // Each band analyzes a different section of the sample array
        int samples_per_band = VIS_SAMPLES / VIS_FREQUENCY_BARS;
        int start_idx = band * samples_per_band;
        int end_idx = start_idx + samples_per_band;
        if (end_idx > VIS_SAMPLES) end_idx = VIS_SAMPLES;
        
        // Calculate RMS energy for this band
        for (int i = start_idx; i < end_idx; i++) {
            band_energy += vis->audio_samples[i] * vis->audio_samples[i];
        }
        
        if (end_idx > start_idx) {
            band_energy = sqrt(band_energy / (end_idx - start_idx));
        }
        
        // Apply some frequency-based weighting
        // Higher frequencies get boosted slightly for visual balance
        double freq_weight = 1.0 + (double)band / VIS_FREQUENCY_BARS * 0.5;
        band_energy *= freq_weight;
        
        // Apply logarithmic scaling for better visual representation
        band_energy = log(1.0 + band_energy * 10.0) / log(11.0);
        
        // Clamp to reasonable range
        if (band_energy > 1.0) band_energy = 1.0;
        if (band_energy < 0.0) band_energy = 0.0;
        
        // Update frequency bands with decay
        vis->frequency_bands[band] = fmax(band_energy, vis->frequency_bands[band] * vis->decay_rate);
        
        // Update peaks
        if (band_energy > vis->peak_data[band]) {
            vis->peak_data[band] = band_energy;
        } else {
            vis->peak_data[band] *= 0.98; // Slower peak decay
        }
    }
    
    // Store in history for effects
    memcpy(vis->history[vis->history_index], vis->frequency_bands, VIS_FREQUENCY_BARS * sizeof(double));
    vis->history_index = (vis->history_index + 1) % VIS_HISTORY_SIZE;
}

gboolean on_visualizer_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    Visualizer *vis = (Visualizer*)user_data;
    
    if (!vis->enabled) {
        // Draw disabled state
        cairo_set_source_rgb(cr, vis->bg_r, vis->bg_g, vis->bg_b);
        cairo_paint(cr);
        
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 16);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, "Visualization Disabled", &extents);
        cairo_move_to(cr, (vis->width - extents.width) / 2, (vis->height + extents.height) / 2);
        cairo_show_text(cr, "Visualization Disabled");
        return FALSE;
    }
    
    // Clear background
    cairo_set_source_rgb(cr, vis->bg_r, vis->bg_g, vis->bg_b);
    cairo_paint(cr);

    // Draw error message if showing (stop showing other visualizations)
    if (vis->showing_error && vis->error_display_time > 0) {
        cairo_set_source_rgb(cr, vis->bg_r, vis->bg_g, vis->bg_b);
        cairo_rectangle(cr, 0, 0, vis->width, 100);
        cairo_fill(cr);
        
        cairo_set_source_rgb(cr, 1, 0.2, 0.2);  // Red
        cairo_set_font_size(cr, 24);
        cairo_move_to(cr, 20, 50);
        cairo_show_text(cr, vis->error_message);
        
        vis->error_display_time -= 0.016;  // Subtract one frame time
        if (vis->error_display_time <= 0) {
            vis->showing_error = false;
        }
        return FALSE;
    }
    
    // Draw visualization based on type
    switch (vis->type) {
        case VIS_WAVEFORM:
            draw_waveform(vis, cr);
            break;
        case VIS_OSCILLOSCOPE:
            draw_oscilloscope(vis, cr);
            break;
        case VIS_BARS:
            draw_bars(vis, cr);
            break;
       case VIS_TRIPPY_BARS:
            draw_trippy(vis, cr);
            break;
       case VIS_RADIAL_BARS:
           draw_radial_bars_bouncing(vis, cr);
           break;                                       
        case VIS_CIRCLE:
            draw_circle(vis, cr);
            break;
        case VIS_VOLUME_METER:
            draw_volume_meter(vis, cr);
            break;
        case VIS_BUBBLES:
            draw_bubbles(vis, cr);
            break;
        case VIS_FIREWORKS:
            draw_fireworks(vis, cr);
            break;            
        case VIS_MATRIX:
            draw_matrix(vis, cr);
            draw_matrix(vis, cr);
            break;
        case VIS_DNA_HELIX:
           draw_dna_helix(vis, cr);
           break; 
        case VIS_DNA2_HELIX:
           draw_dna2_helix(vis, cr);
           break; 
        case VIS_SUDOKU_SOLVER:
           draw_sudoku_solver(vis, cr);
           break; 
        case VIS_FOURIER_TRANSFORM:
           draw_fourier_transform(vis, cr);
           break;
        case VIS_RIPPLES:
           draw_ripples(vis, cr);
           break;           
        case VIS_KALEIDOSCOPE:
           draw_kaleidoscope(vis, cr);
           break;
        case VIS_BOUNCY_BALLS:
          draw_bouncy_balls(vis, cr);
          break;
        case VIS_DIGITAL_CLOCK:
          draw_clock_visualization(vis, cr);
          break;
        case VIS_ANALOG_CLOCK:
          draw_analog_clock(vis, cr);
          break;
        case VIS_ROBOT_CHASER:
          //draw_robot_chaser_visualization(vis, cr);
          draw_robot_chaser_visualization_enhanced(vis, cr);
          break;
        case VIS_RADIAL_WAVE:
          draw_radial_wave(vis, cr);
          break;
        case VIS_BLOCK_STACK:
          draw_blockstack(vis, cr);
          break;
       case VIS_PARROT:
          draw_parrot(vis, cr);
          break;
       case VIS_COMET_BUSTER:
          draw_comet_buster(vis, cr);
          break;          
       case VIS_EYE_OF_SAURON:
          draw_eye_of_sauron(vis, cr);
          break;
       case VIS_TOWER_OF_HANOI:
          draw_hanoi(vis, cr);
          break;    
       case VIS_BEAT_CHESS:
          draw_beat_chess(vis, cr);
          break;
       case VIS_BEAT_CHECKERS:
          draw_beat_checkers(vis, cr);
          break;          
       case VIS_FRACTAL_BLOOM:
          draw_waveform_fractal_bloom(vis, cr);
          break;                                         
       case VIS_SYMMETRY_CASCADE:
          draw_waveform_symmetry_cascade(vis, cr);
          break;                                         
       case VIS_DRAW_WORMHOLE:
          draw_stargate(vis, cr);
          break;                                         
       case VIS_BIRTHDAY:
          draw_birthday_candles(vis, cr);
          break;                                         
       case VIS_RABBITHARE:
          draw_rabbithare(vis, cr);
          break;                                         
       case VIS_MAZE_3D:
          draw_maze3d(vis, cr);
          break;                                         
       case VIS_BOUNCING_CIRCLE:
          draw_bouncing_circle(vis, cr);
          break;
       case VIS_MANDELBROT:
          draw_mandelbrot(vis, cr);
          break;
       case VIS_PONG:
          pong_draw(vis, cr);
          break;    
       case VIS_MINESWEEPER:
          minesweeper_draw(vis, cr);
          break;    
       case VIS_KARAOKE:
          draw_karaoke_boring(vis, cr);
          break;          
       case VIS_KARAOKE_EXCITING:
          draw_karaoke_exciting(vis, cr);
          break;          

    }
    draw_track_info_overlay(vis, cr);
    
    return FALSE;
}


gboolean on_visualizer_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data) {
    Visualizer *vis = (Visualizer*)user_data;
    
    vis->width = gtk_widget_get_allocated_width(widget);
    vis->height = gtk_widget_get_allocated_height(widget);
    
    if (vis->surface) {
        cairo_surface_destroy(vis->surface);
    }
    
    vis->surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                                    CAIRO_CONTENT_COLOR,
                                                    vis->width, vis->height);
    
    return TRUE;
}

gboolean visualizer_timer_callback(gpointer user_data) {
    Visualizer *vis = (Visualizer*)user_data;
    static VisualizationType last_vis_type = VIS_WAVEFORM;
    
    if (vis->enabled) {
        bool vis_type_changed = (last_vis_type != vis->type);
        
        // Check if window is visible on screen
        GdkWindow *gdk_window = gtk_widget_get_window(player->window);
        bool is_minimized = gdk_window && (gdk_window_get_state(gdk_window) & GDK_WINDOW_STATE_ICONIFIED);
        bool is_visible = gtk_widget_get_visible(player->window) && 
                         gdk_window && 
                         !is_minimized &&
                         gdk_window_is_visible(gdk_window);
        
        bool is_playing = player && player->is_playing && !player->is_paused;
        bool is_interactive_game_active = is_interactive_game(vis->type);
        
        // For interactive games: ALWAYS update when window is visible, never skip
        // For other visualizations: only update during playback (or on vis type change)
        bool should_update = vis_type_changed || is_interactive_game_active || is_playing;
        bool should_render = is_visible || vis_type_changed;  // Skip rendering only if minimized
        
        if (!should_update) {
            return TRUE; // Keep timer running but skip updates when paused and not interactive
        }
        
        last_vis_type = vis->type;
        
        // Scale animation speed by playback speed, but cap at 120 FPS equivalent
        double speed_factor = player ? player->playback_speed : 1.0;
        double dt = 0.033 * speed_factor;  // Scale the delta time (33ms = ~30 FPS)
        
        // Cap at ~120 FPS (minimum dt of 0.00833 seconds)
        const double min_dt = 1.0 / 120.0;
        if (dt < min_dt) {
            dt = min_dt;
            speed_factor = dt / 0.033;  // Recalculate speed_factor for rotation/offset
        }
        
        vis->rotation += 0.02 * speed_factor;
        vis->time_offset += 0.1 * speed_factor;
        
        if (vis->rotation > 2.0 * M_PI) vis->rotation -= 2.0 * M_PI;
        
        // Only queue draw if window is visible (skip rendering for minimized windows)
        if (should_render) {
            gtk_widget_queue_draw(vis->drawing_area);
        }
        
        // UPDATE FUNCTIONS - These are called for both interactive and non-interactive visualizations
        // Interactive games run continuously, others only during playback
        switch (vis->type) {
            case VIS_TRIPPY_BARS:
                update_trippy(vis, dt); // 33ms = ~30 FPS
                break;
            case VIS_RADIAL_BARS:
                update_radial_bars_bouncing(vis, dt); // 33ms = ~30 FPS
                break;

            case VIS_FIREWORKS:
                update_fireworks(vis, dt); // 33ms = ~30 FPS
                break;
            case VIS_DNA_HELIX:
                update_dna_helix(vis, dt); // 33ms = ~30 FPS
                break;
            case VIS_DNA2_HELIX:
                update_dna2_helix(vis, dt);
                break;
            case VIS_SUDOKU_SOLVER:  
                update_sudoku_solver(vis, dt);
                break;                
            case VIS_FOURIER_TRANSFORM:
                update_fourier_transform(vis, dt);
                break;
            case VIS_RIPPLES:
                update_ripples(vis, dt);
                break;
            case VIS_KALEIDOSCOPE:
                update_kaleidoscope(vis, dt);
                break;  
            case VIS_BOUNCY_BALLS:
                update_bouncy_balls(vis, dt);
                break;                                              
            case VIS_DIGITAL_CLOCK:
                update_clock_swirls(vis, dt);
                break;
            case VIS_ANALOG_CLOCK:
                update_analog_clock(vis, dt);
                break;
            // ============================================================
            // INTERACTIVE GAMES - These NEVER stop and always maintain 30 FPS
            // ============================================================
            case VIS_ROBOT_CHASER:
                // Game continues regardless of music playback
                update_robot_chaser_visualization(vis, dt);
                break;
            case VIS_RADIAL_WAVE:
                update_radial_wave(vis, dt);
                break;
            case VIS_BLOCK_STACK:
                // Game continues regardless of music playback
                update_blockstack(vis, dt);
                break;
            case VIS_PARROT:
                update_parrot(vis, dt);
                break;
            case VIS_COMET_BUSTER:
                update_comet_buster(vis, dt);
                break;
            case VIS_EYE_OF_SAURON:
                update_eye_of_sauron(vis, dt);
                break;
            case VIS_TOWER_OF_HANOI:
                // Game continues regardless of music playback
                update_hanoi(vis, dt);
                break;
            case VIS_BEAT_CHESS:
                // Game continues regardless of music playback
                update_beat_chess(vis, dt);
                break;
            case VIS_BEAT_CHECKERS:
                // Game continues regardless of music playback
                update_beat_checkers(vis, dt);
                break;                                                
            case VIS_DRAW_WORMHOLE:
                update_stargate(vis, dt);
                break;                                                
            case VIS_RABBITHARE:
                update_rabbithare(vis, dt);
                break;                                                
            case VIS_MAZE_3D:
                // Game continues regardless of music playback
                update_maze3d(vis, dt);
                break;
            case VIS_BOUNCING_CIRCLE:
                update_bouncing_circle(vis, dt);
                break;
            case VIS_MANDELBROT:
                update_mandelbrot(vis, dt);
                break;
            case VIS_PONG:
                // Game continues regardless of music playback
                pong_update(vis, dt);
                break;                                                                               
            case VIS_MINESWEEPER:
                minesweeper_update(vis, dt);
                break;

            case VIS_KARAOKE:
            case VIS_KARAOKE_EXCITING:
                if (vis->cdg_display) {
                    //printf("playtime %.3f\n", playTime);
                    cdg_update(vis->cdg_display, playTime);
                }
                break;                                
            default:
                // No update function needed for other visualization types
                break;
        }
        
        update_track_info_overlay(vis, dt);
    }
    
    return TRUE; // Continue timer - NEVER stop the timer for interactive games
}

void on_visualizer_realize(GtkWidget *widget, gpointer user_data) {
    // The size is already set by on_window_realize(), so we don't need to do anything here
    // Just ensure the widget is properly realized
    printf("Visualizer realized\n");
}


// Callback functions for controls
void on_vis_type_changed(GtkComboBox *combo, gpointer user_data) {
    Visualizer *vis = (Visualizer*)user_data;
    int active = gtk_combo_box_get_active(combo);
    visualizer_set_type(vis, (VisualizationType)active);
}

void on_vis_enabled_toggled(GtkToggleButton *button, gpointer user_data) {
    Visualizer *vis = (Visualizer*)user_data;
    gboolean enabled = gtk_toggle_button_get_active(button);
    visualizer_set_enabled(vis, enabled);
}

void on_sensitivity_changed(GtkRange *range, gpointer user_data) {
    Visualizer *vis = (Visualizer*)user_data;
    vis->sensitivity = gtk_range_get_value(range);
}

GtkWidget* create_visualization_controls(Visualizer *vis) {
    // Get screen info to decide control layout
    GdkScreen *screen = gdk_screen_get_default();
    int screen_width = gdk_screen_get_width(screen);
    int scale = gtk_widget_get_scale_factor(player->window);
    printf("%i %i\n", screen_width, scale);
    if (scale>1) {
        screen_width/=scale;
    }

    bool use_compact_controls = (screen_width <= 1024);
    
    GtkWidget *controls_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, use_compact_controls ? 3 : 5);
    
    // Enable/disable checkbox
    GtkWidget *enable_check = gtk_check_button_new_with_label("Enable");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_check), vis->enabled);
    gtk_widget_set_tooltip_text(enable_check, "Enable/disable visualization");
    g_signal_connect(enable_check, "toggled", G_CALLBACK(on_vis_enabled_toggled), vis);
    gtk_box_pack_start(GTK_BOX(controls_box), enable_check, FALSE, FALSE, 0);
    
    // Type selection - more compact for small screens
    if (!use_compact_controls) {
        GtkWidget *type_label = gtk_label_new("Type:");
        gtk_box_pack_start(GTK_BOX(controls_box), type_label, FALSE, FALSE, 0);
    }
    
    GtkWidget *type_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "3d Maze");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Analog Clock");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Bars");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Beat Checkers (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Beat Chess (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Birthday Cake");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Block Stack");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Bouncing Circle");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Bouncy Balls (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Bubbles (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Circle");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Comet Buster (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "DNA Helix");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "DNA Helix Alternative");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Dancing Parrot");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Digital Clock");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Fireworks (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Fourier Transform");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Fractal Bloom (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Hare/Turtle Race (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Kaleidoscope (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Mandelbrot Fractal");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Matrix Rain (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Minesweeper (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Oscilloscope (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Pong (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Radial Bars");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Radial Wave");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Ripples (i)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Robot Chaser");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Sudoku");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Symmetry Cascade");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "The All Seeing Eye");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Tower of Hanoi");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Trippy Bars");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Volume Meter");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Waveform");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Wormhole Simulation");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Karaoke Classic");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type_combo), "Karaoke Starburst");

    gtk_combo_box_set_active(GTK_COMBO_BOX(type_combo), vis->type);
    gtk_widget_set_tooltip_text(type_combo, "Select visualization type (Q: Next | A: Previous); (i) means interactive");
    gtk_widget_set_name(type_combo, "vis_type_combo");
    g_signal_connect(type_combo, "changed", G_CALLBACK(on_vis_type_changed), vis);
    
    if (use_compact_controls) {
        // Smaller combo box for compact layout
        gtk_widget_set_size_request(type_combo, 120, -1);
    }
    
    gtk_box_pack_start(GTK_BOX(controls_box), type_combo, FALSE, FALSE, 0);
    
    // Sensitivity slider - more compact for small screens
    if (!use_compact_controls) {
        GtkWidget *sens_label = gtk_label_new("Sensitivity:");
        gtk_box_pack_start(GTK_BOX(controls_box), sens_label, FALSE, FALSE, 0);
    }
    
    GtkWidget *sens_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.1, 5.0, 0.1);
    gtk_range_set_value(GTK_RANGE(sens_scale), vis->sensitivity);
    gtk_widget_set_tooltip_text(sens_scale, "Adjust visualization sensitivity to audio");
    
    if (use_compact_controls) {
        // Smaller sensitivity slider for compact layout
        gtk_widget_set_size_request(sens_scale, 80, -1);
        gtk_scale_set_draw_value(GTK_SCALE(sens_scale), FALSE); // Hide value display to save space
    } else {
        gtk_widget_set_size_request(sens_scale, 100, -1);
    }
    
    g_signal_connect(sens_scale, "value-changed", G_CALLBACK(on_sensitivity_changed), vis);
    gtk_box_pack_start(GTK_BOX(controls_box), sens_scale, FALSE, FALSE, 0);
    
    return controls_box;
}
void visualizer_next_mode(Visualizer *vis) {
    if (!vis) return;
    
    int current = (int)vis->type;
    int next = (current + 1) % (VIS_KARAOKE_EXCITING+1);  // Wrap around to first mode
    
    // Skip to first mode if we go past the last
    if (next > VIS_KARAOKE_EXCITING) {
        next = VIS_MAZE_3D;
    }
    
    visualizer_set_type(vis, (VisualizationType)next);
}

void visualizer_prev_mode(Visualizer *vis) {
    if (!vis) return;
    
    int current = (int)vis->type;
    int prev = current - 1;
    
    // Wrap to last mode if we go below first
    if (prev < VIS_MAZE_3D) {
        prev = VIS_KARAOKE_EXCITING;
    }
    
    visualizer_set_type(vis, (VisualizationType)prev);
}

bool save_last_visualization(VisualizationType vis_type) {
    char config_path[1024];
    
#ifdef _WIN32
    char app_data[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, app_data) != S_OK) {
        return false;
    }
    snprintf(config_path, sizeof(config_path), "%s\\Zenamp\\last_visualization.txt", app_data);
#else
    const char *home = getenv("HOME");
    if (!home) {
        return false;
    }
    snprintf(config_path, sizeof(config_path), "%s/.zenamp/last_visualization.txt", home);
#endif
    
    FILE *f = fopen(config_path, "w");
    if (!f) {
        printf("Failed to save last visualization type\n");
        return false;
    }
    
    fprintf(f, "%d\n", (int)vis_type);
    fclose(f);
    printf("Saved last visualization: %d\n", vis_type);
    return true;
}

bool load_last_visualization(VisualizationType *vis_type) {
    char config_path[1024];
    
#ifdef _WIN32
    char app_data[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, app_data) != S_OK) {
        return false;
    }
    snprintf(config_path, sizeof(config_path), "%s\\Zenamp\\last_visualization.txt", app_data);
#else
    const char *home = getenv("HOME");
    if (!home) {
        return false;
    }
    snprintf(config_path, sizeof(config_path), "%s/.zenamp/last_visualization.txt", home);
#endif
    
    FILE *f = fopen(config_path, "r");
    if (!f) {
        printf("No last visualization file found\n");
        return false;
    }
    
    int type_int;
    if (fscanf(f, "%d", &type_int) != 1) {
        fclose(f);
        return false;
    }
    fclose(f);
    
    *vis_type = (VisualizationType)type_int;
    printf("Loaded last visualization: %d\n", type_int);
    return true;
}

void draw_track_info_overlay(Visualizer *vis, cairo_t *cr) {
    if (vis->track_info_fade_alpha <= 0.0) {
        return;  // Don't draw if fully transparent
    }
    
    int width = vis->width;
    int height = vis->height;
    double alpha = vis->track_info_fade_alpha;
    
    // Background and border
    cairo_set_source_rgba(cr, 0, 0, 0, 0.6 * alpha);
    cairo_rectangle(cr, 0, height * 0.35, width, height * 0.30);
    cairo_fill(cr);
    
    cairo_set_source_rgba(cr, 0.3, 0.7, 1.0, alpha);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, 0, height * 0.35, width, height * 0.30);
    cairo_stroke(cr);
    
    // Setup
    double x_center = width / 2.0;
    double y_start = height * 0.42;  // Starting point
    double y_current = y_start;
    
    cairo_text_extents_t extents;
    
    // ===== "Now Playing" Label =====
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgba(cr, 0.5, 0.8, 1.0, alpha);
    
    cairo_text_extents(cr, "* Now Playing *", &extents);
    // Center text horizontally
    cairo_move_to(cr, x_center - extents.width / 2.0, y_current - extents.y_bearing);
    cairo_show_text(cr, "* Now Playing *");
    
    // Move down by actual text height + margin
    y_current += -extents.y_bearing + extents.height + 8.0;  // 8px margin
    
    // ===== Song Title =====
    cairo_set_font_size(cr, 18.0);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha);
    
    cairo_text_extents(cr, vis->track_info_title, &extents);
    cairo_move_to(cr, x_center - extents.width / 2.0, y_current - extents.y_bearing);
    cairo_show_text(cr, vis->track_info_title);
    
    // Move down by actual text height + margin
    y_current += -extents.y_bearing + extents.height + 12.0;  // 12px margin
    
    // ===== Artist and Album =====
    cairo_set_font_size(cr, 13.0);
    cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, alpha);
    
    char artist_album[512];
    snprintf(artist_album, sizeof(artist_album), "%s%s%s",
             vis->track_info_artist,
             (vis->track_info_artist[0] && vis->track_info_album[0]) ? " - " : "",
             vis->track_info_album);
    
    cairo_text_extents(cr, artist_album, &extents);
    cairo_move_to(cr, x_center - extents.width / 2.0, y_current - extents.y_bearing);
    cairo_show_text(cr, artist_album);
    
    // Move down by actual text height + margin
    y_current += -extents.y_bearing + extents.height + 8.0;  // 8px margin
    
    // ===== Duration =====
    cairo_set_font_size(cr, 12.0);
    cairo_set_source_rgba(cr, 0.6, 0.6, 0.6, alpha);
    
    char duration_str[32];
    snprintf(duration_str, sizeof(duration_str), "(%d:%02d)",
             vis->track_info_duration / 60,
             vis->track_info_duration % 60);
    
    cairo_text_extents(cr, duration_str, &extents);
    cairo_move_to(cr, x_center - extents.width / 2.0, y_current - extents.y_bearing);
    cairo_show_text(cr, duration_str);
}

void update_track_info_overlay(Visualizer *vis, double dt) {
    if (vis->track_info_display_time > 0) {
        vis->track_info_display_time -= dt;
        
        // Calculate fade (starts at 1.0, fades to 0.0)
        double fade_duration = 1.0;  // 1 second to fade out
        if (vis->track_info_display_time < fade_duration) {
            // In fade-out phase
            vis->track_info_fade_alpha = vis->track_info_display_time / fade_duration;
        } else {
            // Fully visible
            vis->track_info_fade_alpha = 1.0;
        }
    } else {
        vis->track_info_fade_alpha = 0.0;
    }
}

void show_track_info_overlay(Visualizer *vis, const char *title, 
                             const char *artist, const char *album, 
                             int duration_seconds) {
    if (!vis) return;
    
    strncpy(vis->track_info_title, title ? title : "", 
            sizeof(vis->track_info_title) - 1);
    strncpy(vis->track_info_artist, artist ? artist : "", 
            sizeof(vis->track_info_artist) - 1);
    strncpy(vis->track_info_album, album ? album : "", 
            sizeof(vis->track_info_album) - 1);
    
    vis->track_info_duration = duration_seconds;
    vis->track_info_display_time = 3.0;  // Show for 3 seconds
    vis->track_info_fade_alpha = 1.0;    // Start fully visible
    
    printf("Track info overlay triggered: %s\n", title);
}
