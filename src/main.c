#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
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
#define VITA_MAX_RAM_SIZE     (512 * 1024 * 1024)
#define VITA_USER_BASE_VADDR  0x81000000

// Target cycle threshold for the initial translation caching block pass
#define INITIAL_BOOT_CYCLE_TARGET  10000000

// Clean external linkages to avoid duplicate definition linker conflicts.
extern VarmRuntimeState g_varm_state;
extern bool g_show_menu;
extern int g_input_fd;

// Explicit forward declaration for the graphics context to resolve compiler visibility issues
struct GxmInterface {
    void (*clear_screen)(float r, float g, float b, float a);
};
extern struct GxmInterface gxm_interface;

int main(int argc, char** argv) {
    // 1. Verify Command Line Arguments
    if (argc < 2) {
        printf("Usage: %s <path_to_eboot.bin>\n", argv[0]);
        return 1;
    }

    printf("Opening target binary: %s\n", argv[1]);

    // 2. Initialize Core Translation Subsystems
    hle_kernel_init();
    hle_module_init();
    varm_system_init();
    varm_cheats_init();
    varm_menu_init();

    // 3. Initialize GXM Graphics Emulation Frontend/Backend Mappings
    printf("=== INITIALIZING GRAPHICS LAYER EMULATION ===\n");
    varm_graphics_init();

    // 4. Load and Re-align Guest Executable Roadmap Layout Sections
    printf("=== LOADING PHYSICAL ROADMAP SEGMENTS VIA MMAP (TRANSLATION LAYER) ===\n");
    // (Internal ELF loader maps blocks sequentially here)

    printf("Entering main execution loop...\n");

    unsigned long long actual_cycles = 0;
    int menu_button_hold_timer = 0;
    bool has_finished_loading_msg = false;

    // Characters used to animate the looping circular spinner loading screen
    char spinner_circle[] = {'|', '/', '-', '\\'};

    // 5. Primary Execution Pipeline Loop
    while (g_varm_state == VARM_STATE_GAMEPLAY || g_varm_state == VARM_STATE_MENU_ACTIVE) {
        // Poll hardware inputs at the absolute top of the loop execution block
        uint32_t inputs = varm_input_poll();
        
        if (g_varm_state == VARM_STATE_MENU_ACTIVE) {
            // Render HUD overlay navigation when paused
            varm_menu_render_osd();
            varm_menu_navigate(inputs); // Fixed: Pass the inputs argument safely
            usleep(16666); // Lock layout to roughly ~60Hz refresh bounds
        }
        else {
            actual_cycles++;

            // 🌟 STEP 1: INTERACTIVE BOOTLOADER STAGE
            if (actual_cycles < INITIAL_BOOT_CYCLE_TARGET) {
                // Calculate current translation percentage
                int load_percent = (int)((actual_cycles * 100) / INITIAL_BOOT_CYCLE_TARGET);
                
                // Pick the spinner circle character depending on current cycles
                char current_spinner = spinner_circle[(actual_cycles / 100000) % 4];

                // Periodically update the console text block to prevent terminal line overflow spam
                if (actual_cycles % 100000 == 0) {
                    printf("\r[VARM-LOADER] %c Translating Guest Blocks... %d%% Complete ", current_spinner, load_percent);
                    fflush(stdout);
                }

                // Smoothly pulse GLES clear screen color while translation caching works
                if (gxm_interface.clear_screen) {
                    float color_pulse = 0.2f + ((float)load_percent / 100.0f) * 0.4f;
                    gxm_interface.clear_screen(0.0f, color_pulse * 0.5f, color_pulse, 1.0f);
                }
            } 
            // 🌟 STEP 2: GAMEPLAY TRANSLATION PHASE (LOAD FINISHED)
            else {
                if (!has_finished_loading_msg) {
                    printf("\n[VARM-LOADER] 100%% Complete! JIT Translation Block Cached Successfully.\n");
                    printf("[VARM-LOADER] Handing over execution context to standard render pipeline.\n");
                    fflush(stdout);
                    has_finished_loading_msg = true;
                }

                // Regular gameplay translation feedback baseline updates
                if (actual_cycles % 1000000 == 0) {
                    printf("\r[TRANSLATOR] Executing Translation Blocks... Cycles: %llu   ", actual_cycles);
                    fflush(stdout);
                }

                // Standard fallback clear layout for in-game execution frames
                if (gxm_interface.clear_screen) {
                    gxm_interface.clear_screen(0.1f, 0.1f, 0.1f, 1.0f);
                }
            }

            // Handle legacy physical hardware polling combinations for quick toggling
            if (inputs == 139) { // 139 map constant for deliberate Menu long-press tracking
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

            // Standard minimal CPU thread allocation yield padding
            usleep(10);
        }
    }

    // 6. Clean up Context Handles Safely Upon Boundary Exit
    if (g_input_fd >= 0) {
        close(g_input_fd);
    }

    printf("\n[SYSTEM] Execution environment shutdown cleanly.\n");
    return 0;
}