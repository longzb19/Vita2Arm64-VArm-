#include "varm_menu.h"
#include "varm_system.h"
#include <stdio.h>
#include <stdlib.h>

// Restore the global state variables that the placeholder hid
VarmRuntimeState g_varm_state = VARM_STATE_MENU_ACTIVE;

void varm_menu_init(void) {
    // Start inside the menu runtime overlay
    g_varm_state = 0;
}

void varm_menu_render_overlay(int selected) {
    char cpu_label[64];
    char gpu_label[64];

    // Read values dynamically from the core speed driver variables
    snprintf(cpu_label, sizeof(cpu_label), "VITA CPU CLOCK: [%d MHz]", varm_system_get_cpu_clock());
    snprintf(gpu_label, sizeof(gpu_label), "VITA GPU CLOCK: [%d MHz]", varm_system_get_gpu_clock());

    const char* items[] = {
        "RESUME TRANSLATION RUNTIME",
        "DECRYPT ACTIVE SONY EBOOT CONTAINER",
        "VITA GRAPHICS CONTROLLER CONFIG",
        "INJECT VITA CHEAT PATCH CODES",
        cpu_label,
        gpu_label,
        "QUIT EMULATOR"
    };
    int total_items = sizeof(items) / sizeof(items[0]);

    for (int i = 0; i < total_items; i++) {
        if ((i + 1) == selected) {
            printf(" -> [%d] %s\n", i + 1, items[i]);
        } else {
            printf("    [%d] %s\n", i + 1, items[i]);
        }
    }
    printf("==================================================\n");
}
