#ifndef VARM_ELF_H
#define VARM_ELF_H

#include <stdint.h>

// Explicitly define 32-bit ELF Program Header for cross-architecture safety
// This stops fields from sliding or swapping sizes when compiled on ARM64
typedef struct {
    uint32_t p_type;     // Type of segment
    uint32_t p_offset;   // Offset in the binary file
    uint32_t p_vaddr;    // Virtual target address in guest space
    uint32_t p_paddr;    // Physical address (reserved/unused)
    uint32_t p_filesz;   // Size of segment data inside file
    uint32_t p_memsz;    // Size of segment data inside runtime RAM
    uint32_t p_flags;    // Segment permissions (Read/Write/Execute)
    uint32_t p_align;    // Memory alignment requirements
} Varm_Elf32_Phdr;

// Strict fixed-width Sony module structure layout
typedef struct {
    uint16_t attributes;
    uint8_t  major_version;
    uint8_t  minor_version;
    char     module_name[27];
    uint8_t  type;
    uint32_t gp_value;
    uint32_t ent_top;    // Export table start address
    uint32_t ent_end;    // Export table end address
    uint32_t stub_top;   // Import table start address
    uint32_t stub_end;   // Import table end address
} __attribute__((packed)) Varm_SceModuleInfo;

#endif // VARM_ELF_H
