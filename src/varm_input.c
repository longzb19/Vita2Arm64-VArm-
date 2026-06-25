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
    { HW_KEY_SELECT, VITA_CTRL_SELECT,   "SELECT KEY" },
    { HW_KEY_START,  VITA_CTRL_START,    "START KEY" }
};

uint32_t varm_input_get_translated_state(void) {
    static uint32_t active_vita_state = 0;
    int layout_count = sizeof(layout_config) / sizeof(layout_config[0]);

    struct input_event ev;
    while (read(g_input_fd, &ev, sizeof(struct input_event)) > 0) {
        if (ev.type == EV_KEY) {
            for (int i = 0; i < layout_count; i++) {
                if (ev.code == layout_config[i].hw_code) {
                    if (ev.value == 1) {
                        active_vita_state |= layout_config[i].vita_mask;
                    } else if (ev.value == 0) {
                        active_vita_state &= ~layout_config[i].vita_mask;
                    }
                }
            }

            if (ev.code == HW_KEY_MENU) {
                if (ev.value == 1) {
                    menu_button_held_since = time(NULL);
                } else if (ev.value == 0) {
                    menu_button_held_since = 0;
                }
            }
        }
    }

    if (menu_button_held_since != 0) {
        if (difftime(time(NULL), menu_button_held_since) >= 1.5) {
            g_show_menu = !g_show_menu;
            menu_button_held_since = 0;
            printf("[SYSTEM] Hardware Menu toggle intercepted! Status: %s\n",
                   g_show_menu ? "ACTIVE" : "HIDDEN");
        }
    }

    return active_vita_state;
}

int varm_input_poll(void) {
    if (g_input_fd < 0) return -1;
    struct input_event ev;
    if (read(g_input_fd, &ev, sizeof(struct input_event)) > 0) {
        if (ev.type == EV_KEY) {
            return ev.code;
        }
    }
    return -1;
}
