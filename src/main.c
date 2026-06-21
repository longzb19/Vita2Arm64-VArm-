#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <elf.h>
#include <sys/mman.h>
#include <unistd.h>
#include "varm_gxm_backend.h"

#define SCE_MAGIC 0x00454353 // "SCE\0" in little-endian

typedef struct {
    uint32_t magic;          // "SCE\0"
    uint32_t version;        // Format version
    uint16_t sdk_type;
    uint16_t header_type;    // 1 = SELF, 2 = SRK, 3 = SPK
    uint32_t metadata_count;
    uint64_t header_len;     // Total size of all SELF headers
    uint64_t elf_len;        // Total size of inside ELF data
    uint64_t file_len;       // Total size of entire file
    uint64_t self_type;      // Application type
    uint64_t reserved;
} __attribute__((packed)) SceSelfHeader;

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

    // Determine total file size for bounds verification
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("Opening target binary: %s (Size: %ld bytes)\n", filepath, file_size);

    uint32_t magic_check = 0;
    if (fread(&magic_check, 1, sizeof(uint32_t), file) != sizeof(uint32_t)) {
        printf("[ERROR] Failed to read file magic identifier.\n");
        fclose(file);
        return -1;
    }
    fseek(file, 0, SEEK_SET);

    long elf_header_offset = 0;

    // Check if the binary uses the Sony SCE Container wrapper
    if (magic_check == SCE_MAGIC) {
        SceSelfHeader self_hdr;
        if (fread(&self_hdr, 1, sizeof(SceSelfHeader), file) != sizeof(SceSelfHeader)) {
            printf("[ERROR] Failed to read full SceSelfHeader payload.\n");
            fclose(file);
            return -1;
        }
        printf("[SUCCESS] Sony SCE Container Recognized!\n");

        // DYNAMIC SCANNING LOOP: Instead of hardcoding offsets, scan through the header
        // area to locate where the decrypted inner ELF structure actually begins.
        uint8_t search_buf[4];
        long scan_offset = 0x50; // Start searching right after the SceSelfHeader struct
        int found_elf = 0;

        while (scan_offset < self_hdr.header_len && scan_offset < file_size - 4) {
            fseek(file, scan_offset, SEEK_SET);
            if (fread(search_buf, 1, 4, file) == 4) {
                if (memcmp(search_buf, ELFMAG, SELFMAG) == 0) {
                    elf_header_offset = scan_offset;
                    found_elf = 1;
                    break;
                }
            }
            scan_offset += 4; // Sub-headers are aligned to 4-byte steps
        }

        if (!found_elf) {
            printf("[ERROR] Could not find inner ELF header signature within container bounds.\n");
            fclose(file);
            return -1;
        }
        printf("[SUCCESS] Dynamically located inner ELF structure at offset: 0x%02lX\n", elf_header_offset);
    } else {
        printf("[INFO] No SCE wrapper detected. Parsing raw executable layout.\n");
        elf_header_offset = 0;
    }

    // Read the actual Executable ELF Header structure
    Elf32_Ehdr elf_hdr;
    fseek(file, elf_header_offset, SEEK_SET);
    if (fread(&elf_hdr, 1, sizeof(Elf32_Ehdr), file) != sizeof(Elf32_Ehdr)) {
        printf("[ERROR] Failed to read Executable ELF Header structures.\n");
        fclose(file);
        return -1;
    }

    // Verify standard ELF Magic signatures
    if (memcmp(elf_hdr.e_ident, ELFMAG, SELFMAG) != 0) {
        printf("[ERROR] Invalid Executable ELF Structure at offset 0x%02lX!\n", elf_header_offset);
        fclose(file);
        return -1;
    }

    printf("[SUCCESS] Executable ELF Structure Verified!\n");
    printf(" -> Original Header Entrypoint Address: 0x%08X\n", elf_hdr.e_entry);
    printf(" -> Program Header Table Offset: 0x%08X | Total Headers Found: %d\n", elf_hdr.e_phoff, elf_hdr.e_phnum);

    printf("\n=== LOADING PHYSICAL ROADMAP SEGMENTS VIA PROGRAM HEADERS ===\n");

    // Seek directly to the dynamic Program Header table location
    long phdr_table_offset = elf_header_offset + elf_hdr.e_phoff;

    for (int i = 0; i < elf_hdr.e_phnum; i++) {
        Elf32_Phdr phdr;
        fseek(file, phdr_table_offset + (i * sizeof(Elf32_Phdr)), SEEK_SET);
        if (fread(&phdr, 1, sizeof(Elf32_Phdr), file) != sizeof(Elf32_Phdr)) {
            printf("[WARNING] Failed to read Program Header #%d\n", i);
            continue;
        }

        // Filter out non-loadable segments
        if (phdr.p_type != PT_LOAD && phdr.p_type != 0x70000001) {
            continue;
        }

        uint32_t vaddr = phdr.p_vaddr;
        long absolute_file_offset = elf_header_offset + phdr.p_offset;
        uint32_t file_size = phdr.p_filesz;
        uint32_t mem_size = phdr.p_memsz;

        // Allocate target process space matching the virtual address layout
        void* mapped_mem = mmap(
            (void*)(uintptr_t)vaddr,
            mem_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
            -1,
            0
        );

        if (mapped_mem == MAP_FAILED) {
            printf("[ERROR] Direct process mmap failed for Segment #%d at target Address 0x%08X\n", i, vaddr);
            continue;
        }

        // Load data segment payload into execution space
        if (file_size > 0) {
            fseek(file, absolute_file_offset, SEEK_SET);
            size_t bytes_read = fread(mapped_mem, 1, file_size, file);
            if (bytes_read < file_size) {
                printf("[WARNING] Segment #%d truncated read: %zu of %u bytes\n", i, bytes_read, file_size);
            }
        }

        // Zero-fill remaining allocation boundaries (.bss uninitialized sections)
        if (mem_size > file_size) {
            memset((uint8_t*)mapped_mem + file_size, 0, mem_size - file_size);
        }

        // Set proper runtime page permissions dynamically
        int prot = 0;
        if (phdr.p_flags & PF_R) prot |= PROT_READ;
        if (phdr.p_flags & PF_W) prot |= PROT_WRITE;
        if (phdr.p_flags & PF_X) prot |= PROT_EXEC;

        if (mprotect(mapped_mem, mem_size, prot) != 0) {
            printf("[WARNING] mprotect failed for Segment #%d\n", i);
        }

        printf("Segment #%d: VirtAddr: 0x%08X | FileOffset: 0x%06lX | FileSize: %u | MemSize: %u -> SUCCESS\n",
               i, vaddr, absolute_file_offset, file_size, mem_size);
    }

    printf("[BRIDGE] Execution Context Entrypoint Address Mapped Natively to: 0x%08X\n", elf_hdr.e_entry);

    printf("\n=== INITIALIZING GRAPHICS LAYER EMULATION ===\n");
    V_GxmRendererInterface gxm_renderer;
    V_RenderCoreType selected_core = VARM_RENDER_CORE_GLES;

    if (varm_gxm_init_renderer(selected_core, &gxm_renderer) == 0) {
        if (gxm_renderer.init_display() != 0) {
            printf("[WARNING] Selected core driver initialization failed. Falling back...\n");
        } else {
            printf("[GXM-GLES] Switched context to: OPENGL ES CORE\n");
            printf("[GXM-GLES] Stock muOS EGL/GLES window surface context hooked successfully!\n");
        }
    }

    fclose(file);
    return 0;
}
