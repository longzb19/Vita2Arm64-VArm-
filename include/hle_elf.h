#ifndef HLE_ELF_H
#define HLE_ELF_H

#include <stdint.h>

// Represents an individual function dependency inside the game binary
typedef struct {
    uint32_t func_nid;    // The native function hash (e.g., 0xC5C11EE7 for thread creation)
    uint32_t stub_vaddr;   // The virtual guest memory address where the game calls this function
} Varm_ElfImportStub;

// Represents an entire imported library module dependency (e.g., SceCtrl)
typedef struct {
    uint32_t library_nid;       // The native library block hash (e.g., 0xD2B1A3C4)
    const char* library_name;   // Debug name for readable terminal output
    uint32_t num_functions;     // Total number of functions imported from this module
    Varm_ElfImportStub* stubs;  // Array of specific function slots to patch
} Varm_ElfLibraryImport;

/**
 * hle_elf_bind_imports
 * Loops through an executable's import records and rewrites the jump slots
 * with host execution addresses.
 */
int hle_elf_bind_imports(Varm_ElfLibraryImport* import_block);

#endif // HLE_ELF_H
