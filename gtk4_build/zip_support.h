#ifndef ZIP_SUPPORT_H
#define ZIP_SUPPORT_H

#include <stdbool.h>

typedef struct {
    char audio_file[512];
    char cdg_file[512];
    bool has_audio;
    bool has_cdg;
} KaraokeZipContents;

// Extract karaoke files from ZIP to temporary directory
bool extract_karaoke_zip(const char *zip_path, KaraokeZipContents *contents);

// Clean up extracted temporary files
void cleanup_karaoke_temp_files(KaraokeZipContents *contents);

// Check if file is a ZIP
bool is_zip_file(const char *filename);

#endif // ZIP_SUPPORT_H
