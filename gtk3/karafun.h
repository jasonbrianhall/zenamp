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
    char tmp_mixed_path[4096];     // vocal+backing pre-mixed to a single WAV for playback
    bool has_backing_track;
    
    bool vocal_muted;
    bool backing_muted;
    
    int backing_channel;  // SDL_mixer channel for backing track (-1 = not playing)
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
void karafun_set_backing_channel(int channel);
void karafun_stop_backing(void);

/**
 * Converts the extracted vocal + backing tracks to WAV and mixes them into
 * a single temp WAV file. Called automatically by karafun_load(). Result is
 * available via karafun_get_mixed_path().
 */
bool karafun_prepare_mixed_track(void);
const char* karafun_get_mixed_path(void);

/**
 * Render Karafun lyrics display (for karaoke visualization)
 */
void draw_karafun_lyrics(void *vis, void *cr);

/**
 * When true, draw_karafun_lyrics() skips painting its own opaque
 * background, so a caller (e.g. the "exciting"/starburst visualization)
 * can composite the lyrics on top of something it already drew.
 */
void karafun_set_skip_background(bool skip);

#endif
