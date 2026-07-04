#include "hle_gxm.h"
#include "hle_kernel.h"
#include "varm_gxm_backend.h"
#include <stdio.h>
#include <stdlib.h>

// Mock structures to give the guest binary valid handle IDs
#define MOCK_GXM_CONTEXT_HANDLE 0xDEADC00D

void mock_sceGxmInitialize(V_ARMRegisters *regs) {
    // Guest layout: int sceGxmInitialize(const SceGxmInitializeParams *params);
    uint32_t params_vaddr = regs->r[0];

    printf("[HLE GXM] sceGxmInitialize(params_vaddr: 0x%08X)\n", params_vaddr);

    // Fall back to our linked system graphics hardware drivers
    if (gxm_interface.init_display) {
        int res = gxm_interface.init_display();
        regs->r[0] = res; // Return code from the rendering backend
    } else {
        printf("[HLE GXM ERROR] Graphics backend interface has no display initializer!\n");
        regs->r[0] = -1; // Return failure state
    }
}

void mock_sceGxmTerminate(V_ARMRegisters *regs) {
    printf("[HLE GXM] sceGxmTerminate()\n");

    if (gxm_interface.shutdown_display) {
        gxm_interface.shutdown_display();
    }
    regs->r[0] = 0; // Success
}

void mock_sceGxmCreateContext(V_ARMRegisters *regs) {
    // Guest layout: int sceGxmCreateContext(const SceGxmContextParams *params, SceGxmContext **context);
    uint32_t params_vaddr = regs->r[0];
    uint32_t out_context_ptr_vaddr = regs->r[1];

    printf("[HLE GXM] sceGxmCreateContext(params: 0x%08X, out_ptr: 0x%08X)\n", params_vaddr, out_context_ptr_vaddr);

    if (out_context_ptr_vaddr != 0) {
        // Resolve guest out pointer address to host memory space
        uint32_t *host_context_ptr = (uint32_t*)hle_kernel_resolve_address(out_context_ptr_vaddr, 1);
        if (host_context_ptr) {
            *host_context_ptr = MOCK_GXM_CONTEXT_HANDLE; // Assign tracking handle
        }
    }

    regs->r[0] = 0; // SCE_OK
}

void mock_sceGxmDestroyContext(V_ARMRegisters *regs) {
    // Guest layout: int sceGxmDestroyContext(SceGxmContext *context);
    uint32_t context = regs->r[0];
    printf("[HLE GXM] sceGxmDestroyContext(context handle: 0x%08X)\n", context);
    regs->r[0] = 0;
}

void mock_sceGxmMapMemory(V_ARMRegisters *regs) {
    // Guest layout: int sceGxmMapMemory(void *base, SceSize size, SceGxmMemoryAttribs attribs);
    uint32_t base_vaddr = regs->r[0];
    uint32_t size       = regs->r[1];
    uint32_t attribs    = regs->r[2];

    printf("[HLE GXM] sceGxmMapMemory(Base VAddr: 0x%08X, Size: %u bytes, Attribs: 0x%X)\n", base_vaddr, size, attribs);

    // Verify memory block actually exists inside our translation sandbox tracking arrays
    void* host_addr = hle_kernel_resolve_address(base_vaddr, 1);
    if (!host_addr) {
        printf("[HLE GXM ERROR] Title attempted to map unallocated memory block to GPU!\n");
        regs->r[0] = -1; // Fault code
        return;
    }

    regs->r[0] = 0; // Handshake complete
}

void mock_sceGxmUnmapMemory(V_ARMRegisters *regs) {
    // Guest layout: int sceGxmUnmapMemory(void *base);
    uint32_t base_vaddr = regs->r[0];
    printf("[HLE GXM] sceGxmUnmapMemory(Base VAddr: 0x%08X)\n", base_vaddr);
    regs->r[0] = 0;
}
