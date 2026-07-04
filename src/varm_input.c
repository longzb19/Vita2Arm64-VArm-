#include "varm_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Forward declare profile handler before it gets called inside varm_input_init
void varm_input_load_profile(void);

// Link with the global variables tracked across main.c and varm_menu.c
extern bool g_running;
extern char g_game_id[32];
extern VarmRuntimeState g_varm_state;
extern bool g_show_menu;

// Instantiating Global Virtual Touch and Controller States
VarmVirtualTouchState g_virtual_touch = { false, 0, 0, false, 0, 0 };
VarmTouchProfile g_active_profile;

uint8_t g_stick_lx = 128;
uint8_t g_stick_ly = 128;
uint8_t g_stick_rx = 128;
uint8_t g_stick_ry = 128;

static SDL_GameController *g_controller = NULL;
static bool s_menu_button_was_pressed = false;
static uint32_t s_exit_combo_hold_start = 0;

// Standard Layout Mappings: Connects physical SDL buttons directly to virtual Vita button masks
static SdlControlMap control_layout[] = {
    { SDL_CONTROLLER_BUTTON_DPAD_UP,        VITA_CTRL_UP },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN,      VITA_CTRL_DOWN },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT,      VITA_CTRL_LEFT },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT,     VITA_CTRL_RIGHT },
    { SDL_CONTROLLER_BUTTON_START,          VITA_CTRL_START },
    { SDL_CONTROLLER_BUTTON_BACK,           VITA_CTRL_SELECT },
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER,   VITA_CTRL_LTRIGGER },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,  VITA_CTRL_RTRIGGER },
    { SDL_CONTROLLER_BUTTON_Y,              VITA_CTRL_TRIANGLE },
    { SDL_CONTROLLER_BUTTON_B,              VITA_CTRL_CIRCLE },
    { SDL_CONTROLLER_BUTTON_A,              VITA_CTRL_CROSS },
    { SDL_CONTROLLER_BUTTON_X,              VITA_CTRL_SQUARE }
};

void varm_input_init(void) {
    printf("[INPUT] Initializing physical peripheral subsystem via SDL2 GameController interfaces...\n");

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        printf("[INPUT] ERROR: Failed to open SDL Subsystem loop context: %s\n", SDL_GetError());
        return;
    }

    // Baseline profiles mapping definitions init setup
    g_active_profile.l2 = (TouchTarget){ 200, 272, false }; // Front Screen Left Edge
    g_active_profile.r2 = (TouchTarget){ 760, 272, false }; // Front Screen Right Edge
    g_active_profile.l3 = (TouchTarget){ 240, 272, true };  // Rear Panel Left Area
    g_active_profile.r3 = (TouchTarget){ 720, 272, true };  // Rear Panel Right Area

    // Try to load any custom configurations matching the current active Game title ID
    varm_input_load_profile();

    // Enforce initial search loop for physical controllers attached to active ports
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            g_controller = SDL_GameControllerOpen(i);
            if (g_controller) {
                printf("[INPUT] Hardware controller localized successfully: %s\n", SDL_GameControllerName(g_controller));
                break;
            }
        }
    }

    if (!g_controller) {
        printf("[INPUT] Notice: No hardware controller attached. Falling back to passive keyboard mapping modes.\n");
    }
}

uint32_t varm_input_poll(void) {
    SDL_Event event;
    uint32_t current_mask = 0;

    // Reset temporary state structures before fetching fresh event states
    g_virtual_touch.front_touch_active = false;
    g_virtual_touch.rear_touch_active  = false;

    // Read through the active subsystem queue
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_running = false;
        }
    }

    // If no physical gamepad is bound, return early
    if (!g_controller) return 0;

    // 1. Map Digital Button Masks
    int layout_size = sizeof(control_layout) / sizeof(control_layout[0]);
    for (int i = 0; i < layout_size; i++) {
        if (SDL_GameControllerGetButton(g_controller, control_layout[i].sdl_btn)) {
            current_mask |= control_layout[i].vita_mask;
        }
    }

    // 2. Map Analog Joystick Axes
    int16_t raw_lx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
    int16_t raw_ly = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
    int16_t raw_rx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
    int16_t raw_ry = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);

    // Rescale from standard -32768..32767 SDL boundaries to native 0..255 Vita bounds
    g_stick_lx = (uint8_t)(((raw_lx + 32768) * 255) / 65535);
    g_stick_ly = (uint8_t)(((raw_ly + 32768) * 255) / 65535);
    g_stick_rx = (uint8_t)(((raw_rx + 32768) * 255) / 65535);
    g_stick_ry = (uint8_t)(((raw_ry + 32768) * 255) / 65535);

    // 3. Map Virtual Trigger & Stick Clicks to Touch Zones via active user mappings
    if (SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16384) { // L2 pressed
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

    if (SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16384) { // R2 pressed
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

    if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_LEFTSTICK)) { // L3 pressed
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

    if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK)) { // R3 pressed
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

    // 4. Handle Menu Button OSD Toggles (Guide / Home button)
    bool menu_pressed = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_GUIDE);
    if (menu_pressed && !s_menu_button_was_pressed) {
        if (g_varm_state == VARM_STATE_GAMEPLAY) {
            g_varm_state = VARM_STATE_MENU_ACTIVE;
            g_show_menu = true;
            printf("[INPUT] Menu button pressed. Entering visual overlay configuration menu.\n");
        } else if (g_varm_state == VARM_STATE_MENU_ACTIVE) {
            g_varm_state = VARM_STATE_GAMEPLAY;
            g_show_menu = false;
            printf("[INPUT] Menu button pressed. Resuming translation simulation core.\n");
        }
    }
    s_menu_button_was_pressed = menu_pressed;

    // 5. Emergency Combo Exit Check: Hold L1 + R1 + Share/Back for 2 full seconds
    bool combo_held = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) &&
                      SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) &&
                      SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_BACK);

    if (combo_held) {
        if (s_exit_combo_hold_start == 0) {
            s_exit_combo_hold_start = SDL_GetTicks();
        } else if (SDL_GetTicks() - s_exit_combo_hold_start >= 2000) {
            printf("[INPUT] Hard emergency exit shortcut executed. Shutting translation env down...\n");
            g_running = false;
        }
    } else {
        s_exit_combo_hold_start = 0;
    }

    return current_mask;
}

void varm_input_save_profile(void) {
    char profile_path[256];
    mkdir("./profiles", 0777);
    snprintf(profile_path, sizeof(profile_path), "./profiles/%s.cfg", g_game_id);

    FILE *f = fopen(profile_path, "w");
    if (!f) {
        printf("[PROFILE] ERROR: Unable to create profile file path layout context structure.\n");
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
            g_active_profile.l2 = (TouchTarget){(uint16_t)x, (uint16_t)y, (bool)rear};
        } else if (sscanf(line, "R2=%d,%d,%d", &x, &y, &rear) == 3) {
            g_active_profile.r2 = (TouchTarget){(uint16_t)x, (uint16_t)y, (bool)rear};
        } else if (sscanf(line, "L3=%d,%d,%d", &x, &y, &rear) == 3) {
            g_active_profile.l3 = (TouchTarget){(uint16_t)x, (uint16_t)y, (bool)rear};
        } else if (sscanf(line, "R3=%d,%d,%d", &x, &y, &rear) == 3) {
            g_active_profile.r3 = (TouchTarget){(uint16_t)x, (uint16_t)y, (bool)rear};
        }
    }
    fclose(f);
    printf("[PROFILE] Mappings loaded cleanly from title profile: %s\n", profile_path);
}

// FIX: Added missing implementation to resolve the declaration in varm_input.h
void varm_input_shutdown(void) {
    printf("[INPUT] Tearing down controller bindings and closing SDL subsystems...\n");
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = NULL;
    }
}
