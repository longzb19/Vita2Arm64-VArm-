#ifndef HLE_MODULE_H
#define HLE_MODULE_H

#include <stdint.h>

// Clean typedef for host-side implementation function pointers
typedef void (*HleHostFn)(void);

// Represents a single hooked SONY system function
typedef struct {
    const char* function_name;     // e.g., "sceKernelCreateThread"
    uint32_t nid;                 // SONY's unique identifier hash for this function
    HleHostFn host_implementation; // Function pointer address [Cleaned up via typedef]
} HleFunctionHook;

// Represents a collection of functions inside a SONY system library module
typedef struct {
    const char* module_name;   // e.g., "SceKernelThreadMgr"
    HleFunctionHook* hooks;
    int hook_count;
} HleModule;

// Core Registry Management Functions
void hle_module_init(void);
HleHostFn hle_module_resolve_import(const char* module_name, const char* func_name, uint32_t nid);

// Real placeholder implementations for critical target systems
void mock_sceKernelCreateThread(void);
void mock_sceCtrlPeekBufferPositive(void);
void mock_sceGxmInitialize(void);

#endif // HLE_MODULE_H
