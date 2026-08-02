#ifndef MIDIPLAYER_H
#define MIDIPLAYER_H

#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "virtual_mixer.h"

// MIDI constants
#define MAX_TRACKS      100
#define MAX_FILENAME    256

// MIDI event types
#define NOTE_OFF        0x80
#define NOTE_ON         0x90
#define POLY_PRESSURE   0xA0
#define CONTROL_CHANGE  0xB0
#define PROGRAM_CHANGE  0xC0
#define CHAN_PRESSURE   0xD0
#define PITCH_BEND      0xE0
#define SYSTEM_MESSAGE  0xF0
#define META_EVENT      0xFF

// MIDI meta event types
#define META_END_OF_TRACK   0x2F
#define META_TEMPO          0x51
#define META_TEXT           0x01

// Audio settings
#define SAMPLE_RATE     44100
#define AUDIO_CHANNELS  2
#define AUDIO_BUFFER    1024

// FM Instrument data structure - define this BEFORE the extern declaration
struct FMInstrument {
    unsigned char modChar1;
    unsigned char carChar1;
    unsigned char modChar2;
    unsigned char carChar2;
    unsigned char modChar3;
    unsigned char carChar3;
    unsigned char modChar4;
    unsigned char carChar4;
    unsigned char modChar5;
    unsigned char carChar5;
    unsigned char fbConn;
    unsigned char percNote;
};

// Lyric event structure
typedef struct {
    double timestamp;      // in seconds
    char* text;            // lyric text
    int syllableIndex;     // sequence number
} LyricEvent;

typedef struct {
    LyricEvent* events;
    int count;
    int capacity;
    char* title;
    char* artist;
    char* album;
    char* language;
} LyricData;

// Global variable declarations
extern struct FMInstrument adl[181];
extern VirtualMixer* g_midi_mixer;
extern int g_midi_mixer_channel;
extern LyricData* g_lyrics;  // Global lyrics data

// FM Synthesis Emulator
typedef struct {
    int channels;
    int sampleRate;
    Uint8 registers[256];
    float* buffer;
    
    struct {
        double phase[2];         // Phase for each operator
        double env[2];           // Envelope for each operator
        double freq;             // Frequency
        int algorithm;           // 0=FM, 1=Additive
        bool keyOn;              // Note is active
        double feedback;         // Feedback amount
        double output[2];        // Output of each operator
        
        // Additional parameters
        struct {
            int mult;            // Frequency multiplier
            int ksr;             // Key Scale Rate
            int trem;            // Tremolo enable
            int vib;             // Vibrato enable
            int sus;             // Sustain enable
            int waveform;        // Waveform select
            
            // Envelope parameters
            int attackRate;
            int decayRate;
            int sustainLevel;
            int releaseRate;
            double envPhase;     // Current envelope phase
        } op[2];                 // One for modulator, one for carrier
    } channelState[18];
} FMSynth;

// Function prototypes
void initFMInstruments();
bool initSDL();
void cleanup();
bool loadMidiFile(const char* filename);
void playMidiFile();
void handleEvents();
void updateVolume(int change);
void toggleNormalization();
void generateAudio(void* userdata, Uint8* stream, int len);

// Helper functions
unsigned long readVarLen(FILE* f);
int readString(FILE* f, int len, char* str);
unsigned long convertInteger(char* str, int len);
double ticksToSeconds(uint32_t ticks, uint32_t ticksPerBeat, int tempo);
void parseLyricText(const char* text, int len, LyricData* lyrics, double timestamp);

// Lyric extraction functions
LyricData* extractLyricsFromKar(const char* filename);
void exportToLrc(LyricData* lyrics, const char* outputFile);
LyricEvent* getLyricAtTime(LyricData* lyrics, double currentTime);
LyricEvent** getNextLyrics(LyricData* lyrics, double currentTime, int maxCount, int* count);
void freeLyrics(LyricData* lyrics);
void printLyrics(LyricData* lyrics);
void displayCurrentLyric(double currentTime);

#endif // MIDIPLAYER_H
