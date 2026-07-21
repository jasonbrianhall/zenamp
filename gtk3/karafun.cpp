#include "karafun.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include "visualization.h"
#include <SDL2/SDL_mixer.h>

// Forward declare from kfn.cpp
class KFNArchive;
extern "C" {
    KFNArchive* kfn_archive_open(const char* path);
    void kfn_archive_close(KFNArchive* archive);
    const char* kfn_archive_last_error(KFNArchive* archive);
    unsigned char* kfn_archive_extract_by_name(KFNArchive* archive, const char* name, size_t* out_size);
    const char** kfn_archive_get_entries(KFNArchive* archive, int* count);
    char* kfn_archive_extract_to_temp(KFNArchive* archive, const char* entry_name, const char* ext);
}

static KarafunState g_karafun = {0};
static Mix_Music *g_backing_music = NULL;

static void trim_string(char *str, size_t len) {
    if (!str || len == 0) return;
    for (int i = (int)len - 1; i >= 0; i--) {
        if (!isspace((unsigned char)str[i])) {
            str[i + 1] = '\0';
            break;
        }
    }
    int start = 0;
    while (start < (int)len && isspace((unsigned char)str[start])) {
        start++;
    }
    if (start > 0) {
        memmove(str, str + start, len - start + 1);
    }
}

static char* to_lower_str(char *str) {
    if (!str) return str;
    for (int i = 0; str[i]; i++) {
        str[i] = (char)tolower((unsigned char)str[i]);
    }
    return str;
}

static bool all_digits(const char *str, size_t from) {
    if (!str || from >= strlen(str)) return false;
    for (size_t i = from; str[i]; i++) {
        if (!isdigit((unsigned char)str[i])) return false;
    }
    return true;
}

static bool is_audio_file(const char *name) {
    static const char *exts[] = {
        ".mp3", ".ogg", ".wav", ".m4a", ".aac", ".flac", ".opus", NULL
    };
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    
    char ext_lower[16] = {0};
    strncpy(ext_lower, dot, sizeof(ext_lower) - 1);
    to_lower_str(ext_lower);
    
    for (int i = 0; exts[i]; i++) {
        if (strcmp(ext_lower, exts[i]) == 0) return true;
    }
    return false;
}

static void delete_temp_file(const char *path) {
    if (!path || !path[0]) return;
    unlink(path);
}

static void parse_song_ini(const char *content, size_t content_len) {
    printf("KARAFUN: Parsing song.ini (%zu bytes)\n", content_len);
    
    if (!content || content_len == 0) return;
    
    char *ini_copy = (char*)malloc(content_len + 1);
    if (!ini_copy) return;
    memcpy(ini_copy, content, content_len);
    ini_copy[content_len] = '\0';
    
    char title[512] = "Unknown Song";
    char artist[512] = "Unknown Artist";
    
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
                
                if (strcmp(key, "title") == 0) {
                    strncpy(title, val, sizeof(title) - 1);
                } else if (strcmp(key, "artist") == 0) {
                    strncpy(artist, val, sizeof(artist) - 1);
                } else if (strncmp(key, "text", 4) == 0 && all_digits(key, 4)) {
                    // text0=Yan/kee Doo/dle went to town/_
                    // Split on BOTH "/" and spaces - each creates a sync point
                    
                    // FIRST: Count syllables in this line
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
                        if (syl_t[0] && strcmp(syl_t, "_") != 0) syl_count++;
                        syl_tmp = strtok_r(NULL, " ", &save_tmp);
                    }
                    free(count_copy);
                    
                    // Store line BEFORE adding syllables
                    if (syl_count > 0 && g_karafun.line_count < 1000) {
                        int line_start_idx = g_karafun.word_count;  // Current word index
                        
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
                    
                    // SECOND: Now add syllables as words
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
                        
                        if (syl_trimmed[0] && strcmp(syl_trimmed, "_") != 0) {
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
                    // Convert to milliseconds: centiseconds * 10 = milliseconds
                    // 1772 centiseconds = 17.72 seconds = 17720 milliseconds
                    char *val_copy = (char*)malloc(strlen(val) + 1);
                    strcpy(val_copy, val);
                    char *saveptr = NULL;
                    char *token = strtok_r(val_copy, ",", &saveptr);
                    
                    while (token) {
                        char token_trimmed[256] = {0};
                        strncpy(token_trimmed, token, sizeof(token_trimmed) - 1);
                        trim_string(token_trimmed, strlen(token_trimmed));
                        
                        int centiseconds = atoi(token_trimmed);
                        int ms = centiseconds * 10;  // 1772cs * 10 = 17720ms = 17.72 seconds
                        
                        if (g_karafun.sync_count < 10000) {
                            int *new_syncs = (int*)realloc(g_karafun.sync_times_ms,
                                sizeof(int) * (g_karafun.sync_count + 1));
                            if (new_syncs) {
                                g_karafun.sync_times_ms = new_syncs;
                                g_karafun.sync_times_ms[g_karafun.sync_count++] = ms;
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


bool karafun_load(const char *kfn_path) {
    printf("KARAFUN: Loading %s\n", kfn_path);
    
    if (!kfn_path) return false;
    
    karafun_stop();
    
    // Open KFN archive
    KFNArchive* archive = kfn_archive_open(kfn_path);
    if (!archive) {
        printf("KARAFUN: Failed to open archive\n");
        return false;
    }
    
    printf("KARAFUN: Archive opened\n");
    
    // Extract song.ini
    size_t ini_size = 0;
    unsigned char *ini_buf = kfn_archive_extract_by_name(archive, "song.ini", &ini_size);
    if (!ini_buf) {
        printf("KARAFUN: No song.ini found\n");
        kfn_archive_close(archive);
        return false;
    }
    
    printf("KARAFUN: song.ini extracted (%zu bytes)\n", ini_size);
    parse_song_ini((const char *)ini_buf, ini_size);
    
    // Parse [general] section for source= to find vocal track filename
    char vocal_filename[512] = {0};
    char *general_start = strstr((const char *)ini_buf, "[general]");
    if (general_start) {
        char *source_line = strstr(general_start, "source=");
        if (source_line) {
            source_line += 7;  // Skip "source="
            char *comma = strchr(source_line, ',');
            if (comma) {
                comma++;  // Skip first comma
                comma = strchr(comma, ',');
                if (comma) {
                    comma++;  // Skip to third field
                    char *newline = strchr(comma, '\n');
                    int len = newline ? (newline - comma) : strlen(comma);
                    strncpy(vocal_filename, comma, len);
                    vocal_filename[len] = '\0';
                    trim_string(vocal_filename, strlen(vocal_filename));
                    printf("KARAFUN: Vocal track from source: %s\n", vocal_filename);
                }
            }
        }
    }
    
    free(ini_buf);
    
    // Get entry list
    int entry_count = 0;
    const char** entries = kfn_archive_get_entries(archive, &entry_count);
    printf("KARAFUN: Archive has %d entries\n", entry_count);
    
    // Extract all audio files to temp (for Zenamp to use)
    char *vocal_path = NULL;
    char *backing_path = NULL;
    
    for (int i = 0; i < entry_count; i++) {
        if (!is_audio_file(entries[i])) continue;
        
        printf("KARAFUN: Extracting audio: %s\n", entries[i]);
        
        const char *dot = strrchr(entries[i], '.');
        char *tmp_path = kfn_archive_extract_to_temp(archive, entries[i], dot ? dot : "");
        
        if (!tmp_path) {
            printf("KARAFUN: Failed to extract %s\n", entries[i]);
            continue;
        }
        
        printf("KARAFUN: Extracted to: %s\n", tmp_path);
        
        // Check if this matches the vocal track from Song.ini
        bool is_vocal = (vocal_filename[0] && strcasecmp(entries[i], vocal_filename) == 0);
        
        if (is_vocal) {
            vocal_path = (char*)malloc(strlen(tmp_path) + 1);
            strcpy(vocal_path, tmp_path);
            strncpy(g_karafun.tmp_vocal_path, tmp_path, sizeof(g_karafun.tmp_vocal_path) - 1);
            printf("KARAFUN: Vocal track (will be mixed): %s\n", entries[i]);
        } else {
            backing_path = (char*)malloc(strlen(tmp_path) + 1);
            strcpy(backing_path, tmp_path);
            strncpy(g_karafun.tmp_backing_path, tmp_path, sizeof(g_karafun.tmp_backing_path) - 1);
            g_karafun.has_backing_track = true;
            printf("KARAFUN: Backing track (will be mixed): %s\n", entries[i]);
        }
        
        free(tmp_path);
    }
    
    kfn_archive_close(archive);
    
    if (!vocal_path) {
        printf("KARAFUN: No vocal track found\n");
        if (backing_path) free(backing_path);
        return false;
    }
    
    // Use vocal track for Zenamp playback
    // (Backing track would require complex FFmpeg mixing)
    if (backing_path) {
        // Load backing track with SDL_mixer to play alongside Zenamp's vocal
        g_backing_music = Mix_LoadMUS(backing_path);
        if (g_backing_music) {
            Mix_PlayMusic(g_backing_music, -1);
            printf("KARAFUN: Backing track playing via SDL_mixer\n");
        }
        free(backing_path);
    }
    
    g_karafun.active = true;
    g_karafun.current_word_idx = 0;
    
    printf("KARAFUN: Successfully loaded - Title: %s, Artist: %s\n", g_karafun.title, g_karafun.artist);
    printf("KARAFUN: Sync timing debug (first 20 entries, centiseconds->milliseconds->seconds):\n");
    for (int i = 0; i < g_karafun.sync_count && i < 20; i++) {
        if (i < g_karafun.word_count && g_karafun.words[i]) {
            double seconds = g_karafun.sync_times_ms[i] / 1000.0;
            printf("  [%d] %dcs (%dms = %.2fs) -> %s\n", i, g_karafun.sync_times_ms[i]/10, g_karafun.sync_times_ms[i], seconds, g_karafun.words[i]);
        }
    }
    
    printf("KARAFUN: Audio will be played by Zenamp (vocal) + SDL_mixer (backing)\n");
    
    return true;
}

void karafun_stop(void) {
    printf("KARAFUN: Stopping playback\n");
    
    if (!g_karafun.active) return;
    
    // Stop SDL_mixer backing track
    Mix_HaltMusic();
    
    if (g_backing_music) {
        Mix_FreeMusic(g_backing_music);
        g_backing_music = NULL;
    }
    
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
    
    memset(&g_karafun, 0, sizeof(g_karafun));
    printf("KARAFUN: Fully stopped and cleaned up\n");
}

void karafun_update(double playback_position_seconds) {
    if (!g_karafun.active) return;
    
    int current_ms = (int)(playback_position_seconds * 1000);
    
    // Find current word based on millisecond sync times
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
    return g_karafun.current_word_idx;
}

int karafun_current_line(void) {
    if (!g_karafun.active) return -1;
    
    for (int i = 0; i < g_karafun.line_count; i++) {
        if (g_karafun.current_word_idx < g_karafun.lines[i].start_word_idx) {
            return i - 1;
        }
    }
    return g_karafun.line_count - 1;
}

KarafunState* karafun_get_state(void) {
    return &g_karafun;
}

bool is_karafun_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return false;
    char ext[8] = {0};
    strncpy(ext, dot, sizeof(ext) - 1);
    to_lower_str(ext);
    return strcmp(ext, ".kfn") == 0;
}

const char* karafun_get_vocal_path(void) {
    return g_karafun.tmp_vocal_path[0] ? g_karafun.tmp_vocal_path : NULL;
}

const char* karafun_get_backing_path(void) {
    return g_karafun.tmp_backing_path[0] ? g_karafun.tmp_backing_path : NULL;
}

bool karafun_play_both_tracks(void *audio_player) {
    // This function should be implemented to use the Zenamp audio API
    // to play both vocal_path and backing_path simultaneously
    
    if (!g_karafun.active || !g_karafun.tmp_vocal_path[0]) {
        printf("KARAFUN: No active karaoke loaded\n");
        return false;
    }
    
    printf("KARAFUN: karafun_play_both_tracks called\n");
    printf("KARAFUN: Vocal track: %s\n", g_karafun.tmp_vocal_path);
    if (g_karafun.has_backing_track) {
        printf("KARAFUN: Backing track: %s\n", g_karafun.tmp_backing_path);
    }
    
    // TODO: Call audio_player API to load and play both tracks
    // For now, return true and let main.cpp handle loading the vocal track
    return true;
}

void karafun_set_paused(bool paused) {
    if (!g_karafun.active || !g_backing_music) return;
    
    if (paused) {
        Mix_PauseMusic();
        printf("KARAFUN: Backing track paused\n");
    } else {
        Mix_ResumeMusic();
        printf("KARAFUN: Backing track resumed\n");
    }
}

void karafun_seek(int position_ms) {
    // SDL_mixer doesn't support seeking on all formats
    // We'd need to stop and restart from position, which is complex
    // For now, just log it
    if (!g_karafun.active || !g_backing_music) return;
    
    printf("KARAFUN: Seek to %dms requested (SDL_mixer seek not fully supported)\n", position_ms);
    
    // Ideally: Mix_SetMusicPosition((double)position_ms / 1000.0);
    // But this doesn't work for all formats
}
void draw_karafun_lyrics(void *vis_ptr, void *cr_ptr) {
    Visualizer *vis = (Visualizer*)vis_ptr;
    cairo_t *cr = (cairo_t*)cr_ptr;
    
    if (!vis || !cr) return;
    
    KarafunState *kfn = karafun_get_state();
    if (!kfn || !kfn->active || !kfn->words || kfn->word_count <= 0) return;
    
    // Get current playback position from global playTime (in seconds)
    extern double playTime;
    int current_ms = (int)(playTime * 1000);
    
    // Recalculate current word index based on sync times
    int current_word_idx = 0;
    for (int i = 0; i < kfn->sync_count; i++) {
        if (current_ms >= kfn->sync_times_ms[i]) {
            current_word_idx = i;
        } else {
            break;
        }
    }
    
    // Debug: print sync info occasionally
    static int draw_counter = 0;
    if (draw_counter++ % 10 == 0) {  // Every 10 frames
        if (current_word_idx < kfn->word_count && kfn->words[current_word_idx]) {
            printf("DRAW: playTime=%.2fs (%dms) -> word[%d]=%s (sync=%dms = %.2fs)\n",
                   playTime, current_ms, current_word_idx, kfn->words[current_word_idx],
                   kfn->sync_times_ms[current_word_idx], kfn->sync_times_ms[current_word_idx]/1000.0);
        }
    }
    
    // Black background
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_rectangle(cr, 0, 0, vis->width, vis->height);
    cairo_fill(cr);
    
    // Title at top
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 20);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, kfn->title, &extents);
    cairo_move_to(cr, (vis->width - extents.width) / 2, 40);
    cairo_show_text(cr, kfn->title);
    
    // Artist
    cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
    cairo_set_font_size(cr, 14);
    cairo_text_extents(cr, kfn->artist, &extents);
    cairo_move_to(cr, (vis->width - extents.width) / 2, 70);
    cairo_show_text(cr, kfn->artist);
    
    // Find current line based on current word index
    int current_line = -1;
    for (int i = 0; i < kfn->line_count; i++) {
        if (current_word_idx >= kfn->lines[i].start_word_idx &&
            (i + 1 >= kfn->line_count || 
             current_word_idx < kfn->lines[i + 1].start_word_idx)) {
            current_line = i;
            break;
        }
    }
    
    if (current_line < 0 || current_line >= kfn->line_count || !kfn->lines) return;
    
    // Dynamic font size based on window height
    int font_size = (vis->height / 6);
    if (font_size < 24) font_size = 24;
    if (font_size > 60) font_size = 60;
    
    // Render current line with individual word highlighting
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, font_size);
    
    int line_start_word = kfn->lines[current_line].start_word_idx;
    int line_end_word = line_start_word + kfn->lines[current_line].word_count;
    
    // Bounds check
    if (line_start_word < 0) line_start_word = 0;
    if (line_end_word > kfn->word_count) line_end_word = kfn->word_count;
    
    // First pass: measure total width to center
    double total_width = 0;
    for (int w = line_start_word; w < line_end_word; w++) {
        if (w >= 0 && w < kfn->word_count && kfn->words[w]) {
            cairo_text_extents(cr, kfn->words[w], &extents);
            total_width += extents.width;
            if (w < line_end_word - 1) total_width += 10;  // Space between words
        }
    }
    
    double x_start = (vis->width - total_width) / 2;
    double y_pos = vis->height / 2 + 20;
    double x_pos = x_start;
    
    // Second pass: render each word with highlight if current
    for (int w = line_start_word; w < line_end_word; w++) {
        if (w < 0 || w >= kfn->word_count || !kfn->words[w]) continue;
        
        cairo_text_extents(cr, kfn->words[w], &extents);
        
        // Draw blue highlight if this is the current word
        if (w == current_word_idx) {
            cairo_set_source_rgba(cr, 0.2, 0.4, 1.0, 0.3);
            cairo_rectangle(cr, x_pos - 4, y_pos - extents.height - 4, 
                           extents.width + 8, extents.height + 8);
            cairo_fill(cr);
            
            // Blue border
            cairo_set_source_rgb(cr, 0.2, 0.6, 1.0);
            cairo_set_line_width(cr, 2);
            cairo_rectangle(cr, x_pos - 4, y_pos - extents.height - 4, 
                           extents.width + 8, extents.height + 8);
            cairo_stroke(cr);
            
            // White text for current word
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        } else if (w < current_word_idx) {
            // Past words - white
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        } else {
            // Future words - gray
            cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        }
        
        cairo_move_to(cr, x_pos, y_pos);
        cairo_show_text(cr, kfn->words[w]);
        
        x_pos += extents.width + 10;  // Move to next word position
    }
    
    // Next line (smaller, gray, centered)
    if (current_line + 1 < kfn->line_count && kfn->lines) {
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, font_size / 2);
        
        const char *next_text = kfn->lines[current_line + 1].display_text;
        cairo_text_extents(cr, next_text, &extents);
        cairo_move_to(cr, (vis->width - extents.width) / 2, vis->height / 2 + 80);
        cairo_show_text(cr, next_text);
    }
}
