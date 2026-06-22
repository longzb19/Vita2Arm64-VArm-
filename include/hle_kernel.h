#ifndef HLE_KERNEL_H
#define HLE_KERNEL_H

#include <stdint.h>

// Core setup
void hle_kernel_init(void);

// Memory & segment allocation
int hle_kernel_register_segment(uint32_t raw_vaddr, uint32_t raw_memsz, uint32_t filesz, uint32_t elf_flags);
uint32_t hle_kernel_resolve_address(uint32_t guest_vaddr, uint32_t required_perms);
void hle_kernel_dump_maps(void);

#endif // HLE_KERNEL_H
