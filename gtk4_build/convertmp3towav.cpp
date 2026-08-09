#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include "audio_player.h"
#include "vfs.h"

// Function to read ID3v1 tag (last 128 bytes of MP3)
bool read_id3v1_tag(const std::vector<uint8_t>& mp3_data, AudioMetadata& metadata) {
    if (mp3_data.size() < 128) return false;
    
    const uint8_t* tag_data = mp3_data.data() + mp3_data.size() - 128;
    
    // Check for "TAG" identifier
    if (memcmp(tag_data, "TAG", 3) != 0) return false;
    
    // Extract fields (trim null bytes and spaces)
    auto trim_string = [](const char* data, size_t len) -> std::string {
        std::string result(data, len);
        size_t end = result.find_last_not_of('\0');
        if (end != std::string::npos) {
            result = result.substr(0, end + 1);
        }
        // Trim trailing spaces
        end = result.find_last_not_of(' ');
        if (end != std::string::npos) {
            result = result.substr(0, end + 1);
        }
        return result;
    };
    
    metadata.title = trim_string((const char*)(tag_data + 3), 30);
    metadata.artist = trim_string((const char*)(tag_data + 33), 30);
    metadata.album = trim_string((const char*)(tag_data + 63), 30);
    metadata.year = trim_string((const char*)(tag_data + 93), 4);
    metadata.comment = trim_string((const char*)(tag_data + 97), 28);
    
    // Track number (ID3v1.1)
    if (tag_data[125] == 0 && tag_data[126] != 0) {
        metadata.track_number = tag_data[126];
    }
    
    // Genre
    uint8_t genre_id = tag_data[127];
    if (genre_id < 192) { // Standard genres go up to 191
        const char* genres[] = {
            "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
            "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
            "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
            "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient",
            "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical",
            "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
            "Alternative Rock", "Bass", "Soul", "Punk", "Space", "Meditative",
            "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave",
            "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream",
            "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40"
        };
        if (genre_id < sizeof(genres)/sizeof(genres[0])) {
            metadata.genre = genres[genre_id];
        }
    }
    
    return true;
}

// Basic ID3v2 header parsing (simplified)
bool read_id3v2_basic(const std::vector<uint8_t>& mp3_data, AudioMetadata& metadata) {
    if (mp3_data.size() < 10) return false;
    
    const uint8_t* data = mp3_data.data();
    
    // Check for "ID3" identifier
    if (memcmp(data, "ID3", 3) != 0) return false;
    
    // Get version
    uint8_t major_version = data[3];
    uint8_t minor_version = data[4];
    
    if (major_version < 2 || major_version > 4) return false;
    
    // Get tag size (synchsafe integer)
    uint32_t tag_size = ((data[6] & 0x7F) << 21) |
                       ((data[7] & 0x7F) << 14) |
                       ((data[8] & 0x7F) << 7) |
                       (data[9] & 0x7F);
    
    printf("Found ID3v2.%d.%d tag, size: %u bytes\n", major_version, minor_version, tag_size);
    
    // For simplicity, we'll just note that ID3v2 exists
    // Full ID3v2 parsing is quite complex and would require substantial code
    metadata.custom_tags["id3v2_version"] = std::to_string(major_version) + "." + std::to_string(minor_version);
    metadata.custom_tags["id3v2_size"] = std::to_string(tag_size);
    
    return true;
}

// Estimate MP3 duration and bitrate from frame headers
bool analyze_mp3_frames(const std::vector<uint8_t>& mp3_data, AudioMetadata& metadata, int& sample_rate, int& channels) {
    const uint8_t* data = mp3_data.data();
    size_t size = mp3_data.size();
    size_t pos = 0;
    
    // Skip ID3v2 tag if present
    if (size >= 10 && memcmp(data, "ID3", 3) == 0) {
        uint32_t tag_size = ((data[6] & 0x7F) << 21) |
                           ((data[7] & 0x7F) << 14) |
                           ((data[8] & 0x7F) << 7) |
                           (data[9] & 0x7F);
        pos = 10 + tag_size;
    }
    
    // Look for first MP3 frame header
    while (pos < size - 4) {
        if (data[pos] == 0xFF && (data[pos + 1] & 0xE0) == 0xE0) {
            // Found potential frame header
            uint8_t header[4] = {data[pos], data[pos+1], data[pos+2], data[pos+3]};
            
            // Parse MPEG version
            int version = (header[1] >> 3) & 0x03;
            if (version == 1) continue; // Reserved
            
            // Parse layer
            int layer = (header[1] >> 1) & 0x03;
            if (layer == 0) continue; // Reserved
            
            // Parse bitrate
            int bitrate_index = (header[2] >> 4) & 0x0F;
            if (bitrate_index == 0 || bitrate_index == 15) continue; // Invalid
            
            // Parse sample rate
            int sample_rate_index = (header[2] >> 2) & 0x03;
            if (sample_rate_index == 3) continue; // Reserved
            
            // Bitrate table for MPEG 1 Layer 3
            int bitrates[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
            int sample_rates[] = {44100, 48000, 32000, 0};
            
            if (version == 3) { // MPEG 1
                metadata.bitrate = bitrates[bitrate_index];
                sample_rate = sample_rates[sample_rate_index];
            }
            
            // Parse channel mode
            int channel_mode = (header[3] >> 6) & 0x03;
            channels = (channel_mode == 3) ? 1 : 2; // Mono or stereo
            
            // Estimate duration (rough calculation)
            if (metadata.bitrate > 0) {
                size_t audio_data_size = size - pos;
                if (size >= 128 && memcmp(data + size - 128, "TAG", 3) == 0) {
                    audio_data_size -= 128; // Subtract ID3v1 tag
                }
                metadata.duration_seconds = (audio_data_size * 8) / (metadata.bitrate * 1000);
            }
            
            printf("MP3 Info: %d kbps, %d Hz, %s\n", 
                   metadata.bitrate, sample_rate, (channels == 1) ? "Mono" : "Stereo");
            return true;
        }
        pos++;
    }
    
    return false;
}

// Enhanced conversion function with metadata reading
bool convert_mp3_to_wav(AudioPlayer *player, const char* filename) {
    // Check cache first
    const char* cached_file = get_cached_conversion(&player->conversion_cache, filename);
    if (cached_file) {
        strncpy(player->temp_wav_file, cached_file, sizeof(player->temp_wav_file) - 1);
        player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
        return true;
    }
    
    // Initialize metadata structure
    AudioMetadata metadata;
    
    // Generate a unique virtual filename
    static int virtual_counter = 0;
    char virtual_filename[256];
    snprintf(virtual_filename, sizeof(virtual_filename), "virtual_mp3_%d.wav", virtual_counter++);
    
    strncpy(player->temp_wav_file, virtual_filename, sizeof(player->temp_wav_file) - 1);
    player->temp_wav_file[sizeof(player->temp_wav_file) - 1] = '\0';
    
    printf("Converting MP3 to virtual WAV: %s -> %s\n", filename, virtual_filename);
    
    // Read MP3 file into memory
    FILE* mp3_file = fopen(filename, "rb");
    if (!mp3_file) {
        printf("Cannot open MP3 file: %s\n", filename);
        return false;
    }
    
    fseek(mp3_file, 0, SEEK_END);
    long mp3_size = ftell(mp3_file);
    fseek(mp3_file, 0, SEEK_SET);
    
    std::vector<uint8_t> mp3_data(mp3_size);
    if (fread(mp3_data.data(), 1, mp3_size, mp3_file) != (size_t)mp3_size) {
        printf("Failed to read MP3 file\n");
        fclose(mp3_file);
        return false;
    }
    fclose(mp3_file);
    
    /*// Read metadata from MP3
    printf("\n--- Reading MP3 Metadata ---\n");
    
    // Try ID3v2 first (at beginning of file)
    bool has_id3v2 = read_id3v2_basic(mp3_data, metadata);
    
    // Try ID3v1 (at end of file)
    bool has_id3v1 = read_id3v1_tag(mp3_data, metadata);
    
    // Analyze MP3 frames for technical info
    int detected_sample_rate = 44100;
    int detected_channels = 2;
    bool has_frame_info = analyze_mp3_frames(mp3_data, metadata, detected_sample_rate, detected_channels);
    
    // Update player settings with detected values
    if (has_frame_info) {
        player->sample_rate = detected_sample_rate;
        player->channels = detected_channels;
    } else {
        // Fallback to defaults
        player->sample_rate = 44100;
        player->channels = 2;
    }
    
    // Print metadata information
    if (has_id3v1 || has_id3v2) {
        printf("Metadata found:\n");
        if (!metadata.title.empty()) printf("  Title: %s\n", metadata.title.c_str());
        if (!metadata.artist.empty()) printf("  Artist: %s\n", metadata.artist.c_str());
        if (!metadata.album.empty()) printf("  Album: %s\n", metadata.album.c_str());
        if (!metadata.year.empty()) printf("  Year: %s\n", metadata.year.c_str());
        if (!metadata.genre.empty()) printf("  Genre: %s\n", metadata.genre.c_str());
        if (metadata.track_number > 0) printf("  Track: %d\n", metadata.track_number);
        if (metadata.duration_seconds > 0) {
            int minutes = metadata.duration_seconds / 60;
            int seconds = metadata.duration_seconds % 60;
            printf("  Duration: %d:%02d\n", minutes, seconds);
        }
        if (metadata.bitrate > 0) printf("  Bitrate: %d kbps\n", metadata.bitrate);
        if (!metadata.comment.empty()) printf("  Comment: %s\n", metadata.comment.c_str());
        
        // Print any custom tags
        for (const auto& tag : metadata.custom_tags) {
            printf("  %s: %s\n", tag.first.c_str(), tag.second.c_str());
        }
    } else {
        printf("No metadata found in MP3 file\n");
    }
    printf("--- End Metadata ---\n\n");
    */
    // Convert MP3 to WAV in memory
    std::vector<uint8_t> wav_data;
    if (!convertMp3ToWavInMemory(mp3_data, wav_data)) {
        printf("MP3 to WAV conversion failed\n");
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
    
    printf("MP3 conversion to virtual file complete\n");
    return true;
}
