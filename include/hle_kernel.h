#ifndef HLE_KERNEL_H
#define HLE_KERNEL_H

#include <stdint.h>
#include <stdbool.h>

// Core setup & cleanup lifecycle handlers
void hle_kernel_init(void);
void hle_kernel_shutdown(void);

// Memory & segment allocation (Handles true host 64-bit translations)
int hle_kernel_register_segment(uint32_t raw_vaddr, uint32_t raw_memsz, uint32_t filesz, uint32_t elf_flags);
void* hle_kernel_resolve_address(uint32_t guest_vaddr, uint32_t required_perms);
void hle_kernel_dump_maps(void);

// 🎯 Critical binary connector hook for dependency mapping
void* hle_resolve_nid(uint32_t library_nid, uint32_t func_nid);

#endif // HLE_KERNEL_H
