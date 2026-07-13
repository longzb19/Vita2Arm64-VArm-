#include "varm_input.h"
#include "varm_ctrl.h" // 🔗 Brings in the correct SceCtrlData definition automatically
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void varm_input_load_profile(void);

extern bool g_running;
extern char g_game_id[32];
extern VarmRuntimeState g_varm_state;
extern bool g_show_menu;

VarmVirtualTouchState g_virtual_touch = { false, 0, 0, false, 0, 0 };
VarmTouchProfile g_active_profile;

uint32_t g_current_button_mask = 0; // Tracks live button states for the HLE registry reader
uint8_t g_stick_lx = 128;
uint8_t g_stick_ly = 128;
uint8_t g_stick_rx = 128;
uint8_t g_stick_ry = 128;

static SDL_GameController *g_controller = NULL;
static bool s_menu_button_was_pressed = false;

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

    g_active_profile.l2 = (TouchTarget){ 200, 272, false };
    g_active_profile.r2 = (TouchTarget){ 760, 272, false };
    g_active_profile.l3 = (TouchTarget){ 240, 272, true };
    g_active_profile.r3 = (TouchTarget){ 720, 272, true };

    varm_input_load_profile();

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
    static uint32_t s_keyboard_mask = 0;

    g_virtual_touch.front_touch_active = false;
    g_virtual_touch.rear_touch_active  = false;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_running = false;
        }
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool pressed = (event.type == SDL_KEYDOWN);
            uint32_t key_mask = 0;

            switch (event.key.keysym.sym) {
                case SDLK_UP:     key_mask = VITA_CTRL_UP; break;
                case SDLK_DOWN:   key_mask = VITA_CTRL_DOWN; break;
                case SDLK_LEFT:   key_mask = VITA_CTRL_LEFT; break;
                case SDLK_RIGHT:  key_mask = VITA_CTRL_RIGHT; break;
                case SDLK_RETURN: key_mask = VITA_CTRL_START; break;
                case SDLK_ESCAPE: key_mask = VITA_CTRL_SELECT; break;
                case SDLK_z:      key_mask = VITA_CTRL_CROSS; break;
                case SDLK_x:      key_mask = VITA_CTRL_CIRCLE; break;
                case SDLK_c:      key_mask = VITA_CTRL_SQUARE; break;
                case SDLK_v:      key_mask = VITA_CTRL_TRIANGLE; break;
                case SDLK_m:
                    // Handled via held hotkey logic at the bottom of varm_input_poll
                    break;
            }

            if (key_mask != 0) {
                if (pressed) s_keyboard_mask |= key_mask;
                else s_keyboard_mask &= ~key_mask;
            }
        }
    }

    if (!g_controller) {
        s_menu_button_was_pressed = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_M];
        g_current_button_mask = s_keyboard_mask;
        return s_keyboard_mask;
    }

    uint32_t current_mask = s_keyboard_mask;

    int layout_size = sizeof(control_layout) / sizeof(control_layout[0]);
    for (int i = 0; i < layout_size; i++) {
        if (SDL_GameControllerGetButton(g_controller, control_layout[i].sdl_btn)) {
            current_mask |= control_layout[i].vita_mask;
        }
    }

    int16_t raw_lx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
    int16_t raw_ly = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
    int16_t raw_rx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
    int16_t raw_ry = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);

    g_stick_lx = (uint8_t)(((raw_lx + 32768) * 255) / 65535);
    g_stick_ly = (uint8_t)(((raw_ly + 32768) * 255) / 65535);
    g_stick_rx = (uint8_t)(((raw_rx + 32768) * 255) / 65535);
    g_stick_ry = (uint8_t)(((raw_ry + 32768) * 255) / 65535);

    if (SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16384) {
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

    if (SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16384) {
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

    const uint8_t *kb_state = SDL_GetKeyboardState(NULL);
    bool menu_held = (g_controller && SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_GUIDE)) ||
                     (kb_state && kb_state[SDL_SCANCODE_M]);
    bool start_held = (g_controller && SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_START)) ||
                      (kb_state && kb_state[SDL_SCANCODE_RETURN]);
    bool select_held = (g_controller && SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_BACK)) ||
                       (kb_state && kb_state[SDL_SCANCODE_ESCAPE]);

    static uint32_t s_menu_press_start = 0;
    static bool s_menu_action_triggered = false;

    if (menu_held) {
        if (s_menu_press_start == 0) {
            s_menu_press_start = SDL_GetTicks();
            s_menu_action_triggered = false;
        }

        if (start_held && select_held) {
            if (SDL_GetTicks() - s_menu_press_start >= 1000) {
                printf("[INPUT] Hard emergency exit combo executed (Menu+Start+Select). Shutting down...\n");
                g_running = false;
            }
        }
        else if (!s_menu_action_triggered && (SDL_GetTicks() - s_menu_press_start >= 600)) {
            if (g_varm_state == VARM_STATE_GAMEPLAY) {
                g_varm_state = VARM_STATE_MENU_ACTIVE;
                g_show_menu = true;
                printf("[INPUT] Menu button held. Entering visual overlay configuration menu.\n");
            } else if (g_varm_state == VARM_STATE_MENU_ACTIVE) {
                g_varm_state = VARM_STATE_GAMEPLAY;
                g_show_menu = false;
                printf("[INPUT] Menu button held. Resuming translation simulation core.\n");
            }
            s_menu_action_triggered = true;
        }
    } else {
        s_menu_press_start = 0;
        s_menu_action_triggered = false;
    }

    g_current_button_mask = current_mask;
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

void varm_input_shutdown(void) {
    printf("[INPUT] Tearing down controller bindings and closing SDL subsystems...\n");
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = NULL;
    }
}

// ========================================================================
// 🛠️ HIGH-LEVEL EMULATION (HLE) HARDWARE BRIDGE HOOKS
// ========================================================================

/**
 * varm_ctrl_init
 * Direct host hook bound to the guest's 'sceCtrlInit' function hash.
 */
int varm_ctrl_init(void) {
    printf("[HLE BRIDGE] Guest binary invoked sceCtrlInit. System ready.\n");
    return 0;
}

/**
 * varm_ctrl_peek_buffer_positive
 * Translates active SDL inputs straight into the memory structure of the game.
 */
int varm_ctrl_peek_buffer_positive(int port, SceCtrlData *pad_data, int count) {
    if (!pad_data || count <= 0) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        pad_data[i].timeStamp = SDL_GetTicks();
        pad_data[i].buttons   = g_current_button_mask;
        pad_data[i].leftX     = g_stick_lx;
        pad_data[i].leftY     = g_stick_ly;
        pad_data[i].rightX    = g_stick_rx;
        pad_data[i].rightY    = g_stick_ry;
    }

    return 0;
}
