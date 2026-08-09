#ifndef AIFF_H
#define AIFF_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "audio_player.h"

// AIFF file info structure
typedef struct {
    uint16_t channels;
    uint32_t sample_frames;
    uint16_t bits_per_sample;
    double sample_rate;
    long data_offset;
    uint32_t data_size;
    double duration;
} AIFFInfo;

// WAV header structure for AIFF conversion (renamed to avoid conflict)
typedef struct {
    char riff[4];
    uint32_t file_length;
    char wave[4];
    char fmt[4];
    uint32_t fmt_length;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_length;
} __attribute__((packed)) AIFFWAVHeader;

// Main conversion function
bool convert_aiff_to_wav(AudioPlayer *player, const char* filename);

// AIFF parsing functions
bool parse_aiff_header(FILE* file, AIFFInfo* info);
bool load_aiff_samples(FILE* file, const AIFFInfo* info, int16_t** samples, size_t* sample_count);

// WAV creation functions
bool create_wav_header(AIFFWAVHeader* header, const AIFFInfo* aiff_info, uint32_t data_size);

// Big-endian reading utility functions
uint32_t read_be32(const void* data);
uint16_t read_be16(const void* data);
double read_ieee754_extended(const void* data);

// Little-endian writing utility functions
void write_le32(void* data, uint32_t value);
void write_le16(void* data, uint16_t value);

// Sample conversion functions
void convert_be16_samples(int16_t* samples, size_t count);

// Audio format conversion functions
bool convert_to_stereo(int16_t* input_samples, size_t sample_count, 
                      int input_channels, int16_t** output_samples, 
                      size_t* output_count);
bool resample_audio(int16_t* input_samples, size_t input_count, 
                   double input_rate, int input_channels,
                   int16_t** output_samples, size_t* output_count);

#endif // AIFF_H
