#include "varm_nids.h"
#include <stdlib.h>

// Comparison function for standard C bsearch
int compare_nids(const void *a, const void *b) {
    uint32_t search_key = *(const uint32_t *)a;
    uint32_t table_key = ((const V_NIDMap *)b)->nid;
    
    if (search_key < table_key) return -1;
    if (search_key > table_key) return 1;
    return 0;
}

const char* lookup_varm_nid(uint32_t nid) {
    V_NIDMap *result = bsearch(&nid, varm_nid_table, VARM_NID_COUNT, sizeof(V_NIDMap), compare_nids);
    if (result) {
        return result->func_name;
    }
    return "UnknownFunction";
}
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <elf.h> // Standard Linux ELF structures header

// The exact structure discovered from the vita-toolchain repository
typedef struct {
    uint32_t magic;                 /* 0x53434500 = "SCE\0" */
    uint32_t version;               /* Header version */
    uint16_t sdk_type;
    uint16_t header_type;           /* 1 = SELF, 3 = PKG */
    uint32_t metadata_offset;
    uint64_t header_len;
    uint64_t elf_filesize;
    uint64_t self_filesize;
    uint64_t unknown;
    uint64_t self_offset;
    uint64_t appinfo_offset;
    uint64_t elf_offset;            /* Where the underlying ELF file starts */
    uint64_t phdr_offset;           /* Program header offset */
    uint64_t shdr_offset;           /* Section header offset */
} __attribute__((packed)) SCE_header;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <path_to_vita_binary>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    SCE_header sce_hdr;
    if (fread(&sce_hdr, sizeof(SCE_header), 1, file) != 1) {
        printf("Error: Failed to read Sony container header.\n");
        fclose(file);
        return 1;
    }

    printf("=== VARM-DUMP MODULE LOADER (STEP 1) ===\n");
    printf("Target File: %s\n", argv[1]);

    // Check for Sony's Magic "SCE\0" identifier (0x00454353 in Little Endian)
    if (sce_hdr.magic != 0x00454353) {
        printf("[WARN] Not a wrapped SELF/VELF file. Attempting direct ELF parsing...\n");
        // Reset file pointer to beginning if it's a naked ELF file
        fseek(file, 0, SEEK_SET);
    } else {
        printf("[SUCCESS] Sony SCE Container Recognized!\n");
        printf(" -> Embedded ELF Offset: 0x%llX\n", (unsigned long long)sce_hdr.elf_offset);
        printf(" -> Embedded ELF Size:   %llu bytes\n", (unsigned long long)sce_hdr.elf_filesize);

        // Seek directly to where the true ELF structure lives inside the wrapper
        fseek(file, sce_hdr.elf_offset, SEEK_SET);
    }

    // Parse the inner standard ELF Header
    Elf32_Ehdr elf_hdr;
    if (fread(&elf_hdr, sizeof(Elf32_Ehdr), 1, file) != 1) {
        printf("[ERROR] Failed to read inner ELF Executable header.\n");
        fclose(file);
        return 1;
    }

    // Verify standard ELF Magic (\x7fELF)
    if (elf_hdr.e_ident[EI_MAG0] == ELFMAG0 && elf_hdr.e_ident[EI_MAG1] == ELFMAG1 &&
        elf_hdr.e_ident[EI_MAG2] == ELFMAG2 && elf_hdr.e_ident[EI_MAG3] == ELFMAG3) {

        printf("[SUCCESS] Executable ELF Structure Verified!\n");
        printf(" -> Target Architecture:  %s\n", (elf_hdr.e_machine == EM_ARM) ? "ARM (NATIVE)" : "Unknown/Unsupported");
        printf(" -> Entry Point Address:  0x%08X\n", elf_hdr.e_entry);
        printf(" -> Program Headers Count: %d\n", elf_hdr.e_phnum);
        printf(" -> Section Headers Count: %d\n", elf_hdr.e_shnum);
    } else {
        printf("[ERROR] Inner container data is not a valid ELF executable binary.\n");
    }

    fclose(file);
    return 0;
}
