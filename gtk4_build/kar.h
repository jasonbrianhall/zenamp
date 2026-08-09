#ifndef KAR_H
#define KAR_H

#include <stdbool.h>
#include "karafun.h"

/**
 * Load a KAR (MIDI Karaoke) file and extract lyrics into KarafunState.
 * 
 * Populates the global g_karafun structure (shared with karafun.cpp) with:
 *   - words: lyric syllables extracted from MIDI text meta-events
 *   - word_count: total number of syllables
 *   - sync_times_ms: timing for each syllable (milliseconds)
 *   - lines: display lines grouping syllables
 *   - tmp_mixed_path: path to temporary WAV file for playback
 *   - title, artist: metadata from @T and @AR directives
 * 
 * Returns true on success, false on failure.
 */
bool kar_load(const char *kar_path);

/**
 * Stop KAR playback and clean up all allocated resources.
 * Safe to call even if no KAR file is loaded.
 */
void kar_stop(void);

/**
 * Update the currently-active word index based on playback position.
 * Call this from the main playback loop at each audio frame.
 * 
 * Parameters:
 *   playback_position_seconds: current time in the song (0.0 to duration)
 */
void kar_update(double playback_position_seconds);

/**
 * Check if a filename has the .kar extension.
 */
bool is_kar_ext(const char *filename);

/**
 * Get the current KarafunState populated by kar_load().
 * Returns pointer to the global g_karafun state.
 * Shared with karafun.cpp - do not free or modify directly.
 */
KarafunState* kar_get_state(void);

#endif // KAR_H
