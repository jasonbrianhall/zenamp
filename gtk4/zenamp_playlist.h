// Zenamp (c) Jason Hall 2026 - Queue, Playlist, Settings & Metadata Cache
//
// Split out of zenamp_main.cpp to keep the main file focused on GTK
// window/UI setup and playback control. This covers:
//   - PlayQueue management (init/add/advance/remove/etc.)
//   - M3U playlist load/save (menu handlers + helpers)
//   - Last-playlist / playback-state persistence
//   - Player settings persistence (volume, EQ, visualizer, queue view)
//   - Metadata cache path + save-on-exit helper

#ifndef ZENAMP_PLAYLIST_H
#define ZENAMP_PLAYLIST_H

#include <gtk/gtk.h>
#include <string>
#include <cstddef>

#include "audio_player.h"

// ---------------------------------------------------------------------
// Queue management
// ---------------------------------------------------------------------
void init_queue(PlayQueue *queue);
void clear_queue(PlayQueue *queue);
bool add_to_queue(PlayQueue *queue, const char *filename);
const char* get_current_queue_file(PlayQueue *queue);
bool advance_queue(PlayQueue *queue);
bool previous_queue(PlayQueue *queue);
bool remove_from_queue(PlayQueue *queue, int index);

// ---------------------------------------------------------------------
// Playlist (M3U) load/save
// ---------------------------------------------------------------------
bool isValidM3U(const std::string& path);
void on_menu_load_playlist(GtkWidget *menuitem, gpointer user_data);
void on_menu_save_playlist(GtkWidget *menuitem, gpointer user_data);
bool save_current_queue_on_exit(AudioPlayer *player);

bool get_last_playlist_path(char *path, size_t path_size);
bool save_last_playlist_path(const char *playlist_path);
bool load_last_playlist_path(char *playlist_path, size_t path_size);
bool load_playlist_state(int *current_index, double *position);

// ---------------------------------------------------------------------
// Player settings persistence
// ---------------------------------------------------------------------
bool get_settings_path(char *path, size_t path_size);
bool save_player_settings(AudioPlayer *player);
bool load_player_settings(AudioPlayer *player);

// ---------------------------------------------------------------------
// Metadata cache
// ---------------------------------------------------------------------
bool get_metadata_cache_path(char *path, size_t path_size);
void save_metadata_cache_on_exit();

#endif // ZENAMP_PLAYLIST_H
