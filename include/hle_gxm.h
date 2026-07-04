#ifndef HLE_GXM_H
#define HLE_GXM_H

#include "varm_bridge.h"

// SceGxm HLE Intercept Hooks
void mock_sceGxmInitialize(V_ARMRegisters *regs);
void mock_sceGxmTerminate(V_ARMRegisters *regs);
void mock_sceGxmCreateContext(V_ARMRegisters *regs);
void mock_sceGxmDestroyContext(V_ARMRegisters *regs);
void mock_sceGxmMapMemory(V_ARMRegisters *regs);
void mock_sceGxmUnmapMemory(V_ARMRegisters *regs);

#endif // HLE_GXM_H
