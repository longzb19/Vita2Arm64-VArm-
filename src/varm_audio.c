#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "hle_kernel.h" // 🔗 Links to your kernel virtual address resolver

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
    desired_spec.samples  = 2048;       // 🛠️ INCREASED from 1024 to give JIT compilation headroom
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

    // 🛠️ DEADLOCK PREVENTION:
    // If ALSA fails due to an underrun, it stops draining.
    // We limit our wait time to 5ms so we don't freeze the JIT thread and graphics pipeline.
    int timeout_ms = 0;
    while (SDL_GetQueuedAudioSize(s_audio_device) > 32768) {
        SDL_Delay(1);
        timeout_ms++;
        if (timeout_ms > 5) {
            // Audio engine is stalled/dead. Clear the backlog and drop samples to keep rendering alive!
            SDL_ClearQueuedAudio(s_audio_device);
            break;
        }
    }

    if (SDL_QueueAudio(s_audio_device, sample_buffer, byte_size) < 0) {
        // Log failures non-intrusively to prevent terminal flooding
        static uint32_t fail_count = 0;
        if (fail_count++ % 100 == 0) {
            printf("[AUDIO] SDL_QueueAudio error: %s\n", SDL_GetError());
        }
    }
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

// ========================================================================
// 🛠️ HIGH-LEVEL EMULATION (HLE) AUDIO BRIDGE HOOKS
// ========================================================================

static int s_port_samples[8] = { 256, 256, 256, 256, 256, 256, 256, 256 };

/**
 * varm_audio_out_open_port
 * HLE hook intercepted when the game wants to register a new stereo output track.
 */
int varm_audio_out_open_port(int port_type, int num_samples, int freq, int mode) {
    printf("[HLE BRIDGE] sceAudioOutOpenPort invoked (Samples: %d, Freq: %d Hz)\n", num_samples, freq);

    // Track sample configuration count for this port (1-based index mapping)
    int port_id = 1;
    if (port_id >= 1 && port_id <= 8) {
        s_port_samples[port_id - 1] = num_samples;
    }

    return port_id;
}

/**
 * varm_audio_out_output
 * HLE hook intercepted when the game sends sound data to a port.
 * Translates the guest virtual memory buffer address and pushes it to SDL.
 */
int varm_audio_out_output(int port, uint32_t sample_buffer_vaddr) {
    if (sample_buffer_vaddr == 0) return -1;

    int num_samples = 256;
    if (port >= 1 && port <= 8) {
        num_samples = s_port_samples[port - 1];
    }
    // Vita stereo sound: 2 channels (Stereo) * 2 bytes per sample (16-bit PCM)
    uint32_t byte_size = num_samples * 2 * sizeof(int16_t);

    // Resolve the game's guest virtual address into a readable pointer for your computer
    void* host_buffer = hle_kernel_resolve_address(sample_buffer_vaddr, 1);

    if (host_buffer) {
        // Pass the resolved memory directly down to your SDL pusher engine with accurate byte size
        varm_audio_push_samples((const int16_t*)host_buffer, byte_size);
    }

    return 0; // Return success status code to the guest loop
}

/**
 * varm_audio_out_release_port
 * HLE hook to close down an active guest game sound port.
 */
int varm_audio_out_release_port(int port) {
    printf("[HLE BRIDGE] sceAudioOutReleasePort invoked for Port %d\n", port);
    return 0;
}
