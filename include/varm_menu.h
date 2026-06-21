#ifndef VARM_MENU_H
#define VARM_MENU_H

#include <stdbool.h>

typedef enum {
    VARM_STATE_GAMEPLAY,
    VARM_STATE_MENU_ACTIVE
} VarmRuntimeState;

typedef enum {
    MENU_OPT_DECRYPT,
    MENU_OPT_GRAPHICS,
    MENU_OPT_CHEATS,
    MENU_OPT_OVERCLOCK,
    MENU_OPT_EXIT,
    MENU_OPT_COUNT
} VarmMenuOptions;

// External UI tracking context
extern VarmRuntimeState g_varm_state;

void varm_menu_init(void);
void varm_menu_handle_inputs(int key_code, bool pressed);
void varm_menu_render_overlay(void);

#endif // VARM_MENU_H
