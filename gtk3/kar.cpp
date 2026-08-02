// KAR (MIDI Karaoke) file support - fully integrated with Zenamp
// KAR files are standard MIDI files with embedded lyrics in text meta-events
// This is integrated into the main load_file() flow in zenamp_main.cpp

#include "karafun.h"
#include "audioconverter.h"
#include "kar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <stdint.h>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

// g_karafun is declared in karafun.cpp
extern KarafunState g_karafun;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static void delete_temp_file(const char *path) {
    if (!path || !path[0]) return;
#ifndef _WIN32
    unlink(path);
#else
    DeleteFileA(path);
#endif
}

static void cleanup_karafun_state(void) {
    // Free existing words
    if (g_karafun.words) {
        for (int i = 0; i < g_karafun.word_count; i++) {
            if (g_karafun.words[i]) free(g_karafun.words[i]);
        }
        free(g_karafun.words);
        g_karafun.words = NULL;
    }
    g_karafun.word_count = 0;
    
    // Free sync times
    if (g_karafun.sync_times_ms) {
        free(g_karafun.sync_times_ms);
        g_karafun.sync_times_ms = NULL;
    }
    g_karafun.sync_count = 0;
    
    // Free lines
    if (g_karafun.lines) {
        free(g_karafun.lines);
        g_karafun.lines = NULL;
    }
    g_karafun.line_count = 0;
}

// ============================================================================
// MIDI PARSING HELPERS
// ============================================================================

// Read a variable-length quantity from MIDI data
static uint32_t read_variable_length(const uint8_t *data, int data_len, int *pos) {
    uint32_t value = 0;
    int i = 0;
    while (*pos < data_len && i < 4) {
        uint8_t byte = data[*pos];
        (*pos)++;
        value = (value << 7) | (byte & 0x7F);
        if ((byte & 0x80) == 0) break;
        i++;
    }
    return value;
}

// Extract timing information from delta time and tempo
static int extract_note_time_ms(int delta_ticks, int ticks_per_quarter_note, double tempo_us_per_quarter) {
    double us = (delta_ticks * tempo_us_per_quarter) / ticks_per_quarter_note;
    return (int)(us / 1000.0 + 0.5);
}

// Add a word to the g_karafun words array
static bool add_word(const char *word, int sync_time_ms) {
    if (!word || !word[0] || g_karafun.word_count >= 10000) {
        return false;
    }
    
    char **new_words = (char**)realloc(g_karafun.words,
        sizeof(char*) * (g_karafun.word_count + 1));
    int *new_times = (int*)realloc(g_karafun.sync_times_ms,
        sizeof(int) * (g_karafun.sync_count + 1));
    
    if (!new_words || !new_times) {
        free(new_words);
        free(new_times);
        return false;
    }
    
    g_karafun.words = new_words;
    g_karafun.sync_times_ms = new_times;
    
    char *word_dup = (char*)malloc(strlen(word) + 1);
    if (!word_dup) {
        return false;
    }
    
    strcpy(word_dup, word);
    g_karafun.words[g_karafun.word_count++] = word_dup;
    g_karafun.sync_times_ms[g_karafun.sync_count++] = sync_time_ms;
    
    if (g_karafun.word_count <= 10) {
        printf("KAR: Word %d @ %dms: '%s'\n", g_karafun.word_count - 1, sync_time_ms, word_dup);
    }
    
    return true;
}

// Remove '/' and '\' from a word
static void sanitize_word(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dst_size - 1; i++) {
        if (src[i] == '/' || src[i] == '\\')
            continue; // skip these characters
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

// Build lyric lines from the word array
static void build_lyric_lines(void) {
    if (g_karafun.word_count == 0) {
        g_karafun.line_count = 0;
        return;
    }

    g_karafun.lines = (KarafunLyricLine*)malloc(
        sizeof(KarafunLyricLine) * (g_karafun.word_count + 1));

    if (!g_karafun.lines) {
        printf("KAR: Failed to allocate lyric lines\n");
        return;
    }

    int line_idx = 0;
    int word_idx = 0;

    while (word_idx < g_karafun.word_count && line_idx <= g_karafun.word_count) {
        int line_start = word_idx;
        char display_text[2048] = {0};
        int words_in_line = 0;

        const int WORDS_PER_LINE = 10;
        const int MIN_TIME_GAP_MS = 500;

        while (word_idx < g_karafun.word_count && words_in_line < WORDS_PER_LINE) {
            if (words_in_line > 0 && word_idx > 0) {
                int time_gap = g_karafun.sync_times_ms[word_idx] -
                               g_karafun.sync_times_ms[word_idx - 1];
                if (time_gap > MIN_TIME_GAP_MS) {
                    break;
                }
            }

            // Sanitize the word before adding it
            char clean_word[512];
            sanitize_word(g_karafun.words[word_idx], clean_word, sizeof(clean_word));

            if (words_in_line > 0) {
                strcat(display_text, " ");
            }

            size_t remaining = sizeof(display_text) - strlen(display_text) - 1;
            strncat(display_text, clean_word, remaining);

            word_idx++;
            words_in_line++;
        }

        g_karafun.lines[line_idx].start_word_idx = line_start;
        g_karafun.lines[line_idx].word_count = words_in_line;

        strncpy(g_karafun.lines[line_idx].display_text,
                display_text,
                sizeof(g_karafun.lines[line_idx].display_text) - 1);

        printf("KAR: Line %d (words %d-%d): '%s'\n",
               line_idx, line_start, word_idx - 1, display_text);

        line_idx++;
    }

    g_karafun.line_count = line_idx;
}


// ============================================================================
// KAR PARSING - Main Entry Point
// ============================================================================

bool kar_load(const char *kar_path) {
    if (!kar_path) return false;
    
    printf("KAR: Loading %s\n", kar_path);
    
    // Clean up any existing state
    cleanup_karafun_state();
    memset(&g_karafun, 0, sizeof(g_karafun));
    
    FILE *f = fopen(kar_path, "rb");
    if (!f) {
        printf("KAR: Failed to open file\n");
        return false;
    }
    
    // Read entire file
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *midi_data = (uint8_t*)malloc(file_size);
    if (!midi_data) {
        fclose(f);
        return false;
    }
    
    if (fread(midi_data, 1, file_size, f) != (size_t)file_size) {
        printf("KAR: Failed to read file\n");
        free(midi_data);
        fclose(f);
        return false;
    }
    fclose(f);
    
    // Convert MIDI to WAV for playback
    std::vector<uint8_t> midi_vec(midi_data, midi_data + file_size);
    std::vector<uint8_t> wav_data;
    
    if (!convertMidiToWavInMemory(midi_vec, wav_data)) {
        printf("KAR: Failed to convert MIDI to WAV\n");
        free(midi_data);
        return false;
    }
    
    // Save converted WAV to temp file
    char temp_wav_path[256] = {0};
#ifndef _WIN32
    snprintf(temp_wav_path, sizeof(temp_wav_path), "/tmp/kar_XXXXXX.wav");
    int fd = mkstemps(temp_wav_path, 4);  // 4 for ".wav"
    if (fd == -1) {
        printf("KAR: Failed to create temp WAV file\n");
        free(midi_data);
        return false;
    }
    close(fd);
#else
    char temp_dir[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, temp_dir)) {
        printf("KAR: Failed to get temp directory\n");
        free(midi_data);
        return false;
    }
    
    char base[MAX_PATH];
    if (!GetTempFileNameA(temp_dir, "kar", 0, base)) {
        printf("KAR: Failed to get temp filename\n");
        free(midi_data);
        return false;
    }
    
    snprintf(temp_wav_path, sizeof(temp_wav_path), "%s.wav", base);
    DeleteFileA(base);
#endif
    
    FILE *wav_file = fopen(temp_wav_path, "wb");
    if (!wav_file) {
        printf("KAR: Failed to open temp WAV for writing\n");
        free(midi_data);
        delete_temp_file(temp_wav_path);
        return false;
    }
    
    if (fwrite(wav_data.data(), 1, wav_data.size(), wav_file) != wav_data.size()) {
        printf("KAR: Failed to write WAV data\n");
        fclose(wav_file);
        free(midi_data);
        delete_temp_file(temp_wav_path);
        return false;
    }
    fclose(wav_file);
    
    printf("KAR: Converted to WAV: %s (%zu bytes)\n", temp_wav_path, wav_data.size());
    strncpy(g_karafun.tmp_mixed_path, temp_wav_path, sizeof(g_karafun.tmp_mixed_path) - 1);
    
    // ============================================================================
    // Parse MIDI header
    // ============================================================================
    
    int pos = 0;
    
    // Check "MThd" signature
    if (file_size < 14 || memcmp(&midi_data[pos], "MThd", 4) != 0) {
        printf("KAR: Invalid MIDI header\n");
        free(midi_data);
        return false;
    }
    pos += 4;
    
    // Header length
    uint32_t header_len = (midi_data[pos] << 24) | (midi_data[pos+1] << 16) |
                          (midi_data[pos+2] << 8) | midi_data[pos+3];
    pos += 4;
    
    // Format type
    uint16_t format = (midi_data[pos] << 8) | midi_data[pos+1];
    pos += 2;
    
    // Number of tracks
    uint16_t track_count = (midi_data[pos] << 8) | midi_data[pos+1];
    pos += 2;
    
    // Ticks per quarter note
    int ticks_per_quarter = (midi_data[pos] << 8) | midi_data[pos+1];
    pos += 2;
    
    printf("KAR: Format %d, %d tracks, %d ticks/quarter\n", format, track_count, ticks_per_quarter);
    
    // Default tempo: 120 BPM = 500,000 microseconds per quarter note
    double tempo_us_per_quarter = 500000.0;
    int cumulative_ms = 0;
    
    // ============================================================================
    // Parse tracks looking for lyrics and metadata
    // ============================================================================
    
    for (int track_idx = 0; track_idx < track_count && pos < file_size; track_idx++) {
        // Check "MTrk" signature
        if (pos + 8 > file_size || memcmp(&midi_data[pos], "MTrk", 4) != 0) {
            printf("KAR: Invalid track header at offset %d\n", pos);
            break;
        }
        pos += 4;
        
        uint32_t track_len = (midi_data[pos] << 24) | (midi_data[pos+1] << 16) |
                             (midi_data[pos+2] << 8) | midi_data[pos+3];
        pos += 4;
        
        int track_end = pos + track_len;
        
        // Reset cumulative time at start of each track
        cumulative_ms = 0;
        
        // Parse track events
        while (pos < track_end && pos < file_size) {
            // Read delta time
            int delta_ms = extract_note_time_ms(
                read_variable_length(midi_data, file_size, &pos),
                ticks_per_quarter,
                tempo_us_per_quarter
            );
            cumulative_ms += delta_ms;
            
            if (pos >= file_size) break;
            
            uint8_t status = midi_data[pos];
            pos++;
            
            // Meta event
            if (status == 0xFF) {
                if (pos >= file_size) break;
                
                uint8_t meta_type = midi_data[pos];
                pos++;
                
                uint32_t meta_len = read_variable_length(midi_data, file_size, &pos);
                
                if (pos + meta_len > file_size) break;
                
                // Text Meta event (0x01) - lyrics or title/artist
                if (meta_type == 0x01 && meta_len > 0) {
                    char text[512] = {0};
                    size_t copy_len = (meta_len < sizeof(text) - 1) ? meta_len : sizeof(text) - 1;
                    memcpy(text, &midi_data[pos], copy_len);
                    text[copy_len] = '\0';
                    
                    // Check for @T (title), @AR (artist), @AL (album) directives
                    if (text[0] == '@' && copy_len >= 2) {
                        if (text[1] == 'T' && copy_len > 2) {
                            // Title directive: @T<title>
                            strncpy(g_karafun.title, &text[2], sizeof(g_karafun.title) - 1);
                            printf("KAR: Title: %s\n", g_karafun.title);
                        } else if (text[1] == 'A' && text[2] == 'R' && copy_len > 3) {
                            // Artist directive: @AR<artist>
                            strncpy(g_karafun.artist, &text[3], sizeof(g_karafun.artist) - 1);
                            printf("KAR: Artist: %s\n", g_karafun.artist);
                        }
                        // Skip other @ directives
                    } else if (text[0] && text[0] != '@') {
                        // Regular lyric syllable - add to words array
                        add_word(text, cumulative_ms);
                    }
                }
                // Set Tempo meta event (0x51)
                else if (meta_type == 0x51 && meta_len == 3) {
                    tempo_us_per_quarter = (double)(
                        ((uint32_t)midi_data[pos] << 16) |
                        ((uint32_t)midi_data[pos+1] << 8) |
                        (uint32_t)midi_data[pos+2]
                    );
                    printf("KAR: Tempo changed to %.0f us/quarter (%.1f BPM)\n", 
                           tempo_us_per_quarter, 60000000.0 / tempo_us_per_quarter);
                }
                // End of Track (0x2F)
                else if (meta_type == 0x2F) {
                    // End of track
                }
                
                pos += meta_len;
            }
            // SYSEX event
            else if (status == 0xF0 || status == 0xF7) {
                uint32_t sysex_len = read_variable_length(midi_data, file_size, &pos);
                pos += sysex_len;
            }
            // Regular MIDI event
            else {
                uint8_t event_type = (status & 0xF0);
                
                // Skip event data based on type
                switch (event_type) {
                    case 0x80: // Note Off
                    case 0x90: // Note On
                    case 0xA0: // Polyphonic Pressure
                    case 0xB0: // Control Change
                    case 0xE0: // Pitch Bend
                        pos += 2;
                        break;
                    case 0xC0: // Program Change
                    case 0xD0: // Channel Pressure
                        pos += 1;
                        break;
                    default:
                        break;
                }
            }
        }
        
        // Skip to next track if we didn't read exactly to track_end
        if (pos < track_end) {
            pos = track_end;
        }
    }
    
    free(midi_data);
    
    printf("KAR: Parsed %d words with sync times\n", g_karafun.word_count);
    
    if (g_karafun.word_count == 0) {
        printf("KAR: No lyrics found in file\n");
        return false;
    }
    
    // Build lyric lines from words
    build_lyric_lines();
    
    g_karafun.active = true;
    g_karafun.current_word_idx = 0;
    g_karafun.has_backing_track = false;
    g_karafun.backing_channel = -1;
    
    printf("KAR: Successfully loaded with %d words in %d lines\n", 
           g_karafun.word_count, g_karafun.line_count);
    
    return true;
}

// Update the currently-active word based on playback time (in seconds)
void kar_update(double playback_position_seconds) {
    if (!g_karafun.active || g_karafun.sync_count == 0) {
        return;
    }
    
    int playback_ms = (int)(playback_position_seconds * 1000.0);
    
    // Binary search to find the current word index based on sync times
    int left = 0, right = g_karafun.sync_count - 1;
    int word_idx = 0;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (g_karafun.sync_times_ms[mid] <= playback_ms) {
            word_idx = mid;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    g_karafun.current_word_idx = word_idx;
}

// Stop KAR playback and clean up
void kar_stop(void) {
    if (!g_karafun.active) return;
    
    g_karafun.active = false;
    cleanup_karafun_state();
    
    printf("KAR: Stopped\n");
}

bool is_kar_ext(const char *filename) {
    if (!filename) return false;
    const char *dot = strrchr(filename, '.');
    if (!dot) return false;
    return strcasecmp(dot, ".kar") == 0;
}

// Get the current KarafunState populated by kar_load()
KarafunState* kar_get_state(void) {
    return &g_karafun;
}
