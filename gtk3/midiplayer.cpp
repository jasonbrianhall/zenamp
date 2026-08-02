#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#else
#include <windows.h>
#include <conio.h>
#endif

#include "midiplayer.h"
#include "dbopl_wrapper.h"
#include "virtual_mixer.h"

VirtualMixer* g_midi_mixer = NULL;
int g_midi_mixer_channel = -1;
LyricData* g_lyrics = nullptr;

#ifndef _WIN32
struct termios old_tio;
#endif
volatile sig_atomic_t keep_running = 1;

#ifndef M_PI
#define M_PI 3.1415926154
#endif

// Global variables
struct FMInstrument adl[181];
int globalVolume = 100;
bool enableNormalization = true;
bool isPlaying = false;
bool paused = false;

// MIDI file state
FILE* midiFile = NULL;
int TrackCount = 0;
int DeltaTicks = 0;
double Tempo = 500000;  // Default 120 BPM
double playTime = 0;

// Track state
int tkPtr[MAX_TRACKS] = {0};
double tkDelay[MAX_TRACKS] = {0};
int tkStatus[MAX_TRACKS] = {0};
bool loopStart = false;
bool loopEnd = false;
int loPtr[MAX_TRACKS] = {0};
double loDelay[MAX_TRACKS] = {0};
int loStatus[MAX_TRACKS] = {0};
double loopwait = 0;
int rbPtr[MAX_TRACKS] = {0};
double rbDelay[MAX_TRACKS] = {0};
int rbStatus[MAX_TRACKS] = {0};
double playwait = 0;

// MIDI channel state
int ChPatch[16] = {0};
double ChBend[16] = {0};
int ChVolume[16] = {127};
int ChPanning[16] = {0};
int ChVibrato[16] = {0};

// SDL Audio
SDL_AudioDeviceID audioDevice;
SDL_AudioSpec audioSpec;
pthread_mutex_t audioMutex = PTHREAD_MUTEX_INITIALIZER;

int kbhit() {
#ifdef _WIN32
    return _kbhit();
#else
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
#endif
}

// SDL Audio initialization
bool initSDL() {
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return false;
    }
    
    // Initialize virtual mixer
    g_midi_mixer = mixer_init(SAMPLE_RATE, AUDIO_CHANNELS, enableNormalization);
    if (!g_midi_mixer) {
        fprintf(stderr, "Failed to initialize virtual mixer\n");
        return false;
    }
    
    // Allocate a mixer channel for MIDI audio
    g_midi_mixer_channel = mixer_allocate_channel(g_midi_mixer);
    if (g_midi_mixer_channel < 0) {
        fprintf(stderr, "Failed to allocate mixer channel\n");
        mixer_free(g_midi_mixer);
        g_midi_mixer = NULL;
        return false;
    }
    
    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16;
    want.channels = AUDIO_CHANNELS;
    want.samples = AUDIO_BUFFER;
    want.callback = generateAudio;
    want.userdata = NULL;
    
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &audioSpec, 0);
    if (audioDevice == 0) {
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
        mixer_release_channel(g_midi_mixer, g_midi_mixer_channel);
        mixer_free(g_midi_mixer);
        g_midi_mixer = NULL;
        return false;
    }
    
    // Initialize the OPL emulator
    OPL_Init(SAMPLE_RATE);
    
    // Initialize the FM instruments
    OPL_LoadInstruments();
    
    return true;
}

// Extract lyrics from KAR file
LyricData* extractLyricsFromKar(const char* filename) {
    LyricData* lyrics = new LyricData();
    lyrics->events = new LyricEvent[1000];
    lyrics->count = 0;
    lyrics->capacity = 1000;
    lyrics->title = nullptr;
    lyrics->artist = nullptr;
    lyrics->album = nullptr;
    lyrics->language = nullptr;
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return lyrics;
    }
    
    // Read MIDI header
    char header[4];
    if (fread(header, 1, 4, file) != 4 || strncmp(header, "MThd", 4) != 0) {
        fprintf(stderr, "Error: Not a valid MIDI file\n");
        fclose(file);
        return lyrics;
    }
    
    // Read header length
    char buffer[4];
    fread(buffer, 1, 4, file);
    (void)convertInteger(buffer, 4);  // Read header length but not used for validation
    
    // Read format type, track count, ticks per beat
    fread(buffer, 1, 2, file);
    uint16_t formatType = (uint16_t)convertInteger(buffer, 2);
    
    fread(buffer, 1, 2, file);
    uint16_t trackCount = (uint16_t)convertInteger(buffer, 2);
    
    fread(buffer, 1, 2, file);
    uint16_t ticksPerBeat = (uint16_t)convertInteger(buffer, 2);
    
    printf("KAR Extraction - Format: %d, Tracks: %d, Ticks/Beat: %d\n", formatType, trackCount, ticksPerBeat);
    
    int currentTempo = 500000;  // Default 120 BPM in microseconds per beat
    uint32_t trackTime = 0;
    
    // Process each track
    for (int track = 0; track < trackCount; track++) {
        if (fread(header, 1, 4, file) != 4 || strncmp(header, "MTrk", 4) != 0) {
            fprintf(stderr, "Warning: Invalid track header\n");
            break;
        }
        
        fread(buffer, 1, 4, file);
        uint32_t trackLen = convertInteger(buffer, 4);
        
        long trackStart = ftell(file);
        trackTime = 0;
        
        // Parse track events
        while (ftell(file) < trackStart + (long)trackLen) {
            uint32_t deltaTime = readVarLen(file);
            trackTime += deltaTime;
            
            unsigned char status = fgetc(file);
            
            if (status == 0xFF) {  // Meta event
                unsigned char metaType = fgetc(file);
                uint32_t length = readVarLen(file);
                
                if (metaType == 0x51) {  // Tempo event
                    if (length >= 3) {
                        unsigned char b1 = fgetc(file);
                        unsigned char b2 = fgetc(file);
                        unsigned char b3 = fgetc(file);
                        currentTempo = (b1 << 16) | (b2 << 8) | b3;
                    }
                }
                else if (metaType == 0x01) {  // Text meta event (lyrics)
                    char* textData = (char*)malloc(length + 1);
                    fread(textData, 1, length, file);
                    textData[length] = '\0';
                    
                    double timestamp = ticksToSeconds(trackTime, ticksPerBeat, currentTempo);
                    
                    parseLyricText(textData, length, lyrics, timestamp);
                    free(textData);
                    continue;
                }
                else {
                    fseek(file, length, SEEK_CUR);
                }
            }
            else if ((status & 0xF0) == 0xF0) {
                uint32_t length = readVarLen(file);
                fseek(file, length, SEEK_CUR);
            }
            else {
                unsigned char param1 = fgetc(file);
                (void)param1;  // Avoid unused variable warning
                if ((status & 0xF0) != 0xC0 && (status & 0xF0) != 0xD0) {
                    unsigned char param2 = fgetc(file);
                    (void)param2;  // Avoid unused variable warning
                }
            }
        }
    }
    
    fclose(file);
    printf("Extracted %d lyric events\n", lyrics->count);
    return lyrics;
}

// Export to .lrc format
void exportToLrc(LyricData* lyrics, const char* outputFile) {
    if (!lyrics) return;
    
    FILE* out = fopen(outputFile, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot create output file %s\n", outputFile);
        return;
    }
    
    fprintf(out, "[ti:]\n[ar:]\n[al:]\n\n");
    
    for (int i = 0; i < lyrics->count; i++) {
        int minutes = (int)lyrics->events[i].timestamp / 60;
        int seconds = (int)lyrics->events[i].timestamp % 60;
        int centiseconds = (int)((lyrics->events[i].timestamp - (int)lyrics->events[i].timestamp) * 100);
        
        fprintf(out, "[%02d:%02d.%02d]%s\n", minutes, seconds, centiseconds, lyrics->events[i].text);
    }
    
    fclose(out);
    printf("Lyrics exported to %s\n", outputFile);
}

// Get lyric at current playback time
LyricEvent* getLyricAtTime(LyricData* lyrics, double currentTime) {
    if (!lyrics || lyrics->count == 0) return NULL;
    
    for (int i = 0; i < lyrics->count; i++) {
        if (lyrics->events[i].timestamp >= currentTime) {
            return &lyrics->events[i];
        }
    }
    
    return &lyrics->events[lyrics->count - 1];
}

// Get next N lyrics from current time
LyricEvent** getNextLyrics(LyricData* lyrics, double currentTime, int maxCount, int* count) {
    if (!lyrics || lyrics->count == 0) {
        *count = 0;
        return NULL;
    }
    
    LyricEvent** result = (LyricEvent**)malloc(sizeof(LyricEvent*) * maxCount);
    int resultCount = 0;
    
    for (int i = 0; i < lyrics->count && resultCount < maxCount; i++) {
        if (lyrics->events[i].timestamp >= currentTime) {
            result[resultCount++] = &lyrics->events[i];
        }
    }
    
    *count = resultCount;
    if (resultCount == 0) {
        free(result);
        return NULL;
    }
    
    return result;
}

// Print all extracted lyrics
void printLyrics(LyricData* lyrics) {
    if (!lyrics) return;
    
    printf("\n=== KAR FILE METADATA ===\n");
    if (lyrics->title) printf("Title: %s\n", lyrics->title);
    if (lyrics->artist) printf("Artist: %s\n", lyrics->artist);
    if (lyrics->album) printf("Album: %s\n", lyrics->album);
    if (lyrics->language) printf("Language: %s\n", lyrics->language);
    printf("Total Lyrics: %d\n", lyrics->count);
    printf("\n=== LYRICS WITH TIMING ===\n");
    
    for (int i = 0; i < lyrics->count; i++) {
        printf("[%.3f] %s\n", lyrics->events[i].timestamp, lyrics->events[i].text);
    }
}

// Cleanup lyrics
void freeLyrics(LyricData* lyrics) {
    if (!lyrics) return;
    
    for (int i = 0; i < lyrics->count; i++) {
        delete[] lyrics->events[i].text;
    }
    delete[] lyrics->events;
    
    if (lyrics->title) delete[] lyrics->title;
    if (lyrics->artist) delete[] lyrics->artist;
    if (lyrics->album) delete[] lyrics->album;
    if (lyrics->language) delete[] lyrics->language;
    
    delete lyrics;
}

// Display current lyric during playback
void displayCurrentLyric(double currentTime) {
    if (!g_lyrics || g_lyrics->count == 0) return;
    
    LyricEvent* current = getLyricAtTime(g_lyrics, currentTime);
    if (current) {
        // Only print if text is not empty and not a screen break
        if (current->text && strlen(current->text) > 0 && strcmp(current->text, "\n") != 0) {
            printf("[%.2f] %s\n", current->timestamp, current->text);
        }
    }
}

// Cleanup when shutting down
void cleanup() {
    SDL_CloseAudioDevice(audioDevice);
    SDL_Quit();
    
    // Cleanup OPL
    OPL_Shutdown();
    
    if (midiFile) {
        fclose(midiFile);
        midiFile = NULL;
    }
    
    if (g_lyrics) {
        freeLyrics(g_lyrics);
        g_lyrics = nullptr;
    }
}

// Helper: Read variable length value from MIDI file
unsigned long readVarLen(FILE* f) {
    unsigned char c;
    unsigned long value = 0;
    
    if (fread(&c, 1, 1, f) != 1) return 0;
    
    value = c;
    if (c & 0x80) {
        value &= 0x7F;
        do {
            if (fread(&c, 1, 1, f) != 1) return value;
            value = (value << 7) + (c & 0x7F);
        } while (c & 0x80);
    }
    
    return value;
}

// Helper: Read bytes from file
int readString(FILE* f, int len, char* str) {
    return fread(str, 1, len, f);
}

// Helper: Parse big-endian integer
unsigned long convertInteger(char* str, int len) {
    unsigned long value = 0;
    for (int i = 0; i < len; i++) {
        value = value * 256 + (unsigned char)str[i];
    }
    return value;
}

// Helper: Convert MIDI ticks to seconds
double ticksToSeconds(uint32_t ticks, uint32_t ticksPerBeat, int tempo) {
    double secondsPerBeat = tempo / 1000000.0;
    double secondsPerTick = secondsPerBeat / ticksPerBeat;
    return ticks * secondsPerTick;
}

// Parse text event and extract lyrics/metadata
void parseLyricText(const char* text, int len, LyricData* lyrics, double timestamp) {
    if (len == 0) return;
    if (!lyrics) return;
    
    // Check for metadata headers
    if (text[0] == '@') {
        // Extract metadata
        if (strncmp(text, "@T", 2) == 0 && !lyrics->title) {
            // Title
            int metaLen = len - 2;
            lyrics->title = new char[metaLen + 1];
            strncpy(lyrics->title, text + 2, metaLen);
            lyrics->title[metaLen] = '\0';
            printf("[KAR] Title: %s\n", lyrics->title);
        }
        else if (strncmp(text, "@AR", 3) == 0 && !lyrics->artist) {
            // Artist
            int metaLen = len - 3;
            lyrics->artist = new char[metaLen + 1];
            strncpy(lyrics->artist, text + 3, metaLen);
            lyrics->artist[metaLen] = '\0';
            printf("[KAR] Artist: %s\n", lyrics->artist);
        }
        else if (strncmp(text, "@AL", 3) == 0 && !lyrics->album) {
            // Album
            int metaLen = len - 3;
            lyrics->album = new char[metaLen + 1];
            strncpy(lyrics->album, text + 3, metaLen);
            lyrics->album[metaLen] = '\0';
            printf("[KAR] Album: %s\n", lyrics->album);
        }
        else if (strncmp(text, "@L", 2) == 0 && !lyrics->language) {
            // Language
            int metaLen = len - 2;
            lyrics->language = new char[metaLen + 1];
            strncpy(lyrics->language, text + 2, metaLen);
            lyrics->language[metaLen] = '\0';
            printf("[KAR] Language: %s\n", lyrics->language);
        }
        return;
    }
    
    char buffer[512] = {0};
    int bufPos = 0;
    
    for (int i = 0; i < len; i++) {
        char c = text[i];
        
        if (c == '\\') {
            // Backslash = end of lyric line
            if (bufPos > 0) {
                buffer[bufPos] = '\0';
                
                if (lyrics->count >= lyrics->capacity) {
                    lyrics->capacity *= 2;
                    LyricEvent* newEvents = new LyricEvent[lyrics->capacity];
                    for (int j = 0; j < lyrics->count; j++) {
                        newEvents[j] = lyrics->events[j];
                    }
                    delete[] lyrics->events;
                    lyrics->events = newEvents;
                }
                
                lyrics->events[lyrics->count].timestamp = timestamp;
                lyrics->events[lyrics->count].text = new char[bufPos + 1];
                strcpy(lyrics->events[lyrics->count].text, buffer);
                lyrics->events[lyrics->count].syllableIndex = lyrics->count;
                lyrics->count++;
                bufPos = 0;
            }
        }
        else if (c == '/') {
            // Forward slash = paragraph/screen break
            if (bufPos > 0) {
                buffer[bufPos] = '\0';
                
                if (lyrics->count >= lyrics->capacity) {
                    lyrics->capacity *= 2;
                    LyricEvent* newEvents = new LyricEvent[lyrics->capacity];
                    for (int j = 0; j < lyrics->count; j++) {
                        newEvents[j] = lyrics->events[j];
                    }
                    delete[] lyrics->events;
                    lyrics->events = newEvents;
                }
                
                lyrics->events[lyrics->count].timestamp = timestamp;
                lyrics->events[lyrics->count].text = new char[bufPos + 1];
                strcpy(lyrics->events[lyrics->count].text, buffer);
                lyrics->events[lyrics->count].syllableIndex = lyrics->count;
                lyrics->count++;
                bufPos = 0;
            }
            // Add screen break marker
            if (lyrics->count < lyrics->capacity) {
                lyrics->events[lyrics->count].timestamp = timestamp;
                lyrics->events[lyrics->count].text = new char[2];
                strcpy(lyrics->events[lyrics->count].text, "\n");
                lyrics->count++;
            }
        }
        else if (c != '\0') {
            if (bufPos < (int)sizeof(buffer) - 1) {
                buffer[bufPos++] = c;
            }
        }
    }
    
    // Handle remaining text
    if (bufPos > 0) {
        buffer[bufPos] = '\0';
        
        if (lyrics->count >= lyrics->capacity) {
            lyrics->capacity *= 2;
            LyricEvent* newEvents = new LyricEvent[lyrics->capacity];
            for (int j = 0; j < lyrics->count; j++) {
                newEvents[j] = lyrics->events[j];
            }
            delete[] lyrics->events;
            lyrics->events = newEvents;
        }
        
        lyrics->events[lyrics->count].timestamp = timestamp;
        lyrics->events[lyrics->count].text = new char[bufPos + 1];
        strcpy(lyrics->events[lyrics->count].text, buffer);
        lyrics->events[lyrics->count].syllableIndex = lyrics->count;
        lyrics->count++;
    }
}

// Load and parse MIDI file
bool loadMidiFile(const char* filename) {
    char buffer[256];
    char id[5] = {0};
    unsigned long headerLength;
    int format;
    
    // Open file
    midiFile = fopen(filename, "rb");
    if (!midiFile) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return false;
    }
    
    // Read MIDI header
    if (readString(midiFile, 4, id) != 4 || strncmp(id, "MThd", 4) != 0) {
        fprintf(stderr, "Error: Not a valid MIDI file\n");
        fclose(midiFile);
        midiFile = NULL;
        return false;
    }
    
    // Read header length
    readString(midiFile, 4, buffer);
    headerLength = convertInteger(buffer, 4);
    if (headerLength != 6) {
        fprintf(stderr, "Error: Invalid MIDI header length\n");
        fclose(midiFile);
        midiFile = NULL;
        return false;
    }
    
    // Read format type
    readString(midiFile, 2, buffer);
    format = (int)convertInteger(buffer, 2);
    
    // Read number of tracks
    readString(midiFile, 2, buffer);
    TrackCount = (int)convertInteger(buffer, 2);
    if (TrackCount > MAX_TRACKS) {
        fprintf(stderr, "Error: Too many tracks in MIDI file\n");
        fclose(midiFile);
        midiFile = NULL;
        return false;
    }
    
    // Read time division
    readString(midiFile, 2, buffer);
    DeltaTicks = (int)convertInteger(buffer, 2);
    
    // Initialize track data
    for (int tk = 0; tk < TrackCount; tk++) {
        // Read track header
        if (readString(midiFile, 4, id) != 4 || strncmp(id, "MTrk", 4) != 0) {
            fprintf(stderr, "Error: Invalid track header\n");
            fclose(midiFile);
            midiFile = NULL;
            return false;
        }
        
        // Read track length
        readString(midiFile, 4, buffer);
        unsigned long trackLength = convertInteger(buffer, 4);
        long pos = ftell(midiFile);
        
        // Read first event delay
        tkDelay[tk] = readVarLen(midiFile);
        tkPtr[tk] = ftell(midiFile);
        
        // Skip to next track
        fseek(midiFile, pos + (long)trackLength, SEEK_SET);
    }
    
    // Reset file position for playback
    fseek(midiFile, 0, SEEK_SET);
    
    printf("MIDI file loaded: %s\n", filename);
    printf("Format: %d, Tracks: %d, Time Division: %d\n", format, TrackCount, DeltaTicks);
    
    // Try to load lyrics if file is .kar
    if (strstr(filename, ".kar") || strstr(filename, ".KAR")) {
        LyricData* newLyrics = extractLyricsFromKar(filename);
        if (newLyrics && newLyrics->count > 0) {
            // Free old lyrics safely before replacing
            if (g_lyrics) {
                freeLyrics(g_lyrics);
            }
            g_lyrics = newLyrics;
            printf("Loaded %d lyric events from KAR file\n", g_lyrics->count);
            printf("\n=== KAR LYRICS ===\n");
            printLyrics(g_lyrics);
            printf("=== END LYRICS ===\n\n");
        } else {
            // Extraction failed, free the empty result and clear g_lyrics
            if (newLyrics) {
                freeLyrics(newLyrics);
            }
            if (g_lyrics) {
                freeLyrics(g_lyrics);
                g_lyrics = nullptr;
            }
        }
    } else {
        // Clear lyrics for non-KAR files
        if (g_lyrics) {
            freeLyrics(g_lyrics);
            g_lyrics = NULL;
        }
    }
    
    return true;
}

void handle_sigint(int sig) {
    if (sig == SIGINT) {
    keep_running = 0;
    
#ifndef _WIN32
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
#endif
    
    // Stop audio and perform cleanup
    isPlaying = false;
    SDL_PauseAudioDevice(audioDevice, 1);
    
    printf("\nPlayback interrupted. Cleaning up...\n");
    cleanup();
    
    // Exit the program
    exit(0);
    } else {
        fprintf(stderr, "Unexpected signal %d received\n", sig);
    }
}


// Handle a single MIDI event
void handleMidiEvent(int tk) {
    unsigned char status, data1, data2;
    //unsigned char buffer[256];
    unsigned char evtype;
    unsigned long len;
    
    // Get file position
    fseek(midiFile, tkPtr[tk], SEEK_SET);
    
    // Read status byte or use running status
    if (fread(&status, 1, 1, midiFile) != 1) return;
    
    // Check for running status
    if (status < 0x80) {
        fseek(midiFile, tkPtr[tk], SEEK_SET); // Go back one byte
        status = tkStatus[tk]; // Use running status
    } else {
        tkStatus[tk] = status;
    }
    
    int midCh = status & 0x0F;
    
    // Handle different event types
    switch (status & 0xF0) {
        case NOTE_OFF: {
            // Note Off event
            fread(&data1, 1, 1, midiFile);
            fread(&data2, 1, 1, midiFile);
            
            ChBend[midCh] = 0;
            OPL_NoteOff(midCh, data1);
            break;
        }
        
        case NOTE_ON: {
            // Note On event
            fread(&data1, 1, 1, midiFile);
            fread(&data2, 1, 1, midiFile);
            
            // Note on with velocity 0 is treated as note off
            if (data2 == 0) {
                ChBend[midCh] = 0;
                OPL_NoteOff(midCh, data1);
                break;
            }
            
            OPL_NoteOn(midCh, data1, data2);
            break;
        }
        
        case CONTROL_CHANGE: {
            // Control Change
            fread(&data1, 1, 1, midiFile);
            fread(&data2, 1, 1, midiFile);
            
            switch (data1) {
                case 1:  // Modulation Wheel
                    ChVibrato[midCh] = data2;
                    // Implementation depends on dbopl_wrapper.cpp supporting this
                    // We could add a new function: OPL_SetModulation(midCh, data2);
                    break;
                    
                case 6:  // Data Entry MSB (for RPN/NRPN)
                    // Could be used for fine pitch control
                    break;
                    
                case 7:  // Channel Volume
                    ChVolume[midCh] = data2;
                    OPL_SetVolume(midCh, data2);
                    break;
                    
                case 10: // Pan
                    ChPanning[midCh] = data2;
                    OPL_SetPan(midCh, data2);
                    break;
                    
                case 11: // Expression
                    // Expression is like a secondary volume control
                    // We could scale the existing volume by this value
                    for (int i = 0; i < MAX_OPL_CHANNELS; i++) {
                        if (opl_channels[i].active && opl_channels[i].midi_channel == midCh) {
                            set_channel_volume(i, opl_channels[i].velocity, 
                                             (ChVolume[midCh] * data2) / 127);
                        }
                    }
                    break;
                    
                case 64: // Sustain Pedal
                    // Implement sustain by delaying note-offs
                    // This would require tracking sustained notes
                    break;
                    
                case 71: // Sound Controller 2 - Resonance/Timbre
                    // Could adjust FM feedback parameters
                    break;
                    
                case 72: // Sound Controller 3 - Release Time
                    // Could adjust envelope release rate
                    break;
                    
                case 73: // Sound Controller 4 - Attack Time
                    // Could adjust envelope attack rate
                    break;
                    
                case 74: // Sound Controller 5 - Brightness
                    // Could adjust FM modulation index or carrier frequency
                    break;
                    
                case 91: // Effects 1 Depth (Reverb)
                    // Could implement basic reverb simulation
                    break;
                    
                case 93: // Effects 3 Depth (Chorus)
                    // Could implement chorus effect through slight detuning
                    break;
                    
                case 120: // All Sound Off
                    // Immediately silence all sound (emergency)
                    OPL_Reset();
                    break;
                    
                case 121: // Reset All Controllers
                    // Reset controllers to default
                    for (int i = 0; i < 16; i++) {
                        ChBend[i] = 0;
                        ChVibrato[i] = 0;
                        // Don't reset volume and panning
                    }
                    break;
                    
                case 123: // All Notes Off
                    // Turn off all notes on this channel
                    for (int i = 0; i < MAX_OPL_CHANNELS; i++) {
                        if (opl_channels[i].active && opl_channels[i].midi_channel == midCh) {
                            OPL_NoteOff(midCh, opl_channels[i].midi_note);
                        }
                    }
                    break;
            }
            break;
        }
        
        case PROGRAM_CHANGE: {
            // Program Change
            fread(&data1, 1, 1, midiFile);
            ChPatch[midCh] = data1;
            OPL_ProgramChange(midCh, data1);
            break;
        }
        
        case CHAN_PRESSURE: {
            // Channel Aftertouch
            fread(&data1, 1, 1, midiFile);
            // Could apply pressure to all active notes on this channel
            // Similar to expression control
            for (int i = 0; i < MAX_OPL_CHANNELS; i++) {
                if (opl_channels[i].active && opl_channels[i].midi_channel == midCh) {
                    // Apply aftertouch as a volume scaling
                    set_channel_volume(i, opl_channels[i].velocity, 
                                     (ChVolume[midCh] * data1) / 127);
                }
            }
            break;
        }
        
        case PITCH_BEND: {
            // Pitch Bend
            fread(&data1, 1, 1, midiFile);
            fread(&data2, 1, 1, midiFile);
            
            // Combine LSB and MSB into a 14-bit value
            int bend = (data2 << 7) | data1;
            ChBend[midCh] = bend;
            
            OPL_SetPitchBend(midCh, bend);
            break;
        }
        
        case META_EVENT: case SYSTEM_MESSAGE: {
            // Meta events and system exclusive
            if (status == META_EVENT) {
                // Meta event
                fread(&evtype, 1, 1, midiFile);
                len = readVarLen(midiFile);
                
                if (evtype == META_END_OF_TRACK) {
                    tkStatus[tk] = -1;  // Mark track as ended
                    fseek(midiFile, len, SEEK_CUR);  // Skip event data
                } else if (evtype == META_TEMPO) {
                    // Tempo change
                    char tempo[4] = {0};
                    readString(midiFile, (int)len, tempo);
                    unsigned long tempoVal = convertInteger(tempo, (int)len);
                    Tempo = tempoVal;
                } else if (evtype == META_TEXT) {
                    // Text event - check for loop markers or custom instructions
                    char text[256] = {0};
                    readString(midiFile, (int)len < 255 ? (int)len : 255, text);
                    
                    if (strcmp(text, "loopStart") == 0) {
                        loopStart = true;
                    } else if (strcmp(text, "loopEnd") == 0) {
                        loopEnd = true;
                    } else if (strstr(text, "volume=") == text) {
                        // Custom volume instruction, format: "volume=XX"
                        int volume = atoi(text + 7);
                        if (volume >= 0 && volume <= 127) {
                            ChVolume[midCh] = volume;
                            OPL_SetVolume(midCh, volume);
                        }
                    } else if (strstr(text, "instrument=") == text) {
                        // Custom instrument instruction, format: "instrument=XX"
                        int instrument = atoi(text + 11);
                        if (instrument >= 0 && instrument < 181) {
                            ChPatch[midCh] = instrument;
                            OPL_ProgramChange(midCh, instrument);
                        }
                    }
                    // Could handle other custom text commands here
                } else {
                    // Skip other meta events
                    fseek(midiFile, len, SEEK_CUR);
                }
            } else {
                // System exclusive - skip
                len = readVarLen(midiFile);
                fseek(midiFile, (long)len, SEEK_CUR);
            }
            break;
        }
        
        default: {
            // Unknown or unsupported message type
            // Try to skip based on expected message length
            switch (status & 0xF0) {
                case 0xC0: // Program Change
                case 0xD0: // Channel Pressure
                    fseek(midiFile, 1, SEEK_CUR); // One data byte
                    break;
                default:
                    fseek(midiFile, 2, SEEK_CUR); // Assume two data bytes
                    break;
            }
            break;
        }
    }
    
    // Read next event delay
    unsigned long nextDelay = readVarLen(midiFile);
    tkDelay[tk] += nextDelay;
    
    // Save new file position
    tkPtr[tk] = ftell(midiFile);
}

// Process events for all tracks
void processEvents() {
    // Save rollback info for each track
    for (int tk = 0; tk < TrackCount; tk++) {
        rbPtr[tk] = tkPtr[tk];
        rbDelay[tk] = tkDelay[tk];
        rbStatus[tk] = tkStatus[tk];
        
        // Handle events for tracks that are due
        if (tkStatus[tk] >= 0 && tkDelay[tk] <= 0) {
            handleMidiEvent(tk);
        }
    }
    
// Handle loop points
    if (loopStart) {
        // Save loop beginning point
        for (int tk = 0; tk < TrackCount; tk++) {
            loPtr[tk] = rbPtr[tk];
            loDelay[tk] = rbDelay[tk];
            loStatus[tk] = rbStatus[tk];
        }
        loopwait = playwait;
        loopStart = false;
    } else if (loopEnd) {
        // Return to loop beginning
        for (int tk = 0; tk < TrackCount; tk++) {
            tkPtr[tk] = loPtr[tk];
            tkDelay[tk] = loDelay[tk];
            tkStatus[tk] = loStatus[tk];
        }
        loopEnd = false;
        playwait = loopwait;
    }
    
    // Find the shortest delay from all tracks
    double nextDelay = -1;
    for (int tk = 0; tk < TrackCount; tk++) {
        if (tkStatus[tk] < 0) continue;
        if (nextDelay == -1 || tkDelay[tk] < nextDelay) {
            nextDelay = tkDelay[tk];
        }
    }
    
    // Check if all tracks are ended
    bool allEnded = true;
    for (int tk = 0; tk < TrackCount; tk++) {
        if (tkStatus[tk] >= 0) {
            allEnded = false;
            break;
        }
    }
    
    if (allEnded) {
        // Either loop or stop playback
        if (loopwait > 0) {
            // Return to loop beginning
            for (int tk = 0; tk < TrackCount; tk++) {
                tkPtr[tk] = loPtr[tk];
                tkDelay[tk] = loDelay[tk];
                tkStatus[tk] = loStatus[tk];
            }
            playwait = loopwait;
        } else {
            // Stop playback
            isPlaying = false;
            return;
        }
    }
    
    // Update all track delays
    for (int tk = 0; tk < TrackCount; tk++) {
        tkDelay[tk] -= nextDelay;
    }
    
    // Schedule next event
    double t = nextDelay * Tempo / (DeltaTicks * 1000000.0);
    playwait += t;
}

// SDL audio callback function
void generateAudio(void* userdata, Uint8* stream, int len) {
    (void)userdata; // Unused parameter
    
    // Clear buffer
    memset(stream, 0, len);
    
    if (!isPlaying || paused || !g_midi_mixer) {
        return;
    }
    
    pthread_mutex_lock(&audioMutex);
    
    // Generate OPL audio into mixer channel
    int16_t opl_buffer[1024 * 2];
    int samples = len / (sizeof(int16_t) * AUDIO_CHANNELS);
    OPL_Generate(opl_buffer, samples);
    
    // Write OPL audio to mixer channel
    if (g_midi_mixer_channel >= 0) {
        mixer_write_channel(g_midi_mixer, g_midi_mixer_channel, opl_buffer, samples * AUDIO_CHANNELS);
    }
    
    // Mix channels
    //size_t mixed_samples = mixer_mix_channels(g_midi_mixer);
    
    // Get mixed output
    size_t output_size;
    int16_t* mixed_output = mixer_get_output(g_midi_mixer, &output_size);
    
    // Copy mixed audio to stream
    memcpy(stream, mixed_output, len);
    
    // Update playback time
    playTime += len / (double)(SAMPLE_RATE * sizeof(int16_t) * AUDIO_CHANNELS);
    
    // Process MIDI events based on timing
    playwait -= len / (double)(SAMPLE_RATE * sizeof(int16_t) * AUDIO_CHANNELS);
    while (playwait <= 0.1 && isPlaying) {
        processEvents();
    }
    
    pthread_mutex_unlock(&audioMutex);
}

// Initialize everything and start playback
void playMidiFile() {
    // Initialize variables for all channels
    for (int i = 0; i < 16; i++) {
        ChPatch[i] = 0;
        ChBend[i] = 0;
        ChVolume[i] = 127;
        ChPanning[i] = 64;
        ChVibrato[i] = 0;
    }
    
    // Reset playback state
    playTime = 0;
    isPlaying = true;
    paused = false;
    loopStart = false;
    loopEnd = false;
    playwait = 0;
    loopwait = 0;
    
    // Reset all OPL channels
    OPL_Reset();
    
    // Start audio playback
    SDL_PauseAudioDevice(audioDevice, 0);
    
    printf("Playback started. Press:\n");
    printf("  q - Quit\n");
    printf("  Space - Pause/Resume\n");
    printf("  +/- - Increase/Decrease Volume\n");
    printf("  n - Toggle Volume Normalization\n");
    printf("  Ctrl+C - Stop Playback\n");
    
#ifndef _WIN32
    // Disable input buffering on Unix-like systems
    struct termios new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
#endif
    
    // Set up signal handler for SIGINT (CTRL+C)
    signal(SIGINT, handle_sigint);
    
    // Reset keep_running flag
    keep_running = 1;
    
    // Main loop - handle console input
    while (isPlaying && keep_running) {
        // Check for key press without blocking
        if (kbhit()) {
#ifdef _WIN32
            int ch = _getch();
#else
            int ch = getchar();
#endif
            switch (ch) {
                case ' ':
                    paused = !paused;
                    printf("%s\n", paused ? "Paused" : "Resumed");
                    break;
                case 'q':
                    isPlaying = false;
                    break;
                case '+':
                case '=':
                    updateVolume(10);
                    break;
                case '-':
                    updateVolume(-10);
                    break;
                case 'n':
                    toggleNormalization();
                    break;
            }
        }
        
        // Sleep to prevent CPU hogging
#ifdef _WIN32
        Sleep(10); // 10 milliseconds
#else
        usleep(10000); // 10 milliseconds
#endif
    }
    
#ifndef _WIN32
    // Restore original terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
#endif
    
    // Stop audio
    SDL_PauseAudioDevice(audioDevice, 1);
    
    printf("Playback ended.\n");
}

// Update global volume
void updateVolume(int change) {
    pthread_mutex_lock(&audioMutex);
    
    globalVolume += change;
    if (globalVolume < 10) globalVolume = 10;
    if (globalVolume > 300) globalVolume = 300;
    
    // Remove the loop that calls OPL_SetVolume for all channels
    // The global volume will be applied in OPL_Generate instead
    
    pthread_mutex_unlock(&audioMutex);
    
    printf("Volume: %d%%\n", globalVolume);
}

// Toggle volume normalization
void toggleNormalization() {
    pthread_mutex_lock(&audioMutex);
    
    enableNormalization = !enableNormalization;
    
    pthread_mutex_unlock(&audioMutex);
    
    printf("Normalization: %s\n", enableNormalization ? "ON" : "OFF");
}
