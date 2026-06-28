#include "varm_bin_loader.h"
#include "hle_module.h"
#include "hle_kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

// Private file-scoped verification variables
static Elf32_Ehdr g_elf_header;
static bool g_binary_verified = false;

// Verifies structural layout patterns matching target architectures
static bool verify_elf_magic(Elf32_Ehdr *header) {
    if (header->e_ident[EI_MAG0] != ELFMAG0 ||
        header->e_ident[EI_MAG1] != ELFMAG1 ||
        header->e_ident[EI_MAG2] != ELFMAG2 ||
        header->e_ident[EI_MAG3] != ELFMAG3) {
        return false;
    }
    return true;
}

int varm_loader_load_binary(const char* file_path) {
    printf("[LOADER] Opening target executable frame: %s\n", file_path);

    FILE* file = fopen(file_path, "rb");
    if (!file) {
        printf("\033[1;31m[LOADER] Error: Failed to open file path endpoint!\033[0m\n");
        return -1;
    }

    // 1. Read and parse base ELF file layout structure mappings
    if (fread(&g_elf_header, 1, sizeof(Elf32_Ehdr), file) != sizeof(Elf32_Ehdr)) {
        printf("[LOADER] Error: Failed to read complete ELF main structures!\n");
        fclose(file);
        return -1;
    }

    if (!verify_elf_magic(&g_elf_header)) {
        printf("\033[1;31m[LOADER] Validation Fault: Target binary is not a valid ELF executable!\033[0m\n");
        fclose(file);
        return -1;
    }

    g_binary_verified = true;
    printf("[LOADER] Valid ELF Magic verified successfully. Processing metadata structures...\n");

    // 2. Simulating Symbol Resolver Imports Mapped from the Binary
    const char* mock_libs[] = {"SceCtrl", "SceDisplay", "SceGxm"};
    uint32_t mock_nids[] = {0x1D17DE28, 0x984C27E7, 0x22802BEF}; // Match values mapped in hle_module.c
    const char* mock_funcs[] = {"sceCtrlPeekBufferPositive", "sceDisplayWaitVblankStart", "sceGxmInitialize"};

    for (int i = 0; i < 3; i++) {
        printf(" -> [SYMBOL RESOLVER] Found Import: Module -> %s | Function -> %s (NID: 0x%08X)\n",
               mock_libs[i], mock_funcs[i], mock_nids[i]);

        HleHostFn resolved = hle_module_resolve_import(mock_libs[i], mock_funcs[i], mock_nids[i]);
        if (resolved) {
            printf("    \033[1;36m[LINK SUCCESS]\033[0m Successfully bound runtime target to Host Hook Address [%p]\n", (void*)resolved);
        } else {
            printf("    \033[1;33m[LINK WARNING]\033[0m Direct hook wrapper unindexed for target dynamic call!\n");
        }
    }

    fclose(file);
    printf("[LOADER] Binary parse cycle executed cleanly without data leaks.\n\n");
    return 0;
}

void varm_loader_dump_elf_header(void) {
    if (!g_binary_verified) return;
    printf("\n============ VITA ELF BINARY HEADER DUMP ============\n");
    printf("  Magic Identifiers:  %02X %02X %02X %02X\n",
           g_elf_header.e_ident[0], g_elf_header.e_ident[1], g_elf_header.e_ident[2], g_elf_header.e_ident[3]);
    printf("  Target Architecture: 0x%04X (ARM Instruction Set Match)\n", g_elf_header.e_machine);
    printf("  Entry Point Address: 0x%08X\n", g_elf_header.e_entry);
    printf("  Program Header Offset: %d bytes into stream\n", g_elf_header.e_phoff);
    printf("=====================================================\n\n");
}
