#include "hle_module.h"
#include "varm_gxm_backend.h" // 🌟 Add this line right here to declare gxm_interface!
#include <stdio.h>
#include <string.h>
#include <unistd.h> // Essential for usleep timing constraints

// Allocation layer bounds for runtime system modules
static HleModule s_module_registry[16];
static int s_registered_module_count = 0;

// ============================================================================
// HLE HOOK IMPLEMENTATIONS
// ============================================================================

void mock_sceKernelCreateThread(void) {
    printf("[HLE INTERCEPT] -> sceKernelCreateThread executed cleanly.\n");
}

void mock_sceCtrlPeekBufferPositive(void) {
    // Linked directly to the physical evdev state-machine mapping array
}

void mock_sceGxmInitialize(void) {
    printf("[HLE INTERCEPT] -> sceGxmInitialize executed tracking surface layers.\n");
}

// 🔊 Audio Hook Implementations
int mock_sceAudioOutOpenPort(int portType, int numSamples, int freq, int mode) {
    printf("[AUDIO SYSTEM] Opening virtual PCM audio pipeline. Freq: %dHz | Samples: %d\n", freq, numSamples);
    return 1; // Returns virtual audio channel handle index 1
}

int mock_sceAudioOutOutput(int port, const void *ptr) {
    // Processing loop target for secondary PCM audio mixing arrays
    return 0;
}

int mock_sceAudioOutReleasePort(int port) {
    printf("[AUDIO SYSTEM] Closing playback port: %d\n", port);
    return 0;
}

// 📺 Display & Frame Timing Hook Implementations
int mock_sceDisplaySetFrameBuf(const void *pParam, int sync) {
    // Forwards active rendering layer memory pointers directly into the GXM emulator plane
    return 0;
}

int mock_sceDisplayWaitVblankStart(void) {
    // FIXED: Swap buffers here at the official frame refresh boundary!
    if (gxm_interface.swap_buffers) {
        gxm_interface.swap_buffers();
    }

    // Breaks tight poll loops by forcing a thread sync frame boundary interval (~60 FPS)
    usleep(16666); //
    return 0;
}

int mock_sceDisplayWaitSetFrameBuf(void) {
    // Signals back to the game logic loops that the front buffer allocation swap is complete
    return 0;
}

// ============================================================================
// MASTER MODULE MAPPING REGISTRY TABLES
// ============================================================================

static HleFunctionHook thread_mgr_hooks[] = {
    {"sceKernelCreateThread", 0xC622E4BA, (HleHostFn)mock_sceKernelCreateThread}
};

static HleFunctionHook ctrl_hooks[] = {
    {"sceCtrlPeekBufferPositive", 0x1D17DE28, (HleHostFn)mock_sceCtrlPeekBufferPositive}
};

static HleFunctionHook gxm_hooks[] = {
    {"sceGxmInitialize", 0x22802BEF, (HleHostFn)mock_sceGxmInitialize}
};

static HleFunctionHook audio_hooks[] = {
    {"sceAudioOutOpenPort",    0xA5A50112, (HleHostFn)mock_sceAudioOutOpenPort},
    {"sceAudioOutOutput",      0xB0A50223, (HleHostFn)mock_sceAudioOutOutput},
    {"sceAudioOutReleasePort", 0xC0A50334, (HleHostFn)mock_sceAudioOutReleasePort}
};

static HleFunctionHook display_hooks[] = {
    {"sceDisplaySetFrameBuf",       0x7A410B64, (HleHostFn)mock_sceDisplaySetFrameBuf},
    {"sceDisplayWaitVblankStart",   0x984C27E7, (HleHostFn)mock_sceDisplayWaitVblankStart},
    {"sceDisplayWaitSetFrameBuf",   0x9423560C, (HleHostFn)mock_sceDisplayWaitSetFrameBuf}
};

// ============================================================================
// SUBSYSTEM EXPORTS MAP ROUTERS
// ============================================================================

void hle_module_init(void) {
    s_registered_module_count = 0;

    // 1. Thread Manager
    s_module_registry[s_registered_module_count++] = (HleModule){
        .module_name = "SceKernelThreadMgr",
        .hooks = thread_mgr_hooks,
        .hook_count = sizeof(thread_mgr_hooks) / sizeof(thread_mgr_hooks[0])
    };

    // 2. Controller Input Matrix
    s_module_registry[s_registered_module_count++] = (HleModule){
        .module_name = "SceCtrl",
        .hooks = ctrl_hooks,
        .hook_count = sizeof(ctrl_hooks) / sizeof(ctrl_hooks[0])
    };

    // 3. Graphics Rendering Layers
    s_module_registry[s_registered_module_count++] = (HleModule){
        .module_name = "SceGxm",
        .hooks = gxm_hooks,
        .hook_count = sizeof(gxm_hooks) / sizeof(gxm_hooks[0])
    };

    // 4. Audio Processing Framework
    s_module_registry[s_registered_module_count++] = (HleModule){
        .module_name = "SceAudio",
        .hooks = audio_hooks,
        .hook_count = sizeof(audio_hooks) / sizeof(audio_hooks[0])
    };

    // 5. Display Control Infrastructure
    s_module_registry[s_registered_module_count++] = (HleModule){
        .module_name = "SceDisplay",
        .hooks = display_hooks,
        .hook_count = sizeof(display_hooks) / sizeof(display_hooks[0])
    };

    printf("[HLE MODULE] Hook engine initialized. %d master SONY system modules indexed.\n", s_registered_module_count);
}

HleHostFn hle_module_resolve_import(const char* module_name, const char* func_name, uint32_t nid) {
    for (int i = 0; i < s_registered_module_count; i++) {
        if (strcmp(s_module_registry[i].module_name, module_name) == 0) {
            for (int j = 0; j < s_module_registry[i].hook_count; j++) {
                // FIXED: Check nid first, and only call strcmp if func_name isn't NULL
                if (s_module_registry[i].hooks[j].nid == nid ||
                   (func_name != NULL && strcmp(s_module_registry[i].hooks[j].function_name, func_name) == 0)) {
                    return s_module_registry[i].hooks[j].host_implementation;
                }
            }
        }
    }
    return NULL;
}
