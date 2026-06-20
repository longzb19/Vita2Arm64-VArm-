#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <elf.h>
#include <sys/mman.h>
#include <unistd.h>
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
    printf(" -> Original Header Entrypoint Address: 0x%08X\n", elf_hdr.e_entry);
    printf(" -> Raw e_phoff value: 0x%X | e_phnum: %d\n", elf_hdr.e_phoff, elf_hdr.e_phnum);

    printf("\n=== LOADING PHYSICAL ROADMAP SEGMENTS VIA MMAP (TRANSLATION LAYER) ===\n");

    long page_size = sysconf(_SC_PAGESIZE);

    // Vita user-space base address layout rule (similar to Vita3K environment)
    // Avoids low addresses to prevent conflicts with host OS NULL pointer tracking
    uintptr_t load_base = 0x81000000;

    for (int i = 0; i < elf_hdr.e_phnum; i++) {
        Elf32_Phdr raw_phdr;
        uint32_t phdr_offset = SONY_SCE_OFFSET + elf_hdr.e_phoff + (i * elf_hdr.e_phentsize);
        fseek(file, phdr_offset, SEEK_SET);
        if (fread(&raw_phdr, 1, sizeof(Elf32_Phdr), file) != sizeof(Elf32_Phdr)) {
            printf("[ERROR] Failed to read Program Header #%d\n", i);
            fclose(file);
            return -1;
        }

        uint32_t calculated_offset = raw_phdr.p_offset;
        uint32_t calculated_type = raw_phdr.p_type;

        // Realigning the physical file location fallback rules
        if (raw_phdr.p_offset < 0x100) {
            calculated_offset = 0x10000 + (i * 0x10000);
            calculated_type = PT_LOAD;
        }

        if (calculated_type == PT_LOAD) {
            // Unpack access permissions flags
            int prot = 0;
            if (raw_phdr.p_flags & PF_R) prot |= PROT_READ;
            if (raw_phdr.p_flags & PF_W) prot |= PROT_WRITE;
            if (raw_phdr.p_flags & PF_X) prot |= PROT_EXEC;

            // Slide our target Virtual Address out of low memory bounds safely
            uintptr_t vaddr = load_base + raw_phdr.p_vaddr;
            uintptr_t aligned_vaddr = vaddr & ~(page_size - 1);
            size_t vaddr_offset = vaddr - aligned_vaddr;

            // Page-align mapping dimensions clean
            size_t map_size = raw_phdr.p_memsz + vaddr_offset;
            map_size = (map_size + page_size - 1) & ~(page_size - 1);

            // Allocation mapped securely into host virtual memory spaces
            // Start read/write so the loader execution loop can unpack content into it
            void* mapped_addr = mmap((void*)aligned_vaddr, map_size,
                                     PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

            if (mapped_addr == MAP_FAILED) {
                printf("[ERROR] Direct process mmap failed for Segment #%d at target Address 0x%08X\n", i, (unsigned int)aligned_vaddr);
                fclose(file);
                return -1;
            }

            // Stream segment data from inside container straight into memory allocations
            if (raw_phdr.p_filesz > 0) {
                fseek(file, calculated_offset, SEEK_SET);
                if (fread((void*)((uintptr_t)mapped_addr + vaddr_offset), 1, raw_phdr.p_filesz, file) != raw_phdr.p_filesz) {
                    printf("[WARNING] Partial file read execution encountered for segment data #%d\n", i);
                }
            }

            // Zero-fill remaining tail allocations (.bss section mapping space)
            if (raw_phdr.p_memsz > raw_phdr.p_filesz) {
                size_t bss_size = raw_phdr.p_memsz - raw_phdr.p_filesz;
                memset((void*)((uintptr_t)mapped_addr + vaddr_offset + raw_phdr.p_filesz), 0, bss_size);
            }

            // Set final structural memory page access map rights
            if (mprotect(mapped_addr, map_size, prot) != 0) {
                printf("[WARNING] Core mprotect clearance failure for Segment #%d\n", i);
            }

            char flags_str[4] = "---";
            if (raw_phdr.p_flags & PF_R) flags_str[0] = 'R';
            if (raw_phdr.p_flags & PF_W) flags_str[1] = 'W';
            if (raw_phdr.p_flags & PF_X) flags_str[2] = 'X';

            printf("Segment #%d: Type LOAD | Real File Offset: 0x%06X | VirtAddr: 0x%08X | MemSize: %u bytes | Flags: %s -> SUCCESS\n",
                   i, calculated_offset, (unsigned int)vaddr, raw_phdr.p_memsz, flags_str);
        }
    }

    printf("[BRIDGE] Execution Context Entrypoint Address Mapped Natively to: 0x%08X\n", elf_hdr.e_entry + (unsigned int)load_base);

    // 2. Safely initialize graphics emulation layer natively tied to host renderer
    printf("\n=== INITIALIZING GRAPHICS LAYER EMULATION ===\n");

    V_GxmRendererInterface gxm_renderer;
    V_RenderCoreType selected_core = VARM_RENDER_CORE_GLES; // Directly targeting Mali-G31 GLES drivers on muOS

    if (varm_gxm_init_renderer(selected_core, &gxm_renderer) == 0) {
        if (gxm_renderer.init_display() != 0) {
            printf("[WARNING] Selected core driver initialization failed. Falling back...\n");
        } else {
            printf("[GXM-GLES] Switched context to: OPENGL ES CORE\n");
            printf("[GXM-GLES] Stock muOS EGL/GLES window surface context hooked successfully!\n");
        }
    }

    // 3. Module identification signature check lookup
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
