#include "varm_menu.h"
#include "varm_input.h"
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>

// Concrete allocation of runtime state context
VarmRuntimeState g_varm_state = VARM_STATE_GAMEPLAY;

static int s_selected_menu_item = 0;
const char* menu_options[] = {
    "Resume Game",
    "Core Settings",
    "Save State (Slot 1)",
    "Load State (Slot 1)",
    "Cheats Engine Layout",
    "Exit Translation Environment"
};

void varm_menu_init(void) {
    s_selected_menu_item = 0;
    g_varm_state = VARM_STATE_GAMEPLAY;
}

// 🎮 Clean GLES2 geometric rendering engine pipeline
void varm_menu_render_osd(void) {
    if (!g_show_menu) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLfloat bg_vertices[] = {
        -0.6f,  0.7f, 0.0f,
        -0.6f, -0.7f, 0.0f,
         0.6f, -0.7f, 0.0f,
         0.6f,  0.7f, 0.0f
    };

    // Generic vertex attribute assignment for standard mobile architectures
    glVertexAttrib4f(1, 0.08f, 0.09f, 0.12f, 0.85f);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, bg_vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    float item_spacing = 0.18f;
    float start_y = 0.3f - (s_selected_menu_item * item_spacing);

    GLfloat cursor_vertices[] = {
        -0.52f, start_y + 0.06f, 0.0f,
        -0.52f, start_y - 0.06f, 0.0f,
         0.52f, start_y - 0.06f, 0.0f,
         0.52f, start_y + 0.06f, 0.0f
    };

    glVertexAttrib4f(1, 0.0f, 0.85f, 0.8f, 0.4f);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, cursor_vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glDisableVertexAttribArray(0);
    glDisable(GL_BLEND);
}

void varm_menu_navigate(uint32_t inputs) {
    if (!g_show_menu) return;

    static uint32_t last_inputs = 0;
    uint32_t pressed = inputs & ~last_inputs;
    last_inputs = inputs;

    int total_items = sizeof(menu_options) / sizeof(menu_options[0]);

    if (pressed & VITA_CTRL_DOWN) {
        s_selected_menu_item = (s_selected_menu_item + 1) % total_items;
    }
    if (pressed & VITA_CTRL_UP) {
        s_selected_menu_item = (s_selected_menu_item - 1 + total_items) % total_items;
    }

    if (pressed & VITA_CTRL_CROSS) {
        printf("[MENU] Executing option: %s\n", menu_options[s_selected_menu_item]);
        if (s_selected_menu_item == 0) {
            g_show_menu = false;
            g_varm_state = VARM_STATE_GAMEPLAY;
        } else if (s_selected_menu_item == 5) {
            exit(0);
        }
    }
}
