#ifndef HLE_MODULE_H
#define HLE_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include "varm_bridge.h" // Pulls in the core V_ARMRegisters structural context layout

// Host hook function layout pointers accept the active register configuration state
typedef void (*HleHostFn)(V_ARMRegisters *regs);

typedef struct {
    const char* function_name;
    uint32_t nid;
    HleHostFn host_implementation;
} HleFunctionHook;

typedef struct {
    const char* module_name;
    HleFunctionHook* hooks;
    int hook_count;
} HleModule;

// Peripheral Initialization and Resolution APIs
void hle_module_init(void);
HleHostFn hle_module_resolve_import(const char* module_name, const char* func_name, uint32_t nid);

// 🛠️ Option 2 Bridge: Map the old call in main.c to your new implementation
#define hle_module_bootstrap_symbols hle_module_init

#endif // HLE_MODULE_H
