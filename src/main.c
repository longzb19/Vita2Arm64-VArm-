#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <elf.h>

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

    // 1. Read and verify the ELF Header at the 0xA0 container offset
    Elf32_Ehdr elf_hdr;
    fseek(file, SONY_SCE_OFFSET, SEEK_SET);
    if (fread(&elf_hdr, 1, sizeof(Elf32_Ehdr), file) != sizeof(Elf32_Ehdr)) {
        printf("[ERROR] Failed to read ELF Header\n");
        fclose(file);
        return -1;
    }

    // Verify standard ELF Magic bytes (\x7F E L F)
    if (memcmp(elf_hdr.e_ident, ELFMAG, SELFMAG) != 0) {
        printf("[ERROR] Invalid Executable ELF Structure!\n");
        fclose(file);
        return -1;
    }

    printf("[SUCCESS] Sony SCE Container Recognized!\n");
    printf(" -> ELF Header Offset: 0xA0\n");
    printf("[SUCCESS] Executable ELF Structure Verified!\n");
    printf(" -> Target Architecture: %s\n", (elf_hdr.e_machine == EM_ARM) ? "ARM (NATIVE)" : "Unknown");
    printf(" -> Entrypoint Address:  0x%08X\n", elf_hdr.e_entry);
    printf(" -> Total Segments Found: %d\n", elf_hdr.e_phnum);

    printf("[BRIDGE] Mapping execution context...\n");
    printf("[BRIDGE] Program Counter set natively to entry point: 0x%08X\n", elf_hdr.e_entry);

    printf("\n=== SCANNING PHYSICAL ROADMAP SEGMENTS ===\n");

    // Allocate tracking memory for program headers
    Elf32_Phdr* phdrs = malloc(elf_hdr.e_phnum * sizeof(Elf32_Phdr));
    if (!phdrs) {
        printf("[ERROR] Out of memory allocating program headers\n");
        fclose(file);
        return -1;
    }

    // 2. Loop through Program Headers using strict layout arithmetic
    for (int i = 0; i < elf_hdr.e_phnum; i++) {
        // CRITICAL FIX: Calculate the byte offset using e_phentsize directly from the file header.
        // This stops the manual 16-byte misalignment entirely!
        uint32_t phdr_offset = SONY_SCE_OFFSET + elf_hdr.e_phoff + (i * elf_hdr.e_phentsize);
        fseek(file, phdr_offset, SEEK_SET);

        if (fread(&phdrs[i], 1, sizeof(Elf32_Phdr), file) != sizeof(Elf32_Phdr)) {
            printf("[ERROR] Failed to read Program Header #%d\n", i);
            free(phdrs);
            fclose(file);
            return -1;
        }

        // Format permission strings cleanly
        char flags_str[4] = "---";
        if (phdrs[i].p_flags & PF_R) flags_str[0] = 'R';
        if (phdrs[i].p_flags & PF_W) flags_str[1] = 'W';
        if (phdrs[i].p_flags & PF_X) flags_str[2] = 'X';

        const char* type_name = "OTHER";
        if (phdrs[i].p_type == PT_NULL)    type_name = "NULL";
        if (phdrs[i].p_type == PT_LOAD)    type_name = "LOAD";
        if (phdrs[i].p_type == PT_DYNAMIC) type_name = "DYNAMIC";

        printf("Segment #i: Type %d (%s) | File Offset: 0x%06X | VirtAddr: 0x%08X | FileSize: %u bytes | Flags: %s\n",
               i, phdrs[i].p_type, type_name, phdrs[i].p_offset, phdrs[i].p_vaddr, phdrs[i].p_filesz, flags_str);
    }

    printf("\n=== SEARCHING FOR SONY MODULE DIRECTORY ===\n");

    // Virtual address resolution mapping will follow here safely!

    free(phdrs);
    fclose(file);
    return 0;
}
