#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

static SDL_AudioDeviceID s_audio_device = 0;

/**
 * varm_audio_init
 * Main initializer for Module E. Opens the host device playback channel
 * using standard 16-bit Signed Stereo parameters matching the Vita hardware.
 */
void varm_audio_init(void) {
    printf("[AUDIO] Initializing SceAudio emulation subsystem via SDL2...\n");

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        printf("[AUDIO] ERROR: Failed to open SDL Audio subsystem: %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec desired_spec;
    SDL_AudioSpec obtained_spec;

    SDL_zero(desired_spec);
    desired_spec.freq     = 44100;     // Native Vita standard frequency playback rule
    desired_spec.format   = AUDIO_S16SYS; // 16-bit Signed PCM sample width
    desired_spec.channels = 2;          // Stereo audio layout
    desired_spec.samples  = 1024;       // Low latency buffering execution size
    desired_spec.callback = NULL;       // Using queued pushing instead of manual polling threads

    s_audio_device = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &obtained_spec, 0);
    if (s_audio_device == 0) {
        printf("[AUDIO] ERROR: Failed to localize hardware speakers: %s\n", SDL_GetError());
        return;
    }

    // Unpause hardware playback instantly to prepare data lanes
    SDL_PauseAudioDevice(s_audio_device, 0);
    printf("[AUDIO] Audio pipeline active: 44100Hz, 16-bit Stereo PCM.\n");
}

/**
 * varm_audio_push_samples
 * Feeds a block of raw game PCM stream samples directly into the hardware playback buffer.
 */
void varm_audio_push_samples(const int16_t* sample_buffer, uint32_t byte_size) {
    if (s_audio_device == 0 || !sample_buffer || byte_size == 0) return;

    // Guard against desync audio latency build up.
    // If the queue holds more than 64KB of sound data, drop additional frames
    // to preserve tight video-to-audio sync during gameplay micro-stutters.
    if (SDL_GetQueuedAudioSize(s_audio_device) > 65536) {
        return;
    }

    SDL_QueueAudio(s_audio_device, sample_buffer, byte_size);
}

/**
 * varm_audio_shutdown
 * Clean tear down execution block. Ensures sound buffers don't loop on exit.
 */
void varm_audio_shutdown(void) {
    printf("[AUDIO] Tearing down SceAudio speaker subsystems...\n");
    if (s_audio_device != 0) {
        SDL_CloseAudioDevice(s_audio_device);
        s_audio_device = 0;
    }
}
