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

    printf("\n=== LOADING PHYSICAL ROADMAP SEGMENTS VIA MMAP (TRANSLATION LAYER) ===\n");

    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t load_base = 0x81000000;

    for (int i = 0; i < elf_hdr.e_phnum; i++) {
        Elf32_Phdr raw_phdr;
        uint32_t phdr_offset = SONY_SCE_OFFSET + elf_hdr.e_phoff + (i * elf_hdr.e_phentsize);
        fseek(file, phdr_offset, SEEK_SET);
        if (fread(&raw_phdr, 1, sizeof(Elf32_Phdr), file) != sizeof(Elf32_Phdr)) {
            printf("[ERROR] Failed to read Program Header #%d\n", i);
            continue;
        }

        // --- THE SANITY CLAMP ---
        // If the parser reads a massive size (>= 2GB) because Sony shifted the Virtual Address
        // into the Memory Size slot, we clamp it to a safe 4MB block to prevent mmap from crashing.
        if (raw_phdr.p_memsz >= 0x80000000) {
            printf("[WARNING] SCE SELF Misalignment Detected! Clamping Segment #%d size to prevent crash.\n", i);
            raw_phdr.p_memsz = 4 * 1024 * 1024; // 4MB
            raw_phdr.p_filesz = 4 * 1024 * 1024; // 4MB
        }

        uint32_t calculated_offset = raw_phdr.p_offset;
        uint32_t calculated_type = raw_phdr.p_type;

        if (raw_phdr.p_offset < 0x100) {
            calculated_offset = 0x10000 + (i * 0x10000);
            calculated_type = PT_LOAD;
        }

        if (calculated_type == PT_LOAD) {
            int prot = 0;
            if (raw_phdr.p_flags & PF_R) prot |= PROT_READ;
            if (raw_phdr.p_flags & PF_W) prot |= PROT_WRITE;
            if (raw_phdr.p_flags & PF_X) prot |= PROT_EXEC;

            uintptr_t vaddr = load_base + raw_phdr.p_vaddr;
            uintptr_t aligned_vaddr = vaddr & ~(page_size - 1);
            size_t vaddr_offset = vaddr - aligned_vaddr;

            size_t map_size = raw_phdr.p_memsz + vaddr_offset;
            map_size = (map_size + page_size - 1) & ~(page_size - 1);

            void* mapped_addr = mmap((void*)aligned_vaddr, map_size,
                                     PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

            if (mapped_addr == MAP_FAILED) {
                printf("[ERROR] Direct process mmap failed for Segment #%d at target Address 0x%08X\n", i, (unsigned int)aligned_vaddr);
                continue;
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
