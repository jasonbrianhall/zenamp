#ifndef CONVERTFLACTOWAV_H
#define CONVERTFLACTOWAV_H

#include <vector>
#include <stdint.h>

// Convert FLAC file to WAV data in memory
bool convertFlacToWavInMemory(const std::vector<uint8_t>& flac_data, std::vector<uint8_t>& wav_data);

// Convert FLAC file to WAV file
bool convertFlacToWav(const char* flac_path, const char* wav_path);

#endif // CONVERTFLACTOWAV_H
