#ifndef VARM_MENU_H
#define VARM_MENU_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    VARM_STATE_GAMEPLAY,
    VARM_STATE_MENU_ACTIVE,
    VARM_STATE_EDIT_TOUCH,  // Fixes the undeclared error in varm_menu.c
    VARM_STATE_EXIT         // Fixes the loop boundary error in main.c
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
void varm_menu_navigate(uint32_t inputs);
void varm_menu_render_osd(void);
void varm_menu_handle_inputs(int key_code, bool pressed);
void varm_menu_render_overlay(int selected);

#endif // VARM_MENU_H
