#ifndef VARM_BIN_LOADER_H
#define VARM_BIN_LOADER_H

#include <stdint.h>
#include <stdbool.h>

// Simplified representation of a Sony Module Info block inside decrypted binaries
typedef struct {
    uint16_t module_attributes;
    uint8_t  module_version[2];
    char     module_name[27];
    uint8_t  type;
    uint32_t gp_value;
    uint32_t export_top;
    uint32_t export_end;
    uint32_t import_top;
    uint32_t import_end;
    uint32_t nid;
} SceModuleInfo;

// Table layout tracking a game binary's runtime dependencies
typedef struct {
    uint32_t library_nid;
    char     library_name[32];
    uint16_t num_functions;
    uint32_t nid_table_vaddr;
    uint32_t entry_table_vaddr;
} SceLibraryImport;

// Master lifecycle function definitions
int varm_loader_load_binary(const char* file_path);
void varm_loader_dump_elf_header(void);

#endif // VARM_BIN_LOADER_H
