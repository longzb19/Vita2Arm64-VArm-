#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "varm_bridge.h"

// Standard 32-bit ELF Header Structure
typedef struct {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint32_t      e_entry;     // On Vita, this points directly to SceModuleInfo!
    uint32_t      e_phoff;     // Program header table file offset
    uint32_t      e_shoff;     // Section header table file offset
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} Elf32_Ehdr;

// Standard 32-bit Program Header (Segment) Structure
typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

// Dynamic Helper: Translates a Virtual Address to its true offset in the file buffer
uint32_t va_to_file_offset(uint32_t va, Elf32_Phdr *ph_table, int ph_count) {
    uint32_t target_va = va;

    // Normalize relative or stripped entrypoints to the load base (0x81000000)
    if (target_va < 0x80000000) {
        target_va += 0x81000000;
    }

    for (int i = 0; i < ph_count; i++) {
        if (ph_table[i].p_type == 1) { // PT_LOAD (Loadable text/data segments)
            uint32_t start = ph_table[i].p_vaddr;
            uint32_t end = start + ph_table[i].p_memsz;

            if (target_va >= start && target_va < end) {
                return ph_table[i].p_offset + (target_va - start);
            }
        }
    }
    return 0; // Not found inside a loadable block
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("[ERROR] Missing target binary file parameter!\n");
        return 1;
    }

    printf("Opening target binary: %s\n", argv[1]);
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        printf("[ERROR] Failed to read file execution stream.\n");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *buffer = malloc(file_size);
    if (!buffer) {
        printf("[ERROR] Failed to allocate memory buffer.\n");
        fclose(file);
        return 1;
    }

    if (fread(buffer, 1, file_size, file) != file_size) {
        printf("[ERROR] Short read error while fetching file.\n");
        free(buffer);
        fclose(file);
        return 1;
    }
    fclose(file);

    // Verify SCE Magic Container Check
    if (memcmp(buffer, "SCE", 3) != 0) {
        printf("[ERROR] Invalid file container format target.\n");
        free(buffer);
        return 1;
    }

    uint32_t elf_offset = 0xA0;
    printf("[SUCCESS] Sony SCE Container Recognized!\n -> ELF Header Offset: 0x%X\n", elf_offset);

    // Overlay our standard ELF header struct perfectly onto the buffer
    Elf32_Ehdr *elf_hdr = (Elf32_Ehdr *)(buffer + elf_offset);
    if (elf_hdr->e_ident[0] != 0x7F || elf_hdr->e_ident[1] != 'E' || elf_hdr->e_ident[2] != 'L' || elf_hdr->e_ident[3] != 'F') {
        printf("[ERROR] Malformed secondary ELF internal header structure.\n");
        free(buffer);
        return 1;
    }

    printf("[SUCCESS] Executable ELF Structure Verified!\n");
    printf(" -> Target Architecture: ARM (NATIVE)\n");
    printf(" -> Entrypoint Address:  0x%08X\n", elf_hdr->e_entry);
    printf(" -> Total Segments Found: %d\n", elf_hdr->e_phnum);

    // Clear and prepare our virtual registers
    V_ARMRegisters cpu_ctx;
    memset(&cpu_ctx, 0, sizeof(V_ARMRegisters));
    varm_bridge_setup_env(&cpu_ctx, elf_hdr->e_entry);

    printf("\n=== SCANNING PHYSICAL ROADMAP SEGMENTS ===\n");
    Elf32_Phdr *ph_table = (Elf32_Phdr*)((uint8_t*)elf_hdr + elf_hdr->e_phoff);
    for (int i = 0; i < elf_hdr->e_phnum; i++) {
        char flag_str[4] = "---";
        if (ph_table[i].p_flags & 4) flag_str[0] = 'R';
        if (ph_table[i].p_flags & 2) flag_str[1] = 'W';
        if (ph_table[i].p_flags & 1) flag_str[2] = 'X';

        printf("Segment #%d: Type %d | File Offset: 0x%06X | VirtAddr: 0x%08X | FileSize: %d bytes | Flags: %s\n",
               i, ph_table[i].p_type, ph_table[i].p_offset, ph_table[i].p_vaddr, ph_table[i].p_filesz, flag_str);
    }

    printf("\n=== SEARCHING FOR SONY MODULE DIRECTORY ===\n");
    // Dynamically look up where SceModuleInfo lives in the file using e_entry
    uint32_t info_file_offset = va_to_file_offset(elf_hdr->e_entry, ph_table, elf_hdr->e_phnum);

    if (info_file_offset != 0 && info_file_offset < file_size) {
        SceModuleInfo *mod_info = (SceModuleInfo *)(buffer + info_file_offset);
        printf("[SUCCESS] Found Module Directory:\n");
        printf(" -> Module Name: %s\n", mod_info->name);
        printf(" -> Import Stubs Top:  0x%08X\n", mod_info->stub_top);
        printf(" -> Import Stubs End:  0x%08X\n", mod_info->stub_end);
    } else {
        printf("[WARN] Could not safely resolve Module directory virtual address to file offset.\n");
    }

    free(buffer);
    return 0;
}
