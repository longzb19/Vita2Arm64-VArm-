#include "hle_module.h"
#include "varm_gxm_backend.h"
#include "hle_gxm.h"
#include "varm_input.h" // Links your global hardware analog variables and polling function
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <time.h>

// Macro to automatically calculate exact array size at compile time
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// FIXED: Define the official structural layout expected by native Sony games
typedef struct {
    uint64_t timeStamp;
    uint32_t buttons;
    uint8_t lx; // Left stick X (0-255)
    uint8_t ly; // Left stick Y (0-255)
    uint8_t rx; // Right stick X (0-255)
    uint8_t ry; // Right stick Y (0-255)
    uint8_t reserved[16];
} SceCtrlData;

// External hook to resolve guest virtual addresses to host pointer space
extern void* hle_kernel_resolve_address(uint32_t vaddr, uint32_t required_perms);

static HleModule s_module_registry[16];
static int s_registered_module_count = 0;

static SDL_AudioDeviceID s_audio_device = 0;
static bool s_audio_initialized = false;

// Target nanoseconds for a perfect 60Hz display refresh (16.666667ms)
#define VBLANK_TARGET_NS 16666667
static uint64_t g_vblank_count = 0;
static struct timespec g_last_vblank_time = {0, 0};

// Helper function to safely bring up the host SDL2 audio subsystem
static void ensure_audio_initialized(void) {
    if (s_audio_initialized) return;

    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;

    SDL_memset(&desired, 0, sizeof(desired));
    desired.freq     = 44100;
    desired.format   = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples  = 1024;

    s_audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (s_audio_device == 0) {
        printf("[AUDIO ERROR] Failed to initialize SDL Host Audio Device: %s\n", SDL_GetError());
    } else {
        SDL_PauseAudioDevice(s_audio_device, 0);
        printf("[AUDIO] Host SDL Audio Pipeline activated successfully at 44100Hz Stereo.\n");
    }
    s_audio_initialized = true;
}

// ============================================================================
// HLE HOOK IMPLEMENTATIONS
// ============================================================================

void mock_sceKernelCreateThread(V_ARMRegisters *regs) {
    printf("[HLE INTERCEPT] -> sceKernelCreateThread executed cleanly.\n");
    regs->r[0] = 0x12345678; // Dummy thread UID
}

void mock_sceCtrlPeekBufferPositive(V_ARMRegisters *regs) {
    uint32_t guest_pad_ptr = regs->r[1]; // Register r[1] holds the guest game's buffer destination
    uint32_t count = regs->r[2];         // Register r[2] holds structure count (usually 1)

    if (guest_pad_ptr == 0) {
        regs->r[0] = -1; // Return an error if the game passes a null pointer
        return;
    }

    // Resolve the guest RAM destination into a real host pointer we can write to
    SceCtrlData *pad_data = (SceCtrlData*)hle_kernel_resolve_address(guest_pad_ptr, 1);
    if (pad_data) {
        // Execute your physical SDL controller/keyboard polling engine!
        // This automatically handles configurations, keyboard fallbacks, and masks.
        pad_data->buttons = varm_input_poll();

        // Pass out your real-time global analog stick values (0-255 scaling)
        pad_data->lx = g_stick_lx;
        pad_data->ly = g_stick_ly;
        pad_data->rx = g_stick_rx;
        pad_data->ry = g_stick_ry;
    }

    regs->r[0] = count; // Return count back to register r[0] to signal successful polling
}

void mock_sceCtrlReadBufferPositive(V_ARMRegisters *regs) {
    // Structurally identical to Peek for execution, safe to alias directly
    mock_sceCtrlPeekBufferPositive(regs);
}

void mock_sceDisplayWaitVblankStart(V_ARMRegisters *regs) {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    // Initial frame pass registration
    if (g_last_vblank_time.tv_sec == 0 && g_last_vblank_time.tv_nsec == 0) {
        g_last_vblank_time = current_time;
        g_vblank_count++;
        regs->r[0] = 0;
        return;
    }

    // High-precision delta time calculation
    int64_t elapsed_ns = (current_time.tv_sec - g_last_vblank_time.tv_sec) * 1000000000LL
                         + (current_time.tv_nsec - g_last_vblank_time.tv_nsec);

    // If the game cycle is running too fast, throttle the execution thread
    if (elapsed_ns < VBLANK_TARGET_NS) {
        int64_t sleep_ns = VBLANK_TARGET_NS - elapsed_ns;
        struct timespec sleep_req;

        sleep_req.tv_sec = sleep_ns / 1000000000LL;
        sleep_req.tv_nsec = sleep_ns % 1000000000LL;

        nanosleep(&sleep_req, NULL);
        clock_gettime(CLOCK_MONOTONIC, &g_last_vblank_time);
    } else {
        g_last_vblank_time = current_time;
    }

    g_vblank_count++;
    regs->r[0] = 0; // Return Success to the ARM registers
}

void mock_sceAudioOutOpenPort(V_ARMRegisters *regs) {
    ensure_audio_initialized();
    printf("[HLE INTERCEPT] -> sceAudioOutOpenPort allocated hardware channels.\n");
    regs->r[0] = 1; // Return dummy audio port handle ID
}

void mock_sceAudioOutOutput(V_ARMRegisters *regs) {
    uint32_t guest_ptr = regs->r[1];

    if (s_audio_initialized && s_audio_device > 0 && guest_ptr != 0) {
        void* host_audio_buffer = hle_kernel_resolve_address(guest_ptr, 1);
        if (host_audio_buffer) {
            // Stream the PCM samples straight to the SDL device queue
            SDL_QueueAudio(s_audio_device, host_audio_buffer, 4096);
        }
    }
    regs->r[0] = 0;
}

void mock_sceAudioOutReleasePort(V_ARMRegisters *regs) {
    printf("[HLE INTERCEPT] -> sceAudioOutReleasePort terminated stream handle safely.\n");
    regs->r[0] = 0;
}

// ============================================================================
// MODULE HOOK REGISTRY TABLES
// ============================================================================

static HleFunctionHook thread_mgr_hooks[] = {
    {"sceKernelCreateThread", 0xC5C11EE7, mock_sceKernelCreateThread}
};

static HleFunctionHook ctrl_hooks[] = {
    {"sceCtrlPeekBufferPositive", 0x3A622605, mock_sceCtrlPeekBufferPositive},
    {"sceCtrlReadBufferPositive", 0x6A2774F3, mock_sceCtrlReadBufferPositive}
};

static HleFunctionHook gxm_hooks[] = {
    {"sceGxmInitialize",     0x60D505D4, mock_sceGxmInitialize},
    {"sceGxmTerminate",      0x11111111, mock_sceGxmTerminate},
    {"sceGxmCreateContext",  0x22222222, mock_sceGxmCreateContext},
    {"sceGxmDestroyContext", 0x33333333, mock_sceGxmDestroyContext},
    {"sceGxmMapMemory",      0x44444444, mock_sceGxmMapMemory},
    {"sceGxmUnmapMemory",    0x55555555, mock_sceGxmUnmapMemory}
};

static HleFunctionHook display_hooks[] = {
    {"sceDisplayWaitVblankStart", 0x164627E7, mock_sceDisplayWaitVblankStart}
};

static HleFunctionHook audio_hooks[] = {
    {"sceAudioOutOpenPort",    0xA92A332C, mock_sceAudioOutOpenPort},
    {"sceAudioOutOutput",      0xB3B53EE2, mock_sceAudioOutOutput},
    {"sceAudioOutReleasePort", 0x47B782F4, mock_sceAudioOutReleasePort}
};

void hle_module_init(void) {
    s_registered_module_count = 0;
    s_audio_initialized = false;

    // FIXED: Mount the global GLES rendering hooks and spin up EGL library configurations
    if (varm_gxm_init_renderer(VARM_RENDER_CORE_GLES, &gxm_interface) == 0) {
        if (gxm_interface.init_display) {
            gxm_interface.init_display();
        }
    } else {
        printf("[HLE ERROR] Critical: Graphics translation initialization bridge failed!\n");
    }

    // Registers the indexing modules using compile-time safe sizes
    s_module_registry[s_registered_module_count++] = (HleModule){"SceKernelThreadMgr", thread_mgr_hooks, ARRAY_SIZE(thread_mgr_hooks)};
    s_module_registry[s_registered_module_count++] = (HleModule){"SceCtrl",            ctrl_hooks,       ARRAY_SIZE(ctrl_hooks)};
    s_module_registry[s_registered_module_count++] = (HleModule){"SceGxm",             gxm_hooks,        ARRAY_SIZE(gxm_hooks)};
    s_module_registry[s_registered_module_count++] = (HleModule){"SceAudio",           audio_hooks,      ARRAY_SIZE(audio_hooks)};
    s_module_registry[s_registered_module_count++] = (HleModule){"SceDisplay",         display_hooks,    ARRAY_SIZE(display_hooks)};

    printf("[HLE MODULE] Hook engine active. %d real SONY system modules indexed.\n", s_registered_module_count);
}

HleHostFn hle_module_resolve_import(const char* module_name, const char* func_name, uint32_t nid) {
    for (int i = 0; i < s_registered_module_count; i++) {
        if (strcmp(s_module_registry[i].module_name, module_name) == 0) {
            for (int j = 0; j < s_module_registry[i].hook_count; j++) {
                // Matches based on the unique NID identifier or the string function name
                if (s_module_registry[i].hooks[j].nid == nid ||
                   (func_name && strcmp(s_module_registry[i].hooks[j].function_name, func_name) == 0)) {
                    return s_module_registry[i].hooks[j].host_implementation;
                }
            }
        }
    }
    return NULL;
}
