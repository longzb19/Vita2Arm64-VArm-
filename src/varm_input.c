#include "varm_input.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>   // Added for strstr, sscanf, strncpy
#include <sys/stat.h> // Added for mkdir

// Global States Instantiated Here for the Linker
int g_input_fd = -1;
bool g_show_menu = false;

VarmVirtualTouchState g_virtual_touch = { false, 0, 0, false, 0, 0 };
VarmTouchProfile g_active_profile;
char g_game_id[32] = "DEFAULT";

// Tracker variable (cleaned up duplicate)
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

// Extends parsing engine logic to track game targets cleanly
void varm_input_init_profile(const char* game_path) {
    // Look for unique patterns like "games/PCSA00126/eboot.bin"
    const char* find = strstr(game_path, "games/");
    if (find) {
        sscanf(find, "games/%31[^/]", g_game_id);
    } else {
        strncpy(g_game_id, "DEFAULT", sizeof(g_game_id));
    }

    // Set fallback coordinate targets matching stock positions
    g_active_profile.l2 = (TouchTarget){ 200, 272, true };  // Rear Left Default
    g_active_profile.r2 = (TouchTarget){ 760, 272, true };  // Rear Right Default
    g_active_profile.l3 = (TouchTarget){ 150, 400, false }; // Front Left Default
    g_active_profile.r3 = (TouchTarget){ 810, 400, false }; // Front Right Default

    // Read stored settings file from disk storage block if present
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "./.cached/%s_touch.cfg", g_game_id);

    FILE* file = fopen(config_path, "r");
    if (file) {
        fscanf(file, "L2:%hu,%hu\n", &g_active_profile.l2.x, &g_active_profile.l2.y);
        fscanf(file, "R2:%hu,%hu\n", &g_active_profile.r2.x, &g_active_profile.r2.y);
        fscanf(file, "L3:%hu,%hu\n", &g_active_profile.l3.x, &g_active_profile.l3.y);
        fscanf(file, "R3:%hu,%hu\n", &g_active_profile.r3.x, &g_active_profile.r3.y);
        fclose(file);
        printf("[VARM-INPUT] Loaded custom touch config mapping for game ID: %s\n", g_game_id);
    }
}

void varm_input_save_profile(void) {
    mkdir("./.cached", 0777);
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "./.cached/%s_touch.cfg", g_game_id);

    FILE* file = fopen(config_path, "w");
    if (file) {
        fprintf(file, "L2:%hu,%hu\n", g_active_profile.l2.x, g_active_profile.l2.y);
        fprintf(file, "R2:%hu,%hu\n", g_active_profile.r2.x, g_active_profile.r2.y);
        fprintf(file, "L3:%hu,%hu\n", g_active_profile.l3.x, g_active_profile.l3.y);
        fprintf(file, "R3:%hu,%hu\n", g_active_profile.r3.x, g_active_profile.r3.y);
        fclose(file);
        printf("[VARM-INPUT] Successfully written configuration profile data out to: %s\n", config_path);
    }
}

uint32_t varm_input_get_translated_state(void) {
    if (g_input_fd < 0) return 0;

    static uint32_t active_vita_state = 0;
    int layout_count = sizeof(layout_config) / sizeof(layout_config[0]);
    struct input_event ev;

    while (read(g_input_fd, &ev, sizeof(struct input_event)) > 0) {
        if (ev.type == EV_KEY) {
            for (int i = 0; i < layout_count; i++) {
                if (ev.code == layout_config[i].hw_code) {
                    if (ev.value == 1) active_vita_state |= layout_config[i].vita_mask;
                    else if (ev.value == 0) active_vita_state &= ~layout_config[i].vita_mask;
                }
            }

            // Map inputs dynamically to coordinates retrieved from config profiles
            if (ev.code == HW_KEY_L2) {
                g_virtual_touch.rear_touch_active = (ev.value > 0);
                g_virtual_touch.rear_x = g_active_profile.l2.x;
                g_virtual_touch.rear_y = g_active_profile.l2.y;
            }
            if (ev.code == HW_KEY_R2) {
                g_virtual_touch.rear_touch_active = (ev.value > 0);
                g_virtual_touch.rear_x = g_active_profile.r2.x;
                g_virtual_touch.rear_y = g_active_profile.r2.y;
            }
            if (ev.code == HW_KEY_L3) {
                g_virtual_touch.front_touch_active = (ev.value > 0);
                g_virtual_touch.front_x = g_active_profile.l3.x;
                g_virtual_touch.front_y = g_active_profile.l3.y;
            }
            if (ev.code == HW_KEY_R3) {
                g_virtual_touch.front_touch_active = (ev.value > 0);
                g_virtual_touch.front_x = g_active_profile.r3.x;
                g_virtual_touch.front_y = g_active_profile.r3.y;
            }

            if (ev.code == HW_KEY_MENU) {
                if (ev.value == 1) menu_button_held_since = time(NULL);
                else if (ev.value == 0) menu_button_held_since = 0;
            }
        }
    }

    if (menu_button_held_since != 0 && difftime(time(NULL), menu_button_held_since) >= 1.2) {
        g_show_menu = !g_show_menu;
        menu_button_held_since = 0;
    }

    return active_vita_state;
}
