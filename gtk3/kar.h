#ifndef KAR_H
#define KAR_H

#include <stdbool.h>

/**
 * Load a KAR (MIDI Karaoke) file and extract lyrics.
 * KAR files are standard MIDI files with embedded lyrics in text meta-events.
 * Populates the global g_karafun state with lyrics and sync times.
 * 
 * Returns true on success, false on failure.
 */
bool kar_load(const char *kar_path);

/**
 * Stop playback and clean up KAR lyrics.
 * Call this when switching away from KAR playback.
 */
void kar_stop(void);

/**
 * Update the currently-active word index based on playback position.
 * Call this from the main playback loop at each audio frame.
 * playback_position_seconds: current time in the song (0.0 to duration)
 */
void kar_update(double playback_position_seconds);

/**
 * Check if a filename has the .kar extension.
 */
bool is_kar_ext(const char *filename);

#endif // KAR_H
