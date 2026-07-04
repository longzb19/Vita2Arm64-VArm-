#ifndef VARM_BIN_LOADER_H
#define VARM_BIN_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include "varm_bridge.h"

// Added __attribute__((packed)) and renamed to prevent conflict
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
} SceModuleInfoLoader;

// Table layout tracking a game binary's runtime dependencies
typedef struct {
    uint32_t library_nid;
    char     library_name[32];
    uint16_t num_functions;
    uint32_t nid_table_vaddr;
    uint32_t entry_table_vaddr;
} SceLibraryImport;

int varm_loader_load_binary(const char* file_path);
void varm_loader_dump_elf_header(void);

// 🛠️ Option 2 Bridge: Map the old call in main.c to your new implementation
#define varm_bin_load_executable varm_loader_load_binary

#endif // VARM_BIN_LOADER_H
