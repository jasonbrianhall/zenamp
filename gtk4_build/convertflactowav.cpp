#include <FLAC/stream_decoder.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "convertflactowav.h"
#include "audio_player.h"
#include "vfs.h"

// Structure to hold decoder state and output data
struct FlacDecoderData {
    std::vector<uint8_t>* output_data;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint64_t total_samples;
    bool error_occurred;
    
    // For memory input
    const uint8_t* input_data;
    size_t input_size;
    size_t input_position;
};

// Write WAV header
static void writeWavHeader(std::vector<uint8_t>& wav_data, uint32_t sample_rate, 
                          uint8_t channels, uint8_t bits_per_sample, uint32_t data_size) {
    uint32_t bytes_per_sample = bits_per_sample / 8;
    uint32_t byte_rate = sample_rate * channels * bytes_per_sample;
    uint32_t block_align = channels * bytes_per_sample;
    uint32_t file_size = 36 + data_size;
    
    wav_data.resize(44 + data_size);
    uint8_t* header = wav_data.data();
    
    // RIFF header
    memcpy(header, "RIFF", 4);
    *((uint32_t*)(header + 4)) = file_size;
    memcpy(header + 8, "WAVE", 4);
    
    // fmt chunk
    memcpy(header + 12, "fmt ", 4);
    *((uint32_t*)(header + 16)) = 16; // chunk size
    *((uint16_t*)(header + 20)) = 1;  // PCM format
    *((uint16_t*)(header + 22)) = channels;
    *((uint32_t*)(header + 24)) = sample_rate;
    *((uint32_t*)(header + 28)) = byte_rate;
    *((uint16_t*)(header + 32)) = block_align;
    *((uint16_t*)(header + 34)) = bits_per_sample;
    
    // data chunk
    memcpy(header + 36, "data", 4);
    *((uint32_t*)(header + 40)) = data_size;
}

// FLAC decoder callbacks
static FLAC__StreamDecoderReadStatus flac_read_callback(
    const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data) {
    (void)decoder;
    FlacDecoderData* data = (FlacDecoderData*)client_data;
    
    if (data->input_position >= data->input_size) {
        *bytes = 0;
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }
    
    size_t remaining = data->input_size - data->input_position;
    size_t to_read = (*bytes < remaining) ? *bytes : remaining;
    
    memcpy(buffer, data->input_data + data->input_position, to_read);
    data->input_position += to_read;
    *bytes = to_read;
    
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderWriteStatus flac_write_callback(
    const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame, 
    const FLAC__int32* const buffer[], void* client_data) {
    (void)decoder;
    FlacDecoderData* data = (FlacDecoderData*)client_data;
    
    uint32_t samples = frame->header.blocksize;
    uint8_t channels = frame->header.channels;
    uint8_t bits_per_sample = frame->header.bits_per_sample;
    
    // Convert samples to bytes and append to output
    for (uint32_t i = 0; i < samples; i++) {
        for (uint8_t ch = 0; ch < channels; ch++) {
            FLAC__int32 sample = buffer[ch][i];
            
            if (bits_per_sample == 16) {
                int16_t sample16 = (int16_t)sample;
                data->output_data->push_back(sample16 & 0xFF);
                data->output_data->push_back((sample16 >> 8) & 0xFF);
            } else if (bits_per_sample == 24) {
                // Convert 24-bit to 16-bit for compatibility
                int16_t sample16 = (int16_t)(sample >> 8);
                data->output_data->push_back(sample16 & 0xFF);
                data->output_data->push_back((sample16 >> 8) & 0xFF);
            } else if (bits_per_sample == 8) {
                int8_t sample8 = (int8_t)sample;
                data->output_data->push_back(sample8);
            }
        }
    }
    
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flac_metadata_callback(const FLAC__StreamDecoder* decoder, 
                                  const FLAC__StreamMetadata* metadata, void* client_data) {
    (void)decoder;
    FlacDecoderData* data = (FlacDecoderData*)client_data;
    
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        data->sample_rate = metadata->data.stream_info.sample_rate;
        data->channels = metadata->data.stream_info.channels;
        data->bits_per_sample = metadata->data.stream_info.bits_per_sample;
        data->total_samples = metadata->data.stream_info.total_samples;
        
        printf("FLAC: %u Hz, %u channels, %u bits, %llu samples\n",
               data->sample_rate, data->channels, data->bits_per_sample, 
               (unsigned long long)data->total_samples);
    }
}

static void flac_error_callback(const FLAC__StreamDecoder* decoder, 
                               FLAC__StreamDecoderErrorStatus status, void* client_data) {
    (void)decoder;
    FlacDecoderData* data = (FlacDecoderData*)client_data;
    
    printf("FLAC decoder error: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
    data->error_occurred = true;
}

bool convertFlacToWavInMemory(const std::vector<uint8_t>& flac_data, std::vector<uint8_t>& wav_data) {
    FLAC__StreamDecoder* decoder = FLAC__stream_decoder_new();
    if (!decoder) {
        printf("Failed to create FLAC decoder\n");
        return false;
    }
    
    FlacDecoderData decoder_data;
    decoder_data.output_data = &wav_data;
    decoder_data.sample_rate = 0;
    decoder_data.channels = 0;
    decoder_data.bits_per_sample = 0;
    decoder_data.total_samples = 0;
    decoder_data.error_occurred = false;
    decoder_data.input_data = flac_data.data();
    decoder_data.input_size = flac_data.size();
    decoder_data.input_position = 0;
    
    // Clear output data
    wav_data.clear();
    
    // Set up decoder
    FLAC__stream_decoder_set_md5_checking(decoder, false);
    
    FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(
        decoder,
        flac_read_callback,
        nullptr, // seek callback
        nullptr, // tell callback
        nullptr, // length callback
        nullptr, // eof callback
        flac_write_callback,
        flac_metadata_callback,
        flac_error_callback,
        &decoder_data
    );
    
    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        printf("FLAC decoder init failed: %s\n", 
               FLAC__StreamDecoderInitStatusString[init_status]);
        FLAC__stream_decoder_delete(decoder);
        return false;
    }
    
    // Decode metadata first
    if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder)) {
        printf("Failed to process FLAC metadata\n");
        FLAC__stream_decoder_delete(decoder);
        return false;
    }
    
    // Reserve space for audio data (estimate)
    if (decoder_data.total_samples > 0) {
        size_t estimated_size = decoder_data.total_samples * decoder_data.channels * 2; // Assume 16-bit output
        wav_data.reserve(44 + estimated_size);
    }
    
    // Skip WAV header for now, decode audio data starting at position 44
    wav_data.resize(44);
    
    // Decode all audio data
    if (!FLAC__stream_decoder_process_until_end_of_stream(decoder)) {
        printf("Failed to decode FLAC audio data\n");
        FLAC__stream_decoder_delete(decoder);
        return false;
    }
    
    if (decoder_data.error_occurred) {
        FLAC__stream_decoder_delete(decoder);
        return false;
    }
    
    // Calculate actual audio data size
    uint32_t audio_data_size = wav_data.size() - 44;
    
    // Convert bits per sample for WAV compatibility (always output 16-bit)
    uint8_t output_bits_per_sample = 16;
    
    // Write proper WAV header
    writeWavHeader(wav_data, decoder_data.sample_rate, decoder_data.channels, 
                   output_bits_per_sample, audio_data_size);
    
    FLAC__stream_decoder_delete(decoder);
    
    printf("FLAC conversion complete: %u bytes of audio data\n", audio_data_size);
    return true;
}

bool convertFlacToWav(const char* flac_path, const char* wav_path) {
    // Read FLAC file into memory
    FILE* flac_file = fopen(flac_path, "rb");
    if (!flac_file) {
        printf("Cannot open FLAC file: %s\n", flac_path);
        return false;
    }
    
    fseek(flac_file, 0, SEEK_END);
    long flac_size = ftell(flac_file);
    fseek(flac_file, 0, SEEK_SET);
    
    std::vector<uint8_t> flac_data(flac_size);
    if (fread(flac_data.data(), 1, flac_size, flac_file) != (size_t)flac_size) {
        printf("Failed to read FLAC file\n");
        fclose(flac_file);
        return false;
    }
    fclose(flac_file);
    
    // Convert to WAV
    std::vector<uint8_t> wav_data;
    if (!convertFlacToWavInMemory(flac_data, wav_data)) {
        return false;
    }
    
    // Write WAV file
    FILE* wav_file = fopen(wav_path, "wb");
    if (!wav_file) {
        printf("Cannot create WAV file: %s\n", wav_path);
        return false;
    }
    
    if (fwrite(wav_data.data(), 1, wav_data.size(), wav_file) != wav_data.size()) {
        printf("Failed to write WAV file\n");
        fclose(wav_file);
        return false;
    }
    
    fclose(wav_file);
    return true;
}

bool convert_flac_to_wav(AudioPlayer *player, const char* filename) {
    // Check cache first
    const char* cached_file = get_cached_conversion(&player->conversion_cache, filename);
    if (cached_file) {
        strncpy(player->temp_wav_file, cached_file, sizeof(player->temp_wav_file) - 1);
        player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
        return true;
    }
    
    // Generate a unique virtual filename
    static int virtual_counter = 0;
    char virtual_filename[256];
    snprintf(virtual_filename, sizeof(virtual_filename), "virtual_flac_%d.wav", virtual_counter++);
    
    strncpy(player->temp_wav_file, virtual_filename, sizeof(player->temp_wav_file) - 1);
    player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
    
    printf("Converting FLAC to virtual WAV: %s -> %s\n", filename, virtual_filename);
    
    // Read FLAC file into memory
    FILE* flac_file = fopen(filename, "rb");
    if (!flac_file) {
        printf("Cannot open FLAC file: %s\n", filename);
        return false;
    }
    
    fseek(flac_file, 0, SEEK_END);
    long flac_size = ftell(flac_file);
    fseek(flac_file, 0, SEEK_SET);
    
    std::vector<uint8_t> flac_data(flac_size);
    if (fread(flac_data.data(), 1, flac_size, flac_file) != (size_t)flac_size) {
        printf("Failed to read FLAC file\n");
        fclose(flac_file);
        return false;
    }
    fclose(flac_file);
    
    // Convert FLAC to WAV in memory
    std::vector<uint8_t> wav_data;
    if (!convertFlacToWavInMemory(flac_data, wav_data)) {
        printf("FLAC to WAV conversion failed\n");
        return false;
    }
    
    // Create virtual file and write WAV data
    VirtualFile* vf = create_virtual_file(virtual_filename);
    if (!vf) {
        printf("Cannot create virtual WAV file: %s\n", virtual_filename);
        return false;
    }
    
    if (!virtual_file_write(vf, wav_data.data(), wav_data.size())) {
        printf("Failed to write virtual WAV file\n");
        return false;
    }
    
    // Add to cache after successful conversion
    add_to_conversion_cache(&player->conversion_cache, filename, virtual_filename);
    
    printf("FLAC conversion to virtual file complete\n");
    return true;
}
