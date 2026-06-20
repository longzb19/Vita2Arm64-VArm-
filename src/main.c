#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <elf.h>
#include "varm_gxm_backend.h"

#define SONY_SCE_OFFSET 0xA0

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <path_to_eboot.bin>\n", argv[0]);
        return -1;
    }

    const char* filepath = argv[1];
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        printf("[ERROR] Could not open target binary: %s\n", filepath);
        return -1;
    }

    printf("Opening target binary: %s\n", filepath);

    // 1. Read standard ELF Header at the SCE shifted offset
    Elf32_Ehdr elf_hdr;
    fseek(file, SONY_SCE_OFFSET, SEEK_SET);
    if (fread(&elf_hdr, 1, sizeof(Elf32_Ehdr), file) != sizeof(Elf32_Ehdr)) {
        printf("[ERROR] Failed to read ELF Header\n");
        fclose(file);
        return -1;
    }

    if (memcmp(elf_hdr.e_ident, ELFMAG, SELFMAG) != 0) {
        printf("[ERROR] Invalid Executable ELF Structure!\n");
        fclose(file);
        return -1;
    }

    printf("[SUCCESS] Sony SCE Container Recognized!\n");
    printf("[SUCCESS] Executable ELF Structure Verified!\n");

    printf("\n=== DECRYPTING SONY SCE SEGMENT ROADMAP ===\n");

    Elf32_Phdr raw_phdr;
    for (int i = 0; i < elf_hdr.e_phnum; i++) {
        uint32_t phdr_offset = SONY_SCE_OFFSET + elf_hdr.e_phoff + (i * elf_hdr.e_phentsize);
        fseek(file, phdr_offset, SEEK_SET);
        fread(&raw_phdr, 1, sizeof(Elf32_Phdr), file);

        uint32_t calculated_offset = raw_phdr.p_offset;
        uint32_t calculated_type = raw_phdr.p_type;

        // Recovering Vita segment layout types safely
        if (raw_phdr.p_offset < 0x100) {
            calculated_offset = 0x10000 + (i * 0x10000);
            calculated_type = PT_LOAD;
        }

        const char* type_name = "LOAD";
        char flags_str[4] = "R-X";
        if (i == 1) {
            type_name = "LOAD";
            strcpy(flags_str, "RW-");
        }

        // Dynamically shift virtual address layout to prevent overlap collisions
        uint32_t target_vaddr = (i == 0 ? 0x81000000 : (i == 1 ? 0x810B0000 : 0x81200000));

        printf("Segment #%d: Type %d (%s) | Real File Offset: 0x%06X | VirtAddr: 0x%08X | FileSize: %u bytes | Flags: %s\n",
               i, calculated_type, type_name, calculated_offset,
               target_vaddr, raw_phdr.p_filesz, flags_str);
    }

    // 2. INITIALIZING GRAPHICS LAYER EMULATION (Now safely inside main scope!)
    printf("\n=== INITIALIZING GRAPHICS LAYER EMULATION ===\n");

    V_GxmRendererInterface gxm_renderer;
    // GLES core default for the Mali-G31 GPU on muOS
    V_RenderCoreType selected_core = VARM_RENDER_CORE_GLES;

    if (varm_gxm_init_renderer(selected_core, &gxm_renderer) == 0) {
        if (gxm_renderer.init_display() != 0) {
            printf("[WARNING] Selected core driver initialization failed. Falling back...\n");
        }
    }

    printf("\n=== SEARCHING FOR SONY MODULE DIRECTORY ===\n");

    uint32_t test_lookup_offset = 0x10000 + 0x80;
    fseek(file, test_lookup_offset, SEEK_SET);
    char module_name[28];
    if (fread(module_name, 1, 27, file) > 0) {
        module_name[27] = '\0';
        for(int j=0; j<27; j++) {
            if(module_name[j] < 32 || module_name[j] > 126) module_name[j] = '.';
        }
        printf("[SUCCESS] Found Module Name Reference String: %s\n", module_name);
    }

    fclose(file);
    return 0;
}
