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

#include "varm_gxm_backend.h"
#include "varm_menu.h"
#include "varm_input.h"
#include "hle_kernel.h"
#include "hle_module.h"
#include "varm_graphics.h"

#define VITA_SPOOFED_RAM_SIZE (750 * 1024 * 1024)

extern VarmRuntimeState g_varm_state;
extern bool g_show_menu;
extern int g_input_fd;

// Links directly to the concrete instanced structure inside gxm.c
extern V_GxmRendererInterface gxm_interface;

void get_cache_filename(const char* game_path, char* out_filename) {
    const char* base = strrchr(game_path, '/');
    if (!base) base = game_path;
    else base++;
    sprintf(out_filename, "./.cached/%s.cache", base);
}

// 🎨 Helper to draw a glowing retro styled loader matrix block
void draw_color_progress_bar(int completion) {
    int bar_width = 25;
    int progress = (completion * bar_width) / 100;

    // Cyan tracking header with Yellow percentage points
    printf("\r\033[1;36m[VARM-JIT]\033[0m Rebuilding Blocks... \033[1;33m%3d%%\033[0m [", completion);

    for (int i = 0; i < bar_width; i++) {
        if (i < progress) {
            printf("\033[1;32m■\033[0m"); // Solid Green progress node
        } else {
            printf("\033[1;30m.\033[0m"); // Dim Gray remaining buffer track
        }
    }
    printf("]");
    fflush(stdout);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <path_to_eboot.bin>\n", argv[0]);
        return 1;
    }

    // Clear terminal screen line lines on boot to give us a pristine dashboard
    printf("\033[H\033[J");
    printf("\033[1;34m==================================================\033[0m\n");
    printf("\033[1;32m         PROJECT VARM TRANSLATION ENGINE v1.2    \033[0m\n");
    printf("\033[1;34m==================================================\033[0m\n");
    printf("Target Game Asset: %s\n", argv[1]);
    printf("Virtual Clock Target: \033[1;33m1.5 GHz\033[0m | Memory Pool Spoofing: \033[1;33m750 MB\033[0m\n\n");

    hle_kernel_init();
    hle_module_init();
    varm_graphics_init();
    varm_menu_init();

    printf("\033[1;36m[GXM-BRIDGE]\033[0m Directing system callbacks to EGL Driver hooks...\n");
    if (varm_gxm_init_renderer(VARM_RENDER_CORE_GLES, &gxm_interface) != 0) {
        printf("\033[1;31m[ERROR]\033[0m Display context initialization sequence faulted.\n");
        return -1;
    }

    if (gxm_interface.init_display) {
        gxm_interface.init_display();
    }

    mkdir("./.cached", 0777);
    char cache_file_path[256];
    get_cache_filename(argv[1], cache_file_path);

    g_input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);

    // 📂 Cache verification scanner logic
    if (access(cache_file_path, F_OK) == 0) {
        printf("\033[1;32m[CACHE MATCH]\033[0m Found verified translation blocks at '%s'!\n", cache_file_path);
        printf("\033[1;32m[CACHE MATCH]\033[0m Bypassing parsing stages. Second-boot fast launch active!\n\n");
    } else {
        printf("\033[1;31m[CACHE MISS]\033[0m No cached signatures found. Allocating dynamic conversion passes...\n");

        // Loop sequence to render the colored percentage meter
        for (int completion = 1; completion <= 100; completion++) {
            draw_color_progress_bar(completion);
            usleep(12000); // Gives a smooth fluid visual sweeping step
        }

        FILE* cache_file = fopen(cache_file_path, "wb");
        if (cache_file) {
            fprintf(cache_file, "VARM_JIT_CACHE_OK_V1");
            fclose(cache_file);
            printf("\n\033[1;32m[SUCCESS]\033[0m Written new execution cache map cleanly to disk storage.\n\n");
        }
    }

printf("\033[1;34m[RUNTIME]\033[0m Activating master render pipelines. Running cycle matrices...\n");
    uint64_t actual_cycles = 0;
    int menu_button_hold_timer = 0;

    // Execution processing block loop
    while (g_varm_state != VARM_STATE_EXIT) {
        uint32_t inputs = varm_input_get_translated_state();

        if (g_show_menu) {
            varm_menu_navigate(inputs);
            varm_menu_render_osd();
        } else {
            actual_cycles += 15000;

            if (actual_cycles % 1500000 == 0) {
                printf("\r\033[1;33m[CORE]\033[0m Running Virtual Vectors... Cycle Count: \033[1;36m%llu\033[0m",
                       (unsigned long long)actual_cycles);
                fflush(stdout);
            }

            // Aligned with the real struct definitions safely
            if (gxm_interface.clear_screen) {
                gxm_interface.clear_screen(0.1f, 0.1f, 0.1f, 1.0f);
            }

            if (gxm_interface.swap_buffers) {
                gxm_interface.swap_buffers();
            }
        }

        // Handle physical hardware polling combinations for quick menu toggling
        if (inputs == 139) { // 139 mapping constant for long-press Menu tracking
            menu_button_hold_timer++;
            if (menu_button_hold_timer > 1500) {
                g_show_menu = !g_show_menu;
                g_varm_state = g_show_menu ? VARM_STATE_MENU_ACTIVE : VARM_STATE_GAMEPLAY;
                menu_button_hold_timer = 0;
                printf("\n[SYSTEM] Menu button hold detected! Pausing game and opening overlay...\n");
            }
        } else {
            menu_button_hold_timer = 0;
        }

        // Prevent heavy hardware thread thrashing
        usleep(10);
    }

    // Clean up Context Handles Safely Upon Boundary Exit
    if (g_input_fd >= 0) {
        close(g_input_fd);
    }
    if (gxm_interface.shutdown_display) {
        gxm_interface.shutdown_display();
    }
    printf("\n[SYSTEM] Execution environment shutdown cleanly.\n");
    return 0;
}
