#include "varm_input.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <stdbool.h>
#include <time.h>

int g_input_fd = -1;
bool g_show_menu = false;
static time_t menu_button_held_since = 0;

// Remapping table: Rearrange pairs to map hardware keys to Vita controller masks
static ControlMap layout_config[] = {
    { HW_KEY_UP,     VITA_CTRL_UP,       "D-PAD UP" },
    { HW_KEY_DOWN,   VITA_CTRL_DOWN,     "D-PAD DOWN" },
    { HW_KEY_LEFT,   VITA_CTRL_LEFT,     "D-PAD LEFT" },
    { HW_KEY_RIGHT,  VITA_CTRL_RIGHT,    "D-PAD RIGHT" },
    { HW_KEY_B,      VITA_CTRL_CROSS,    "BUTTON B -> CROSS" },
    { HW_KEY_A,      VITA_CTRL_CIRCLE,   "BUTTON A -> CIRCLE" },
    { HW_KEY_Y,      VITA_CTRL_SQUARE,   "BUTTON Y -> SQUARE" },
    { HW_KEY_X,      VITA_CTRL_TRIANGLE, "BUTTON X -> TRIANGLE" },
    { HW_KEY_L1,     VITA_CTRL_LTRIGGER, "L1 TRIGGER" },
    { HW_KEY_R1,     VITA_CTRL_RTRIGGER, "R1 TRIGGER" },
    { HW_KEY_SELECT, VITA_CTRL_SELECT,   "SELECT BUTTON" },
    { HW_KEY_START,  VITA_CTRL_START,    "START BUTTON" }
};

#define TOTAL_MAPPED_BUTTONS (sizeof(layout_config) / sizeof(ControlMap))

void varm_input_init() {
    // Open the primary evdev device path
    g_input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (g_input_fd < 0) {
        g_input_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
    }

    if (g_input_fd >= 0) {
        printf("[VARM-INPUT] Input Translation Core initialized successfully.\n");
    } else {
        printf("[VARM-INPUT] ERROR: Handheld input subsystem was unreachable.\n");
    }
}

// Scans all buttons at once and handles the menu long-press state tracking
unsigned int varm_input_get_translated_state(void) {
    if (g_input_fd < 0) return 0;

    struct input_event ev;
    static unsigned int active_vita_state = 0;

    // Drain the event buffer cleanly in a single location
    while (read(g_input_fd, &ev, sizeof(struct input_event)) > 0) {
        if (ev.type == EV_KEY) {

            // 1. Handle standard gameplay remappings
            for (int i = 0; i < TOTAL_MAPPED_BUTTONS; i++) {
                if (ev.code == layout_config[i].physical_code) {
                    if (ev.value == 1) { // Button Pressed
                        active_vita_state |= layout_config[i].vita_mask;
                        printf("[PAD-HOOK] %s Pressed! (Bitmask: 0x%08X)\n",
                               layout_config[i].button_name, active_vita_state);
                    } else if (ev.value == 0) { // Button Released
                        active_vita_state &= ~layout_config[i].vita_mask;
                    }
                }
            }

            // 2. Track the Hardware Menu button for a 1.5-second hold
            if (ev.code == HW_KEY_MENU) {
                if (ev.value == 1) {
                    // Button was just pressed down, mark start time
                    menu_button_held_since = time(NULL);
                } else if (ev.value == 0) {
                    // Button released early, reset timer
                    menu_button_held_since = 0;
                }
            }
        }
    }

    // 3. Evaluate the hold timer outside the read loop
    if (menu_button_held_since != 0) {
        if (difftime(time(NULL), menu_button_held_since) >= 1.5) {
            g_show_menu = !g_show_menu;
            menu_button_held_since = 0; // Reset timer immediately to avoid rapid toggle flickering
            printf("[SYSTEM] Menu state toggled via long press! Active: %s\n",
                   g_show_menu ? "TRUE" : "FALSE");

            // Optional: If you want to force runtime state updates into main loop
            // extern int g_varm_state;
            // g_varm_state = g_show_menu ? 0 : 1;
        }
    }

    return active_vita_state;
}

// Legacy polling stub if needed elsewhere by your menu framework
int varm_input_poll(void) {
    if (g_input_fd < 0) return 0;
    struct input_event ev;
    if (read(g_input_fd, &ev, sizeof(struct input_event)) > 0) {
        if (ev.type == EV_KEY && ev.value == 1) {
            return ev.code;
        }
    }
    return 0;
}
