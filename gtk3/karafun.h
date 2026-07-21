#ifndef KARAFUN_H
#define KARAFUN_H

#include <stdbool.h>
#include <cairo.h>

typedef struct {
    int start_word_idx;
    int word_count;
    char display_text[2048];
} KarafunLyricLine;

typedef struct {
    bool active;
    char title[512];
    char artist[512];
    
    char **words;
    int word_count;
    int *sync_times_ms;           // MILLISECONDS, not seconds
    int sync_count;
    KarafunLyricLine *lines;
    int line_count;
    
    int current_word_idx;
    
    char tmp_vocal_path[4096];
    char tmp_backing_path[4096];
    bool has_backing_track;
    
    bool vocal_muted;
    bool backing_muted;
} KarafunState;

bool karafun_load(const char *kfn_path);
void karafun_stop(void);
void karafun_update(double playback_position_seconds);
int karafun_current_word(void);
int karafun_current_line(void);
KarafunState* karafun_get_state(void);
bool is_karafun_ext(const char *filename);
const char* karafun_get_vocal_path(void);
const char* karafun_get_backing_path(void);

/**
 * Play both vocal and backing tracks via Zenamp audio API
 * Returns true if successfully playing
 */
bool karafun_play_both_tracks(void *audio_player);

/**
 * Pause/resume backing track to match Zenamp's state
 */
void karafun_set_paused(bool paused);

/**
 * Seek backing track to match Zenamp's position (milliseconds)
 */
void karafun_seek(int position_ms);

/**
 * Render Karafun lyrics display (for karaoke visualization)
 */
void draw_karafun_lyrics(void *vis, void *cr);

#endif
