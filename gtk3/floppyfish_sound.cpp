#include <SDL2/SDL.h>
#include <vector>
#include <math.h>
#include <cstdio>
// --- Procedural sound effects -----------------------------------------------
// No audio assets on disk - both sounds are synthesized on the fly as short
// PCM buffers and handed to SDL's audio queue, so there's nothing to ship
// or load.
SDL_AudioDeviceID g_audio_dev = 0;
SDL_AudioSpec g_audio_spec;

// Queues a short frequency sweep from f0 to f1 Hz over dur seconds, with a
// linear fade-out envelope so it doesn't click at the end.
void floppyfish_audio_play_sweep(double f0, double f1, double dur, double volume) {
    if (g_audio_dev == 0) return;
    int sr = g_audio_spec.freq;
    int n = (int)(sr * dur);
    if (n <= 0) return;
    std::vector<Sint16> buf(n);
    double phase = 0.0;
    for (int i = 0; i < n; i++) {
        double t = (double)i / sr;
        double freq = f0 + (f1 - f0) * (t / dur);
        phase += 2.0 * M_PI * freq / sr;
        double env = 1.0 - (double)i / n;
        double s = sin(phase) * env * volume;
        buf[i] = (Sint16)(s * 32000.0);
    }
    SDL_QueueAudio(g_audio_dev, buf.data(), (Uint32)(buf.size() * sizeof(Sint16)));
}


void floppyfish_audio_init() {
    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &g_audio_spec, 0);
    if (g_audio_dev != 0) SDL_PauseAudioDevice(g_audio_dev, 0);
    else fprintf(stderr, "Audio unavailable: %s\n", SDL_GetError());
}

void floppyfish_audio_play_dead() {
    // 1. Sharp watery impact (fast upward blip)
    floppyfish_audio_play_sweep(300.0, 520.0, 0.03, 0.40);

    // 2. Droplet scatter (three tiny jitter sweeps)
    for (int i = 0; i < 3; i++) {
        double start = 600.0 + (rand() % 200);   // 600–800 Hz
        double end   = 350.0 + (rand() % 150);   // 350–500 Hz
        floppyfish_audio_play_sweep(start, end, 0.02, 0.35);
    }

    // 3. Heavy wet flop (slow downward bend)
    floppyfish_audio_play_sweep(480.0, 160.0, 0.12, 0.45);
}

// Queues a short sequence of steady notes back to back, each with its own
// quick decay envelope - used for the little rising "chime" on scoring.
void floppyfish_audio_play_notes(const double *freqs, const double *durs, int count, double volume) {
    if (g_audio_dev == 0) return;
    int sr = g_audio_spec.freq;
    std::vector<Sint16> buf;
    for (int k = 0; k < count; k++) {
        int n = (int)(sr * durs[k]);
        double phase = 0.0;
        for (int i = 0; i < n; i++) {
            double env = 1.0 - (double)i / n;
            phase += 2.0 * M_PI * freqs[k] / sr;
            double s = sin(phase) * env * volume;
            buf.push_back((Sint16)(s * 32000.0));
        }
    }
    if (!buf.empty()) SDL_QueueAudio(g_audio_dev, buf.data(), (Uint32)(buf.size() * sizeof(Sint16)));
}

// A quick upward "whoosh" for the flap.
void floppyfish_audio_play_flap() {
    floppyfish_audio_play_sweep(320.0, 640.0, 0.09, 0.28);
}

// A bright two-note rising chime for scoring a point.
void floppyfish_audio_play_score() {
    static const double freqs[2] = {880.0, 1318.5};  // A5 -> E6
    static const double durs[2]  = {0.07, 0.11};
    floppyfish_audio_play_notes(freqs, durs, 2, 0.30);
}

