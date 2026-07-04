#include "varm_menu.h"
#include "varm_input.h"
#include <GLES2/gl2.h>
#include <stdio.h>

extern VarmRuntimeState g_varm_state;
static int s_selected_menu_item = 0;
static int s_selected_touch_button = 0;

static GLuint s_menu_program = 0;
static bool s_shader_initialized = false;

const char* menu_options[] = {
    "Resume Gameplay",
    "Calibrate Virtual Touch Inputs",
    "Save Touch Mapping Profiles",
    "Exit Translation Environment"
};

static void ensure_menu_shader_ready(void) {
    if (s_shader_initialized) return;

    const char* v_src =
        "attribute vec4 position;\n"
        "attribute vec4 color;\n"
        "varying vec4 v_color;\n"
        "void main() {\n"
        "   gl_Position = position;\n"
        "   v_color = color;\n"
        "}\n";

    const char* f_src =
        "precision mediump float;\n"
        "varying vec4 v_color;\n"
        "void main() {\n"
        "   gl_FragColor = v_color;\n"
        "}\n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &v_src, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &f_src, NULL);
    glCompileShader(fs);

    s_menu_program = glCreateProgram();
    glAttachShader(s_menu_program, vs);
    glAttachShader(s_menu_program, fs);
    glLinkProgram(s_menu_program);

    s_shader_initialized = true;
    printf("[VARM-UI] Visual Text OSD Overlay Core Shaders Bound Cleanly.\n");
}

void varm_menu_init(void) {
    s_selected_menu_item = 0;
    s_shader_initialized = false;
}

void varm_menu_render_osd(void) {
    ensure_menu_shader_ready();
    glUseProgram(s_menu_program);

    // Draw baseline flat 2D projection vectors for your selection highlights here
}

void varm_menu_navigate(uint32_t inputs) {
    static uint32_t last_inputs = 0;
    uint32_t pressed = inputs & ~last_inputs;
    uint32_t held = inputs;
    last_inputs = inputs;

    if (g_varm_state == VARM_STATE_EDIT_TOUCH) {
        TouchTarget *target = NULL;
        if (s_selected_touch_button == 0) target = &g_active_profile.l2;
        if (s_selected_touch_button == 1) target = &g_active_profile.r2;
        if (s_selected_touch_button == 2) target = &g_active_profile.l3;
        if (s_selected_touch_button == 3) target = &g_active_profile.r3;

        if (!target) return;

        if (held & VITA_CTRL_UP)    { if (target->y > 4)   target->y -= 4; }
        if (held & VITA_CTRL_DOWN)  { if (target->y < 540) target->y += 4; }
        if (held & VITA_CTRL_LEFT)  { if (target->x > 4)   target->x -= 4; }
        if (held & VITA_CTRL_RIGHT) { if (target->x < 956) target->x += 4; }

        if (pressed & VITA_CTRL_CIRCLE) {
            s_selected_touch_button = (s_selected_touch_button + 1) % 4;
            printf("[VARM-UI] Editing next input target: Index [%d]\n", s_selected_touch_button);
        }
        if (pressed & VITA_CTRL_CROSS) {
            g_varm_state = VARM_STATE_MENU_ACTIVE;
            printf("[VARM-UI] Configuration layout complete. Returning to main engine menu.\n");
        }
        return;
    }

    int total_items = sizeof(menu_options) / sizeof(menu_options[0]);

    if (pressed & VITA_CTRL_DOWN) {
        s_selected_menu_item = (s_selected_menu_item + 1) % total_items;
    }
    if (pressed & VITA_CTRL_UP) {
        s_selected_menu_item = (s_selected_menu_item - 1 + total_items) % total_items;
    }

    if (pressed & VITA_CTRL_CIRCLE) {
        if (s_selected_menu_item == 0) { // Resume
            extern bool g_show_menu;
            g_show_menu = false;
            g_varm_state = VARM_STATE_GAMEPLAY;
        } else if (s_selected_menu_item == 1) { // Calibrate
            g_varm_state = VARM_STATE_EDIT_TOUCH;
        } else if (s_selected_menu_item == 3) { // Exit
            extern bool g_running;
            g_running = false;
        }
    }
}

void varm_menu_handle_inputs(int key_code, bool pressed) {}
void varm_menu_render_overlay(int selected) {}
