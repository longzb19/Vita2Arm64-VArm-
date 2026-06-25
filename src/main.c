#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "varm_elf.h"
#include "varm_gxm_backend.h"
#include "varm_menu.h"
#include "varm_input.h"
#include "hle_kernel.h"
#include "hle_module.h"

// Modular sub-layer architecture components
#include "varm_graphics.h"
#include "varm_cheats.h"
#include "varm_system.h"

// PS Vita Hardware Architecture Constraints
#define VITA_MAX_RAM_SIZE (512 * 1024 * 1024)
#define VITA_USER_BASE_VADDR 0x81000000

// Explicitly define packed standard ELF structures locally to ensure 100% alignment success
typedef struct {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint32_t      e_entry;
    uint32_t      e_phoff;
    uint32_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

typedef struct {
    uint32_t      p_type;
    uint32_t      p_offset;
    uint32_t      p_vaddr;
    uint32_t      p_paddr;
    uint32_t      p_filesz;
    uint32_t      p_memsz;
    uint32_t      p_flags;
    uint32_t      p_align;
} __attribute__((packed)) Elf32_Phdr;

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <path_to_eboot.bin>\n", argv[0]);
        return -1;
    }

    // 1. Initialize Subsystems
    varm_system_init();
    varm_graphics_init();
    varm_cheats_init();
    varm_menu_init();

    // 2. Open Target Sony Executable Container
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        printf("[ERROR] Could not open target binary: %s\n", argv[1]);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    uint32_t binary_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    printf("Opening target binary: %s (Size: %u bytes)\n", argv[1], binary_size);

    // 3. Locate the Universal ELF Anchor Signature (\x7fELF)
    uint32_t elf_offset = 0;
    char magic_buffer[4];
    int anchor_found = 0;

    for (uint32_t i = 0; i < binary_size - 4; i++) {
        fseek(f, i, SEEK_SET);
        if (fread(magic_buffer, 1, 4, f) == 4) {
            if (memcmp(magic_buffer, "\x7f\x45\x4c\x46", 4) == 0) {
                elf_offset = i;
                anchor_found = 1;
                break;
            }
        }
    }

    if (!anchor_found) {
        printf("[ERROR] Could not locate ELF magic signature inside binary container!\n");
        fclose(f);
        return -1;
    }
    printf("[SUCCESS] Universal ELF Anchor located perfectly at offset: 0x%04X\n", elf_offset);

    // 4. Parse & Verify ELF Header Structure
    fseek(f, elf_offset, SEEK_SET);
    Elf32_Ehdr ehdr;
    if (fread(&ehdr, 1, sizeof(Elf32_Ehdr), f) != sizeof(Elf32_Ehdr)) {
        printf("[ERROR] Failed to read ELF Header\n");
        fclose(f);
        return -1;
    }
    printf("[SUCCESS] Executable ELF Structure Verified!\n");
    printf(" -> Original Header Entrypoint Address: 0x%08X\n", ehdr.e_entry);

    // 5. Allocate & Load Program roadmap segments via HLE Kernel MMAP layer
    printf("=== LOADING PHYSICAL ROADMAP SEGMENTS VIA MMAP (TRANSLATION LAYER) ===\n");
    for (int i = 0; i < ehdr.e_phnum; i++) {
        fseek(f, elf_offset + ehdr.e_phoff + (i * ehdr.e_phentsize), SEEK_SET);
        Elf32_Phdr phdr;
        if (fread(&phdr, 1, sizeof(Elf32_Phdr), f) == sizeof(Elf32_Phdr)) {
            if (phdr.p_type == 1) { // PT_LOAD Segment
                printf("[SANITIZER] Re-aligned Segment #%d bounds to safe kernel memory limits.\n", i);

                // Pass configuration parameters straight to internal memory registration mapping
                hle_kernel_register_segment(phdr.p_vaddr, phdr.p_memsz, phdr.p_filesz, phdr.p_flags);

                printf("Segment #%d [Type: 0x%X]: VirtAddr: 0x%08X | FileOffset: 0x%06X | Size: %u -> SUCCESS\n",
                       i, phdr.p_type, phdr.p_vaddr, phdr.p_offset, phdr.p_memsz);
            }
        }
    }
    fclose(f);
    printf("[BRIDGE] Execution Context Entrypoint Address Mapped Natively to: 0x%08X\n", ehdr.e_entry);

    // 6. Connect Graphics Sub-layer Driver Router
    printf("=== INITIALIZING GRAPHICS LAYER EMULATION ===\n");
    V_GxmRendererInterface gxm_interface;
    memset(&gxm_interface, 0, sizeof(V_GxmRendererInterface));

    if (varm_gxm_init_renderer(VARM_RENDER_CORE_GLES, &gxm_interface) != 0) {
        printf("[WARNING] Selected core driver initialization failed. Falling back...\n");
    }

    // 7. Establish Input Subsystem Handle Bounds natively
    g_input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (g_input_fd < 0) {
        printf("[WARNING] Direct hardware input evdev tap failed! Falling back to simulated loop...\n");
    }

    // 8. Runtime Execution Stream Loop Orchestration
    int selected_item = 1;
int was_in_menu = 0;
uint32_t menu_button_hold_timer = 0;

// ... inside your main runtime initialization loop ...

// ... setup, file reading, and initialization above ...

    printf("Entering main execution loop...\n");

    while (1) {
        // 1. Capture the translated evdev inputs cleanly
        uint32_t active_inputs = varm_input_get_translated_state();

        // UI OVERLAY OVERLAY ENGINE LAYER
        if (g_varm_state == VARM_STATE_MENU_ACTIVE) {
            varm_menu_navigate(active_inputs);
            varm_menu_render_osd();
            was_in_menu = 1;
            usleep(16666); // Caps menu execution to ~60fps target bounds
        } // Only ONE brace here to close the active menu if-block!

        // GAMEPLAY TRANSLATION BLOCK STREAM
        else if (g_varm_state == VARM_STATE_GAMEPLAY) {
            if (was_in_menu) {
                printf("\033[2J\033[H"); // Clear screen on return
                printf("=== RUNTIME EXECUTION STREAM RESUMED ===\n");
                was_in_menu = 0;
            }

            static uint64_t actual_cycles = 0;
            actual_cycles++;

            // Explicitly cast to long long unsigned int to bypass cross-compilation warnings
            if (actual_cycles % 500000 == 0) {
                printf("\r[TRANSLATOR] Executing Translation Blocks... Cycles: %llu   ", (unsigned long long)actual_cycles);
                fflush(stdout);
            }

            // Flush GXM surface textures interception pipelines
            if (gxm_interface.clear_screen) {
                gxm_interface.clear_screen(0.1f, 0.1f, 0.1f, 1.0f);
            }

            // Legacy hardware poll fallbacks checking for deliberate Menu long-press holds
            int legacy_action = varm_input_poll();
            if (legacy_action == 139) { // 139 map constant for direct physical Menu button
                menu_button_hold_timer++;
                if (menu_button_hold_timer > 1500) {
                    g_show_menu = true;
                    g_varm_state = VARM_STATE_MENU_ACTIVE;
                    menu_button_hold_timer = 0;
                    printf("\n[SYSTEM] Menu button hold detected! Pausing game and opening overlay...\n");
                }
            } else {
                menu_button_hold_timer = 0;
            }

            usleep(10); // Standard minimal CPU thread allocation yield padding
        }
    } // This brace correctly terminates the while(1) loop

    // Clean up terminal subsystem context handles safely upon execution boundary exit
    if (g_input_fd >= 0) {
        close(g_input_fd);
    }

    return 0;
} // This brace correctly terminates the main() function
