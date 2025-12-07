#include "wad.h"
#include "miniz.h"
#include <stdio.h>
#include <string.h>

// Open WAD file (which is a ZIP archive)
bool wad_open(WadArchive *wad, const char *filename) {
    if (!wad || !filename) {
        fprintf(stderr, "Error: Invalid WAD parameters\n");
        return false;
    }
    
    // Copy filename for reference
    strncpy(wad->filename, filename, sizeof(wad->filename) - 1);
    wad->filename[sizeof(wad->filename) - 1] = '\0';
    
    // Allocate ZIP archive structure
    mz_zip_archive *zip = (mz_zip_archive *)malloc(sizeof(mz_zip_archive));
    if (!zip) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return false;
    }
    
    // Initialize archive
    memset(zip, 0, sizeof(mz_zip_archive));
    
    // Open the ZIP file
    if (!mz_zip_reader_init_file(zip, filename, 0)) {
        fprintf(stderr, "Error: Failed to open WAD file '%s': %s\n", 
                filename, mz_zip_get_error_string(mz_zip_get_last_error(zip)));
        free(zip);
        return false;
    }
    
    wad->zip_archive = (void *)zip;
    
    fprintf(stdout, "✓ WAD file opened: %s\n", filename);
    fprintf(stdout, "  Files in archive: %u\n", mz_zip_reader_get_num_files(zip));
    
    return true;
}

// Close WAD file
void wad_close(WadArchive *wad) {
    if (!wad || !wad->zip_archive) return;
    
    mz_zip_archive *zip = (mz_zip_archive *)wad->zip_archive;
    mz_zip_reader_end(zip);
    free(zip);
    wad->zip_archive = NULL;
    
    fprintf(stdout, "✓ WAD file closed: %s\n", wad->filename);
}

// Extract file from WAD
bool wad_extract_file(WadArchive *wad, const char *internal_path, WadFile *out_file) {
    if (!wad || !wad->zip_archive || !internal_path || !out_file) {
        fprintf(stderr, "Error: Invalid extract parameters\n");
        return false;
    }
    
    mz_zip_archive *zip = (mz_zip_archive *)wad->zip_archive;
    
    // Find file index
    int file_index = mz_zip_reader_locate_file(zip, internal_path, NULL, 0);
    if (file_index < 0) {
        fprintf(stderr, "Error: File '%s' not found in WAD\n", internal_path);
        return false;
    }
    
    // Get file info
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(zip, file_index, &file_stat)) {
        fprintf(stderr, "Error: Failed to read file stats for '%s'\n", internal_path);
        return false;
    }
    
    // Allocate buffer
    out_file->size = file_stat.m_uncomp_size;
    out_file->data = (char *)malloc(out_file->size);
    if (!out_file->data) {
        fprintf(stderr, "Error: Memory allocation failed for file '%s' (%zu bytes)\n", 
                internal_path, out_file->size);
        out_file->size = 0;
        return false;
    }
    
    // Extract file
    if (!mz_zip_reader_extract_to_mem(zip, file_index, out_file->data, 
                                       out_file->size, 0)) {
        fprintf(stderr, "Error: Failed to extract file '%s' from WAD\n", internal_path);
        free(out_file->data);
        out_file->data = NULL;
        out_file->size = 0;
        return false;
    }
    
    fprintf(stdout, "  Extracted: %s (%zu bytes)\n", internal_path, out_file->size);
    return true;
}

// Free extracted file
void wad_free_file(WadFile *file) {
    if (file && file->data) {
        free(file->data);
        file->data = NULL;
        file->size = 0;
    }
}

// Get number of files in WAD
int wad_get_file_count(WadArchive *wad) {
    if (!wad || !wad->zip_archive) return 0;
    
    mz_zip_archive *zip = (mz_zip_archive *)wad->zip_archive;
    return (int)mz_zip_reader_get_num_files(zip);
}

// Get filename at index
const char* wad_get_filename(WadArchive *wad, int index) {
    if (!wad || !wad->zip_archive) return NULL;
    
    mz_zip_archive *zip = (mz_zip_archive *)wad->zip_archive;
    mz_zip_archive_file_stat file_stat;
    
    if (!mz_zip_reader_file_stat(zip, index, &file_stat)) {
        return NULL;
    }
    
    return file_stat.m_filename;
}

// Check if file exists in WAD
bool wad_file_exists(WadArchive *wad, const char *internal_path) {
    if (!wad || !wad->zip_archive || !internal_path) return false;
    
    mz_zip_archive *zip = (mz_zip_archive *)wad->zip_archive;
    int file_index = mz_zip_reader_locate_file(zip, internal_path, NULL, 0);
    return file_index >= 0;
}

