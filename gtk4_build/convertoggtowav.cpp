#include <vorbis/vorbisfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <cstdint>
#include "convertoggtowav.h"
#include "audio_player.h"
#include "vfs.h"

// Memory-based OGG reading callbacks
struct MemoryOggData {
    const uint8_t* data;
    size_t size;
    size_t pos;
};

static size_t memory_read_func(void* ptr, size_t size, size_t nmemb, void* datasource) {
    MemoryOggData* mem_data = (MemoryOggData*)datasource;
    size_t bytes_to_read = size * nmemb;
    size_t bytes_available = mem_data->size - mem_data->pos;
    
    if (bytes_to_read > bytes_available) {
        bytes_to_read = bytes_available;
    }
    
    if (bytes_to_read > 0) {
        memcpy(ptr, mem_data->data + mem_data->pos, bytes_to_read);
        mem_data->pos += bytes_to_read;
    }
    
    return bytes_to_read / size;
}

static int memory_seek_func(void* datasource, ogg_int64_t offset, int whence) {
    MemoryOggData* mem_data = (MemoryOggData*)datasource;
    
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

static int memory_close_func(void* datasource) {
    (void)datasource;
    return 0;
}

static long memory_tell_func(void* datasource) {
    MemoryOggData* mem_data = (MemoryOggData*)datasource;
    return mem_data->pos;
}

bool convertOggToWavInMemory(const std::vector<uint8_t>& ogg_data, std::vector<uint8_t>& wav_data) {
    // Set up memory-based OGG reading
    MemoryOggData mem_data;
    mem_data.data = ogg_data.data();
    mem_data.size = ogg_data.size();
    mem_data.pos = 0;
    
    ov_callbacks callbacks;
    callbacks.read_func = memory_read_func;
    callbacks.seek_func = memory_seek_func;
    callbacks.close_func = memory_close_func;
    callbacks.tell_func = memory_tell_func;
    
    OggVorbis_File vf;
    if (ov_open_callbacks(&mem_data, &vf, NULL, 0, callbacks) < 0) {
        printf("Invalid OGG Vorbis data in memory\n");
        return false;
    }
    
    vorbis_info* vi = ov_info(&vf, -1);
    long total_samples = ov_pcm_total(&vf, -1);
    
    printf("OGG (memory): %ld Hz, %d channels, %ld samples\n", vi->rate, vi->channels, total_samples);
    
    // Calculate WAV file size
    int sample_rate = vi->rate;
    int channels = vi->channels;
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
    char buffer[4096];
    int current_section;
    long bytes_read;
    
    while ((bytes_read = ov_read(&vf, buffer, sizeof(buffer), 0, 2, 1, &current_section)) > 0) {
        append_bytes(buffer, bytes_read);
    }
    
    ov_clear(&vf);
    
    printf("OGG to WAV memory conversion complete (%zu bytes)\n", wav_data.size());
    return true;
}

bool convertOggToWav(const char* ogg_filename, const char* wav_filename) {
    FILE* ogg_file = fopen(ogg_filename, "rb");
    if (!ogg_file) {
        printf("Cannot open OGG file: %s\n", ogg_filename);
        return false;
    }
    
    OggVorbis_File vf;
    if (ov_open_callbacks(ogg_file, &vf, NULL, 0, OV_CALLBACKS_DEFAULT) < 0) {
        printf("Invalid OGG Vorbis file\n");
        fclose(ogg_file);
        return false;
    }
    
    vorbis_info* vi = ov_info(&vf, -1);
    long total_samples = ov_pcm_total(&vf, -1);
    
    printf("OGG: %ld Hz, %d channels, %ld samples\n", vi->rate, vi->channels, total_samples);
    
    // Create WAV file
    FILE* wav_file = fopen(wav_filename, "wb");
    if (!wav_file) {
        printf("Cannot create WAV file: %s\n", wav_filename);
        ov_clear(&vf);
        return false;
    }
    
    // Write WAV header
    int sample_rate = vi->rate;
    int channels = vi->channels;
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
    char buffer[4096];
    int current_section;
    long bytes_read;
    
    while ((bytes_read = ov_read(&vf, buffer, sizeof(buffer), 0, 2, 1, &current_section)) > 0) {
        fwrite(buffer, 1, bytes_read, wav_file);
    }
    
    fclose(wav_file);
    ov_clear(&vf);
    
    printf("OGG conversion complete\n");
    return true;
}

bool convert_ogg_to_wav(AudioPlayer *player, const char* filename) {
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
    snprintf(virtual_filename, sizeof(virtual_filename), "virtual_ogg_%d.wav", virtual_counter++);
    
    strncpy(player->temp_wav_file, virtual_filename, sizeof(player->temp_wav_file) - 1);
    player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
    
    printf("Converting OGG to virtual WAV: %s -> %s\n", filename, virtual_filename);
    
    // Read OGG file into memory
    FILE* ogg_file = fopen(filename, "rb");
    if (!ogg_file) {
        printf("Cannot open OGG file: %s\n", filename);
        return false;
    }
    
    fseek(ogg_file, 0, SEEK_END);
    long ogg_size = ftell(ogg_file);
    fseek(ogg_file, 0, SEEK_SET);
    
    std::vector<uint8_t> ogg_data(ogg_size);
    if (fread(ogg_data.data(), 1, ogg_size, ogg_file) != (size_t)ogg_size) {
        printf("Failed to read OGG file\n");
        fclose(ogg_file);
        return false;
    }
    fclose(ogg_file);
    
    // Convert OGG to WAV in memory
    std::vector<uint8_t> wav_data;
    if (!convertOggToWavInMemory(ogg_data, wav_data)) {
        printf("OGG to WAV conversion failed\n");
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
    
    printf("OGG conversion to virtual file complete\n");
    return true;
}
