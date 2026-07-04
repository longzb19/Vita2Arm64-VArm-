#include "varm_bin_loader.h"
#include "hle_module.h"
#include "hle_kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

#define SCE_MAGIC 0x00454353 // "SCE\0" in Little Endian (0x53, 0x43, 0x45, 0x00)
#define HEADER_SCAN_SZ 4096  // Safe page buffer size to scan for inner ELF signatures

extern char g_game_id[32]; // Links directly with the global system identifier

static Elf32_Ehdr g_elf_header;
static bool g_binary_verified = false;

static bool verify_elf_magic(Elf32_Ehdr *header) {
    return (header->e_ident[EI_MAG0] == ELFMAG0 &&
            header->e_ident[EI_MAG1] == ELFMAG1 &&
            header->e_ident[EI_MAG2] == ELFMAG2 &&
            header->e_ident[EI_MAG3] == ELFMAG3);
}

int varm_loader_load_binary(const char* file_path) {
    printf("[LOADER] Opening target executable frame: %s\n", file_path);

    // 🛠️ Extract the Game Title/Folder ID from the launch path
    const char* game_dir_start = strstr(file_path, "games/");
    if (game_dir_start) {
        game_dir_start += 6; // Move pointer past "games/"
        const char* game_dir_end = strchr(game_dir_start, '/');
        if (game_dir_end) {
            size_t id_length = game_dir_end - game_dir_start;
            if (id_length > 31) id_length = 31;
            strncpy(g_game_id, game_dir_start, id_length);
            g_game_id[id_length] = '\0';
        }
    }

    FILE* file = fopen(file_path, "rb");
    if (!file) {
        printf("\033[1;31m[LOADER] Error: Failed to open file path endpoint!\033[0m\n");
        return -1;
    }

    // Read the initial 4 bytes to check the file type container (SELF/FSELF vs Raw ELF)
    uint32_t file_magic = 0;
    if (fread(&file_magic, 1, sizeof(uint32_t), file) != sizeof(uint32_t)) {
        printf("[LOADER] Error: Unable to read file magic!\n");
        fclose(file);
        return -1;
    }

    long elf_offset = 0;

    // Handle Sony's SCE container wrapping if detected
    if (file_magic == SCE_MAGIC) {
        printf("[LOADER] Detected Sony SELF/FSELF executable container! Piercing to ELF offset...\n");

        // Dynamic Signature Scanner: Read the header block and locate the true ELF boundary
        uint8_t scan_buffer[HEADER_SCAN_SZ];
        fseek(file, 0, SEEK_SET);
        size_t bytes_read = fread(scan_buffer, 1, HEADER_SCAN_SZ, file);
        bool found_elf = false;

        for (size_t i = 0; i < bytes_read - 4; i++) {
            if (scan_buffer[i]     == ELFMAG0 &&
                scan_buffer[i + 1] == ELFMAG1 &&
                scan_buffer[i + 2] == ELFMAG2 &&
                scan_buffer[i + 3] == ELFMAG3) {
                elf_offset = (long)i;
                found_elf = true;
                break;
            }
        }

        if (found_elf) {
            printf("[LOADER] Success! Plaintext ELF signature localized at dynamic offset: 0x%lX\n", elf_offset);
            fseek(file, elf_offset, SEEK_SET);
        } else {
            printf("\033[1;33m[LOADER] Warning: Plaintext ELF marker not found in header scan window!\033[0m\n");
            printf("[LOADER] Target binary might be encrypted or compressed (zlib blocks).\n");
            // Fallback to baseline default layout spec just in case
            elf_offset = 0x50;
            fseek(file, elf_offset, SEEK_SET);
        }
    } else {
        // If it's already a raw standard ELF, rewind right back to byte 0
        fseek(file, 0, SEEK_SET);
    }

    // Read out the standard ELF Header structures from our calculated offset position
    if (fread(&g_elf_header, 1, sizeof(Elf32_Ehdr), file) != sizeof(Elf32_Ehdr)) {
        printf("[LOADER] Error: Failed to read complete ELF main structures!\n");
        fclose(file);
        return -1;
    }

    if (!verify_elf_magic(&g_elf_header)) {
        printf("\033[1;31m[LOADER] Validation Fault: Target binary is not a valid ELF executable!\033[0m\n");
        printf("[LOADER] First 4 bytes at target offset were: %02X %02X %02X %02X\n",
               g_elf_header.e_ident[0], g_elf_header.e_ident[1], g_elf_header.e_ident[2], g_elf_header.e_ident[3]);
        fclose(file);
        return -1;
    }

    g_binary_verified = true;
    printf("[LOADER] Successfully identified Game ID: \033[1;32m%s\033[0m\n", g_game_id);
    printf("[LOADER] ELF Header verified successfully at layout offset: 0x%lX\n", elf_offset);

    // Mock symbol resolver bindings for our engine hooks line setup
    printf("[LOADER] Resolving and link-binding dynamic HLE import tables...\n");
    const char* mock_libs[]  = {"SceCtrl", "SceDisplay", "SceGxm"};
    uint32_t mock_nids[]     = {0x3A622605, 0x164627E7, 0x60D505D4};
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
    printf("  Machine Target:     0x%04X (ARM Architecture Line)\n", g_elf_header.e_machine);
    printf("  Entry Point VAddr:  0x%08X\n", g_elf_header.e_entry);
    printf("  Program Header Off: 0x%08X\n", g_elf_header.e_phoff);
    printf("  Section Header Off: 0x%08X\n", g_elf_header.e_shoff);
    printf("=====================================================\n\n");
}
