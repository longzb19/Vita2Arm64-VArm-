#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h> // Critical for allocating physical pages via mmap
#include <unistd.h>
#include "hle_kernel.h"
#include "hle_module.h" // Links with HleHostFn and import resolution
#include "varm_ctrl.h"  // 📱 Links to physical input system routines
#include "hle_thread.h" // 📱 Links to Linux pthread synchronization hooks

// Forward declarations for graphics hooks to prevent linking layout conflicts
extern int varm_gxm_init_renderer(void);
extern int varm_gxm_parse_command_buffer(void);

#define VITA_MAX_RAM_SIZE    (750 * 1024 * 1024)
#define VITA_USER_BASE_VADDR 0x81000000

#define PF_X        (1 << 0)
#define PF_W        (1 << 1)
#define PF_R        (1 << 2)

// ========================================================================
// 📁 STRUCT DEFINITIONS & GLOBALS
// ========================================================================

typedef struct {
    uint32_t start_vaddr;
    uint32_t size;
    uint32_t permissions;
    void* host_map_ptr; // Holds the real 64-bit Linux memory address
} Varm_KernelSegment;

#define MAX_SEGMENTS 32
Varm_KernelSegment g_registered_segments[MAX_SEGMENTS];
int g_segment_count = 0;

// ========================================================================
// 🚀 MEMORY MANAGEMENT & INTERCEPT ROUTINES
// ========================================================================

int hle_kernel_register_segment(uint32_t raw_vaddr, uint32_t raw_memsz, uint32_t filesz, uint32_t elf_flags) {
    if (g_segment_count >= MAX_SEGMENTS) {
        printf("[HLE KERNEL ERROR] Maximum segment limit reached (%d).\n", MAX_SEGMENTS);
        return -1;
    }

    // Align size to host page size boundary (typically 4KB) for mmap stability
    long page_size = sysconf(_SC_PAGESIZE);
    uint32_t aligned_size = (raw_memsz + page_size - 1) & ~(page_size - 1);

    // Set page protections matching standard ELF allocation schemes
    int prot = PROT_READ | PROT_WRITE;
    if (elf_flags & PF_X) prot |= PROT_EXEC;

    void* host_ptr = mmap(NULL, aligned_size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (host_ptr == MAP_FAILED) {
        perror("[HLE KERNEL ERROR] Allocation failed via host mmap call");
        return -1;
    }

    // Clear memory space to guarantee clean guest allocations without junk context bleeding
    memset(host_ptr, 0, aligned_size);

    g_registered_segments[g_segment_count].start_vaddr = raw_vaddr;
    g_registered_segments[g_segment_count].size = aligned_size;
    g_registered_segments[g_segment_count].permissions = elf_flags;
    g_registered_segments[g_segment_count].host_map_ptr = host_ptr;

    printf("[HLE KERNEL] Registered segment %d: 0x%08X - 0x%08X (%u bytes aligned) -> Host: %p\n",
           g_segment_count, raw_vaddr, raw_vaddr + aligned_size, aligned_size, host_ptr);

    g_segment_count++;
    return 0;
}

void* hle_kernel_resolve_address(uint32_t guest_vaddr, uint32_t required_perms) {
    for (int i = 0; i < g_segment_count; i++) {
        uint32_t start = g_registered_segments[i].start_vaddr;
        uint32_t size  = g_registered_segments[i].size;

        if (guest_vaddr >= start && guest_vaddr < (start + size)) {
            // Verify permission sets if required_perms enforcement flag is requested
            if (required_perms && !(g_registered_segments[i].permissions & required_perms)) {
                printf("[HLE KERNEL PERM VIOLATION] Unauthorized access mapping flag failure at 0x%08X\n", guest_vaddr);
                return NULL;
            }
            // Compute the real 64-bit host pointer offset relative to the mmap segment boundary
            uintptr_t offset = guest_vaddr - start;
            return (void*)((uintptr_t)g_registered_segments[i].host_map_ptr + offset);
        }
    }
    return NULL;
}

void hle_kernel_dump_maps(void) {
    printf("\n=== HLE SYSTEM MEMORY ROADMAP DUMP ===\n");
    for (int i = 0; i < g_segment_count; i++) {
        uint32_t start = g_registered_segments[i].start_vaddr;
        uint32_t perms = g_registered_segments[i].permissions;
        printf(" Map #%d: 0x%08X - 0x%08X -> Host: %p | [%c%c%c]\n", i, start, start + g_registered_segments[i].size,
               g_registered_segments[i].host_map_ptr,
               (perms & PF_R) ? 'r' : '-', (perms & PF_W) ? 'w' : '-', (perms & PF_X) ? 'x' : '-');
    }
    printf("======================================\n");
}

// ========================================================================
// 🎯 NID MOLECULAR RESOLUTION HUB
// ========================================================================

void* hle_resolve_nid(uint32_t library_nid, uint32_t func_nid) {
    // Scans modules systematically to resolve the guest hash to a host functional callback
    const char* target_modules[] = {
        "SceKernelThreadMgr",
        "SceCtrl",
        "SceGxm",
        "SceAudio",
        "SceDisplay"
    };

    // 1. Core library scope lookup loop
    for (size_t i = 0; i < sizeof(target_modules) / sizeof(target_modules[0]); i++) {
        HleHostFn resolved_fn = hle_module_resolve_import(target_modules[i], NULL, func_nid);
        if (resolved_fn != NULL) {
            return (void*)resolved_fn;
        }
    }

    // 2. Global fallback sweep if module grouping structures are relaxed or transparent
    HleHostFn global_fallback = hle_module_resolve_import(NULL, NULL, func_nid);
    if (global_fallback != NULL) {
        return (void*)global_fallback;
    }

    printf("\033[1;31m[HLE KERNEL ERROR] Unresolved NID hash pointer signature: 0x%08X (Lib NID: 0x%08X)\033[0m\n", func_nid, library_nid);
    return NULL;
}

// ========================================================================
// 🏁 LIFECYCLE CONTROLLERS
// ========================================================================

void hle_kernel_init(void) {
    printf("[HLE KERNEL] Initializing translation layer subsystems...\n");
    g_segment_count = 0;
    memset(g_registered_segments, 0, sizeof(g_registered_segments));
}

void hle_kernel_shutdown(void) {
    printf("[HLE KERNEL] Unmapping active virtual segment regions from host execution context...\n");
    for (int i = 0; i < g_segment_count; i++) {
        if (g_registered_segments[i].host_map_ptr != NULL) {
            munmap(g_registered_segments[i].host_map_ptr, g_registered_segments[i].size);
            g_registered_segments[i].host_map_ptr = NULL;
        }
    }
    g_segment_count = 0;
    printf("[HLE KERNEL] Shutdown cycles complete.\n");
}
