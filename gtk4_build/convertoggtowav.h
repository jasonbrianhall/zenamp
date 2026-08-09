#ifndef CONVERTOGGTOWAV_H
#define CONVERTOGGTOWAV_H

#ifdef __cplusplus
extern "C" {
#include <vector>
#endif

bool convertOggToWav(const char* ogg_filename, const char* wav_filename);

#ifdef __cplusplus
}
bool convertOggToWavInMemory(const std::vector<uint8_t>& ogg_data, std::vector<uint8_t>& wav_data);
#endif

#endif // CONVERTOGGTOWAV_H
