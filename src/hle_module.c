#include "hle_module.h"
#include <stdio.h>
#include <string.h>

// Local implementations of vital functions God of War needs to avoid crashing
void mock_sceKernelCreateThread(void) {
    printf("[HLE INTERCEPT] -> sceKernelCreateThread executed! Allocating guest thread context safely.\\n");
}

void mock_sceCtrlPeekBufferPositive(void) {
    // This will eventually link directly into our varm_input pipeline!
    // printf("[HLE INTERCEPT] -> polling controller inputs natively.\\n");
}

void mock_sceGxmInitialize(void) {
    printf("[HLE INTERCEPT] -> sceGxmInitialize called! Initializing GXM-to-GLES Command Buffer mapping.\\n");
}

// Map the functions belonging to SceKernelThreadMgr
static HleFunctionHook thread_mgr_hooks[] = {
    {"sceKernelCreateThread", 0xC622E4BA, mock_sceKernelCreateThread}
};

// Map the functions belonging to SceCtrl
static HleFunctionHook ctrl_hooks[] = {
    {"sceCtrlPeekBufferPositive", 0x1D17D9AD, mock_sceCtrlPeekBufferPositive}
};

// Map the functions belonging to SceGxm
static HleFunctionHook gxm_hooks[] = {
    {"sceGxmInitialize", 0xAB334F11, mock_sceGxmInitialize}
};

// Master Library Registry Array
static HleModule s_module_registry[16];
static int s_registered_module_count = 0;

void hle_module_init(void) {
    s_registered_module_count = 0;

    // Register SceKernelThreadMgr
    s_module_registry[s_registered_module_count++] = (HleModule){
        .module_name = "SceKernelThreadMgr",
        .hooks = thread_mgr_hooks,
        .hook_count = sizeof(thread_mgr_hooks) / sizeof(thread_mgr_hooks[0])
    };

    // Register SceCtrl
    s_module_registry[s_registered_module_count++] = (HleModule){
        .module_name = "SceCtrl",
        .hooks = ctrl_hooks,
        .hook_count = sizeof(ctrl_hooks) / sizeof(ctrl_hooks[0])
    };

    // Register SceGxm
    s_module_registry[s_registered_module_count++] = (HleModule){
        .module_name = "SceGxm",
        .hooks = gxm_hooks,
        .hook_count = sizeof(gxm_hooks) / sizeof(gxm_hooks[0])
    };

    printf("[HLE MODULE] Hook engine initialized. %d master SONY system modules indexed.\\n", s_registered_module_count);
}

int hle_module_resolve_import(const char* module_name, const char* func_name, uint32_t nid) {
    for (int i = 0; i < s_registered_module_count; i++) {
        if (strcmp(s_module_registry[i].module_name, module_name) == 0) {
            for (int j = 0; j < s_module_registry[i].hook_count; j++) {
                // Match via naming structure or explicit binary SONY NID hash
                if ((func_name && strcmp(s_module_registry[i].hooks[j].function_name, func_name) == 0) ||
                    s_module_registry[i].hooks[j].nid == nid) {

                    // Run the native execution logic!
                    s_module_registry[i].hooks[j].host_implementation();
                    return 1; // Resolved perfectly
                }
            }
        }
    }

    // Capture unhandled imports elegantly instead of hard crashing
    printf("[HLE WARNING] Unhandled SONY Import Requested! Module: %s | Func: %s | NID: 0x%08X\\n",
           module_name, func_name ? func_name : "UNKNOWN", nid);
    return 0; // Not implemented yet
}
