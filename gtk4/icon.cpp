#include "icon.h"
#include <glib.h>
#include <SDL2/SDL.h>

// Global animation state
IconAnimationState *g_icon_animation = NULL;

// Timeout constant (in milliseconds)
#define ICON_ANIMATION_TIMEOUT 50  // ~20 FPS, adjust as needed
#define ICON_RESET_DELAY 1000      // 1 second before returning to first frame

GdkPixbuf* load_icon_from_base64(void) {
    GError *error = NULL;
    GdkPixbuf *pixbuf = NULL;
    
    // Decode base64 data
    gsize decoded_len;
    guchar *decoded_data = g_base64_decode(icon_base64_data, &decoded_len);
    
    if (!decoded_data) {
        g_warning("Failed to decode base64 icon data");
        return NULL;
    }
    
    // Create GdkPixbuf from decoded PNG data
    GInputStream *stream = g_memory_input_stream_new_from_data(decoded_data, decoded_len, g_free);
    
    if (stream) {
        pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, &error);
        g_object_unref(stream);
        
        if (error) {
            g_warning("Failed to create pixbuf from icon data: %s", error->message);
            g_error_free(error);
            return NULL;
        }
    } else {
        g_free(decoded_data);
        g_warning("Failed to create memory stream for icon data");
        return NULL;
    }
    
    return pixbuf;
}

// Load animation from base64
static GdkPixbufAnimation* load_animation_from_base64(void) {
    GError *error = NULL;
    GdkPixbufAnimation *animation = NULL;
    
    // Decode base64 data
    gsize decoded_len;
    guchar *decoded_data = g_base64_decode(icon_base64_data, &decoded_len);
    
    if (!decoded_data) {
        g_warning("Failed to decode base64 animation data");
        return NULL;
    }
    
    // Create animation from decoded data
    GInputStream *stream = g_memory_input_stream_new_from_data(decoded_data, decoded_len, g_free);
    
    if (stream) {
        animation = gdk_pixbuf_animation_new_from_stream(stream, NULL, &error);
        g_object_unref(stream);
        
        if (error) {
            g_warning("Failed to create animation from icon data: %s", error->message);
            g_error_free(error);
            return NULL;
        }
    } else {
        g_free(decoded_data);
        g_warning("Failed to create memory stream for animation data");
        return NULL;
    }
    
    return animation;
}

// Frame update callback - plays animation ONCE per click
gboolean on_icon_animation_frame(gpointer user_data) {
    IconAnimationState *state = (IconAnimationState *)user_data;
    
    if (!state || !state->is_playing || !state->image_widget) {
        return FALSE;
    }
    
    // Get current pixbuf frame
    GdkPixbuf *frame = gdk_pixbuf_animation_iter_get_pixbuf(state->iter);
    
    if (frame) {
        gtk_image_set_from_pixbuf(state->image_widget, frame);
        gtk_widget_queue_draw(GTK_WIDGET(state->image_widget));
    }
    
    // Get delay for current frame
    gint delay_ms = gdk_pixbuf_animation_iter_get_delay_time(state->iter);
    if (delay_ms < 0) {
        delay_ms = ICON_ANIMATION_TIMEOUT;
    }
    
    GTimeVal current_time;
    g_get_current_time(&current_time);
    
    // Advance by adding the delay to current time
    current_time.tv_usec += delay_ms * 1000;
    if (current_time.tv_usec >= 1000000) {
        current_time.tv_sec += current_time.tv_usec / 1000000;
        current_time.tv_usec %= 1000000;
    }
    
    // Before advancing, check position
    gint frame_number = 0;
    
    // Advance the iterator
    gdk_pixbuf_animation_iter_advance(state->iter, &current_time);
    
    // Check if we're back at frame 0 (animation looped)
    // If the iterator on_currently_loading_frame changes state, we've looped
    static gint last_frame_check = 0;
    gint current_frame_check = gdk_pixbuf_animation_iter_get_delay_time(state->iter);
    
    // Alternative: track frame count manually
    state->frame_count++;
    
    // Assume animation is short (< 100 frames), so after a reasonable number
    // of frames, we've gone through the whole animation once
    // For most icons: ~10-20 frames max
    if (state->frame_count > 100) {  // Safety: stops after 100 frames (5 seconds at 20 FPS)
        g_debug("Animation completed (frame count: %d)", state->frame_count);
        state->is_playing = FALSE;
        state->frame_count = 0;
        
        // Return to first frame
        if (state->first_frame) {
            gtk_image_set_from_pixbuf(state->image_widget, state->first_frame);
            gtk_widget_queue_draw(GTK_WIDGET(state->image_widget));
        }
        
        return FALSE;  // Stop animation
    }
    
    return TRUE;  // Continue animation
}

// Callback to reset animation to first frame
static gboolean on_icon_reset_to_first_frame(gpointer user_data) {
    IconAnimationState *state = (IconAnimationState *)user_data;
    
    if (state && state->first_frame && state->image_widget) {
        gtk_image_set_from_pixbuf(state->image_widget, state->first_frame);
        gtk_widget_queue_draw(GTK_WIDGET(state->image_widget));
        state->is_playing = FALSE;
        
        // Remove animation timeout if still running
        if (state->timeout_id != 0) {
            g_source_remove(state->timeout_id);
            state->timeout_id = 0;
        }
    }
    
    return FALSE;  // Don't call again
}

// Initialize animation state
IconAnimationState* init_icon_animation(GtkImage *image_widget) {
    if (!image_widget) {
        g_warning("Invalid image widget for animation");
        return NULL;
    }
    
    IconAnimationState *state = g_malloc0(sizeof(IconAnimationState));
    
    // Load animation
    state->animation = load_animation_from_base64();
    if (!state->animation) {
        g_warning("Failed to load animation from base64");
        g_free(state);
        return NULL;
    }
    
    // Create animation iterator with current time
    GTimeVal current_time;
    g_get_current_time(&current_time);
    state->iter = gdk_pixbuf_animation_get_iter(state->animation, &current_time);
    
    if (!state->iter) {
        g_warning("Failed to create animation iterator");
        g_object_unref(state->animation);
        g_free(state);
        return NULL;
    }
    
    // Get first frame and store it
    state->first_frame = gdk_pixbuf_animation_iter_get_pixbuf(state->iter);
    if (state->first_frame) {
        g_object_ref(state->first_frame);
    }
    
    state->image_widget = image_widget;
    state->is_playing = FALSE;
    state->timeout_id = 0;
    state->frame_count = 0;  // Track frames for play-once logic
    
    // Check if this is actually an animated image
    gboolean is_static = gdk_pixbuf_animation_is_static_image(state->animation);
    
    if (is_static) {
        g_warning("Warning: loaded image is static, not animated");
    } else {
        g_debug("Icon animation initialized successfully");
    }
    
    // Display first frame immediately
    if (state->first_frame) {
        gtk_image_set_from_pixbuf(image_widget, state->first_frame);
    }
    
    return state;
}

// Start animation
void start_icon_animation(void) {
    if (!g_icon_animation || g_icon_animation->is_playing) {
        g_debug("Animation already playing or not initialized");
        return;
    }
    
    g_icon_animation->is_playing = TRUE;
    g_icon_animation->frame_count = 0;  // Reset frame counter for new play
    
    // Reset iterator to beginning with current time
    GTimeVal current_time;
    g_get_current_time(&current_time);
    
    if (g_icon_animation->iter) {
        g_object_unref(g_icon_animation->iter);
    }
    g_icon_animation->iter = gdk_pixbuf_animation_get_iter(g_icon_animation->animation, &current_time);
    
    if (!g_icon_animation->iter) {
        g_warning("Failed to recreate animation iterator");
        g_icon_animation->is_playing = FALSE;
        return;
    }
    
    // Start animation frame update
    g_icon_animation->timeout_id = g_timeout_add(
        ICON_ANIMATION_TIMEOUT,
        on_icon_animation_frame,
        g_icon_animation
    );
    
    SDL_Log("Animation started! (plays once)");
}

// Stop animation after delay, return to first frame
void stop_icon_animation(void) {
    if (!g_icon_animation) {
        return;
    }
    
    if (g_icon_animation->timeout_id != 0) {
        g_source_remove(g_icon_animation->timeout_id);
        g_icon_animation->timeout_id = 0;
    }
    
    // Schedule return to first frame after ICON_RESET_DELAY
    g_timeout_add(ICON_RESET_DELAY, on_icon_reset_to_first_frame, g_icon_animation);
    
    g_debug("Icon animation stop scheduled");
}

// Cleanup animation
void cleanup_icon_animation(void) {
    if (!g_icon_animation) {
        return;
    }
    
    if (g_icon_animation->timeout_id != 0) {
        g_source_remove(g_icon_animation->timeout_id);
        g_icon_animation->timeout_id = 0;
    }
    
    g_icon_animation->is_playing = FALSE;
}

// Destroy animation state
void destroy_icon_animation(IconAnimationState *state) {
    if (!state) {
        return;
    }
    
    if (state->timeout_id != 0) {
        g_source_remove(state->timeout_id);
    }
    
    if (state->iter) {
        g_object_unref(state->iter);
    }
    
    if (state->animation) {
        g_object_unref(state->animation);
    }
    
    if (state->first_frame) {
        g_object_unref(state->first_frame);
    }
    
    g_free(state);
}

// Handle icon click event. "button-press-event" -> GtkGestureClick's
// "pressed" signal; the button number comes from the gesture, not an event.
void on_icon_button_press(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer user_data) {
    (void)n_press;
    (void)x;
    (void)y;
    (void)user_data;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    if (button != 1) {  // Only respond to left-click
        return;
    }
    
    if (!g_icon_animation) {
        g_warning("Icon animation not initialized");
        return;
    }
    
    // If animation is already playing, don't restart
    if (g_icon_animation->is_playing) {
        return;
    }
    
    SDL_Log("Icon clicked - starting animation!");
    start_icon_animation();
}

// Set static window icon from first frame.
//
// NOTE(gtk4): gtk_window_set_icon() (and set_icon_list/set_default_icon) are
// removed entirely in GTK4 - there is no API left for setting a window's
// icon from an arbitrary in-memory GdkPixbuf/texture at runtime. GTK4
// expects window/taskbar icons to come from the desktop icon theme via
// gtk_window_set_icon_name(), which needs a *named* icon already installed
// in the theme (typically via the app's .desktop file and an icon in
// /usr/share/icons/.../<app-id>.png at install time) - not something this
// function's base64-embedded PNG can provide directly.
//
// This falls back to a generic themed icon name so the window still has
// *something* sensible in the titlebar/taskbar. To get the actual custom
// icon back, package it as a proper icon-theme entry at install time and
// reference that name here (or via the app's desktop file / GApplication
// icon-name, which most desktop shells prefer anyway).
void set_window_icon_from_base64(GtkWindow *window) {
    gtk_window_set_icon_name(window, "multimedia-audio-player");
}
