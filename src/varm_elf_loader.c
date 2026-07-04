#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "varm_elf.h"
#include "hle_kernel.h"

/**
 * varm_elf_parse_sony_metadata
 * Optional/HLE Hook: Checks if the loaded binary contains valid Sony module info
 * to register the game's internal system title names and export tables.
 */
static void varm_elf_parse_sony_metadata(FILE* file, uint32_t offset) {
    Varm_SonyModuleHeader sony_hdr; //[cite: 27]

    if (fseek(file, offset, SEEK_SET) != 0) return;
    if (fread(&sony_hdr, 1, sizeof(Varm_SonyModuleHeader), file) != sizeof(Varm_SonyModuleHeader)) return; //[cite: 27]

    // Print out the internal module metadata embedded inside the game binary
    // Null-terminate safely to prevent printing raw garbage strings from memory
    sony_hdr.module_name[26] = '\0'; //[cite: 27]
    printf("[VARM-ELF] Sony Module Name : %s\n", sony_hdr.module_name); //[cite: 27]
    printf("[VARM-ELF] Module Version  : %u.%u\n", sony_hdr.major_version, sony_hdr.minor_version); //[cite: 27]
    printf("[VARM-ELF] Export Boundaries: 0x%08X - 0x%08X\n", sony_hdr.ent_top, sony_hdr.ent_end); //[cite: 27]
}

/**
 * varm_elf_load_executable
 * Main entry point for Module A. Opens a 32-bit Vita ELF file, validates its structure,
 * loads execution blocks to guest space, and captures the target hardware entry point.
 */
int varm_elf_load_executable(const char* filepath, uint32_t* out_entry_point) {
    if (!filepath || !out_entry_point) {
        printf("[VARM-ELF] Error: Invalid initialization arguments passed to loader.\n");
        return -1;
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        printf("[VARM-ELF] Error: Failed to open binary target path: %s\n", filepath);
        return -1;
    }

    // 1. Read and Validate the Master 32-bit ELF Header
    Varm_Elf32_Ehdr ehdr; //[cite: 27]
    if (fread(&ehdr, 1, sizeof(Varm_Elf32_Ehdr), file) != sizeof(Varm_Elf32_Ehdr)) { //[cite: 27]
        printf("[VARM-ELF] Error: Binary file is too small to contain a valid ELF header.\n");
        fclose(file);
        return -1;
    }

    // Check for standard "\177ELF" magic identity signatures[cite: 27]
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) { //[cite: 27]
        printf("[VARM-ELF] Error: Magic identity mismatch! This file is not a valid ELF binary.\n");
        fclose(file);
        return -1;
    }

    printf("[VARM-ELF] Validated 32-bit ELF file. Target Entry Point: 0x%08X\n", ehdr.e_entry); //[cite: 27]
    *out_entry_point = ehdr.e_entry; //[cite: 27]

    // 2. Loop Through and Map Every Single Program Segment Header
    if (fseek(file, ehdr.e_phoff, SEEK_SET) != 0) { //[cite: 27]
        printf("[VARM-ELF] Error: Failed to locate the Program Header table offset.\n");
        fclose(file);
        return -1;
    }

    Varm_Elf32_Phdr phdr; //[cite: 27]
    uint32_t sony_metadata_offset = 0;

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) { //[cite: 27]
        if (fread(&phdr, 1, sizeof(Varm_Elf32_Phdr), file) != sizeof(Varm_Elf32_Phdr)) { //[cite: 27]
            printf("[VARM-ELF] Error: Truncated program segment header encountered at index %d.\n", i);
            fclose(file);
            return -1;
        }

        // We only care about PT_LOAD segments (blocks that actually contain raw game code or assets)[cite: 27]
        if (phdr.p_type == PT_LOAD) { //[cite: 27]
            printf("[VARM-ELF] Processing Segment %u -> Virtual Address Target: 0x%08X [Size: %u bytes]\n",
                   i, phdr.p_vaddr, phdr.p_memsz); //[cite: 27]

            // Request the virtual memory subsystem to grant us a host-accessible memory pointer
            // We use write_flag = 1 because we are manually populating code blocks from the file
            void* host_destination = hle_kernel_resolve_address(phdr.p_vaddr, 1); //[cite: 27]

            if (!host_destination) {
                printf("[VARM-ELF] Critical: Virtual memory manager failed to allocate target space at 0x%08X!\n", phdr.p_vaddr); //[cite: 27]
                fclose(file);
                return -1;
            }

            // Capture the current file pointer to jump back later
            long current_ph_position = ftell(file);

            // Seek straight to the raw binary block payload location inside the file
            if (fseek(file, phdr.p_offset, SEEK_SET) != 0) { //[cite: 27]
                printf("[VARM-ELF] Error: Corrupted segment file offset pointer lookup failed.\n");
                fclose(file);
                return -1;
            }

            // Copy raw segment information out of file storage into allocated guest RAM bounds
            if (phdr.p_filesz > 0) { //[cite: 27]
                if (fread(host_destination, 1, phdr.p_filesz, file) != phdr.p_filesz) { //[cite: 27]
                    printf("[VARM-ELF] Error: Failed to parse complete segment data from binary payload.\n");
                    fclose(file);
                    return -1;
                }
            }

            // Crucial Emulation Step: Handle the BSS block allocation space.
            // If runtime RAM bounds are larger than binary footprint sizes, the remainder contains uninitialized variables.
            // These MUST be explicitly zeroed out to prevent game logic loops from parsing unpredictable memory states.
            if (phdr.p_memsz > phdr.p_filesz) { //[cite: 27]
                uint32_t bss_remainder_bytes = phdr.p_memsz - phdr.p_filesz; //[cite: 27]
                uint8_t* bss_start_address = (uint8_t*)host_destination + phdr.p_filesz; //[cite: 27]
                memset(bss_start_address, 0, bss_remainder_bytes);
            }

            // Catch the potential location of Sony's special metadata header if it sits near raw code
            if (i == 0 && phdr.p_filesz >= sizeof(Varm_SonyModuleHeader)) { //[cite: 27]
                sony_metadata_offset = phdr.p_offset; //[cite: 27]
            }

            // Return to our program header array sequence iteration tracking step
            fseek(file, current_ph_position, SEEK_SET);
        }
    }

    // If an identification segment block was captured, parse the module tracking details out of it
    if (sony_metadata_offset > 0) {
        varm_elf_parse_sony_metadata(file, sony_metadata_offset);
    }

    printf("[VARM-ELF] Executable Module Loading Completed Successfully.\n");
    fclose(file);
    return 0;
}
