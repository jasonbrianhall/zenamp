#include <opus/opusfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <cstdint>
#include "convertopustowav.h"
#include "audio_player.h"
#include "vfs.h"

// Memory-based Opus reading callbacks
struct MemoryOpusData {
    const uint8_t* data;
    size_t size;
    size_t pos;
};

static int memory_opus_read_func(void* stream, unsigned char* ptr, int nbytes) {
    MemoryOpusData* mem_data = (MemoryOpusData*)stream;
    size_t bytes_to_read = nbytes;
    size_t bytes_available = mem_data->size - mem_data->pos;
    
    if (bytes_to_read > bytes_available) {
        bytes_to_read = bytes_available;
    }
    
    if (bytes_to_read > 0) {
        memcpy(ptr, mem_data->data + mem_data->pos, bytes_to_read);
        mem_data->pos += bytes_to_read;
    }
    
    return (int)bytes_to_read;
}

static int memory_opus_seek_func(void* stream, opus_int64 offset, int whence) {
    MemoryOpusData* mem_data = (MemoryOpusData*)stream;
    
    switch (whence) {
        case SEEK_SET:
            mem_data->pos = offset;
            break;
        case SEEK_CUR:
            mem_data->pos += offset;
            break;
        case SEEK_END:
            mem_data->pos = mem_data->size + offset;
            break;
        default:
            return -1;
    }
    
    if (mem_data->pos > mem_data->size) {
        mem_data->pos = mem_data->size;
    }
    
    return 0;
}

static opus_int64 memory_opus_tell_func(void* stream) {
    MemoryOpusData* mem_data = (MemoryOpusData*)stream;
    return mem_data->pos;
}

static int memory_opus_close_func(void* stream) {
    (void)stream;
    return 0;
}

bool convertOpusToWavInMemory(const std::vector<uint8_t>& opus_data, std::vector<uint8_t>& wav_data) {
    // Set up memory-based Opus reading
    MemoryOpusData mem_data;
    mem_data.data = opus_data.data();
    mem_data.size = opus_data.size();
    mem_data.pos = 0;
    
    OpusFileCallbacks callbacks;
    callbacks.read = memory_opus_read_func;
    callbacks.seek = memory_opus_seek_func;
    callbacks.tell = memory_opus_tell_func;
    callbacks.close = memory_opus_close_func;
    
    int error;
    OggOpusFile* of = op_open_callbacks(&mem_data, &callbacks, NULL, 0, &error);
    if (!of) {
        printf("Invalid Opus data in memory (error: %d)\n", error);
        return false;
    }
    
    // Get Opus info
    const OpusHead* head = op_head(of, -1);
    if (!head) {
        printf("Failed to get Opus head info\n");
        op_free(of);
        return false;
    }
    
    ogg_int64_t total_samples = op_pcm_total(of, -1);
    if (total_samples < 0) {
        printf("Failed to get Opus sample count\n");
        op_free(of);
        return false;
    }
    
    printf("Opus (memory): %d Hz, %d channels, %lld samples\n", 
           48000, head->channel_count, total_samples);
    
    // Opus always decodes to 48kHz, but we can downsample if needed
    int sample_rate = 48000;
    int channels = head->channel_count;
    int bits_per_sample = 16;
    long data_size = total_samples * channels * (bits_per_sample / 8);
    long file_size = data_size + 36;
    
    // Prepare WAV data vector
    wav_data.clear();
    wav_data.reserve(44 + data_size); // WAV header + audio data
    
    // Helper function to append data to vector
    auto append_bytes = [&wav_data](const void* data, size_t size) {
        const uint8_t* bytes = (const uint8_t*)data;
        wav_data.insert(wav_data.end(), bytes, bytes + size);
    };
    
    // Write WAV header to memory
    append_bytes("RIFF", 4);
    append_bytes(&file_size, 4);
    append_bytes("WAVE", 4);
    
    // fmt chunk
    append_bytes("fmt ", 4);
    int fmt_chunk_size = 16;
    short audio_format = 1; // PCM
    int byte_rate = sample_rate * channels * bits_per_sample / 8;
    short block_align = channels * bits_per_sample / 8;
    
    append_bytes(&fmt_chunk_size, 4);
    append_bytes(&audio_format, 2);
    append_bytes(&channels, 2);
    append_bytes(&sample_rate, 4);
    append_bytes(&byte_rate, 4);
    append_bytes(&block_align, 2);
    append_bytes(&bits_per_sample, 2);
    
    // data chunk
    append_bytes("data", 4);
    append_bytes(&data_size, 4);
    
    // Convert audio data
    opus_int16 buffer[5760 * 8]; // 120ms buffer for up to 8 channels
    int samples_read;
    
    while ((samples_read = op_read(of, buffer, sizeof(buffer)/sizeof(opus_int16), NULL)) > 0) {
        // samples_read is per channel, so total bytes = samples_read * channels * 2
        size_t bytes_to_write = samples_read * channels * sizeof(opus_int16);
        append_bytes(buffer, bytes_to_write);
    }
    
    if (samples_read < 0) {
        printf("Error reading Opus data: %d\n", samples_read);
        op_free(of);
        return false;
    }
    
    op_free(of);
    
    printf("Opus to WAV memory conversion complete (%zu bytes)\n", wav_data.size());
    return true;
}

bool convertOpusToWav(const char* opus_filename, const char* wav_filename) {
    int error;
    OggOpusFile* of = op_open_file(opus_filename, &error);
    if (!of) {
        printf("Cannot open Opus file: %s (error: %d)\n", opus_filename, error);
        return false;
    }
    
    // Get Opus info
    const OpusHead* head = op_head(of, -1);
    if (!head) {
        printf("Failed to get Opus head info\n");
        op_free(of);
        return false;
    }
    
    ogg_int64_t total_samples = op_pcm_total(of, -1);
    if (total_samples < 0) {
        printf("Failed to get Opus sample count\n");
        op_free(of);
        return false;
    }
    
    printf("Opus: %d Hz, %d channels, %lld samples\n", 
           48000, head->channel_count, total_samples);
    
    // Create WAV file
    FILE* wav_file = fopen(wav_filename, "wb");
    if (!wav_file) {
        printf("Cannot create WAV file: %s\n", wav_filename);
        op_free(of);
        return false;
    }
    
    // Write WAV header
    int sample_rate = 48000;
    int channels = head->channel_count;
    int bits_per_sample = 16;
    long data_size = total_samples * channels * (bits_per_sample / 8);
    long file_size = data_size + 36;
    
    // RIFF header
    fwrite("RIFF", 1, 4, wav_file);
    fwrite(&file_size, 4, 1, wav_file);
    fwrite("WAVE", 1, 4, wav_file);
    
    // fmt chunk
    fwrite("fmt ", 1, 4, wav_file);
    int fmt_chunk_size = 16;
    short audio_format = 1; // PCM
    int byte_rate = sample_rate * channels * bits_per_sample / 8;
    short block_align = channels * bits_per_sample / 8;
    
    fwrite(&fmt_chunk_size, 4, 1, wav_file);
    fwrite(&audio_format, 2, 1, wav_file);
    fwrite(&channels, 2, 1, wav_file);
    fwrite(&sample_rate, 4, 1, wav_file);
    fwrite(&byte_rate, 4, 1, wav_file);
    fwrite(&block_align, 2, 1, wav_file);
    fwrite(&bits_per_sample, 2, 1, wav_file);
    
    // data chunk
    fwrite("data", 1, 4, wav_file);
    fwrite(&data_size, 4, 1, wav_file);
    
    // Convert audio data
    opus_int16 buffer[5760 * 8]; // 120ms buffer for up to 8 channels
    int samples_read;
    
    while ((samples_read = op_read(of, buffer, sizeof(buffer)/sizeof(opus_int16), NULL)) > 0) {
        // samples_read is per channel, so total bytes = samples_read * channels * 2
        size_t bytes_to_write = samples_read * channels * sizeof(opus_int16);
        fwrite(buffer, 1, bytes_to_write, wav_file);
    }
    
    if (samples_read < 0) {
        printf("Error reading Opus data: %d\n", samples_read);
        fclose(wav_file);
        op_free(of);
        return false;
    }
    
    fclose(wav_file);
    op_free(of);
    
    printf("Opus conversion complete\n");
    return true;
}

bool convert_opus_to_wav(AudioPlayer *player, const char* filename) {
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
    snprintf(virtual_filename, sizeof(virtual_filename), "virtual_opus_%d.wav", virtual_counter++);
    
    strncpy(player->temp_wav_file, virtual_filename, sizeof(player->temp_wav_file) - 1);
    player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
    
    printf("Converting Opus to virtual WAV: %s -> %s\n", filename, virtual_filename);
    
    // Read Opus file into memory
    FILE* opus_file = fopen(filename, "rb");
    if (!opus_file) {
        printf("Cannot open Opus file: %s\n", filename);
        return false;
    }
    
    fseek(opus_file, 0, SEEK_END);
    long opus_size = ftell(opus_file);
    fseek(opus_file, 0, SEEK_SET);
    
    std::vector<uint8_t> opus_data(opus_size);
    if (fread(opus_data.data(), 1, opus_size, opus_file) != (size_t)opus_size) {
        printf("Failed to read Opus file\n");
        fclose(opus_file);
        return false;
    }
    fclose(opus_file);
    
    // Convert Opus to WAV in memory
    std::vector<uint8_t> wav_data;
    if (!convertOpusToWavInMemory(opus_data, wav_data)) {
        printf("Opus to WAV conversion failed\n");
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
    
    printf("Opus conversion to virtual file complete\n");
    return true;
}
