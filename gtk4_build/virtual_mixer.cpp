#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "virtual_mixer.h"

// Helper function to clamp values
static inline int16_t clamp_sample(float sample) {
    if (sample > 32767.0f) return 32767;
    if (sample < -32768.0f) return -32768;
    return (int16_t)sample;
}

// Initialize the virtual mixer
VirtualMixer* mixer_init(int sample_rate, int num_channels, bool normalize) {
    VirtualMixer* mixer = calloc(1, sizeof(VirtualMixer));
    if (!mixer) return NULL;

    mixer->sample_rate = sample_rate > 0 ? sample_rate : MIXER_SAMPLE_RATE;
    mixer->num_channels = num_channels > 0 ? num_channels : 2;
    mixer->normalize = normalize;

    // Allocate output buffer
    mixer->output_buffer_size = MIXER_BUFFER_SIZE * sizeof(int16_t) * mixer->num_channels;
    mixer->output_buffer = malloc(mixer->output_buffer_size);
    
    if (!mixer->output_buffer) {
        free(mixer);
        return NULL;
    }

    // Initialize mixer channels
    for (int i = 0; i < MAX_MIXER_CHANNELS; i++) {
        mixer->channels[i].buffer = NULL;
        mixer->channels[i].active = false;
        mixer->channels[i].volume = 1.0f;
        mixer->channels[i].pan = 0.0f;
    }

    return mixer;
}

// Free the mixer resources
void mixer_free(VirtualMixer* mixer) {
    if (!mixer) return;

    // Free individual channel buffers
    for (int i = 0; i < MAX_MIXER_CHANNELS; i++) {
        if (mixer->channels[i].buffer) {
            free(mixer->channels[i].buffer);
        }
    }

    // Free output buffer
    if (mixer->output_buffer) {
        free(mixer->output_buffer);
    }

    free(mixer);
}

// Allocate a new mixer channel
int mixer_allocate_channel(VirtualMixer* mixer) {
    if (!mixer) return -1;

    for (int i = 0; i < MAX_MIXER_CHANNELS; i++) {
        if (!mixer->channels[i].active) {
            mixer->channels[i].buffer = malloc(MIXER_BUFFER_SIZE * sizeof(int16_t));
            if (!mixer->channels[i].buffer) return -1;

            mixer->channels[i].buffer_size = MIXER_BUFFER_SIZE;
            mixer->channels[i].write_pos = 0;
            mixer->channels[i].read_pos = 0;
            mixer->channels[i].active = true;
            mixer->channels[i].volume = 1.0f;
            mixer->channels[i].pan = 0.0f;

            return i;
        }
    }

    return -1;
}

// Release a mixer channel
void mixer_release_channel(VirtualMixer* mixer, int channel_id) {
    if (!mixer || channel_id < 0 || channel_id >= MAX_MIXER_CHANNELS) return;

    if (mixer->channels[channel_id].buffer) {
        free(mixer->channels[channel_id].buffer);
        mixer->channels[channel_id].buffer = NULL;
    }

    mixer->channels[channel_id].active = false;
    mixer->channels[channel_id].write_pos = 0;
    mixer->channels[channel_id].read_pos = 0;
}

// Write audio data to a mixer channel
void mixer_write_channel(VirtualMixer* mixer, int channel_id, 
                         const int16_t* data, size_t size) {
    if (!mixer || channel_id < 0 || channel_id >= MAX_MIXER_CHANNELS) return;

    MixerChannel* channel = &mixer->channels[channel_id];
    if (!channel->active || !channel->buffer) return;

    // Resize buffer if needed
    if (channel->write_pos + size > channel->buffer_size) {
        size_t new_size = channel->buffer_size * 2;
        int16_t* new_buffer = realloc(channel->buffer, new_size * sizeof(int16_t));
        
        if (!new_buffer) return;  // Allocation failed

        channel->buffer = new_buffer;
        channel->buffer_size = new_size;
    }

    // Copy data to channel buffer
    memcpy(channel->buffer + channel->write_pos, data, size * sizeof(int16_t));
    channel->write_pos += size;
}

// Mix all active channels
size_t mixer_mix_channels(VirtualMixer* mixer) {
    if (!mixer) return 0;

    // Reset output buffer
    memset(mixer->output_buffer, 0, mixer->output_buffer_size);

    size_t max_read_size = 0;
    size_t total_samples = 0;

    // Find max read size among active channels
    for (int i = 0; i < MAX_MIXER_CHANNELS; i++) {
        if (mixer->channels[i].active) {
            size_t remaining = mixer->channels[i].write_pos - mixer->channels[i].read_pos;
            if (remaining > max_read_size) {
                max_read_size = remaining;
            }
        }
    }

    // Mix samples
    total_samples = max_read_size;
    for (size_t sample = 0; sample < max_read_size; sample++) {
        float left_mix = 0.0f;
        float right_mix = 0.0f;

        for (int i = 0; i < MAX_MIXER_CHANNELS; i++) {
            MixerChannel* channel = &mixer->channels[i];
            
            if (channel->active && channel->read_pos < channel->write_pos) {
                float sample_val = (float)channel->buffer[channel->read_pos];
                float scaled_vol = sample_val * channel->volume;

                // Apply panning
                float pan_left = fminf(1.0f, 1.0f - channel->pan);
                float pan_right = fminf(1.0f, 1.0f + channel->pan);

                left_mix += scaled_vol * pan_left;
                right_mix += scaled_vol * pan_right;

                channel->read_pos++;
            }
        }

        // Normalization (optional)
        if (mixer->normalize) {
            // Simple normalization - divide by number of active channels
            int active_count = 0;
            for (int i = 0; i < MAX_MIXER_CHANNELS; i++) {
                if (mixer->channels[i].active) active_count++;
            }
            
            if (active_count > 0) {
                left_mix /= active_count;
                right_mix /= active_count;
            }
        }

        // Convert back to 16-bit samples
        if (mixer->num_channels == 2) {
            mixer->output_buffer[sample * 2] = clamp_sample(left_mix);
            mixer->output_buffer[sample * 2 + 1] = clamp_sample(right_mix);
        } else {
            // Mono mixing
            float mono_mix = (left_mix + right_mix) * 0.5f;
            mixer->output_buffer[sample] = clamp_sample(mono_mix);
        }
    }

    // Cleanup channels that have been fully read
    for (int i = 0; i < MAX_MIXER_CHANNELS; i++) {
        MixerChannel* channel = &mixer->channels[i];
        
        if (channel->active && channel->read_pos >= channel->write_pos) {
            channel->read_pos = 0;
            channel->write_pos = 0;
        }
    }

    return total_samples;
}

// Get the mixed output buffer
int16_t* mixer_get_output(VirtualMixer* mixer, size_t* out_size) {
    if (!mixer) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    if (out_size) {
        *out_size = mixer->num_channels * (mixer->output_buffer_size / sizeof(int16_t));
    }

    return mixer->output_buffer;
}

// Set channel volume and pan
void mixer_set_channel_volume(VirtualMixer* mixer, int channel_id, 
                              float volume, float pan) {
    if (!mixer || channel_id < 0 || channel_id >= MAX_MIXER_CHANNELS) return;

    MixerChannel* channel = &mixer->channels[channel_id];
    
    if (!channel->active) return;

    // Clamp volume between 0 and 1
    channel->volume = fmaxf(0.0f, fminf(1.0f, volume));
    
    // Clamp pan between -1 and 1
    channel->pan = fmaxf(-1.0f, fminf(1.0f, pan));
}
