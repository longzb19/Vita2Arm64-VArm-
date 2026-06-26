#include "varm_menu.h"
#include "varm_input.h"
#include <GLES2/gl2.h>
#include <stdio.h>

VarmRuntimeState g_varm_state = VARM_STATE_GAMEPLAY;
static int s_selected_menu_item = 0;
static int s_selected_touch_button = 0; // 0=L2, 1=R2, 2=L3, 3=R3

const char* menu_options[] = {
    "Resume Gameplay",
    "Calibrate Virtual Touch Inputs",
    "Save Touch Mapping Profiles",
    "Exit Translation Environment"
};

void varm_menu_init(void) {
    s_selected_menu_item = 0;
    g_varm_state = VARM_STATE_GAMEPLAY;
}

// Proportional calculation helper: maps a standard 960x544 coordinate space onto OpenGLES NDC (-1.0 to 1.0)
static void convert_vita_to_ndc(uint16_t x, uint16_t y, float *out_x, float *out_y) {
    *out_x = ((float)x / 960.0f) * 2.0f - 1.0f;
    *out_y = -(((float)y / 544.0f) * 2.0f - 1.0f); // Invert standard vertical window axes
}

void varm_menu_render_osd(void) {
    if (!g_show_menu) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 1️⃣ SUB-ENGINE DRAW ROUTINE: Active Vector Crosshair Configurator Layout
    if (g_varm_state == VARM_STATE_EDIT_TOUCH) {
        TouchTarget *target = NULL;
        if (s_selected_touch_button == 0) target = &g_active_profile.l2;
        else if (s_selected_touch_button == 1) target = &g_active_profile.r2;
        else if (s_selected_touch_button == 2) target = &g_active_profile.l3;
        else if (s_selected_touch_button == 3) target = &g_active_profile.r3;

        if (!target) return;

        float ndc_x, ndc_y;
        convert_vita_to_ndc(target->x, target->y, &ndc_x, &ndc_y);

        // Render crosshair layout tracking frames
        GLfloat cross_vertices[] = {
            ndc_x - 0.08f, ndc_y, 0.0f,  ndc_x + 0.08f, ndc_y, 0.0f,
            ndc_x, ndc_y - 0.10f, 0.0f,  ndc_x, ndc_y + 0.10f, 0.0f
        };

        glEnableVertexAttribArray(0);
        // If it's a Rear touch configuration, highlight in Cyber Pink. If Front, highlight in Electric Cyan.
        if (target->is_rear) {
            glVertexAttrib4f(1, 1.0f, 0.0f, 0.5f, 0.95f);
        } else {
            glVertexAttrib4f(1, 0.0f, 0.85f, 1.0f, 0.95f);
        }
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, cross_vertices);
        glDrawArrays(GL_LINES, 0, 4);
        glDisableVertexAttribArray(0);
        glDisable(GL_BLEND);
        return;
    }

    // 2️⃣ STANDARD UI RENDERING: HUD Dashboard Framework Backplate
    GLfloat bg_vertices[] = {
        -0.65f,  0.70f, 0.0f, -0.65f, -0.70f, 0.0f,
         0.65f, -0.70f, 0.0f,  0.65f,  0.70f, 0.0f
    };
    glVertexAttrib4f(1, 0.04f, 0.05f, 0.07f, 0.92f); // Deep Graphite Glass Alpha Layer
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, bg_vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Render Cyan framing accents around the backdrop
    glVertexAttrib4f(1, 0.00f, 0.85f, 1.00f, 0.85f);
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    // Compute interactive list positioning transformations
    float item_spacing = 0.22f;
    float start_y = 0.40f - (s_selected_menu_item * item_spacing);

    GLfloat cursor_vertices[] = {
        -0.58f, start_y + 0.07f, 0.0f, -0.58f, start_y - 0.07f, 0.0f,
         0.58f, start_y - 0.07f, 0.0f,  0.58f, start_y + 0.07f, 0.0f
    };
    glVertexAttrib4f(1, 0.00f, 0.85f, 1.00f, 0.30f);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, cursor_vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glVertexAttrib4f(1, 0.00f, 1.00f, 0.70f, 0.90f); // Bright Cyber Lime selection highlight
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisable(GL_BLEND);
}

void varm_menu_navigate(uint32_t inputs) {
    if (!g_show_menu) return;

    static uint32_t last_inputs = 0;
    uint32_t pressed = inputs & ~last_inputs;
    uint32_t held = inputs; // Track analog shifting states for positioning adjustments
    last_inputs = inputs;

    // 🛠️ MAPPING MODIFIER: Handle real-time vector crosshair adjustments
    if (g_varm_state == VARM_STATE_EDIT_TOUCH) {
        TouchTarget *target = NULL;
        if (s_selected_touch_button == 0) target = &g_active_profile.l2;
        else if (s_selected_touch_button == 1) target = &g_active_profile.r2;
        else if (s_selected_touch_button == 2) target = &g_active_profile.l3;
        else if (s_selected_touch_button == 3) target = &g_active_profile.r3;

        if (!target) return;

        // Shift coordinates up to boundaries safely using the D-Pad
        if (held & VITA_CTRL_UP)    { if (target->y > 4)   target->y -= 4; }
        if (held & VITA_CTRL_DOWN)  { if (target->y < 540) target->y += 4; }
        if (held & VITA_CTRL_LEFT)  { if (target->x > 4)   target->x -= 4; }
        if (held & VITA_CTRL_RIGHT) { if (target->x < 956) target->x += 4; }

        // Press A to cycle between input targets or press B to save and exit out
        if (pressed & VITA_CTRL_CIRCLE) { // Button A -> Circle
            s_selected_touch_button = (s_selected_touch_button + 1) % 4;
            printf("[VARM-UI] Editing next input target: Index [%d]\n", s_selected_touch_button);
        }
        if (pressed & VITA_CTRL_CROSS) {  // Button B -> Cross
            g_varm_state = VARM_STATE_MENU_ACTIVE;
            printf("[VARM-UI] Configuration layout complete. Returning to main engine menu.\n");
        }
        return;
    }

    // Standard core menu selection options processing
    int total_items = sizeof(menu_options) / sizeof(menu_options[0]);

    if (pressed & VITA_CTRL_DOWN) {
        s_selected_menu_item = (s_selected_menu_item + 1) % total_items;
    }
    if (pressed & VITA_CTRL_UP) {
        s_selected_menu_item = (s_selected_menu_item - 1 + total_items) % total_items;
    }

    if (pressed & VITA_CTRL_CIRCLE) { // Button A selection execute trigger
        if (s_selected_menu_item == 0) {
            g_show_menu = false;
            g_varm_state = VARM_STATE_GAMEPLAY;
        } else if (s_selected_menu_item == 1) {
            s_selected_touch_button = 0;
            g_varm_state = VARM_STATE_EDIT_TOUCH; // Shift engine context to position mapping mode
        } else if (s_selected_menu_item == 2) {
            varm_input_save_profile(); // Output configuration file directly to disk space
        } else if (s_selected_menu_item == 3) {
            g_varm_state = VARM_STATE_EXIT;
        }
    }
}
