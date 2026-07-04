#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h> // 👈 Critical for allocating physical pages via mmap
#include <unistd.h>
#include "hle_kernel.h"

#define VITA_MAX_RAM_SIZE    (750 * 1024 * 1024)
#define VITA_USER_BASE_VADDR 0x81000000

#define PF_X        (1 << 0)
#define PF_W        (1 << 1)
#define PF_R        (1 << 2)

typedef struct {
    uint32_t start_vaddr;
    uint32_t size;
    uint32_t permissions;
    void* host_map_ptr; // 👈 This will now hold our real 64-bit Linux memory address
} Varm_KernelSegment;

static Varm_KernelSegment g_registered_segments[16];
static int g_segment_count = 0;

int hle_kernel_register_segment(uint32_t raw_vaddr, uint32_t raw_memsz, uint32_t filesz, uint32_t elf_flags) {
    uint32_t valid_vaddr = raw_vaddr;
    uint32_t valid_memsz = raw_memsz;

    if (valid_vaddr == 0x00000000) {
        valid_vaddr = VITA_USER_BASE_VADDR;
    }

    if (valid_memsz > VITA_MAX_RAM_SIZE) {
        if (filesz > 0 && filesz <= VITA_MAX_RAM_SIZE) {
            valid_memsz = (filesz + 0xFFF) & ~0xFFF;
        } else {
            valid_memsz = 65536;
        }
    }

    uint32_t valid_end = valid_vaddr + valid_memsz;

    // Check for overlapping regions
    for (int i = 0; i < g_segment_count; i++) {
        uint32_t ex_start = g_registered_segments[i].start_vaddr;
        uint32_t ex_end = ex_start + g_registered_segments[i].size;

        if ((valid_vaddr >= ex_start && valid_vaddr < ex_end) ||
            (valid_end > ex_start && valid_end <= ex_end) ||
            (valid_vaddr <= ex_start && valid_end >= ex_end)) {
            printf("[HLE KERNEL WARNING] Overlap detected with index %d!\n", i);
            return -2;
        }
    }

    if (g_segment_count >= 16) {
        printf("[HLE KERNEL ERROR] Maximum tracking blocks exceeded.\n");
        return -1;
    }

    // 🛠️ FIX STEP: Allocate actual host virtual memory page blocks via anonymous mmap
    void* host_alloc = mmap(NULL, valid_memsz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (host_alloc == MAP_FAILED) {
        printf("[HLE KERNEL ERROR] Failed to allocate host memory segment backing block!\n");
        return -3;
    }

    g_registered_segments[g_segment_count].start_vaddr = valid_vaddr;
    g_registered_segments[g_segment_count].size = valid_memsz;
    g_registered_segments[g_segment_count].permissions = elf_flags;
    g_registered_segments[g_segment_count].host_map_ptr = host_alloc;

    char perm_str[4] = "---";
    if (elf_flags & PF_R) perm_str[0] = 'r';
    if (elf_flags & PF_W) perm_str[1] = 'w';
    if (elf_flags & PF_X) perm_str[2] = 'x';

    printf("[HLE KERNEL] Registered segment index %d (VAddr: 0x%08X -> Host: %p | Size: %u bytes)\n",
           g_segment_count, valid_vaddr, host_alloc, valid_memsz);

    g_segment_count++;
    return 0;
}

// hle_kernel.c
// 🛠️ FIX: Renamed to match the prototype in hle_kernel.h
void* hle_kernel_resolve_address(uint32_t guest_vaddr, uint32_t required_perms) {
    for (int i = 0; i < g_segment_count; i++) {
        uint32_t start = g_registered_segments[i].start_vaddr;
        uint32_t end = start + g_registered_segments[i].size;

        if (guest_vaddr >= start && guest_vaddr < end) {
            if (required_perms != 0 && !(g_registered_segments[i].permissions & required_perms)) {
                printf("[MMU FAULT] Protection Violation at Guest Address: 0x%08X\n", guest_vaddr);
                return NULL;
            }
            // Compute the real 64-bit host pointer offset relative to the mmap segment
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

void hle_kernel_init(void) {
    printf("[HLE KERNEL] Initializing translation layer subsystems...\n");
    g_segment_count = 0;
    memset(g_registered_segments, 0, sizeof(g_registered_segments));
}

// Clean release loop to handle exiting titles without generating leaks
void hle_kernel_shutdown(void) {
    printf("[HLE KERNEL] Tearing down mapped memory allocations cleanly...\n");
    for (int i = 0; i < g_segment_count; i++) {
        if (g_registered_segments[i].host_map_ptr != NULL) {
            munmap(g_registered_segments[i].host_map_ptr, g_registered_segments[i].size);
            g_registered_segments[i].host_map_ptr = NULL;
        }
    }
    g_segment_count = 0;
}
