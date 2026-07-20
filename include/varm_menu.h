#ifndef VARM_MENU_H
#define VARM_MENU_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    VARM_STATE_GAMEPLAY,
    VARM_STATE_MENU_ACTIVE,
    VARM_STATE_EDIT_TOUCH,
    VARM_STATE_VITAGRAFIX,   // 📊 Integration: VitaGrafix settings submenu
    VARM_STATE_CHEATS,       // 🎮 Integration: Cheat Code Manager submenu
    VARM_STATE_EXIT
} VarmRuntimeState;

// External UI tracking context
extern VarmRuntimeState g_varm_state;

// External global configs for VitaGrafix (accessible by graphics/display loops)
extern bool g_vitagrafix_uncap;
extern int g_vitagrafix_res_preset;

void varm_menu_init(void);
void varm_menu_navigate(uint32_t inputs);
void varm_menu_render_osd(void);
void varm_menu_draw_loading(int completion);
void varm_menu_draw_mock_game(void);
void varm_menu_shutdown(void);

#endif // VARM_MENU_H
