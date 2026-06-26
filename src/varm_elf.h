#ifndef VARM_ELF_H
#define VARM_ELF_H

#include <stdint.h>

// ELF Constants normally found in <elf.h>
#define ELFMAG   "\177ELF"
#define SELFMAG  4
#define PT_LOAD  1

// Explicitly 32-bit ELF Header definitions for cross-architecture safety
typedef struct {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint32_t      e_entry;
    uint32_t      e_phoff;
    uint32_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} Varm_Elf32_Ehdr;

// Explicitly 32-bit ELF Program Header
typedef struct {
    uint32_t p_type;     // Type of segment
    uint32_t p_offset;   // File offset where segment resides
    uint32_t p_vaddr;    // Target Virtual address in memory
    uint32_t p_paddr;    // Physical address (typically unused)
    uint32_t p_filesz;   // Size of data inside file container
    uint32_t p_memsz;    // Allocated size inside virtual memory space
    uint32_t p_flags;    // Execution permissions (rwx)
    uint32_t p_align;    // Memory boundary page alignments
} __attribute__((packed)) Varm_Elf32_Phdr;

// Strict fixed-width Sony module structure layout
typedef struct {
    uint16_t attributes;
    uint8_t  major_version;
    uint8_t  minor_version;
    char     module_name[27];
    uint8_t  type;
    uint32_t gp_value;
    uint32_t ent_top;
    uint32_t ent_end;
    uint32_t stub_top;
    uint32_t stub_end;
} __attribute__((packed)) Varm_SceModuleInfo;

#endif // VARM_ELF_H
