#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "wav_converter.h"

// Initialize WAV converter
WAVConverter* wav_converter_init(const char* filename, 
                                 uint32_t sample_rate, 
                                 uint16_t num_channels) {
    // Validate inputs
    if (!filename || sample_rate == 0 || num_channels == 0 || num_channels > 2) {
        return NULL;
    }

    // Allocate converter
    WAVConverter* converter = calloc(1, sizeof(WAVConverter));
    if (!converter) {
        return NULL;
    }

    // Open output file
    converter->output_file = fopen(filename, "wb");
    if (!converter->output_file) {
        free(converter);
        return NULL;
    }

    // Prepare WAV header
    strncpy(converter->header.riff_header, "RIFF", 4);
    strncpy(converter->header.wave_header, "WAVE", 4);
    strncpy(converter->header.fmt_header, "fmt ", 4);
    strncpy(converter->header.data_header, "data", 4);

    // Set header values
    converter->header.fmt_chunk_size = 16;
    converter->header.audio_format = 1;  // PCM
    converter->header.num_channels = num_channels;
    converter->header.sample_rate = sample_rate;
    converter->header.bits_per_sample = 16;
    converter->header.block_align = num_channels * 2;  // 2 bytes per sample
    converter->header.byte_rate = sample_rate * num_channels * 2;

    // Write initial header (will be updated later)
    if (fwrite(&converter->header, sizeof(WAVHeader), 1, converter->output_file) != 1) {
        fclose(converter->output_file);
        free(converter);
        return NULL;
    }

    // Store data start position for later update
    converter->data_start_pos = sizeof(WAVHeader);
    converter->header_written = true;
    converter->total_samples = 0;

    return converter;
}

// Write audio samples to WAV file
bool wav_converter_write(WAVConverter* converter, 
                         const int16_t* samples, 
                         size_t num_samples) {
    if (!converter || !samples || num_samples == 0) {
        return false;
    }

    // Write samples
    size_t written = fwrite(samples, sizeof(int16_t), num_samples, converter->output_file);
    if (written != num_samples) {
        return false;
    }

    converter->total_samples += num_samples;
    return true;
}

// Finalize and close WAV file
bool wav_converter_finish(WAVConverter* converter) {
    if (!converter || !converter->output_file) {
        return false;
    }

    // Update WAV header with final file sizes
    size_t total_data_size = converter->total_samples * sizeof(int16_t);
    size_t file_size = total_data_size + sizeof(WAVHeader) - 8;

    // Seek to wav_size field and update
    long wav_size_offset = offsetof(WAVHeader, wav_size);
    fseek(converter->output_file, wav_size_offset, SEEK_SET);
    fwrite(&file_size, sizeof(uint32_t), 1, converter->output_file);
    
    // Seek to data_size field and update
    long data_size_offset = offsetof(WAVHeader, data_size);
    fseek(converter->output_file, data_size_offset, SEEK_SET);
    fwrite(&total_data_size, sizeof(uint32_t), 1, converter->output_file);

    return true;
}

// Free converter resources
void wav_converter_free(WAVConverter* converter) {
    if (!converter) {
        return;
    }

    if (converter->output_file) {
        fclose(converter->output_file);
    }

    free(converter);
}
