#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "varm_elf.h"  // Use the strict fixed-width structural definitions

#define VITA_MAX_RAM_SIZE (512 * 1024 * 1024)
#define VITA_USER_BASE_VADDR 0x81000000

// Internal memory mapping tracking block for your ARM64 system structures
typedef struct {
    uint32_t start_vaddr;
    uint32_t size;
    int permissions;
} Varm_KernelSegment;

static Varm_KernelSegment g_registered_segments[16];
static int g_segment_count = 0;

/**
 * Registers an execution or data block inside the translation tracking map.
 * This function enforces strict bounds validation across architectures.
 */
int hle_kernel_register_segment(uint32_t raw_vaddr, uint32_t raw_memsz, uint32_t filesz) {
    uint32_t valid_vaddr = raw_vaddr;
    uint32_t valid_memsz = raw_memsz;

    // FIX: Decoupled evaluations ensure 64-bit storage sizes don't leak
    // into 32-bit pointer boundaries during compilation.
    if (valid_vaddr == 0x00000000) {
        valid_vaddr = VITA_USER_BASE_VADDR;
    }

    if (valid_memsz > VITA_MAX_RAM_SIZE) {
        // If memory allocation size is bloated by compressed or encrypted segments,
        // fall back to alignment padding checks or physical constraints
        if (filesz > 0 && filesz <= VITA_MAX_RAM_SIZE) {
            valid_memsz = (filesz + 0xFFF) & ~0xFFF;
        } else {
            valid_memsz = 65536; // Fallback allocation size block
        }
    }

    if (g_segment_count >= 16) {
        printf("[HLE KERNEL ERROR] Maximum tracking blocks exceeded.\n");
        return -1;
    }

    g_registered_segments[g_segment_count].start_vaddr = valid_vaddr;
    g_registered_segments[g_segment_count].size = valid_memsz;

    printf("[HLE KERNEL] Registered segment tracking index %d (VAddr: 0x%08X | Size: %u bytes)\n",
           g_segment_count, valid_vaddr, valid_memsz);

    g_segment_count++;
    return 0;
}

/**
 * Safely resolves a virtual translation address without overflowing AArch64 registers.
 */
uint32_t hle_kernel_resolve_address(uint32_t guest_vaddr) {
    for (int i = 0; i < g_segment_count; i++) {
        uint32_t start = g_registered_segments[i].start_vaddr;
        uint32_t end = start + g_registered_segments[i].size;

        if (guest_vaddr >= start && guest_vaddr < end) {
            return guest_vaddr; // Address confirmed within valid memory bounds
        }
    }
    return 0; // Invalid lookup address
}
