#include "varm_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Add this line right here:
void varm_input_load_profile(void);

// Link with the global game ID tracking variable from main.c
extern char g_game_id[32];

// Instantiating Global Variables
bool g_show_menu = false;
VarmVirtualTouchState g_virtual_touch = { false, 0, 0, false, 0, 0 };
VarmTouchProfile g_active_profile;

uint8_t g_stick_lx = 128;
uint8_t g_stick_ly = 128;
uint8_t g_stick_rx = 128;
uint8_t g_stick_ry = 128;

static SDL_GameController *g_controller = NULL;
static bool menu_button_was_pressed = false;  // Tracks the previous frame's state for single click
static uint32_t exit_combo_hold_start = 0;    // Tracks how long the exit combo is held down

// Standard Mappings for SDL Controller Abstraction Layer
static SdlControlMap control_layout[] = {
    { SDL_CONTROLLER_BUTTON_DPAD_UP,        VITA_CTRL_UP },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN,      VITA_CTRL_DOWN },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT,      VITA_CTRL_LEFT },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT,     VITA_CTRL_RIGHT },
    { SDL_CONTROLLER_BUTTON_A,              VITA_CTRL_CROSS },    // South Button -> Cross
    { SDL_CONTROLLER_BUTTON_B,              VITA_CTRL_CIRCLE },   // East Button -> Circle
    { SDL_CONTROLLER_BUTTON_X,              VITA_CTRL_SQUARE },   // West Button -> Square
    { SDL_CONTROLLER_BUTTON_Y,              VITA_CTRL_TRIANGLE }, // North Button -> Triangle
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER,   VITA_CTRL_LTRIGGER },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,  VITA_CTRL_RTRIGGER },
    { SDL_CONTROLLER_BUTTON_START,          VITA_CTRL_START },
    { SDL_CONTROLLER_BUTTON_BACK,           VITA_CTRL_SELECT }
};

int varm_input_init(void) {
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        printf("[INPUT] Error: Failed to initialize SDL GameController subsystem: %s\n", SDL_GetError());
        return -1;
    }

    int joysticks = SDL_NumJoysticks();
    for (int i = 0; i < joysticks; i++) {
        if (SDL_IsGameController(i)) {
            g_controller = SDL_GameControllerOpen(i);
            if (g_controller) {
                printf("[INPUT] Successfully bound hardware controller: %s\n", SDL_GameControllerName(g_controller));
                break;
            }
        }
    }

    if (!g_controller) {
        printf("[INPUT] Warning: No hardware controller detected. Polling raw values.\n");
    }

    // Load default profile layouts fallback
    g_active_profile.l2 = (TouchTarget){150, 272, false};
    g_active_profile.r2 = (TouchTarget){810, 272, false};
    g_active_profile.l3 = (TouchTarget){200, 400, true};
    g_active_profile.r3 = (TouchTarget){760, 400, true};

    varm_input_load_profile();
    return 0;
}

uint32_t varm_input_poll(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_varm_state = VARM_STATE_EXIT;
            return 0;
        }
    }

    if (!g_controller) return 0;

    uint32_t vita_mask = 0;
    int layout_size = sizeof(control_layout) / sizeof(control_layout[0]);

    // 1. Process Core Digital Buttons
    for (int i = 0; i < layout_size; i++) {
        if (SDL_GameControllerGetButton(g_controller, control_layout[i].sdl_btn)) {
            vita_mask |= control_layout[i].vita_mask;
        }
    }

    // 2. Process Dual Analog Sticks (Rescale SDL -32768..32767 to Vita 0..255)
    int16_t sdl_lx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
    int16_t sdl_ly = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
    int16_t sdl_rx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
    int16_t sdl_ry = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);

    g_stick_lx = (uint8_t)(((sdl_lx + 32768) * 255) / 65535);
    g_stick_ly = (uint8_t)(((sdl_ly + 32768) * 255) / 65535);
    g_stick_rx = (uint8_t)(((sdl_rx + 32768) * 255) / 65535);
    g_stick_ry = (uint8_t)(((sdl_ry + 32768) * 255) / 65535);

    // 3. Evaluate L2/R2/L3/R3 Triggers mapped onto Virtual Touch Coordinates
    g_virtual_touch.front_touch_active = false;
    g_virtual_touch.rear_touch_active = false;

    // L2 Trigger -> Virtual Touch mapped zone
    if (SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) {
        if (g_active_profile.l2.is_rear) {
            g_virtual_touch.rear_touch_active = true;
            g_virtual_touch.rear_x = g_active_profile.l2.x;
            g_virtual_touch.rear_y = g_active_profile.l2.y;
        } else {
            g_virtual_touch.front_touch_active = true;
            g_virtual_touch.front_x = g_active_profile.l2.x;
            g_virtual_touch.front_y = g_active_profile.l2.y;
        }
    }

    // R2 Trigger -> Virtual Touch mapped zone
    if (SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) {
        if (g_active_profile.r2.is_rear) {
            g_virtual_touch.rear_touch_active = true;
            g_virtual_touch.rear_x = g_active_profile.r2.x;
            g_virtual_touch.rear_y = g_active_profile.r2.y;
        } else {
            g_virtual_touch.front_touch_active = true;
            g_virtual_touch.front_x = g_active_profile.r2.x;
            g_virtual_touch.front_y = g_active_profile.r2.y;
        }
    }

    // L3 / Click Left Stick
    if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_LEFTSTICK)) {
        if (g_active_profile.l3.is_rear) {
            g_virtual_touch.rear_touch_active = true;
            g_virtual_touch.rear_x = g_active_profile.l3.x;
            g_virtual_touch.rear_y = g_active_profile.l3.y;
        } else {
            g_virtual_touch.front_touch_active = true;
            g_virtual_touch.front_x = g_active_profile.l3.x;
            g_virtual_touch.front_y = g_active_profile.l3.y;
        }
    }

    // R3 / Click Right Stick
    if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK)) {
        if (g_active_profile.r3.is_rear) {
            g_virtual_touch.rear_touch_active = true;
            g_virtual_touch.rear_x = g_active_profile.r3.x;
            g_virtual_touch.rear_y = g_active_profile.r3.y;
        } else {
            g_virtual_touch.front_touch_active = true;
            g_virtual_touch.front_x = g_active_profile.r3.x;
            g_virtual_touch.front_y = g_active_profile.r3.y;
        }
    }

    // 4. Unified Hotkey System (Single-press menu toggle + Multi-button exit combo)
    bool btn_menu   = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_GUIDE);
    bool btn_start  = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_START);
    bool btn_select = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_BACK);

    // --- COMBO CHECK: Hold Menu + Start + Select to Exit ---
    if (btn_menu && btn_start && btn_select) {
        uint32_t now = SDL_GetTicks();
        if (exit_combo_hold_start == 0) {
            exit_combo_hold_start = now;
        } else if (now - exit_combo_hold_start > 600) { // Held down for 600ms continuous
            printf("\n[SYSTEM] Combo hotkey hold verified. Exiting runtime environment cleanly...\n");
            g_varm_state = VARM_STATE_EXIT;
        }
    } else {
        exit_combo_hold_start = 0; // Reset timer immediately if any button in the combo is released
    }

    // --- SINGLE PRESS CHECK: Press Menu once to toggle interface ---
    if (btn_menu && !menu_button_was_pressed) {
        // Only toggle if we aren't intentionally pressing the other combo keys
        if (!btn_start && !btn_select) {
            g_show_menu = !g_show_menu;

            // Synchronize runtime state machine contexts
            g_varm_state = g_show_menu ? VARM_STATE_MENU_ACTIVE : VARM_STATE_GAMEPLAY;

            printf("[SYSTEM] Menu layout hotkey toggled overlay display visible: %s\n",
                   g_show_menu ? "TRUE" : "FALSE");
        }
    }

    // Save current frame state to compare against next frame tick
    menu_button_was_pressed = btn_menu;

    return vita_mask;
}

void varm_input_save_profile(void) {
    char profile_path[256];
    snprintf(profile_path, sizeof(profile_path), "./profiles/%s.cfg", g_game_id);

    struct stat st = {0};
    if (stat("./profiles", &st) == -1) {
        mkdir("./profiles", 0755);
    }

    FILE *f = fopen(profile_path, "w");
    if (!f) {
        printf("[PROFILE] Error: Failed to open %s for saving runtime layouts.\n", profile_path);
        return;
    }

    fprintf(f, "L2=%d,%d,%d\n", g_active_profile.l2.x, g_active_profile.l2.y, g_active_profile.l2.is_rear);
    fprintf(f, "R2=%d,%d,%d\n", g_active_profile.r2.x, g_active_profile.r2.y, g_active_profile.r2.is_rear);
    fprintf(f, "L3=%d,%d,%d\n", g_active_profile.l3.x, g_active_profile.l3.y, g_active_profile.l3.is_rear);
    fprintf(f, "R3=%d,%d,%d\n", g_active_profile.r3.x, g_active_profile.r3.y, g_active_profile.r3.is_rear);
    fclose(f);
    printf("[PROFILE] Configurations written successfully to target structure: %s\n", profile_path);
}

void varm_input_load_profile(void) {
    char profile_path[256];
    snprintf(profile_path, sizeof(profile_path), "./profiles/%s.cfg", g_game_id);

    FILE *f = fopen(profile_path, "r");
    if (!f) {
        printf("[PROFILE] Notice: No unique configuration file found for %s. Using default baseline map.\n", g_game_id);
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        int x, y, rear;
        if (sscanf(line, "L2=%d,%d,%d", &x, &y, &rear) == 3) {
            g_active_profile.l2 = (TouchTarget){x, y, rear};
        } else if (sscanf(line, "R2=%d,%d,%d", &x, &y, &rear) == 3) {
            g_active_profile.r2 = (TouchTarget){x, y, rear};
        } else if (sscanf(line, "L3=%d,%d,%d", &x, &y, &rear) == 3) {
            g_active_profile.l3 = (TouchTarget){x, y, rear};
        } else if (sscanf(line, "R3=%d,%d,%d", &x, &y, &rear) == 3) {
            g_active_profile.r3 = (TouchTarget){x, y, rear};
        }
    }
    fclose(f);
    printf("[PROFILE] Dynamic configuration block imported successfully from file mapping structure.\n");
}

void varm_input_shutdown(void) {
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}
