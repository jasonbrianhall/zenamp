#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "aiff.h"
#include "audio_player.h"
#include "vfs.h"


// Helper function to read big-endian 32-bit integer
uint32_t read_be32(const void* data) {
    const unsigned char* bytes = (const unsigned char*)data;
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

// Helper function to read big-endian 16-bit integer
uint16_t read_be16(const void* data) {
    const unsigned char* bytes = (const unsigned char*)data;
    return (bytes[0] << 8) | bytes[1];
}

// Helper function to write little-endian 32-bit integer
void write_le32(void* data, uint32_t value) {
    unsigned char* bytes = (unsigned char*)data;
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;
}

// Helper function to write little-endian 16-bit integer
void write_le16(void* data, uint16_t value) {
    unsigned char* bytes = (unsigned char*)data;
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
}

// Read IEEE 754 80-bit extended precision float (sample rate)
double read_ieee754_extended(const void* data) {
    const unsigned char* bytes = (const unsigned char*)data;
    
    // Extract sign, exponent, and mantissa
    bool sign = (bytes[0] & 0x80) != 0;
    uint16_t exponent = ((bytes[0] & 0x7F) << 8) | bytes[1];
    
    // Handle special cases
    if (exponent == 0) return 0.0;
    if (exponent == 0x7FFF) return sign ? -INFINITY : INFINITY;
    
    // Extract mantissa (64-bit) - Fix: Check for integer bit
    uint64_t mantissa = 0;
    for (int i = 2; i < 10; i++) {
        mantissa = (mantissa << 8) | bytes[i];
    }
    
    // Quick check for common sample rates to avoid floating point errors
    // Fix: Updated the mantissa checks with correct values
    if (exponent == 0x400E) {  // 2^14 + bias
        // Check upper 32 bits of mantissa for common rates
        uint32_t upper_mantissa = (uint32_t)(mantissa >> 32);
        if (upper_mantissa == 0xAC440000UL) return 44100.0;
        if (upper_mantissa == 0xBB800000UL) return 48000.0;
        // Fix: Corrected value for 39062.5 Hz (uncommon, but used in some pro audio)
        if (upper_mantissa == 0x98968000UL) return 39062.5;
    }
    if (exponent == 0x400D) {  // Half the rates above
        uint32_t upper_mantissa = (uint32_t)(mantissa >> 32);
        if (upper_mantissa == 0xAC440000UL) return 22050.0;
        if (upper_mantissa == 0xBB800000UL) return 24000.0;
    }
    if (exponent == 0x400C) {  // Quarter rates
        uint32_t upper_mantissa = (uint32_t)(mantissa >> 32);
        if (upper_mantissa == 0xAC440000UL) return 11025.0;
    }
    if (exponent == 0x400F) {  // 2^15
        uint32_t upper_mantissa = (uint32_t)(mantissa >> 32);
        if (upper_mantissa == 0x80000000UL) return 65536.0;
    }
    
    // Fix: Improved general calculation for non-standard rates
    // In 80-bit extended precision, the integer bit is explicit (bit 63)
    // Check if the integer bit is set (it should be for normalized numbers)
    if ((mantissa & (1ULL << 63)) == 0) {
        // Denormalized number or zero
        if (mantissa == 0) return 0.0;
        // Handle denormalized numbers (rare in sample rates)
        exponent = 1;
    }
    
    // Convert mantissa to double precision
    // Remove the implicit integer bit and normalize to [1.0, 2.0)
    double normalized_mantissa = 1.0 + (double)(mantissa & 0x7FFFFFFFFFFFFFFFULL) / (1ULL << 63);
    
    // Apply exponent (bias is 16383 for 80-bit extended precision)
    double result = ldexp(normalized_mantissa, exponent - 16383);
    
    return sign ? -result : result;
}

// Convert big-endian 16-bit samples to host byte order
void convert_be16_samples(int16_t* samples, size_t count) {
    // AIFF samples are stored as big-endian in the file
    // WAV files expect little-endian samples
    // We need to swap the bytes for each 16-bit sample
    
    unsigned char* byte_ptr = (unsigned char*)samples;
    
    for (size_t i = 0; i < count; i++) {
        // Get the two bytes of the current sample
        unsigned char byte0 = byte_ptr[i * 2];     // High byte (big-endian)
        unsigned char byte1 = byte_ptr[i * 2 + 1]; // Low byte (big-endian)
        
        // Swap them for little-endian
        byte_ptr[i * 2] = byte1;     // Low byte first (little-endian)
        byte_ptr[i * 2 + 1] = byte0; // High byte second (little-endian)
    }
}

// Convert to stereo (2 channels)
bool convert_to_stereo(int16_t* input_samples, size_t sample_count, 
                      int input_channels, int16_t** output_samples, 
                      size_t* output_count) {
    if (input_channels == 2) {
        // Already stereo, just copy
        *output_samples = (int16_t*)malloc(sample_count * sizeof(int16_t));
        if (!*output_samples) return false;
        memcpy(*output_samples, input_samples, sample_count * sizeof(int16_t));
        *output_count = sample_count;
        return true;
    } else if (input_channels == 1) {
        // Mono to stereo: duplicate each sample
        size_t frames = sample_count / input_channels;
        *output_count = frames * 2;
        *output_samples = (int16_t*)malloc(*output_count * sizeof(int16_t));
        if (!*output_samples) return false;
        
        for (size_t i = 0; i < frames; i++) {
            (*output_samples)[i * 2] = input_samples[i];     // Left
            (*output_samples)[i * 2 + 1] = input_samples[i]; // Right
        }
        return true;
    } else {
        // Multi-channel to stereo: mix down to 2 channels
        size_t frames = sample_count / input_channels;
        *output_count = frames * 2;
        *output_samples = (int16_t*)malloc(*output_count * sizeof(int16_t));
        if (!*output_samples) return false;
        
        for (size_t i = 0; i < frames; i++) {
            int32_t left_sum = 0, right_sum = 0;
            
            // Simple downmix: take first channel as left, second as right
            // Mix remaining channels equally to both
            left_sum = input_samples[i * input_channels];
            right_sum = input_channels > 1 ? input_samples[i * input_channels + 1] : left_sum;
            
            // Mix additional channels
            for (int ch = 2; ch < input_channels; ch++) {
                int32_t sample = input_samples[i * input_channels + ch];
                left_sum += sample / 2;
                right_sum += sample / 2;
            }
            
            // Clamp to 16-bit range
            left_sum = left_sum > 32767 ? 32767 : (left_sum < -32768 ? -32768 : left_sum);
            right_sum = right_sum > 32767 ? 32767 : (right_sum < -32768 ? -32768 : right_sum);
            
            (*output_samples)[i * 2] = (int16_t)left_sum;
            (*output_samples)[i * 2 + 1] = (int16_t)right_sum;
        }
        return true;
    }
}

// Simple linear interpolation resampling
bool resample_audio(int16_t* input_samples, size_t input_count, 
                   double input_rate, int input_channels,
                   int16_t** output_samples, size_t* output_count) {
    if (fabs(input_rate - 44100.0) < 0.1) {
        // No resampling needed (within tolerance)
        *output_samples = (int16_t*)malloc(input_count * sizeof(int16_t));
        if (!*output_samples) return false;
        memcpy(*output_samples, input_samples, input_count * sizeof(int16_t));
        *output_count = input_count;
        return true;
    }
    
    double ratio = 44100.0 / input_rate;
    size_t input_frames = input_count / input_channels;
    size_t output_frames = (size_t)(input_frames * ratio);
    *output_count = output_frames * input_channels;
    
    *output_samples = (int16_t*)malloc(*output_count * sizeof(int16_t));
    if (!*output_samples) return false;
    
    for (size_t out_frame = 0; out_frame < output_frames; out_frame++) {
        double src_frame_exact = (double)out_frame / ratio;
        size_t src_frame = (size_t)src_frame_exact;
        double fraction = src_frame_exact - src_frame;
        
        for (int ch = 0; ch < input_channels; ch++) {
            int16_t sample1 = input_samples[src_frame * input_channels + ch];
            int16_t sample2 = (src_frame + 1 < input_frames) ? 
                             input_samples[(src_frame + 1) * input_channels + ch] : sample1;
            
            // Linear interpolation
            int32_t interpolated = (int32_t)(sample1 * (1.0 - fraction) + sample2 * fraction);
            (*output_samples)[out_frame * input_channels + ch] = (int16_t)interpolated;
        }
    }
    
    return true;
}

// Create WAV header with proper little-endian format - FIXED for 44.1kHz stereo
bool create_wav_header(AIFFWAVHeader* header, const AIFFInfo* aiff_info, uint32_t data_size) {
    if (!header || !aiff_info) return false;
    
    // Clear the header
    memset(header, 0, sizeof(AIFFWAVHeader));
    
    // RIFF header
    memcpy(header->riff, "RIFF", 4);
    write_le32(&header->file_length, data_size + 36); // 44 - 8 = 36
    memcpy(header->wave, "WAVE", 4);
    
    // Format chunk - FIXED: Always use 44.1kHz stereo
    memcpy(header->fmt, "fmt ", 4);
    write_le32(&header->fmt_length, 16);
    write_le16(&header->audio_format, 1); // PCM
    write_le16(&header->num_channels, 2); // Force stereo
    
    // FIXED: Always use 44.1kHz
    uint32_t sample_rate_int = 44100;
    write_le32(&header->sample_rate, sample_rate_int);
    
    // FIXED: Recalculate byte rate for stereo 44.1kHz
    uint32_t byte_rate = 44100 * 2 * 2; // 44.1kHz * 2 channels * 2 bytes per sample
    write_le32(&header->byte_rate, byte_rate);
    write_le16(&header->block_align, 2 * 2); // 2 channels * 2 bytes per sample
    write_le16(&header->bits_per_sample, 16);
    
    // Data chunk
    memcpy(header->data, "data", 4);
    write_le32(&header->data_length, data_size);
    
    // DEBUG: Print the values being written
    printf("DEBUG WAV Header: Sample Rate = %u Hz (forced to 44.1kHz stereo)\n", sample_rate_int);
    printf("DEBUG WAV Header: Byte Rate = %u, Channels = 2\n", byte_rate);
    
    return true;
}

// Parse AIFF file header and return format information
bool parse_aiff_header(FILE* file, AIFFInfo* info) {
    if (!file || !info) return false;
    
    // Initialize info structure
    memset(info, 0, sizeof(AIFFInfo));
    
    // Read FORM header (12 bytes)
    char form_header[12];
    if (fread(form_header, 1, 12, file) != 12) {
        return false;
    }
    
    // Verify AIFF format
    if (strncmp(form_header, "FORM", 4) != 0 || strncmp(form_header + 8, "AIFF", 4) != 0) {
        return false;
    }
    
    uint32_t form_size = read_be32(form_header + 4);
    
    // Read chunks
    long current_pos = 12; // After FORM header
    bool found_comm = false, found_ssnd = false;
    
    while (current_pos < (long)(form_size + 8) && (!found_comm || !found_ssnd)) {
        fseek(file, current_pos, SEEK_SET);
        
        char chunk_header[8];
        if (fread(chunk_header, 1, 8, file) != 8) {
            break;
        }
        
        uint32_t chunk_size = read_be32(chunk_header + 4);
        
        if (strncmp(chunk_header, "COMM", 4) == 0) {
            // Common chunk - contains audio format info
            char comm_data[18];
            if (fread(comm_data, 1, 18, file) != 18) {
                return false;
            }
            
            info->channels = read_be16(comm_data);
            info->sample_frames = read_be32(comm_data + 2);
            info->bits_per_sample = read_be16(comm_data + 6);
            info->sample_rate = read_ieee754_extended(comm_data + 8);
            
            if (info->sample_rate > 0) {
                info->duration = (double)info->sample_frames / info->sample_rate;
            }
            
            found_comm = true;
            
        } else if (strncmp(chunk_header, "SSND", 4) == 0) {
            // Sound data chunk
            char ssnd_header[8];
            if (fread(ssnd_header, 1, 8, file) != 8) {
                return false;
            }
            
            uint32_t offset = read_be32(ssnd_header);
            
            // Calculate actual audio data position
            info->data_offset = current_pos + 16 + offset; // chunk header + ssnd header + offset
            info->data_size = chunk_size - 8 - offset; // chunk size - ssnd header - offset
            
            found_ssnd = true;
        }
        
        // Move to next chunk (chunks are word-aligned)
        current_pos += 8 + chunk_size;
        if (chunk_size % 2) current_pos++; // Pad to word boundary
    }
    
    // Validate that we found required chunks
    return found_comm && found_ssnd && info->channels > 0 && info->sample_rate > 0;
}

// Load audio samples from AIFF file
bool load_aiff_samples(FILE* file, const AIFFInfo* info, int16_t** samples, size_t* sample_count) {
    if (!file || !info || !samples || !sample_count) return false;
    
    // Currently only support 16-bit samples
    if (info->bits_per_sample != 16) {
        printf("Unsupported AIFF sample size: %d bits (only 16-bit supported)\n", info->bits_per_sample);
        return false;
    }
    
    // Seek to audio data
    if (fseek(file, info->data_offset, SEEK_SET) != 0) {
        return false;
    }
    
    // Calculate number of samples
    size_t total_samples = info->data_size / 2; // 16-bit = 2 bytes per sample
    
    // Allocate memory for samples
    int16_t* audio_data = (int16_t*)malloc(info->data_size);
    if (!audio_data) {
        return false;
    }
    
    // Read audio data
    if (fread(audio_data, 1, info->data_size, file) != info->data_size) {
        free(audio_data);
        return false;
    }
    
    // Convert big-endian samples to host byte order
    convert_be16_samples(audio_data, total_samples);
    
    *samples = audio_data;
    *sample_count = total_samples;
    
    return true;
}

bool convert_aiff_to_wav(AudioPlayer *player, const char* filename) {
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
    snprintf(virtual_filename, sizeof(virtual_filename), "virtual_aiff_%d.wav", virtual_counter++);
    
    strncpy(player->temp_wav_file, virtual_filename, sizeof(player->temp_wav_file) - 1);
    player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
    
    printf("Converting AIFF to virtual WAV: %s -> %s\n", filename, virtual_filename);
    
    // Open AIFF file
    FILE* aiff_file = fopen(filename, "rb");
    if (!aiff_file) {
        printf("Cannot open AIFF file: %s\n", filename);
        return false;
    }
    
    // Parse AIFF header
    AIFFInfo aiff_info;
    if (!parse_aiff_header(aiff_file, &aiff_info)) {
        printf("Failed to parse AIFF header\n");
        fclose(aiff_file);
        return false;
    }
    
    printf("AIFF: %.1f Hz, %d channels, %d bits, %.2f seconds\n", 
           aiff_info.sample_rate, aiff_info.channels, 
           aiff_info.bits_per_sample, aiff_info.duration);
    
    // Load AIFF samples
    int16_t* samples = NULL;
    size_t sample_count = 0;
    if (!load_aiff_samples(aiff_file, &aiff_info, &samples, &sample_count)) {
        printf("Failed to load AIFF samples\n");
        fclose(aiff_file);
        return false;
    }
    fclose(aiff_file);
    
    // Step 1: Resample to 44100 Hz if needed
    int16_t* resampled_samples = NULL;
    size_t resampled_count = 0;
    printf("Resampling from %.1f Hz to 44100 Hz...\n", aiff_info.sample_rate);
    if (!resample_audio(samples, sample_count, aiff_info.sample_rate, 
                       aiff_info.channels, &resampled_samples, &resampled_count)) {
        printf("Failed to resample audio\n");
        free(samples);
        return false;
    }
    free(samples); // Free original samples
    
    // Step 2: Convert to stereo
    int16_t* stereo_samples = NULL;
    size_t stereo_count = 0;
    printf("Converting from %d channels to stereo...\n", aiff_info.channels);
    if (!convert_to_stereo(resampled_samples, resampled_count, 
                          aiff_info.channels, &stereo_samples, &stereo_count)) {
        printf("Failed to convert to stereo\n");
        free(resampled_samples);
        return false;
    }
    free(resampled_samples); // Free resampled samples
    
    // Calculate WAV data size for stereo 44.1kHz output
    uint32_t wav_data_size = stereo_count * sizeof(int16_t);
    
    // Create WAV header with forced 44.1kHz stereo format
    AIFFWAVHeader wav_header;
    if (!create_wav_header(&wav_header, &aiff_info, wav_data_size)) {
        printf("Failed to create WAV header\n");
        free(stereo_samples);
        return false;
    }
    
    // Create virtual file and write WAV data
    VirtualFile* vf = create_virtual_file(virtual_filename);
    if (!vf) {
        printf("Cannot create virtual WAV file: %s\n", virtual_filename);
        free(stereo_samples);
        return false;
    }
    
    // Write WAV header
    if (!virtual_file_write(vf, &wav_header, sizeof(wav_header))) {
        printf("Failed to write WAV header to virtual file\n");
        free(stereo_samples);
        return false;
    }
    
    // Write audio data (stereo samples at 44.1kHz)
    if (!virtual_file_write(vf, stereo_samples, wav_data_size)) {
        printf("Failed to write audio data to virtual file\n");
        free(stereo_samples);
        return false;
    }
    
    free(stereo_samples);
    
    // Add to cache after successful conversion
    add_to_conversion_cache(&player->conversion_cache, filename, virtual_filename);
    
    printf("AIFF conversion to virtual file complete: 44.1kHz stereo WAV\n");
    return true;
}
