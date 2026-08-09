#include <opus/opus.h>
#include <stdio.h>

int main() {
    int error;
    OpusEncoder *encoder = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &error);
    if (error != OPUS_OK || !encoder) {
        printf("Opus encoder creation failed: %s\n", opus_strerror(error));
        return 1;
    }
    printf("Opus encoder created successfully.\n");
    opus_encoder_destroy(encoder);
    return 0;
}

