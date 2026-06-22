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

// Inside your varm_menu.c
void varm_menu_render_overlay(int selected) {
    printf("\033[H"); // Snap to top
    printf("==================================================\n");
    printf("         VITA2ARM SYSTEM DIAGNOSTIC OVERLAY       \n");
    printf("==================================================\n");

    char* items[] = {
        "RESUME TRANSLATION RUNTIME",
        "DECRYPT ACTIVE SONY EBOOT CONTAINER",
        "VITA GRAPHICS CONTROLLER CONFIG",
        "INJECT VITA CHEAT PATCH CODES",
        "VITA CPU CLOCK: [500]",
        "VITA GPU CLOCK: [222]"
    };

    for(int i = 0; i < 6; i++) {
        if (selected == (i + 1))
            printf(" -> [%d] %s\n", i+1, items[i]);
        else
            printf("    [%d] %s\n", i+1, items[i]);
    }
    printf("==================================================\n");
}
