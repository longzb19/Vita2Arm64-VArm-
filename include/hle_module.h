#ifndef HLE_MODULE_H
#define HLE_MODULE_H

#include <stdint.h>

// Dynamic function pointer signature blueprint for host-side intercepts
typedef void (*HleHostFn)(void);

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

// Master setup lifecycle functions
void hle_module_init(void);
HleHostFn hle_module_resolve_import(const char* module_name, const char* func_name, uint32_t nid);

// --- Core Engine Intercept Envelopes ---
void mock_sceKernelCreateThread(void);
void mock_sceCtrlPeekBufferPositive(void);
void mock_sceGxmInitialize(void);

int mock_sceAudioOutOpenPort(int portType, int numSamples, int freq, int mode);
int mock_sceAudioOutOutput(int port, const void *ptr);
int mock_sceAudioOutReleasePort(int port);

int mock_sceDisplaySetFrameBuf(const void *pParam, int sync);
int mock_sceDisplayWaitVblankStart(void);
int mock_sceDisplayWaitSetFrameBuf(void);

#endif // HLE_MODULE_H
