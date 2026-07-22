// Karafun (.kfn) support - integrated with Zenamp's audio system
// Based on felixchirp's fc_kfn.cpp approach:
//   - Vocal track plays via normal load_file() -> SDL audio
//   - Backing track plays as Mix_Chunk on separate channel
//   - Sync via playTime (no separate karafun_update needed)

#include "karafun.h"
#include "kfn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifndef _WIN32
#  include <unistd.h>
#else
#  include <windows.h>
#endif
#include <strings.h>
#include <string>
#include <vector>
#include "visualization.h"
#include <SDL2/SDL_mixer.h>
#include "audioconverter.h"
#include "convertoggtowav.h"
#include "convertflactowav.h"
#include "convertopustowav.h"

// convertm4atowav{lin,win}.cpp expose this but ship no shared header
extern bool convertM4aToWavInMemory(const std::vector<uint8_t>& m4a_data, std::vector<uint8_t>& wav_data);

static KarafunState g_karafun = {0};

// See karafun_set_skip_background() / karafun.h.
static bool g_karafun_skip_background = false;

void karafun_set_skip_background(bool skip) {
    g_karafun_skip_background = skip;
}

// ============================================================================
// HELPERS
// ============================================================================

static void trim_string(char *str, size_t len) {
    if (!str || len == 0) return;
    // Trim trailing whitespace
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\r' || str[len-1] == '\n')) {
        str[--len] = '\0';
    }
    // Trim leading whitespace
    size_t start = 0;
    while (start < len && (str[start] == ' ' || str[start] == '\t')) start++;
    if (start > 0) {
        memmove(str, str + start, len - start + 1);
    }
}

static void to_lower_str(char *str) {
    if (!str) return;
    for (; *str; str++) *str = tolower((unsigned char)*str);
}

static bool all_digits(const char *str, size_t from) {
    if (!str || from >= strlen(str)) return false;
    for (size_t i = from; str[i]; i++) {
        if (!isdigit((unsigned char)str[i])) return false;
    }
    return true;
}

static bool is_audio_file(const char *name) {
    if (!name) return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    char ext[8] = {0};
    for (int i = 0; i < 7 && dot[i]; i++) ext[i] = tolower((unsigned char)dot[i]);
    return strcmp(ext, ".ogg") == 0 || strcmp(ext, ".mp3") == 0 || 
           strcmp(ext, ".wav") == 0 || strcmp(ext, ".m4a") == 0 || 
           strcmp(ext, ".aac") == 0 || strcmp(ext, ".flac") == 0;
}

static void delete_temp_file(const char *path) {
    if (!path || !path[0]) return;
#ifndef _WIN32
    unlink(path);
#else
    DeleteFileA(path);
#endif
}

// ============================================================================
// VOCAL/BACKING -> WAV -> MIX
// ============================================================================

static bool read_file_bytes(const char *path, std::vector<uint8_t> &out) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return false; }
    out.resize((size_t)sz);
    size_t rd = fread(out.data(), 1, (size_t)sz, f);
    fclose(f);
    return rd == (size_t)sz;
}

static std::string get_ext_lower(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "";
    std::string ext(dot);
    for (auto &c : ext) c = (char)tolower((unsigned char)c);
    return ext;
}

// Reads the extracted track off disk and converts it to WAV in memory using
// the same per-format converters the rest of the player uses (ogg/mp3/flac/
// opus/m4a). If it's already a WAV, passes it through untouched.
static bool convert_track_to_wav(const char *path, std::vector<uint8_t> &wav_out) {
    std::vector<uint8_t> raw;
    if (!read_file_bytes(path, raw)) {
        printf("KARAFUN: Failed to read %s for mixing\n", path);
        return false;
    }

    std::string ext = get_ext_lower(path);

    if (ext == ".wav")  { wav_out = raw; return true; }
    if (ext == ".ogg")  return convertOggToWavInMemory(raw, wav_out);
    if (ext == ".mp3")  return convertMp3ToWavInMemory(raw, wav_out);
    if (ext == ".flac") return convertFlacToWavInMemory(raw, wav_out);
    if (ext == ".opus") return convertOpusToWavInMemory(raw, wav_out);
    if (ext == ".m4a" || ext == ".aac") return convertM4aToWavInMemory(raw, wav_out);

    printf("KARAFUN: Unsupported extension '%s' for mixing\n", ext.c_str());
    return false;
}

// Writes a WAV byte buffer to a fresh temp file and hands back its path.
static bool write_temp_wav(const std::vector<uint8_t> &wav_data, char *out_path, size_t out_path_size) {
#ifndef _WIN32
    char path[] = "/tmp/karafun_mix_XXXXXX.wav";
    int fd = mkstemps(path, 4); // ".wav" = 4 chars
    if (fd < 0) {
        printf("KARAFUN: mkstemps failed for mixed WAV\n");
        return false;
    }
    FILE *f = fdopen(fd, "wb");
    if (!f) { ::close(fd); unlink(path); return false; }
#else
    char tmp_dir[MAX_PATH];
    if (!GetTempPathA(sizeof(tmp_dir), tmp_dir)) return false;
    char base[MAX_PATH];
    if (!GetTempFileNameA(tmp_dir, "kfm_", 0, base)) return false;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s.wav", base);
    DeleteFileA(base);
    FILE *f = fopen(path, "wb");
    if (!f) return false;
#endif

    size_t written = fwrite(wav_data.data(), 1, wav_data.size(), f);
    fclose(f);
    if (written != wav_data.size()) {
        printf("KARAFUN: Failed writing mixed WAV to %s\n", path);
        delete_temp_file(path);
        return false;
    }

    strncpy(out_path, path, out_path_size - 1);
    out_path[out_path_size - 1] = '\0';
    return true;
}

// Converts vocal (+ backing, if present) to WAV, mixes them, writes a single
// temp WAV file. This is what should be handed to load_file() for playback
// instead of playing vocal + backing as two separate tracks.
bool karafun_prepare_mixed_track(void) {
    if (!g_karafun.tmp_vocal_path[0]) {
        printf("KARAFUN: No vocal track to mix\n");
        return false;
    }

    std::vector<uint8_t> vocalWav;
    if (!convert_track_to_wav(g_karafun.tmp_vocal_path, vocalWav)) {
        printf("KARAFUN: Failed to convert vocal track to WAV\n");
        return false;
    }

    if (!g_karafun.has_backing_track || !g_karafun.tmp_backing_path[0]) {
        // Vocal-only file: still funnel it through the same temp-WAV path
        // so callers always load karafun_get_mixed_path() and never have
        // to special-case "no backing track".
        return write_temp_wav(vocalWav, g_karafun.tmp_mixed_path, sizeof(g_karafun.tmp_mixed_path));
    }

    std::vector<uint8_t> backingWav;
    if (!convert_track_to_wav(g_karafun.tmp_backing_path, backingWav)) {
        printf("KARAFUN: Failed to convert backing track to WAV\n");
        return false;
    }

    std::vector<uint8_t> mixedWav;
    if (!mixTwoWavFiles(vocalWav, backingWav, mixedWav)) {
        printf("KARAFUN: Mixing vocal + backing failed "
               "(sample rate / channels / bit depth mismatch)\n");
        return false;
    }

    if (!write_temp_wav(mixedWav, g_karafun.tmp_mixed_path, sizeof(g_karafun.tmp_mixed_path))) {
        return false;
    }

    printf("KARAFUN: Mixed vocal+backing -> %s\n", g_karafun.tmp_mixed_path);
    return true;
}

const char* karafun_get_mixed_path(void) {
    return g_karafun.active && g_karafun.tmp_mixed_path[0] ? g_karafun.tmp_mixed_path : NULL;
}

// Re-derives the single playback WAV honoring the current vocal_muted /
// backing_muted flags, and swaps it into g_karafun.tmp_mixed_path (deleting
// the old temp file). Used when toggling V/B mid-playback — the initial
// load always goes through karafun_prepare_mixed_track() with both tracks
// enabled instead.
static bool karafun_remix_for_mute_state(void) {
    if (!g_karafun.tmp_vocal_path[0]) {
        printf("KARAFUN: No vocal track to remix\n");
        return false;
    }

    bool has_backing = g_karafun.has_backing_track && g_karafun.tmp_backing_path[0];

    // Never allow both tracks to end up muted -- there'd be nothing to
    // play. Whichever track was just toggled on wins; force the other back on.
    if (g_karafun.vocal_muted && (g_karafun.backing_muted || !has_backing)) {
        g_karafun.backing_muted = false;
    }
    if (g_karafun.backing_muted && g_karafun.vocal_muted) {
        g_karafun.vocal_muted = false;
    }

    std::vector<uint8_t> outputWav;

    if (g_karafun.vocal_muted && has_backing) {
        // Backing only.
        if (!convert_track_to_wav(g_karafun.tmp_backing_path, outputWav)) {
            printf("KARAFUN: Failed to convert backing track to WAV\n");
            return false;
        }
    } else if (g_karafun.backing_muted || !has_backing) {
        // Vocal only (backing explicitly muted, or there just isn't one).
        if (!convert_track_to_wav(g_karafun.tmp_vocal_path, outputWav)) {
            printf("KARAFUN: Failed to convert vocal track to WAV\n");
            return false;
        }
    } else {
        // Neither muted: full mix, same as the initial load.
        std::vector<uint8_t> vocalWav, backingWav;
        if (!convert_track_to_wav(g_karafun.tmp_vocal_path, vocalWav) ||
            !convert_track_to_wav(g_karafun.tmp_backing_path, backingWav)) {
            printf("KARAFUN: Failed to convert tracks to WAV for remix\n");
            return false;
        }
        if (!mixTwoWavFiles(vocalWav, backingWav, outputWav)) {
            printf("KARAFUN: Remixing vocal + backing failed\n");
            return false;
        }
    }

    char new_path[sizeof(g_karafun.tmp_mixed_path)] = {0};
    if (!write_temp_wav(outputWav, new_path, sizeof(new_path))) {
        return false;
    }

    delete_temp_file(g_karafun.tmp_mixed_path);
    strncpy(g_karafun.tmp_mixed_path, new_path, sizeof(g_karafun.tmp_mixed_path) - 1);
    g_karafun.tmp_mixed_path[sizeof(g_karafun.tmp_mixed_path) - 1] = '\0';

    printf("KARAFUN: Remixed (vocal_muted=%d, backing_muted=%d) -> %s\n",
           g_karafun.vocal_muted, g_karafun.backing_muted, g_karafun.tmp_mixed_path);
    return true;
}

bool karafun_toggle_vocal_mute(void) {
    if (!g_karafun.active) return false;
    g_karafun.vocal_muted = !g_karafun.vocal_muted;
    if (!karafun_remix_for_mute_state()) {
        g_karafun.vocal_muted = !g_karafun.vocal_muted;  // roll back
        return false;
    }
    return true;
}

bool karafun_toggle_backing_mute(void) {
    if (!g_karafun.active || !g_karafun.has_backing_track) return false;
    g_karafun.backing_muted = !g_karafun.backing_muted;
    if (!karafun_remix_for_mute_state()) {
        g_karafun.backing_muted = !g_karafun.backing_muted;  // roll back
        return false;
    }
    return true;
}

bool karafun_is_vocal_muted(void) {
    return g_karafun.active && g_karafun.vocal_muted;
}

bool karafun_is_backing_muted(void) {
    return g_karafun.active && g_karafun.backing_muted;
}

// ============================================================================
// PARSING (song.ini from KFN archive)
// ============================================================================

static void parse_song_ini(const char *content, size_t content_len) {
    printf("KARAFUN: Parsing song.ini (%zu bytes)\n", content_len);
    
    if (!content || content_len == 0) return;
    
    char *ini_copy = (char*)malloc(content_len + 1);
    if (!ini_copy) return;
    memcpy(ini_copy, content, content_len);
    ini_copy[content_len] = '\0';
    
    char title[512] = "Unknown Song";
    char artist[512] = "Unknown Artist";

    // Title/artist live in [general], which comes BEFORE [eff6] in the file.
    // The scan below intentionally starts at [eff6] (see comment there), so
    // it never sees [general] — pull title/artist out separately first.
    {
        bool in_general = false;
        char *scan = ini_copy;
        char *scan_end = ini_copy + content_len;
        while (scan && scan < scan_end) {
            char *newline = strchr(scan, '\n');
            if (!newline) newline = scan_end;

            int line_len = newline - scan;
            if (line_len > 2048) line_len = 2048;

            char line[2048] = {0};
            strncpy(line, scan, line_len);
            trim_string(line, strlen(line));

            if (line[0] == '[') {
                in_general = (strcasecmp(line, "[general]") == 0);
            } else if (in_general && line[0] && line[0] != ';' && line[0] != '#') {
                char *eq = strchr(line, '=');
                if (eq) {
                    *eq = '\0';
                    char key[256] = {0};
                    char val[512] = {0};
                    strncpy(key, line, sizeof(key) - 1);
                    strncpy(val, eq + 1, sizeof(val) - 1);
                    trim_string(key, strlen(key));
                    trim_string(val, strlen(val));
                    to_lower_str(key);

                    if (strcmp(key, "title") == 0 && val[0]) {
                        strncpy(title, val, sizeof(title) - 1);
                    } else if (strcmp(key, "artist") == 0 && val[0]) {
                        strncpy(artist, val, sizeof(artist) - 1);
                    }
                }
            }

            scan = newline + 1;
        }
    }
    
    // Find [eff6] section which contains the lyrics
    char *eff6_start = strstr(ini_copy, "[eff6]");
    if (!eff6_start) {
        printf("KARAFUN: No [eff6] section found\n");
        free(ini_copy);
        return;
    }
    
    char *line_start = eff6_start;
    char *content_end = ini_copy + content_len;
    
    while (line_start && line_start < content_end) {
        char *newline = strchr(line_start, '\n');
        if (!newline) newline = (char*)content_end;
        
        int line_len = newline - line_start;
        if (line_len > 2048) line_len = 2048;
        
        char line[2048] = {0};
        strncpy(line, line_start, line_len);
        trim_string(line, strlen(line));
        
        if (line[0] && line[0] != ';' && line[0] != '#' && line[0] != '[') {
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                char key[256] = {0};
                char val[2048] = {0};
                
                strncpy(key, line, sizeof(key) - 1);
                strncpy(val, eq + 1, sizeof(val) - 1);
                trim_string(key, strlen(key));
                trim_string(val, strlen(val));
                to_lower_str(key);
                
                if (strncmp(key, "text", 4) == 0 && all_digits(key, 4)) {
                    // text0=Yan/kee Doo/dle went to town/_
                    // Split on "/" and spaces to get syllables
                    
                    // Count syllables first (including underscores, which consume sync times)
                    int syl_count = 0;
                    char *count_copy = (char*)malloc(strlen(val) + 1);
                    strcpy(count_copy, val);
                    for (char *p = count_copy; *p; p++) {
                        if (*p == '/') *p = ' ';
                    }
                    char *save_tmp = NULL;
                    char *syl_tmp = strtok_r(count_copy, " ", &save_tmp);
                    while (syl_tmp) {
                        char syl_t[512] = {0};
                        strncpy(syl_t, syl_tmp, sizeof(syl_t) - 1);
                        trim_string(syl_t, strlen(syl_t));
                        if (syl_t[0]) syl_count++;  // Count ALL syllables, including underscores
                        syl_tmp = strtok_r(NULL, " ", &save_tmp);
                    }
                    free(count_copy);
                    
                    // Store line BEFORE adding syllables
                    if (syl_count > 0 && g_karafun.line_count < 1000) {
                        int line_start_idx = g_karafun.word_count;
                        
                        KarafunLyricLine *new_lines = (KarafunLyricLine*)realloc(
                            g_karafun.lines,
                            sizeof(KarafunLyricLine) * (g_karafun.line_count + 1));
                        if (new_lines) {
                            g_karafun.lines = new_lines;
                            
                            // Reconstruct display text
                            char display_text[2048] = {0};
                            char *display_copy = (char*)malloc(strlen(val) + 1);
                            strcpy(display_copy, val);
                            for (char *p = display_copy; *p; p++) {
                                if (*p == '/') *p = ' ';
                            }
                            char *save2 = NULL;
                            char *syl = strtok_r(display_copy, " ", &save2);
                            while (syl) {
                                char syl_trim[512] = {0};
                                strncpy(syl_trim, syl, sizeof(syl_trim) - 1);
                                trim_string(syl_trim, strlen(syl_trim));
                                if (syl_trim[0] && strcmp(syl_trim, "_") != 0) {
                                    if (display_text[0]) strcat(display_text, " ");
                                    strcat(display_text, syl_trim);
                                }
                                syl = strtok_r(NULL, " ", &save2);
                            }
                            free(display_copy);
                            
                            g_karafun.lines[g_karafun.line_count].start_word_idx = line_start_idx;
                            g_karafun.lines[g_karafun.line_count].word_count = syl_count;
                            strncpy(g_karafun.lines[g_karafun.line_count].display_text,
                                   display_text, sizeof(g_karafun.lines[0].display_text) - 1);
                            g_karafun.line_count++;
                            printf("KARAFUN: Line %d (words %d-%d): %s\n", g_karafun.line_count - 1, 
                                   line_start_idx, line_start_idx + syl_count - 1, display_text);
                        }
                    }
                    
                    // NOW add syllables as words (including underscores as placeholders)
                    char *val_copy = (char*)malloc(strlen(val) + 1);
                    strcpy(val_copy, val);
                    for (char *p = val_copy; *p; p++) {
                        if (*p == '/') *p = ' ';
                    }
                    char *saveptr = NULL;
                    char *syllable = strtok_r(val_copy, " ", &saveptr);
                    
                    while (syllable) {
                        char syl_trimmed[512] = {0};
                        strncpy(syl_trimmed, syllable, sizeof(syl_trimmed) - 1);
                        trim_string(syl_trimmed, strlen(syl_trimmed));
                        
                        if (syl_trimmed[0]) {  // Add ALL syllables, including underscores
                            if (g_karafun.word_count < 10000) {
                                char **new_words = (char**)realloc(g_karafun.words,
                                    sizeof(char*) * (g_karafun.word_count + 1));
                                if (new_words) {
                                    g_karafun.words = new_words;
                                    char *word_dup = (char*)malloc(strlen(syl_trimmed) + 1);
                                    if (word_dup) {
                                        strcpy(word_dup, syl_trimmed);
                                        g_karafun.words[g_karafun.word_count++] = word_dup;
                                    }
                                }
                            }
                        }
                        
                        syllable = strtok_r(NULL, " ", &saveptr);
                    }
                    free(val_copy);
                } else if (strncmp(key, "sync", 4) == 0 && all_digits(key, 4)) {
                    // sync0=177,179,182,185,...  (centiseconds!)
                    // Convert to seconds: centiseconds / 100.0
                    char *val_copy = (char*)malloc(strlen(val) + 1);
                    strcpy(val_copy, val);
                    char *saveptr = NULL;
                    char *token = strtok_r(val_copy, ",", &saveptr);
                    
                    while (token) {
                        char token_trimmed[256] = {0};
                        strncpy(token_trimmed, token, sizeof(token_trimmed) - 1);
                        trim_string(token_trimmed, strlen(token_trimmed));
                        
                        int centiseconds = atoi(token_trimmed);
                        double seconds = centiseconds / 100.0;
                        
                        if (g_karafun.sync_count < 10000) {
                            int *new_syncs = (int*)realloc(g_karafun.sync_times_ms,
                                sizeof(int) * (g_karafun.sync_count + 1));
                            if (new_syncs) {
                                g_karafun.sync_times_ms = new_syncs;
                                g_karafun.sync_times_ms[g_karafun.sync_count++] = (int)(seconds * 1000.0);
                            }
                        }
                        
                        token = strtok_r(NULL, ",", &saveptr);
                    }
                    free(val_copy);
                }
            }
        }
        
        line_start = newline + 1;
    }
    
    strncpy(g_karafun.title, title, sizeof(g_karafun.title) - 1);
    strncpy(g_karafun.artist, artist, sizeof(g_karafun.artist) - 1);
    g_karafun.current_word_idx = 0;
    
    printf("KARAFUN: Parsed - %d lines, %d words, %d sync times\n",
           g_karafun.line_count, g_karafun.word_count, g_karafun.sync_count);
    
    free(ini_copy);
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool karafun_load(const char *kfn_path) {
    printf("KARAFUN: Loading %s\n", kfn_path);
    karafun_stop();
    
    // Open KFN archive using C++ class
    KFNArchive archive;
    if (!archive.open(kfn_path)) {
        printf("KARAFUN: Failed to open archive: %s\n", archive.lastError().c_str());
        return false;
    }
    
    // Extract song.ini
    printf("KARAFUN: Extracting song.ini\n");
    const KFNEntry *ini_entry = archive.find("song.ini");
    if (!ini_entry) {
        printf("KARAFUN: No song.ini found\n");
        return false;
    }
    
    size_t ini_size = 0;
    unsigned char *ini_data = archive.extract(*ini_entry, ini_size);
    if (!ini_data || ini_size == 0) {
        printf("KARAFUN: Failed to extract song.ini\n");
        return false;
    }
    
    // Parse lyrics and sync
    parse_song_ini((const char*)ini_data, ini_size);
    free(ini_data);
    
    if (g_karafun.word_count == 0) {
        printf("KARAFUN: No lyrics parsed\n");
        return false;
    }
    
    // Extract audio files
    printf("KARAFUN: Extracting audio tracks\n");
    const auto &entries = archive.entries();
    
    char *vocal_path = NULL;
    char *backing_path = NULL;
    
    for (const auto &entry : entries) {
        if (!is_audio_file(entry.filename.c_str())) continue;
        
        printf("KARAFUN: Found audio: %s\n", entry.filename.c_str());
        
        std::string ext = entry.filename;
        size_t dot = ext.find_last_of('.');
        if (dot == std::string::npos) ext = ".ogg";
        else ext = ext.substr(dot);
        
        std::string tmp_path = archive.extractToTemp(entry, ext.c_str());
        if (tmp_path.empty()) {
            printf("KARAFUN: Failed to extract %s\n", entry.filename.c_str());
            continue;
        }
        
        // Simple heuristic: if contains "instru" or "beat" it's backing, else vocal
        std::string lower = entry.filename;
        for (auto &c : lower) c = tolower((unsigned char)c);
        
        bool is_backing = lower.find("instru") != std::string::npos || 
                         lower.find("beat") != std::string::npos;
        
        if (is_backing && !backing_path) {
            backing_path = (char*)malloc(tmp_path.length() + 1);
            strcpy(backing_path, tmp_path.c_str());
            strncpy(g_karafun.tmp_backing_path, tmp_path.c_str(), sizeof(g_karafun.tmp_backing_path) - 1);
            g_karafun.has_backing_track = true;
        } else if (!is_backing && !vocal_path) {
            vocal_path = (char*)malloc(tmp_path.length() + 1);
            strcpy(vocal_path, tmp_path.c_str());
            strncpy(g_karafun.tmp_vocal_path, tmp_path.c_str(), sizeof(g_karafun.tmp_vocal_path) - 1);
        }
    }
    
    if (!vocal_path) {
        printf("KARAFUN: No vocal track found\n");
        if (backing_path) free(backing_path);
        return false;
    }
    
    g_karafun.active = true;
    g_karafun.current_word_idx = 0;

    // Convert vocal (+ backing) to WAV and mix down to a single playback
    // file. karafun_get_mixed_path() is what the caller should load.
    if (!karafun_prepare_mixed_track()) {
        printf("KARAFUN: Failed to prepare mixed playback track\n");
        free(vocal_path);
        if (backing_path) free(backing_path);
        g_karafun.active = false;
        return false;
    }

    printf("KARAFUN: Successfully loaded - Title: %s, Artist: %s\n", g_karafun.title, g_karafun.artist);
    printf("KARAFUN: Sync timing (first 20 words):\n");
    for (int i = 0; i < g_karafun.sync_count && i < 20; i++) {
        if (i < g_karafun.word_count && g_karafun.words[i]) {
            double seconds = g_karafun.sync_times_ms[i] / 1000.0;
            printf("  [%d] %.2fs -> %s\n", i, seconds, g_karafun.words[i]);
        }
    }
    
    free(vocal_path);
    if (backing_path) free(backing_path);
    
    return true;
}

void karafun_stop(void) {
    if (!g_karafun.active) return;
    
    printf("KARAFUN: Stopping\n");
    
    // Stop backing track
    karafun_stop_backing();
    
    for (int i = 0; i < g_karafun.word_count; i++) {
        if (g_karafun.words && g_karafun.words[i]) {
            free(g_karafun.words[i]);
        }
    }
    if (g_karafun.words) {
        free(g_karafun.words);
        g_karafun.words = NULL;
    }
    if (g_karafun.sync_times_ms) {
        free(g_karafun.sync_times_ms);
        g_karafun.sync_times_ms = NULL;
    }
    if (g_karafun.lines) {
        free(g_karafun.lines);
        g_karafun.lines = NULL;
    }
    
    delete_temp_file(g_karafun.tmp_vocal_path);
    delete_temp_file(g_karafun.tmp_backing_path);
    delete_temp_file(g_karafun.tmp_mixed_path);
    
    memset(&g_karafun, 0, sizeof(g_karafun));
    g_karafun.backing_channel = -1;
}

void karafun_update(double playback_position_seconds) {
    if (!g_karafun.active) return;
    
    int current_ms = (int)(playback_position_seconds * 1000);
    
    // Find current word based on sync times
    int idx = 0;
    for (int i = 0; i < g_karafun.sync_count; i++) {
        if (current_ms >= g_karafun.sync_times_ms[i]) {
            idx = i;
        } else {
            break;
        }
    }
    
    g_karafun.current_word_idx = idx;
}

int karafun_current_word(void) {
    return g_karafun.active ? g_karafun.current_word_idx : -1;
}

int karafun_current_line(void) {
    if (!g_karafun.active || g_karafun.current_word_idx >= g_karafun.word_count) return -1;
    
    for (int i = 0; i < g_karafun.line_count; i++) {
        int line_end = (i + 1 < g_karafun.line_count) ? 
            g_karafun.lines[i + 1].start_word_idx : g_karafun.word_count;
        
        if (g_karafun.current_word_idx >= g_karafun.lines[i].start_word_idx &&
            g_karafun.current_word_idx < line_end) {
            return i;
        }
    }
    return -1;
}

KarafunState* karafun_get_state(void) {
    return &g_karafun;
}

bool is_karafun_ext(const char *filename) {
    if (!filename) return false;
    const char *dot = strrchr(filename, '.');
    if (!dot) return false;
    char ext[8] = {0};
    for (int i = 0; i < 7 && dot[i]; i++) ext[i] = tolower((unsigned char)dot[i]);
    return strcmp(ext, ".kfn") == 0;
}

const char* karafun_get_vocal_path(void) {
    return g_karafun.active && g_karafun.tmp_vocal_path[0] ? g_karafun.tmp_vocal_path : NULL;
}

const char* karafun_get_backing_path(void) {
    return g_karafun.active && g_karafun.tmp_backing_path[0] ? g_karafun.tmp_backing_path : NULL;
}

void karafun_set_backing_channel(int channel) {
    g_karafun.backing_channel = channel;
    printf("KARAFUN: Set backing channel to %d\n", channel);
}

void karafun_stop_backing(void) {
    if (g_karafun.backing_channel >= 0) {
        Mix_HaltChannel(g_karafun.backing_channel);
        g_karafun.backing_channel = -1;
        printf("KARAFUN: Stopped backing track\n");
    }
}

void draw_karafun_lyrics(void *vis_ptr, void *cr_ptr)
{
    Visualizer *vis = (Visualizer*)vis_ptr;
    cairo_t *cr = (cairo_t*)cr_ptr;

    if (!vis || !cr || !g_karafun.active || !g_karafun.words || g_karafun.word_count <= 0)
        return;

    extern double playTime;
    int current_ms = (int)(playTime * 1000);

    //
    // --- FIND CURRENT WORD ---
    //
    int current_word_idx = 0;
    for (int i = 0; i < g_karafun.sync_count; i++) {
        if (current_ms >= g_karafun.sync_times_ms[i])
            current_word_idx = i;
        else
            break;
    }

    //
    // --- BACKGROUND ---
    //
    if (!g_karafun_skip_background) {
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_rectangle(cr, 0, 0, vis->width, vis->height);
        cairo_fill(cr);
    }

    //
    // --- TITLE ---
    //
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 20);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, g_karafun.title, &ext);
    cairo_move_to(cr, (vis->width - ext.width) / 2, 40);
    cairo_show_text(cr, g_karafun.title);

    //
    // --- ARTIST ---
    //
    cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
    cairo_set_font_size(cr, 14);
    cairo_text_extents(cr, g_karafun.artist, &ext);
    cairo_move_to(cr, (vis->width - ext.width) / 2, 70);
    cairo_show_text(cr, g_karafun.artist);

    //
    // --- FIND CURRENT LINE ---
    //
    int current_line = -1;
    for (int i = 0; i < g_karafun.line_count; i++) {
        int line_end = (i + 1 < g_karafun.line_count)
            ? g_karafun.lines[i + 1].start_word_idx
            : g_karafun.word_count;

        if (current_word_idx >= g_karafun.lines[i].start_word_idx &&
            current_word_idx < line_end) {
            current_line = i;
            break;
        }
    }

    if (current_line < 0)
        return;

    const char *current_text = g_karafun.lines[current_line].display_text;

    //
    // --- DYNAMIC FONT SIZE (WIDTH + HEIGHT AWARE) ---
    //
    int font_size = vis->height / 6;
    if (font_size < 8) font_size = 8;
    if (font_size > 60) font_size = 60;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

    // shrink until text fits width
    for (;;) {
        cairo_set_font_size(cr, font_size);
        cairo_text_extents(cr, current_text, &ext);

        if (ext.width <= vis->width * 0.90)
            break;

        font_size--;
        if (font_size <= 8)
            break;
    }

    //
    // --- CENTER TEXT ---
    //
    cairo_text_extents(cr, current_text, &ext);
    double text_x = (vis->width - ext.width) / 2;
    double text_y = vis->height / 2 + 20;

    //
    // --- FULL WORD HIGHLIGHT BOX ---
    //
    const char *current_word = g_karafun.words[current_word_idx];

    // count occurrences before this word
    int occurrence = 0;
    for (int i = g_karafun.lines[current_line].start_word_idx; i < current_word_idx; i++) {
        if (strcmp(g_karafun.words[i], current_word) == 0)
            occurrence++;
    }

    // find Nth occurrence in the display text
    const char *pos = current_text;
    for (int i = 0; i <= occurrence; i++) {
        pos = strstr(pos, current_word);
        if (!pos) break;
        if (i < occurrence)
            pos++;
    }

    if (pos) {
        size_t before_len = pos - current_text;

        char before[2048];
        strncpy(before, current_text, before_len);
        before[before_len] = '\0';

        // measure BEFORE text
        cairo_text_extents(cr, before, &ext);
        double word_x = text_x + ext.x_advance;

        // measure WORD
        cairo_text_extents(cr, current_word, &ext);
        double word_w = ext.x_advance;

        // padding
        double pad_x = font_size * 0.15;
        double pad_y = font_size * 0.35;

        double box_x = word_x - pad_x;
        double box_y = text_y - ext.height - pad_y;
        double box_w = word_w + pad_x * 2;
        double box_h = ext.height + pad_y * 2;

        cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.35);
        cairo_rectangle(cr, box_x, box_y, box_w, box_h);
        cairo_fill(cr);
    }

    //
    // --- DRAW CURRENT LINE ---
    //
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, current_text);

    //
    // --- NEXT LINE ---
    //
    if (current_line + 1 < g_karafun.line_count) {
        const char *next_text = g_karafun.lines[current_line + 1].display_text;

        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, font_size / 2);

        cairo_text_extents(cr, next_text, &ext);
        cairo_move_to(cr, (vis->width - ext.width) / 2, vis->height / 2 + 80);
        cairo_show_text(cr, next_text);
    }
}





// Render lyrics display
/*void draw_karafun_lyrics(void *vis_ptr, void *cr_ptr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    cairo_t *cr = (cairo_t*)cr_ptr;

    if (!vis || !cr || !g_karafun.active || !g_karafun.words || g_karafun.word_count <= 0)
        return;

    extern double playTime;
    int current_ms = (int)(playTime * 1000);

    // Find current word
    int current_word_idx = 0;
    for (int i = 0; i < g_karafun.sync_count; i++) {
        if (current_ms >= g_karafun.sync_times_ms[i])
            current_word_idx = i;
        else
            break;
    }

    // Background
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_rectangle(cr, 0, 0, vis->width, vis->height);
    cairo_fill(cr);

    // Title
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 20);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, g_karafun.title, &extents);
    cairo_move_to(cr, (vis->width - extents.width) / 2, 40);
    cairo_show_text(cr, g_karafun.title);

    // Artist
    cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
    cairo_set_font_size(cr, 14);
    cairo_text_extents(cr, g_karafun.artist, &extents);
    cairo_move_to(cr, (vis->width - extents.width) / 2, 70);
    cairo_show_text(cr, g_karafun.artist);

    // Find current line
    int current_line = -1;
    for (int i = 0; i < g_karafun.line_count; i++) {
        int line_end = (i + 1 < g_karafun.line_count)
            ? g_karafun.lines[i + 1].start_word_idx
            : g_karafun.word_count;

        if (current_word_idx >= g_karafun.lines[i].start_word_idx &&
            current_word_idx < line_end) {
            current_line = i;
            break;
        }
    }

    if (current_line < 0)
        return;

    // Dynamic font size
    int font_size = vis->height / 6;
    if (font_size < 24) font_size = 24;
    if (font_size > 60) font_size = 60;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, font_size);

    const char *current_text = g_karafun.lines[current_line].display_text;

    // Center text
    cairo_text_extents(cr, current_text, &extents);
    double text_x = (vis->width - extents.width) / 2;
    double text_y = vis->height / 2 + 20;

    // Highlight logic
    const char *current_word = g_karafun.words[current_word_idx];
    const char *word_pos = strstr(current_text, current_word);

    if (word_pos) {
        size_t before_len = word_pos - current_text;
        size_t word_len = strlen(current_word);

        // Draw BEFORE (white)
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, text_x, text_y);

        char before[2048];
        strncpy(before, current_text, before_len);
        before[before_len] = '\0';
        cairo_show_text(cr, before);

        // Measure BEFORE width
        cairo_text_extents(cr, before, &extents);
        double word_x = text_x + extents.width;

        // Draw WORD (blue)
        cairo_set_source_rgb(cr, 0.2, 0.6, 1.0);
        cairo_move_to(cr, word_x, text_y);
        cairo_show_text(cr, current_word);

        // Measure WORD width
        cairo_text_extents(cr, current_word, &extents);
        double after_x = word_x + extents.width;

        // Draw SPACE AFTER WORD (white)
        const char *after_ptr = word_pos + word_len;
        if (*after_ptr == ' ') {
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_move_to(cr, after_x, text_y);
            cairo_show_text(cr, " ");

            cairo_text_extents(cr, " ", &extents);
            after_x += extents.width;
            after_ptr++; // skip space
        }

        // Draw REMAINDER (white)
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, after_x, text_y);
        cairo_show_text(cr, after_ptr);
    } else {
        // No highlight
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, text_x, text_y);
        cairo_show_text(cr, current_text);
    }

    // Next line
    if (current_line + 1 < g_karafun.line_count) {
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, font_size / 2);

        const char *next_text = g_karafun.lines[current_line + 1].display_text;
        cairo_text_extents(cr, next_text, &extents);
        cairo_move_to(cr, (vis->width - extents.width) / 2, vis->height / 2 + 80);
        cairo_show_text(cr, next_text);
    }
}



*/
