#include "hle_elf.h"
#include "hle_kernel.h"
#include <stdio.h>

// Pull in the external resolution system from your core kernel module
extern void* hle_resolve_nid(uint32_t library_nid, uint32_t func_nid);

int hle_elf_bind_imports(Varm_ElfLibraryImport* import_block) {
    if (!import_block) {
        printf("[ELF BINDER ERROR] Passed an empty or invalid import block reference.\n");
        return -1;
    }

    printf("[ELF BINDER] Hooking into module table: '%s' (Lib NID: 0x%08X)\n",
           import_block->library_name, import_block->library_nid);

    int resolved_count = 0;

    for (uint32_t i = 0; i < import_block->num_functions; i++) {
        uint32_t f_nid = import_block->stubs[i].func_nid;
        uint32_t stub_addr = import_block->stubs[i].stub_vaddr;

        // Step 1: Request the physical host function pointer from the engine registry
        void* host_function_target = hle_resolve_nid(import_block->library_nid, f_nid);

        if (host_function_target != NULL) {
            // Step 2: Resolve the guest binary's virtual stub address to our real 64-bit host memory space
            // We pass 0 here to bypass permission restriction during boot setup loader routines
            void* host_patch_target = hle_kernel_resolve_address(stub_addr, 0);

            if (host_patch_target != NULL) {
                // Step 3: Overwrite the virtual jump slot directly with the real host C function memory address
                *(uintptr_t*)host_patch_target = (uintptr_t)host_function_target;

                printf("[ELF BINDER] Successfully linked stub [VAddr: 0x%08X -> Host Target: %p]\n",
                       stub_addr, host_function_target);
                resolved_count++;
            } else {
                printf("[ELF BINDER ERROR] MMU translation failed for target virtual address: 0x%08X\n", stub_addr);
            }
        } else {
            printf("[ELF BINDER WARNING] Skipping unmapped function dependency signature: 0x%08X\n", f_nid);
        }
    }

    printf("[ELF BINDER] Verification check complete for '%s'. Safely bound %d out of %d dependencies.\n\n",
           import_block->library_name, resolved_count, import_block->num_functions);

    return (resolved_count == import_block->num_functions) ? 0 : 1;
}
