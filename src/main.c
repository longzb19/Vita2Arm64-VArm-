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
        perror("[ERROR] Could not open file");
        return 1;
    }

    SCE_header sce_hdr;
    long elf_start_pos = 0;

    // Check if the binary is wrapped in Sony's SCE container layout
    if (fread(&sce_hdr, sizeof(SCE_header), 1, file) == 1 && sce_hdr.magic == 0x53434500) {
        printf("[SUCCESS] Sony SCE Container Recognized!\n");
        printf(" -> ELF Offset: 0x%llX\n", (unsigned long long)sce_hdr.elf_offset);
        elf_start_pos = (long)sce_hdr.elf_offset;
    } else {
        printf("[INFO] No SCE container detected. Treating as clean raw ELF.\n");
    }

    // Seek to where the standard ELF executable layout structure starts
    fseek(file, elf_start_pos, SEEK_SET);

    Elf32_Ehdr elf_hdr;
    if (fread(&elf_hdr, sizeof(Elf32_Ehdr), 1, file) != 1) {
        printf("[ERROR] Failed to read ELF main execution header.\n");
        fclose(file);
        return 1;
    }

    // Confirm standard ELF file signature (\x7fELF)
    if (elf_hdr.e_ident[EI_MAG0] != ELFMAG0 || elf_hdr.e_ident[EI_MAG1] != ELFMAG1 ||
        elf_hdr.e_ident[EI_MAG2] != ELFMAG2 || elf_hdr.e_ident[EI_MAG3] != ELFMAG3) {
        printf("[ERROR] File is not a valid ELF executable.\n");
        fclose(file);
        return 1;
    }

    printf("[SUCCESS] Executable ELF Structure Verified!\n");
    printf(" -> Target Architecture: %s\n", (elf_hdr.e_machine == EM_ARM) ? "ARM (NATIVE)" : "Unknown");
    printf(" -> Entrypoint Memory Address: 0x%08X\n", elf_hdr.e_entry);
    printf(" -> Program Headers Count: %d\n", elf_hdr.e_phnum);

    printf("\n=== SCANNING PROGRAM ROADMAP SEGMENTS ===\n");
    
    // Read and print every Program Header segment mapped in the binary layout
    Elf32_Phdr phdr;
    for (int i = 0; i < elf_hdr.e_phnum; i++) {
        // Calculate exact location of this header block on disk
        long phdr_offset = elf_start_pos + elf_hdr.e_phoff + (i * elf_hdr.e_phentsize);
        fseek(file, phdr_offset, SEEK_SET);
        
        if (fread(&phdr, sizeof(Elf32_Phdr), 1, file) == 1) {
            printf("Segment #%d: Type %d | File Offset: 0x%06X | Memory VAddr: 0x%08X | Size: %d bytes | Flags: ", 
                   i, phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_memsz);
            
            // Check segment access permissions (Read, Write, Execute)
            if (phdr.p_flags & PF_R) printf("R"); else printf("-");
            if (phdr.p_flags & PF_W) printf("W"); else printf("-");
            if (phdr.p_flags & PF_X) printf("X"); else printf("-");
            
            if (phdr.p_type == PT_LOAD) {
                printf(" [LOADABLE");
                if (phdr.p_flags & PF_X) printf(" -> ENGINE CODE]"); else printf(" -> RAM DATA]");
            }
            printf("\n");
        }
    }

    fclose(file);
    return 0;
}
