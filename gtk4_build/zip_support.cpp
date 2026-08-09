#include "zip_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

// We'll use miniz - a single-file public domain ZIP library
// Add miniz.h and miniz.c to your project from: https://github.com/richgel999/miniz
#include "miniz.h"

static char temp_extract_dir[512] = {0};

static void get_temp_directory(char *buffer, size_t size) {
#ifdef _WIN32
    GetTempPath((DWORD)size, buffer);
    strcat(buffer, "audioplayer_karaoke\\");
#else
    snprintf(buffer, size, "/tmp/audioplayer_karaoke/");
#endif
}

static void ensure_temp_directory(void) {
    if (temp_extract_dir[0] == '\0') {
        get_temp_directory(temp_extract_dir, sizeof(temp_extract_dir));
        mkdir(temp_extract_dir, 0755);
    }
}

static bool has_extension(const char *filename, const char *ext) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return false;
    
    char lower_ext[16];
    strncpy(lower_ext, dot, sizeof(lower_ext) - 1);
    lower_ext[sizeof(lower_ext) - 1] = '\0';
    
    for (char *p = lower_ext; *p; p++) {
        *p = tolower(*p);
    }
    
    return strcmp(lower_ext, ext) == 0;
}

static bool is_audio_file(const char *filename) {
    return has_extension(filename, ".mp3") ||
           has_extension(filename, ".ogg") ||
           has_extension(filename, ".flac") ||
           has_extension(filename, ".wav") ||
           has_extension(filename, ".m4a") ||
           has_extension(filename, ".opus");
}

bool is_zip_file(const char *filename) {
    return has_extension(filename, ".zip");
}

bool extract_karaoke_zip(const char *zip_path, KaraokeZipContents *contents) {
    if (!zip_path || !contents) return false;
    
    memset(contents, 0, sizeof(KaraokeZipContents));
    ensure_temp_directory();
    
    // Open ZIP archive
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    
    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) {
        printf("Failed to open ZIP file: %s\n", zip_path);
        return false;
    }
    
    // Get number of files in archive
    int num_files = (int)mz_zip_reader_get_num_files(&zip);
    printf("ZIP contains %d files\n", num_files);
    
    // Extract relevant files
    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) {
            continue;
        }
        
        // Skip directories
        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            continue;
        }
        
        // Get just the filename (no path)
        const char *filename = file_stat.m_filename;
        const char *last_slash = strrchr(filename, '/');
        if (last_slash) {
            filename = last_slash + 1;
        }
        
        #ifdef _WIN32
        const char *last_backslash = strrchr(filename, '\\');
        if (last_backslash) {
            filename = last_backslash + 1;
        }
        #endif
        
        // Check if it's a file we want
        bool is_cdg = has_extension(filename, ".cdg");
        bool is_audio = is_audio_file(filename);
        
        if (!is_cdg && !is_audio) {
            continue;
        }
        
        // Construct output path
        char output_path[1024];
        snprintf(output_path, sizeof(output_path), "%s%s", temp_extract_dir, filename);
        
        printf("Extracting: %s -> %s\n", file_stat.m_filename, output_path);
        
        // Extract file
        if (!mz_zip_reader_extract_to_file(&zip, i, output_path, 0)) {
            printf("Failed to extract: %s\n", file_stat.m_filename);
            continue;
        }
        
        // Store path
        if (is_cdg && !contents->has_cdg) {
            strncpy(contents->cdg_file, output_path, sizeof(contents->cdg_file) - 1);
            contents->has_cdg = true;
        } else if (is_audio && !contents->has_audio) {
            strncpy(contents->audio_file, output_path, sizeof(contents->audio_file) - 1);
            contents->has_audio = true;
        }
        
        // If we have both, we can stop
        if (contents->has_cdg && contents->has_audio) {
            break;
        }
    }
    
    mz_zip_reader_end(&zip);
    
    if (!contents->has_cdg) {
        printf("No .cdg file found in ZIP\n");
        cleanup_karaoke_temp_files(contents);
        return false;
    }
    
    if (!contents->has_audio) {
        printf("No audio file found in ZIP\n");
        cleanup_karaoke_temp_files(contents);
        return false;
    }
    
    printf("Successfully extracted karaoke files:\n");
    printf("  CDG: %s\n", contents->cdg_file);
    printf("  Audio: %s\n", contents->audio_file);
    
    return true;
}

void cleanup_karaoke_temp_files(KaraokeZipContents *contents) {
    if (!contents) return;
    
    if (contents->has_cdg && contents->cdg_file[0]) {
        remove(contents->cdg_file);
        contents->cdg_file[0] = '\0';
    }
    
    if (contents->has_audio && contents->audio_file[0]) {
        remove(contents->audio_file);
        contents->audio_file[0] = '\0';
    }
    
    contents->has_cdg = false;
    contents->has_audio = false;
}
