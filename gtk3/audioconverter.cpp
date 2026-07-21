#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <vector>
#include <iostream>
#include <cstring>
#include <unistd.h>  // for mkstemp
#include "midiplayer.h"
#include "dbopl_wrapper.h"

#ifdef WIN32
#include <windows.h>
#endif

// External variables and functions needed from midiplayer.cpp
extern double playTime;
extern bool isPlaying;
extern double playwait;
extern int globalVolume;
extern void processEvents();

bool convertMidiToWav(const char* midi_filename, const char* wav_filename, int volume);

// In-memory MIDI to WAV conversion function
#ifndef WIN32
bool convertMidiToWavInMemory(const std::vector<uint8_t>& midiData, std::vector<uint8_t>& wavData) {
    // Create temporary files using mkstemp (safer than tmpnam)
    char tempMidiPath[] = "/tmp/midi_XXXXXX";
    char tempWavPath[] = "/tmp/wav_XXXXXX";
    
    // Create temporary MIDI file
    int midifd = mkstemp(tempMidiPath);
    if (midifd == -1) {
        std::cerr << "Failed to create temporary MIDI file" << std::endl;
        return false;
    }
    
    // Write MIDI data to temp file
    ssize_t written = write(midifd, midiData.data(), midiData.size());
    close(midifd);
    
    if (written != (ssize_t)midiData.size()) {
        std::cerr << "Failed to write MIDI data to temporary file" << std::endl;
        unlink(tempMidiPath);
        return false;
    }
    
    // Create temporary WAV file
    int wavfd = mkstemp(tempWavPath);
    if (wavfd == -1) {
        std::cerr << "Failed to create temporary WAV file" << std::endl;
        unlink(tempMidiPath);
        return false;
    }
    close(wavfd); // We just need the filename, close the descriptor
    
    // Use the existing convertMidiToWav function
    bool conversionSuccess = convertMidiToWav(tempMidiPath, tempWavPath, 1000);
    
    if (!conversionSuccess) {
        std::cerr << "MIDI to WAV conversion failed" << std::endl;
        unlink(tempMidiPath);
        unlink(tempWavPath);
        return false;
    }
    
    // Read the resulting WAV file into memory
    FILE* wavFile = fopen(tempWavPath, "rb");
    if (!wavFile) {
        std::cerr << "Failed to open temporary WAV file for reading" << std::endl;
        unlink(tempMidiPath);
        unlink(tempWavPath);
        return false;
    }
    
    // Get the WAV file size
    fseek(wavFile, 0, SEEK_END);
    long wavSize = ftell(wavFile);
    fseek(wavFile, 0, SEEK_SET);
    
    if (wavSize <= 0) {
        std::cerr << "WAV file is empty or invalid" << std::endl;
        fclose(wavFile);
        unlink(tempMidiPath);
        unlink(tempWavPath);
        return false;
    }
    
    // Read the WAV header first (44 bytes)
    uint8_t header[44];
    size_t headerBytesRead = fread(header, 1, 44, wavFile);
    
    if (headerBytesRead != 44) {
        std::cerr << "Failed to read WAV header" << std::endl;
        fclose(wavFile);
        unlink(tempMidiPath);
        unlink(tempWavPath);
        return false;
    }
    
    // Validate it's a WAV file
    if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F' ||
        header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E') {
        std::cerr << "Invalid WAV file format" << std::endl;
        fclose(wavFile);
        unlink(tempMidiPath);
        unlink(tempWavPath);
        return false;
    }
    
    // Extract the data size from the WAV file
    uint32_t dataSize = wavSize - 44; // Total file size minus header size
    
    // Read the audio data
    std::vector<uint8_t> audioData(dataSize);
    size_t dataBytesRead = fread(audioData.data(), 1, dataSize, wavFile);
    fclose(wavFile);
    
    if (dataBytesRead != dataSize) {
        std::cerr << "Failed to read WAV data" << std::endl;
        unlink(tempMidiPath);
        unlink(tempWavPath);
        return false;
    }
    
    // Create a new WAV header that matches the MP3 conversion format
    struct WavHeader {
        // RIFF header
        char riff_header[4] = {'R', 'I', 'F', 'F'};
        uint32_t wav_size;        // File size - 8
        char wave_header[4] = {'W', 'A', 'V', 'E'};
        
        // Format chunk
        char fmt_header[4] = {'f', 'm', 't', ' '};
        uint32_t fmt_chunk_size = 16;
        uint16_t audio_format = 1; // PCM
        uint16_t num_channels = 2; // Stereo
        uint32_t sample_rate = 44100; // 44.1kHz
        uint32_t byte_rate = sample_rate * num_channels * 2; // bytes per second
        uint16_t block_align = num_channels * 2; // bytes per sample
        uint16_t bits_per_sample = 16; // 16-bit
        
        // Data chunk
        char data_header[4] = {'d', 'a', 't', 'a'};
        uint32_t data_bytes;      // Size of data
    };
    
    // Fill in the header with the correct values
    WavHeader newHeader;
    newHeader.data_bytes = dataSize;
    newHeader.wav_size = 36 + dataSize; // 36 is the size of the header excluding RIFF header
    
    // Create the final WAV data with our new header + the audio data
    wavData.resize(sizeof(newHeader) + dataSize);
    
    // Copy the header to the beginning of wavData
    std::memcpy(wavData.data(), &newHeader, sizeof(newHeader));
    
    // Copy the PCM data after the header
    std::memcpy(wavData.data() + sizeof(newHeader), audioData.data(), dataSize);
    
    // Clean up temporary files
    unlink(tempMidiPath);
    unlink(tempWavPath);
    
    std::cerr << "MIDI to WAV conversion successful, created WAV with " << dataSize 
              << " bytes of audio data" << std::endl;
    
    return true;
}
#else // For Windows platforms
bool convertMidiToWavInMemory(const std::vector<uint8_t>& midiData, std::vector<uint8_t>& wavData) {
    // Create a temporary file to write the MIDI data
    char tempDir[MAX_PATH];
    char tempMidiPath[MAX_PATH];
    char tempWavPath[MAX_PATH];
    
    // Get the temporary directory path
    if (GetTempPathA(MAX_PATH, tempDir) == 0) {
        std::cerr << "Failed to get temporary directory path" << std::endl;
        return false;
    }
    
    // Create temporary file names
    if (GetTempFileNameA(tempDir, "mid", 0, tempMidiPath) == 0) {
        std::cerr << "Failed to create temporary MIDI file name" << std::endl;
        return false;
    }
    
    if (GetTempFileNameA(tempDir, "wav", 0, tempWavPath) == 0) {
        std::cerr << "Failed to create temporary WAV file name" << std::endl;
        DeleteFileA(tempMidiPath);
        return false;
    }
    
    // Write MIDI data to temp file
    FILE* tempMidi = fopen(tempMidiPath, "wb");
    if (!tempMidi) {
        std::cerr << "Failed to open temporary MIDI file for writing" << std::endl;
        DeleteFileA(tempMidiPath);
        DeleteFileA(tempWavPath);
        return false;
    }
    
    fwrite(midiData.data(), 1, midiData.size(), tempMidi);
    fclose(tempMidi);
    
    // Use the existing convertMidiToWav function
    bool conversionSuccess = convertMidiToWav(tempMidiPath, tempWavPath, 1000);
    
    if (!conversionSuccess) {
        std::cerr << "MIDI to WAV conversion failed" << std::endl;
        DeleteFileA(tempMidiPath);
        DeleteFileA(tempWavPath);
        return false;
    }
    
    // Read the resulting WAV file into memory
    FILE* wavFile = fopen(tempWavPath, "rb");
    if (!wavFile) {
        std::cerr << "Failed to open temporary WAV file for reading" << std::endl;
        DeleteFileA(tempMidiPath);
        DeleteFileA(tempWavPath);
        return false;
    }
    
    // Get the WAV file size
    fseek(wavFile, 0, SEEK_END);
    long wavSize = ftell(wavFile);
    fseek(wavFile, 0, SEEK_SET);
    
    if (wavSize <= 0) {
        std::cerr << "WAV file is empty or invalid" << std::endl;
        fclose(wavFile);
        DeleteFileA(tempMidiPath);
        DeleteFileA(tempWavPath);
        return false;
    }
    
    // Read the WAV header first (44 bytes)
    uint8_t header[44];
    size_t headerBytesRead = fread(header, 1, 44, wavFile);
    
    if (headerBytesRead != 44) {
        std::cerr << "Failed to read WAV header" << std::endl;
        fclose(wavFile);
        DeleteFileA(tempMidiPath);
        DeleteFileA(tempWavPath);
        return false;
    }
    
    // Validate it's a WAV file
    if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F' ||
        header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E') {
        std::cerr << "Invalid WAV file format" << std::endl;
        fclose(wavFile);
        DeleteFileA(tempMidiPath);
        DeleteFileA(tempWavPath);
        return false;
    }
    
    // Extract the data size from the WAV file
    uint32_t dataSize = wavSize - 44; // Total file size minus header size
    
    // Read the audio data
    std::vector<uint8_t> audioData(dataSize);
    size_t dataBytesRead = fread(audioData.data(), 1, dataSize, wavFile);
    fclose(wavFile);
    
    if (dataBytesRead != dataSize) {
        std::cerr << "Failed to read WAV data" << std::endl;
        DeleteFileA(tempMidiPath);
        DeleteFileA(tempWavPath);
        return false;
    }
    
    // Create a new WAV header that matches the MP3 conversion format
    struct WavHeader {
        // RIFF header
        char riff_header[4] = {'R', 'I', 'F', 'F'};
        uint32_t wav_size;        // File size - 8
        char wave_header[4] = {'W', 'A', 'V', 'E'};
        
        // Format chunk
        char fmt_header[4] = {'f', 'm', 't', ' '};
        uint32_t fmt_chunk_size = 16;
        uint16_t audio_format = 1; // PCM
        uint16_t num_channels = 2; // Stereo
        uint32_t sample_rate = 44100; // 44.1kHz
        uint32_t byte_rate = sample_rate * num_channels * 2; // bytes per second
        uint16_t block_align = num_channels * 2; // bytes per sample
        uint16_t bits_per_sample = 16; // 16-bit
        
        // Data chunk
        char data_header[4] = {'d', 'a', 't', 'a'};
        uint32_t data_bytes;      // Size of data
    };
    
    // Fill in the header with the correct values
    WavHeader newHeader;
    newHeader.data_bytes = dataSize;
    newHeader.wav_size = 36 + dataSize; // 36 is the size of the header excluding RIFF header
    
    // Create the final WAV data with our new header + the audio data
    wavData.resize(sizeof(newHeader) + dataSize);
    
    // Copy the header to the beginning of wavData
    std::memcpy(wavData.data(), &newHeader, sizeof(newHeader));
    
    // Copy the PCM data after the header
    std::memcpy(wavData.data() + sizeof(newHeader), audioData.data(), dataSize);
    
    // Clean up temporary files
    DeleteFileA(tempMidiPath);
    DeleteFileA(tempWavPath);
    
    std::cerr << "MIDI to WAV conversion successful, created WAV with " << dataSize 
              << " bytes of audio data" << std::endl;
    
    return true;
}
#endif


// Convert MP3 to WAV using SDL2_mixer
bool convertMp3ToWavInMemory(const std::vector<uint8_t>& mp3Data, std::vector<uint8_t>& wavData) {
    // Initialize SDL and SDL_mixer if not already initialized
    static bool sdl_initialized = false;
    if (!sdl_initialized) {
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL could not initialize! SDL Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Initialize SDL_mixer with default frequency, format, channels, and chunksize
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            std::cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
            return false;
        }
        
        sdl_initialized = true;
    }
    
    // Create an SDL_RWops from the MP3 data
    SDL_RWops* rw = SDL_RWFromMem((void*)mp3Data.data(), mp3Data.size());
    if (!rw) {
        std::cerr << "Failed to create RWops from memory! SDL Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Load the MP3 as a chunk (using SDL_mixer)
    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 0); // 0 means don't free the RWops when done
    
    // Make sure to close the RWops
    SDL_RWclose(rw);
    
    if (!chunk) {
        std::cerr << "Failed to load MP3! SDL_mixer Error: " << Mix_GetError() << std::endl;
        return false;
    }
    
    // At this point, SDL_mixer has decoded the MP3 to raw PCM data
    // chunk->abuf contains the audio data and chunk->alen is the length
    
    // Create a minimal WAV header
    struct WavHeader {
        // RIFF header
        char riff_header[4] = {'R', 'I', 'F', 'F'};
        uint32_t wav_size;        // File size - 8
        char wave_header[4] = {'W', 'A', 'V', 'E'};
        
        // Format chunk
        char fmt_header[4] = {'f', 'm', 't', ' '};
        uint32_t fmt_chunk_size = 16;
        uint16_t audio_format = 1; // PCM
        uint16_t num_channels = 2; // Stereo (SDL_mixer default)
        uint32_t sample_rate = 44100; // SDL_mixer default
        uint32_t byte_rate = sample_rate * num_channels * 2; // bytes per second
        uint16_t block_align = num_channels * 2; // bytes per sample
        uint16_t bits_per_sample = 16; // SDL_mixer default
        
        // Data chunk
        char data_header[4] = {'d', 'a', 't', 'a'};
        uint32_t data_bytes;      // chunk->alen
    };
    
    // Fill in the header with the correct values
    WavHeader header;
    header.data_bytes = chunk->alen;
    header.wav_size = 36 + header.data_bytes; // 36 is the size of the header excluding RIFF header
    
    // Create the WAV data with header + audio data
    wavData.resize(sizeof(header) + chunk->alen);
    
    // Copy the header to the beginning of wavData
    std::memcpy(wavData.data(), &header, sizeof(header));
    
    // Copy the PCM data after the header
    std::memcpy(wavData.data() + sizeof(header), chunk->abuf, chunk->alen);
    
    // Free the chunk
    Mix_FreeChunk(chunk);
    
    return true;
}

bool mixTwoWavFiles(const std::vector<uint8_t>& wavA, const std::vector<uint8_t>& wavB, std::vector<uint8_t>& mixedWav) {
    if (wavA.size() < 44 || wavB.size() < 44) {
        std::cerr << "Invalid WAV input" << std::endl;
        return false;
    }

    // Extract headers
    const uint8_t* hdrA = wavA.data();
    const uint8_t* hdrB = wavB.data();

    auto readLE16 = [](const uint8_t* p) {
        return (uint16_t)(p[0] | (p[1] << 8));
    };
    auto readLE32 = [](const uint8_t* p) {
        return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
    };

    uint16_t channelsA = readLE16(hdrA + 22);
    uint32_t sampleRateA = readLE32(hdrA + 24);
    uint16_t bitsA = readLE16(hdrA + 34);

    uint16_t channelsB = readLE16(hdrB + 22);
    uint32_t sampleRateB = readLE32(hdrB + 24);
    uint16_t bitsB = readLE16(hdrB + 34);

    // Ensure formats match
    if (channelsA != channelsB || sampleRateA != sampleRateB || bitsA != bitsB || bitsA != 16) {
        std::cerr << "WAV formats do not match or are not 16-bit PCM" << std::endl;
        return false;
    }

    // PCM data starts at byte 44
    const int HEADER_SIZE = 44;
    size_t dataSizeA = wavA.size() - HEADER_SIZE;
    size_t dataSizeB = wavB.size() - HEADER_SIZE;

    size_t mixedSize = std::min(dataSizeA, dataSizeB);

    const int16_t* pcmA = (const int16_t*)(wavA.data() + HEADER_SIZE);
    const int16_t* pcmB = (const int16_t*)(wavB.data() + HEADER_SIZE);

    size_t samples = mixedSize / 2; // 2 bytes per sample

    std::vector<int16_t> mixedPCM(samples);

    for (size_t i = 0; i < samples; i++) {
        int32_t s = (int32_t)pcmA[i] + (int32_t)pcmB[i];

        // Clip to 16-bit
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;

        mixedPCM[i] = (int16_t)s;
    }

    // Build output WAV
    mixedWav.resize(HEADER_SIZE + mixedSize);

    // Copy header from A (safe because formats match)
    std::memcpy(mixedWav.data(), wavA.data(), HEADER_SIZE);

    // Fix data size fields
    uint32_t dataBytes = mixedSize;
    uint32_t riffSize = 36 + dataBytes;

    mixedWav[4] = riffSize & 0xFF;
    mixedWav[5] = (riffSize >> 8) & 0xFF;
    mixedWav[6] = (riffSize >> 16) & 0xFF;
    mixedWav[7] = (riffSize >> 24) & 0xFF;

    mixedWav[40] = dataBytes & 0xFF;
    mixedWav[41] = (dataBytes >> 8) & 0xFF;
    mixedWav[42] = (dataBytes >> 16) & 0xFF;
    mixedWav[43] = (dataBytes >> 24) & 0xFF;

    // Copy mixed PCM
    std::memcpy(mixedWav.data() + HEADER_SIZE, mixedPCM.data(), mixedSize);

    return true;
}

