#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "varm_elf.h"  // Use the strict fixed-width structural definitions

#define VITA_MAX_RAM_SIZE (512 * 1024 * 1024)
#define VITA_USER_BASE_VADDR 0x81000000

// Standard ELF/SCE Flag Bindings for Memory Mapping Protections
#define PF_X        (1 << 0)    // Execute
#define PF_W        (1 << 1)    // Write
#define PF_R        (1 << 2)    // Read

// Internal memory mapping tracking block for your ARM64 system structures
typedef struct {
    uint32_t start_vaddr;
    uint32_t size;
    uint32_t permissions;  // Properly tracked protection flags (rwx)
    void* host_map_ptr; // Hook for host translation storage boundaries
} Varm_KernelSegment;

static Varm_KernelSegment g_registered_segments[16];
static int g_segment_count = 0;

/**
 * Registers an execution or data block inside the translation tracking map.
 * Enforces strict bounds validation, overlap verification, and maps permissions.
 */
int hle_kernel_register_segment(uint32_t raw_vaddr, uint32_t raw_memsz, uint32_t filesz, uint32_t elf_flags) {
    uint32_t valid_vaddr = raw_vaddr;
    uint32_t valid_memsz = raw_memsz;

    // Decoupled evaluations ensure 64-bit storage sizes don't leak into 32-bit boundaries
    if (valid_vaddr == 0x00000000) {
        valid_vaddr = VITA_USER_BASE_VADDR;
    }

    if (valid_memsz > VITA_MAX_RAM_SIZE) {
        // Fall back to alignment padding checks or physical constraints
        if (filesz > 0 && filesz <= VITA_MAX_RAM_SIZE) {
            valid_memsz = (filesz + 0xFFF) & ~0xFFF;
        } else {
            valid_memsz = 65536; // Fallback allocation size block
        }
    }

    uint32_t valid_end = valid_vaddr + valid_memsz;

    // =========================================================================
    // DEFENSIVE CHECK: PREVENT OVERLAPPING KERNEL MEMORY MAPS
    // =========================================================================
    for (int i = 0; i < g_segment_count; i++) {
        uint32_t ex_start = g_registered_segments[i].start_vaddr;
        uint32_t ex_end = ex_start + g_registered_segments[i].size;

        if ((valid_vaddr >= ex_start && valid_vaddr < ex_end) ||
            (valid_end > ex_start && valid_end <= ex_end) ||
            (valid_vaddr <= ex_start && valid_end >= ex_end)) {

            printf("[HLE KERNEL WARNING] Overlap detected with index %d! Collipsing region adjustment.\n", i);
            return -2;
        }
    }

    if (g_segment_count >= 16) {
        printf("[HLE KERNEL ERROR] Maximum tracking blocks exceeded.\n");
        return -1;
    }

    // Assign corrected boundaries safely
    g_registered_segments[g_segment_count].start_vaddr = valid_vaddr;
    g_registered_segments[g_segment_count].size = valid_memsz;
    g_registered_segments[g_segment_count].permissions = elf_flags;
    g_registered_segments[g_segment_count].host_map_ptr = NULL; // Ready for mmap integration later

    // Generate readable string representation of permissions for clean logging
    char perm_str[4] = "---";
    if (elf_flags & PF_R) perm_str[0] = 'r';
    if (elf_flags & PF_W) perm_str[1] = 'w';
    if (elf_flags & PF_X) perm_str[2] = 'x';

    printf("[HLE KERNEL] Registered segment tracking index %d (VAddr: 0x%08X | Size: %u bytes | Perms: [%s])\n",
           g_segment_count, valid_vaddr, valid_memsz, perm_str);

    g_segment_count++;
    return 0;
}

/**
 * Safely resolves a virtual translation address and verifies permission clearance.
 */
uint32_t hle_kernel_resolve_address(uint32_t guest_vaddr, uint32_t required_perms) {
    for (int i = 0; i < g_segment_count; i++) {
        uint32_t start = g_registered_segments[i].start_vaddr;
        uint32_t end = start + g_registered_segments[i].size;

        if (guest_vaddr >= start && guest_vaddr < end) {
            // Validate memory protection access violations natively
            if (required_perms != 0 && !(g_registered_segments[i].permissions & required_perms)) {
                printf("[KERNEL MMU FAULT] Protection Violation at 0x%08X (Missing required attributes)\n", guest_vaddr);
                return 0;
            }
            return guest_vaddr; // Address confirmed within valid memory bounds
        }
    }
    return 0; // Invalid lookup address (Segfault)
}

/**
 * Diagnostic helper to dump current memory mappings on translation faults.
 */
void hle_kernel_dump_maps(void) {
    printf("\n=== HLE SYSTEM MEMORY ROADMAP DUMP ===\n");
    for (int i = 0; i < g_segment_count; i++) {
        uint32_t start = g_registered_segments[i].start_vaddr;
        uint32_t perms = g_registered_segments[i].permissions;
        printf(" Map #%d: 0x%08X - 0x%08X | [%c%c%c]\n", i, start, start + g_registered_segments[i].size,
               (perms & PF_R) ? 'r' : '-', (perms & PF_W) ? 'w' : '-', (perms & PF_X) ? 'x' : '-');
    }
    printf("======================================\n");
}
