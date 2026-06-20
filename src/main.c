#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <elf.h>

// Sony PlayStation Vita custom container header
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
    uint64_t elf_offset;            /* Offset where the unencrypted ELF starts */
    uint64_t phdr_offset;           /* SCE Program header table offset */
    uint64_t shdr_offset;           /* SCE Section header table offset */
} __attribute__((packed)) SCE_header;
// Blueprint for a Vita Import Library block
typedef struct {
    uint16_t struct_size;     /* Size of this structure (usually 0x24) */
    uint16_t version;         /* Structure version */
    uint16_t flags;           /* Import flags */
    uint16_t num_functions;   /* Total number of functions imported from this library */
    uint32_t libname_nid;     /* NID hash of the target library name (e.g., SceGxm) */
    uint32_t libname_ptr;     /* Virtual memory pointer to the text name string */
    uint32_t nid_table_ptr;   /* Pointer to an array of 32-bit function NIDs the game wants */
    uint32_t func_table_ptr;  /* Pointer to the jump table where our loader must write the real addresses */
} __attribute__((packed)) SceLibStubTable;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <path_to_vita_binary>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("[ERROR] Could not open file");
        return 1;
    }

    SCE_header sce_hdr;
    long elf_start_pos = 0;

    if (fread(&sce_hdr, sizeof(SCE_header), 1, file) == 1 &&
       (sce_hdr.magic == 0x53434500 || sce_hdr.magic == 0x00454353)) {
        printf("[SUCCESS] Sony SCE Container Recognized!\n");
        printf(" -> ELF Header Offset:  0x%llX\n", (unsigned long long)sce_hdr.elf_offset);
        elf_start_pos = (long)sce_hdr.elf_offset;
    } else {
        printf("[INFO] No SCE container detected. Treating as clean raw ELF.\n");
    }

    fseek(file, elf_start_pos, SEEK_SET);

    Elf32_Ehdr elf_hdr;
    if (fread(&elf_hdr, sizeof(Elf32_Ehdr), 1, file) != 1) {
        printf("[ERROR] Failed to read ELF main execution header.\n");
        fclose(file);
        return 1;
    }

    if (elf_hdr.e_ident[EI_MAG0] != ELFMAG0 || elf_hdr.e_ident[EI_MAG1] != ELFMAG1 ||
        elf_hdr.e_ident[EI_MAG2] != ELFMAG2 || elf_hdr.e_ident[EI_MAG3] != ELFMAG3) {
        printf("[ERROR] File is not a valid ELF executable.\n");
        fclose(file);
        return 1;
    }

    printf("[SUCCESS] Executable ELF Structure Verified!\n");
    printf(" -> Target Architecture: %s\n", (elf_hdr.e_machine == EM_ARM) ? "ARM (NATIVE)" : "Unknown");
    printf(" -> Entrypoint Address:  0x%08X\n", elf_hdr.e_entry);
    printf(" -> Total Segments Found: %d\n", elf_hdr.e_phnum);

    printf("\n=== SCANNING PHYSICAL ROADMAP SEGMENTS ===\n");

    // Seek straight to the isolated Program Header table via the SCE container pointer
    fseek(file, (long)sce_hdr.phdr_offset, SEEK_SET);

    Elf32_Phdr phdr;
    for (int i = 0; i < elf_hdr.e_phnum; i++) {
        if (fread(&phdr, sizeof(Elf32_Phdr), 1, file) == 1) {
            printf("Segment #%d: Type %u | File Offset: 0x%06X | VirtAddr: 0x%08X | PhysAddr: 0x%08X | FileSize: %u bytes | MemSize: %u bytes | Flags: ",
                   i, phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_paddr, phdr.p_filesz, phdr.p_memsz);

            if (phdr.p_flags & PF_R) printf("R"); else printf("-");
            if (phdr.p_flags & PF_W) printf("W"); else printf("-");
            if (phdr.p_flags & PF_X) printf("X"); else printf("-");

            if (phdr.p_type == PT_LOAD) {
                printf(" [LOADABLE");
                if (phdr.p_flags & PF_X) printf(" -> ENGINE CODE/TEXT]"); else printf(" -> RAM DATA]");
            }
            printf("\n");
        }
    }

    fclose(file);
    return 0;
}
