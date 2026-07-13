#include "varm_ctrl.h"
#include "varm_input.h" // 🔗 Link to your SDL2 input variables and poll definitions
#include <stdio.h>
#include <string.h>

int varm_ctrl_init(void) {
    printf("[VARM-CTRL] Linking Vita HLE input hooks to SDL2 backend...\n");
    // Initialize your SDL peripheral subsystem context directly
    varm_input_init();
    return 0;
}

int varm_ctrl_peek_buffer_positive(int port, SceCtrlData *pad_data, int count) {
    if (!pad_data || count <= 0) return -1;

    // Zero out the target buffer structures safely
    memset(pad_data, 0, sizeof(SceCtrlData) * count);

    // 1. Pump events and pull the unified button mask from your SDL mapper
    uint32_t active_buttons = varm_input_poll();

    // 2. Populate the structural layout that the game engine is querying
    for (int i = 0; i < count; i++) {
        pad_data[i].timeStamp = 0; // Simple baseline timing tracking
        pad_data[i].buttons   = active_buttons;

        // Feed the real-time analog values captured by your SDL hardware scanner
        pad_data[i].leftX     = g_stick_lx;
        pad_data[i].leftY     = g_stick_ly;
        pad_data[i].rightX    = g_stick_rx;
        pad_data[i].rightY    = g_stick_ry;
    }

    return 0;
}
