#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

#include "varm_gxm_backend.h"
#include "varm_menu.h"
#include "varm_input.h"
#include "hle_kernel.h"
#include "hle_module.h"
#include "varm_graphics.h"
#include "varm_cheats.h"
#include "varm_bin_loader.h"
#include "varm_jit.h"

#define VITA_SPOOFED_RAM_SIZE (750 * 1024 * 1024)

VarmRuntimeState g_varm_state = VARM_STATE_GAMEPLAY;
int g_input_fd = -1;
char g_game_id[32] = "UNKNOWN";

bool g_show_menu = true;
bool g_running = true;

void get_cache_directory(char *out_path, size_t max_len) {
    snprintf(out_path, max_len, "./cache/%s/", g_game_id);
}

int main(int argc, char **argv) {
    printf("====================================================================\n");
    printf("                    PROJECT VITA2ARM -- v1.8 \n");
    printf("====================================================================\n");

    if (argc < 2) {
        printf("[FATAL] Error: Target executable file payload parameter missing.\n");
        printf("Usage: %s <path_to_eboot.bin>\n", argv[0]);
        return -1;
    }

    hle_kernel_init();

    printf("\n[Step 1] Loading Sony Executable Container...\n");
    if (varm_loader_load_binary(argv[1]) != 0) {
        printf("[FATAL] Executable validation or binary parsing phase dropped.\n");
        hle_kernel_shutdown();
        return -1;
    }

    printf("\n[Step 2] Resolving HLE Library Native Modules...\n");
    hle_module_init();

    printf("\n[Step 3] Mapping Core System VAddr Memory Highway Blocks...\n");
    hle_kernel_dump_maps();

    printf("\n[Step 4] Starting JIT Translational Compilation Engine Loops...\n");
    varm_jit_init(argv[1]);

    // Step 5: Ready the peripheral interfaces and graphics bridges safely
    varm_graphics_init();
    varm_input_init();

    // Wire up driver handlers to keep UI and game code unified
    varm_gxm_init_renderer(VARM_RENDER_CORE_GLES, &gxm_interface);
    if (gxm_interface.init_display) {
        gxm_interface.init_display();
    }

    GxmSurfaceContext main_surface = {0};
    if (gxm_interface.allocate_surface) {
        gxm_interface.allocate_surface(&main_surface);
    }

    uint32_t inputs;

    // --- MAIN ENGINE RUNTIME LOOP ---
    while (g_running) {
        inputs = varm_input_poll();

        if (gxm_interface.clear_screen) {
            gxm_interface.clear_screen(0.1f, 0.1f, 0.1f, 1.0f);
        }

        if (g_varm_state == VARM_STATE_GAMEPLAY) {
            varm_jit_execute_cycle();
            varm_cheats_inject();
        }

        if (g_show_menu || g_varm_state == VARM_STATE_MENU_ACTIVE || g_varm_state == VARM_STATE_EDIT_TOUCH) {
            varm_menu_render_osd();
            varm_menu_navigate(inputs);
        }

        if (gxm_interface.swap_buffers) {
            gxm_interface.swap_buffers();
        }

        usleep(16666);
    }

    printf("\n[SYSTEM] Tearing down active runtime instances gracefully...\n");
    if (gxm_interface.shutdown_display) {
        gxm_interface.shutdown_display();
    }
    varm_jit_execute_cycle();
    hle_kernel_shutdown();
    printf("[SYSTEM] Teardown execution pipeline finalized clean.\n");

    return 0;
}
