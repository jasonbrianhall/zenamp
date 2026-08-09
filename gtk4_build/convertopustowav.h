#ifndef CONVERTOPUSTOWAV_H
#define CONVERTOPUSTOWAV_H

#include <vector>
#include <cstdint>
#include "audio_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert Opus data in memory to WAV format in memory
 * @param opus_data Input Opus audio data as byte vector
 * @param wav_data Output WAV audio data as byte vector
 * @return true if conversion successful, false otherwise
 */
bool convertOpusToWavInMemory(const std::vector<uint8_t>& opus_data, std::vector<uint8_t>& wav_data);

/**
 * Convert Opus file to WAV file
 * @param opus_filename Path to input Opus file
 * @param wav_filename Path to output WAV file
 * @return true if conversion successful, false otherwise
 */
bool convertOpusToWav(const char* opus_filename, const char* wav_filename);

/**
 * Convert Opus file to virtual WAV file with caching support
 * @param player AudioPlayer instance with conversion cache
 * @param filename Path to input Opus file
 * @return true if conversion successful, false otherwise
 */
bool convert_opus_to_wav(AudioPlayer *player, const char* filename);

#ifdef __cplusplus
}
#endif

#endif // CONVERTOPUSTOWAV_H
