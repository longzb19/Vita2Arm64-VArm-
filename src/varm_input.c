#include "varm_input.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

int g_input_fd = -1;

// 🛠️ REMAPPING TABLE: You can rearrange these pairs anytime!
// This configuration changes Nintendo-style physical layouts to PlayStation standards
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

#define TOTAL_MAPPED_BUTTONS (sizeof(layout_config) / sizeof(layout_config[0]))

void varm_input_init(void) {
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

// Scans all buttons at once and returns a combined bitwise mask for the runtime engine
unsigned int varm_input_get_translated_state(void) {
    if (g_input_fd < 0) return 0;

    struct input_event ev;
    static unsigned int active_vita_state = 0;

    // Drain the event buffer to get current physical layout updates
    while (read(g_input_fd, &ev, sizeof(struct input_event)) > 0) {
        if (ev.type == EV_KEY) {
            // Check if the hardware key is defined in our configuration table
            for (int i = 0; i < TOTAL_MAPPED_BUTTONS; i++) {
                if (ev.code == layout_config[i].physical_code) {
                    if (ev.value == 1) { // Button Pressed
                        active_vita_state |= layout_config[i].vita_mask;
                        printf("[PAD-HOOK] %s Pressed! (Bitmask updated: 0x%08X)\n",
                               layout_config[i].button_name, active_vita_state);
                    } else if (ev.value == 0) { // Button Released
                        active_vita_state &= ~layout_config[i].vita_mask;
                    }
                }
            }
        }
    }
    return active_vita_state;
}
