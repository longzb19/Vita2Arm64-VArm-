#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "varm_elf.h"

#define VITA_MAX_RAM_SIZE (512 * 1024 * 1024)
#define VITA_USER_BASE_VADDR 0x81000000

#define PF_X        (1 << 0)
#define PF_W        (1 << 1)
#define PF_R        (1 << 2)

typedef struct {
    uint32_t start_vaddr;
    uint32_t size;
    uint32_t permissions;
    void* host_map_ptr;
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

    for (int i = 0; i < g_segment_count; i++) {
        uint32_t ex_start = g_registered_segments[i].start_vaddr;
        uint32_t ex_end = ex_start + g_registered_segments[i].size;

        if ((valid_vaddr >= ex_start && valid_vaddr < ex_end) ||
            (valid_end > ex_start && valid_end <= ex_end) ||
            (valid_vaddr <= ex_start && valid_end >= ex_end)) {

            printf("[HLE KERNEL WARNING] Overlap detected with index %d! Collapsing region adjustment.\n", i);
            return -2;
        }
    }

    if (g_segment_count >= 16) {
        printf("[HLE KERNEL ERROR] Maximum tracking blocks exceeded.\n");
        return -1;
    }

    g_registered_segments[g_segment_count].start_vaddr = valid_vaddr;
    g_registered_segments[g_segment_count].size = valid_memsz;
    g_registered_segments[g_segment_count].permissions = elf_flags;
    g_registered_segments[g_segment_count].host_map_ptr = NULL;

    char perm_str[4] = "---";
    if (elf_flags & PF_R) perm_str[0] = 'r';
    if (elf_flags & PF_W) perm_str[1] = 'w';
    if (elf_flags & PF_X) perm_str[2] = 'x';

    printf("[HLE KERNEL] Registered segment tracking index %d (VAddr: 0x%08X | Size: %u bytes | Perms: [%s])\n",
           g_segment_count, valid_vaddr, valid_memsz, perm_str);

    g_segment_count++;
    return 0;
}

uint32_t hle_kernel_resolve_address(uint32_t guest_vaddr, uint32_t required_perms) {
    for (int i = 0; i < g_segment_count; i++) {
        uint32_t start = g_registered_segments[i].start_vaddr;
        uint32_t end = start + g_registered_segments[i].size;

        if (guest_vaddr >= start && guest_vaddr < end) {
            if (required_perms != 0 && !(g_registered_segments[i].permissions & required_perms)) {
                printf("[KERNEL MMU FAULT] Protection Violation at 0x%08X\n", guest_vaddr);
                return 0;
            }
            return guest_vaddr;
        }
    }
    return 0;
}

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
