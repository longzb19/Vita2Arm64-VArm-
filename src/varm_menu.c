#include "varm_menu.h"
#include <stdio.h>
#include <stdlib.h>

VarmRuntimeState g_varm_state = VARM_STATE_GAMEPLAY;
static int s_selected_index = 0;
static bool s_overclock_enabled = false;

void varm_menu_init(void) {
    g_varm_state = VARM_STATE_GAMEPLAY;
    s_selected_index = 0;
}

static void execute_menu_selection(void) {
    switch (s_selected_index) {
        case MENU_OPT_DECRYPT:
            printf("[VARM TUI] Running built-in container decryption parser...\n");
            // Your internal decryption function call goes here
            break;

        case MENU_OPT_GRAPHICS:
            printf("[VARM TUI] Adjusting resolution scaling bounds...\n");
            break;

        case MENU_OPT_CHEATS:
            printf("[VARM TUI] Scanning mmap boundary buffers for active cheat codes...\n");
            break;

        case MENU_OPT_OVERCLOCK:
            s_overclock_enabled = !s_overclock_enabled;
            if (s_overclock_enabled) {
                printf("[VARM TUI] Forcing H700 CPU to PERFORMANCE governor.\n");
                system("echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null");
            } else {
                printf("[VARM TUI] Restoring H700 CPU to ONDEMAND governor.\n");
                system("echo ondemand > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor 2>/dev/null");
            }
            break;

        case MENU_OPT_EXIT:
            g_varm_state = VARM_STATE_GAMEPLAY;
            printf("[VARM TUI] Returning to module execution context.\n");
            break;
    }
}

// Intercepts input events passed from your main application loop
void varm_menu_handle_inputs(int key_code, bool pressed) {
    // Structural representation of hotkey parsing: Menu Button + X
    // Adjust key codes based on whether you are polling evdev or an SDL backend
    static bool menu_button_held = false;
    static bool x_button_held = false;

    if (key_code == 125) menu_button_held = pressed; // Example code for Menu
    if (key_code == 42)  x_button_held = pressed;    // Example code for X

    if (menu_button_held && x_button_held) {
        if (g_varm_state == VARM_STATE_GAMEPLAY && pressed) {
            g_varm_state = VARM_STATE_MENU_ACTIVE;
            s_selected_index = 0;
            return;
        }
    }

    if (g_varm_state == VARM_STATE_MENU_ACTIVE && pressed) {
        if (key_code == 108) { // D-Pad Down
            s_selected_index = (s_selected_index + 1) % MENU_OPT_COUNT;
        } else if (key_code == 103) { // D-Pad Up
            s_selected_index = (s_selected_index - 1 + MENU_OPT_COUNT) % MENU_OPT_COUNT;
        } else if (key_code == 28) { // A Button / Enter
            execute_menu_selection();
        }
    }
}

void varm_menu_render_overlay(void) {
    // Clear color to classic TestDisk / diagnostic blue screen
    // If you use OpenGL ES inside varm_gxm_backend:
    // glClearColor(0.0f, 0.0f, 0.55f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT);

    printf("\n==================================================\n");
    printf("         VITA2ARM SYSTEM DIAGNOSTIC OVERLAY       \n");
    printf("==================================================\n");
    printf(" %s [1] DECRYPT ACTIVE SONY EBOOT CONTAINER\n", (s_selected_index == MENU_OPT_DECRYPT) ? "->" : "  ");
    printf(" %s [2] VITA GRAPHICS CONTROLLER CONFIG\n",   (s_selected_index == MENU_OPT_GRAPHICS) ? "->" : "  ");
    printf(" %s [3] INJECT VITA CHEAT PATCH CODES\n",      (s_selected_index == MENU_OPT_CHEATS) ? "->" : "  ");
    printf(" %s [4] ALLWINNER H700 CPU OVERCLOCK: [%s]\n", (s_selected_index == MENU_OPT_OVERCLOCK) ? "->" : "  ", s_overclock_enabled ? "HIGH-SPEED" : "STOCK");
    printf(" %s [5] RESUME TRANSLATION RUNTIME\n",         (s_selected_index == MENU_OPT_EXIT) ? "->" : "  ");
    printf("==================================================\n");
}
