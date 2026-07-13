#include "varm_bin_loader.h"
#include "varm_elf.h"
#include "hle_module.h"
#include "hle_kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCE_MAGIC 0x00454353 // "SCE\0" in Little Endian (0x53, 0x43, 0x45, 0x00)
#define HEADER_SCAN_SZ 4096  // Safe page buffer size to scan for inner ELF signatures

extern char g_game_id[32]; // Links directly with the global system identifier

static Varm_Elf32_Ehdr g_elf_header;
static bool g_binary_verified = false;

static bool verify_elf_magic(Varm_Elf32_Ehdr *header) {
    return (header->e_ident[0] == 0x7F &&
            header->e_ident[1] == 'E' &&
            header->e_ident[2] == 'L' &&
            header->e_ident[3] == 'F');
}

/**
 * Walks the binary's internal Sony import structures dynamically.
 * Replaces the old hardcoded mock array loop entirely.
 */
static void varm_loader_resolve_dynamic_imports(FILE* file, uint32_t stub_top, uint32_t stub_end) {
    uint32_t current_stub_vaddr = stub_top;
    int resolved_count = 0;
    int missing_count = 0;

    printf("[LINKER] Dynamic Import Table Detected: 0x%08X - 0x%08X. Resolving modules...\n", stub_top, stub_end);

    while (current_stub_vaddr < stub_end) {
        // Resolve the guest virtual address to our host-accessible pointer
        SceLibraryImport* imp = (SceLibraryImport*)hle_kernel_resolve_address(current_stub_vaddr, 1);
        if (!imp || imp->library_nid == 0) {
            break;
        }

        printf(" -> [MODULE DEPENDENCY] Found Import: %s (Library NID: 0x%08X) | Functions: %d\n",
               imp->library_name, imp->library_nid, imp->num_functions);

        // Resolve the arrays containing the function NID hashes
        uint32_t* nid_table = (uint32_t*)hle_kernel_resolve_address(imp->nid_table_vaddr, 1);

        if (nid_table) {
            // Walk through every single function this library requests
            for (uint16_t f = 0; f < imp->num_functions; f++) {
                uint32_t func_nid = nid_table[f];

                // Query our central hle_module registry using the true binary NID hash
                HleHostFn host_hook = hle_module_resolve_import(imp->library_name, NULL, func_nid);

                if (host_hook) {
                    resolved_count++;
                    printf("    \033[1;36m[LINK SUCCESS]\033[0m Bound %s Function NID [0x%08X] -> Host Handle [%p]\n",
                           imp->library_name, func_nid, (void*)host_hook);
                } else {
                    missing_count++;
                    printf("    \033[1;33m[LINK WARNING]\033[0m Missing HLE implementation for NID: 0x%08X\n", func_nid);
                }
            }
        }

        // Advance to the next import entry layout in the table block
        current_stub_vaddr += sizeof(SceLibraryImport);
    }

    printf("[LINKER] Dynamic Link Phase Done. Bound: \033[1;32m%d\033[0m functions | Unhandled: \033[1;33m%d\033[0m functions.\n",
           resolved_count, missing_count);
}

int varm_loader_load_binary(const char* file_path) {
    printf("[LOADER] Opening target executable frame: %s\n", file_path);

    // Extract the Game Title/Folder ID from the launch path
    const char* game_dir_start = strstr(file_path, "games/");
    if (game_dir_start) {
        game_dir_start += 6;
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

    uint32_t file_magic = 0;
    if (fread(&file_magic, 1, sizeof(uint32_t), file) != sizeof(uint32_t)) {
        printf("[LOADER] Error: Unable to read file magic!\n");
        fclose(file);
        return -1;
    }

    long elf_offset = 0;

    // Handle Sony's SCE container layout piercing safely
    if (file_magic == SCE_MAGIC) {
        printf("[LOADER] Detected Sony SELF/FSELF executable container! Piercing to ELF offset...\n");

        uint8_t scan_buffer[HEADER_SCAN_SZ];
        fseek(file, 0, SEEK_SET);
        size_t bytes_read = fread(scan_buffer, 1, HEADER_SCAN_SZ, file);
        bool found_elf = false;

        for (size_t i = 0; i < bytes_read - 4; i++) {
            if (scan_buffer[i]     == 0x7F &&
                scan_buffer[i + 1] == 'E' &&
                scan_buffer[i + 2] == 'L' &&
                scan_buffer[i + 3] == 'F') {
                elf_offset = (long)i;
                found_elf = true;
                break;
            }
        }

        if (found_elf) {
            printf("[LOADER] Success! Plaintext ELF signature localized at offset: 0x%lX\n", elf_offset);
            fseek(file, elf_offset, SEEK_SET);
        } else {
            printf("\033[1;33m[LOADER] Warning: Plaintext ELF marker not found in scan window!\033[0m\n");
            elf_offset = 0x50;
            fseek(file, elf_offset, SEEK_SET);
        }
    } else {
        fseek(file, 0, SEEK_SET);
    }

    // Read out cross-architecture safe 32-bit ELF Header structures
    if (fread(&g_elf_header, 1, sizeof(Varm_Elf32_Ehdr), file) != sizeof(Varm_Elf32_Ehdr)) {
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
    printf("[LOADER] Successfully identified Game ID: \033[1;32m%s\033[0m\n", g_game_id);

    // --- INTEGRATED CRITICAL FIX: MAP RAW CODE SEGMENTS TO GUEST RAM ---
    if (fseek(file, elf_offset + g_elf_header.e_phoff, SEEK_SET) != 0) {
        printf("[LOADER] Error: Failed to locate Program Header table.\n");
        fclose(file);
        return -1;
    }

    Varm_Elf32_Phdr phdr;
    uint32_t sony_metadata_offset = 0;

    for (uint16_t i = 0; i < g_elf_header.e_phnum; i++) {
        if (fread(&phdr, 1, sizeof(Varm_Elf32_Phdr), file) != sizeof(Varm_Elf32_Phdr)) {
            printf("[LOADER] Error: Truncated program segment header at index %d.\n", i);
            fclose(file);
            return -1;
        }

        if (phdr.p_type == PT_LOAD) {
            printf("[LOADER] Mapping Segment %u -> Guest VAddr Space: 0x%08X [%u bytes]\n",
                   i, phdr.p_vaddr, phdr.p_memsz);

            void* host_destination = hle_kernel_resolve_address(phdr.p_vaddr, 1);
            if (!host_destination) {
                printf("[LOADER] Critical: Virtual memory allocation failed at 0x%08X!\n", phdr.p_vaddr);
                fclose(file);
                return -1;
            }

            long current_ph_position = ftell(file);

            // Seek and extract binary segment payload data
            if (fseek(file, elf_offset + phdr.p_offset, SEEK_SET) != 0) {
                printf("[LOADER] Error: Corrupted segment file offset pointer lookup failed.\n");
                fclose(file);
                return -1;
            }

            if (phdr.p_filesz > 0) {
                if (fread(host_destination, 1, phdr.p_filesz, file) != phdr.p_filesz) {
                    printf("[LOADER] Error: Failed to parse complete segment data layout payload.\n");
                    fclose(file);
                    return -1;
                }
            }

            // Explicitly zero-out remaining BSS uninitialized variables boundary space
            if (phdr.p_memsz > phdr.p_filesz) {
                uint32_t bss_remainder_bytes = phdr.p_memsz - phdr.p_filesz;
                uint8_t* bss_start_address = (uint8_t*)host_destination + phdr.p_filesz;
                memset(bss_start_address, 0, bss_remainder_bytes);
            }

            // Trap structural locations for Sony module info headers
            if (i == 0 && phdr.p_filesz >= sizeof(Varm_SonyModuleHeader)) {
                sony_metadata_offset = elf_offset + phdr.p_offset;
            }

            fseek(file, current_ph_position, SEEK_SET);
        }
    }

    // --- INTEGRATED CRITICAL FIX: PARSE METADATA & BIND IMPORTS DYNAMICALLY ---
    if (sony_metadata_offset > 0) {
        Varm_SonyModuleHeader sony_hdr;
        if (fseek(file, sony_metadata_offset, SEEK_SET) == 0) {
            if (fread(&sony_hdr, 1, sizeof(Varm_SonyModuleHeader), file) == sizeof(Varm_SonyModuleHeader)) {
                sony_hdr.module_name[26] = '\0';
                printf("[LOADER] Sony Module Metadata Loaded: %s (v%u.%u)\n",
                       sony_hdr.module_name, sony_hdr.major_version, sony_hdr.minor_version);

                // Dynamically walk the actual import table using embedded addresses!
                if (sony_hdr.stub_top < sony_hdr.stub_end) {
                    varm_loader_resolve_dynamic_imports(file, sony_hdr.stub_top, sony_hdr.stub_end);
                }
            }
        }
    } else {
        printf("\033[1;33m[LOADER] Warning: No embedded Sony Module Info header discovered.\033[0m\n");
    }

    fclose(file);
    printf("[LOADER] Binary segment loading and dependency resolution cycles complete.\n\n");
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
