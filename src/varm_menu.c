#include "varm_menu.h"
#include "varm_input.h"
#include "varm_jit.h"   // 🔗 LINKED: Grab the live Program Counter & instruction cycle metrics!
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
}

void varm_menu_init(void) {
    s_selected_menu_item = 0;
    s_selected_touch_button = 0;
    s_shader_initialized = false;
}

#include <math.h>

static void draw_rect(float x1, float y1, float x2, float y2, float r, float g, float b, float a) {
    GLfloat vertices[] = {
        x1, y1, 0.0f, 1.0f,   r, g, b, a,
        x1, y2, 0.0f, 1.0f,   r, g, b, a,
        x2, y2, 0.0f, 1.0f,   r, g, b, a,

        x1, y1, 0.0f, 1.0f,   r, g, b, a,
        x2, y2, 0.0f, 1.0f,   r, g, b, a,
        x2, y1, 0.0f, 1.0f,   r, g, b, a
    };

    GLint pos_loc = glGetAttribLocation(s_menu_program, "position");
    GLint col_loc = glGetAttribLocation(s_menu_program, "color");

    if (pos_loc != -1) {
        glEnableVertexAttribArray(pos_loc);
        glVertexAttribPointer(pos_loc, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), vertices);
    }
    if (col_loc != -1) {
        glEnableVertexAttribArray(col_loc);
        glVertexAttribPointer(col_loc, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), &vertices[4]);
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);

    if (pos_loc != -1) glDisableVertexAttribArray(pos_loc);
    if (col_loc != -1) glDisableVertexAttribArray(col_loc);
}

static void draw_play_icon(float x, float y, float size, float r, float g, float b, float a) {
    GLfloat vertices[] = {
        x - size, y + size, 0.0f, 1.0f,  r, g, b, a,
        x - size, y - size, 0.0f, 1.0f,  r, g, b, a,
        x + size, y,        0.0f, 1.0f,  r, g, b, a
    };
    GLint pos_loc = glGetAttribLocation(s_menu_program, "position");
    GLint col_loc = glGetAttribLocation(s_menu_program, "color");
    if (pos_loc != -1) {
        glEnableVertexAttribArray(pos_loc);
        glVertexAttribPointer(pos_loc, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), vertices);
    }
    if (col_loc != -1) {
        glEnableVertexAttribArray(col_loc);
        glVertexAttribPointer(col_loc, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), &vertices[4]);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (pos_loc != -1) glDisableVertexAttribArray(pos_loc);
    if (col_loc != -1) glDisableVertexAttribArray(col_loc);
}

static void draw_crosshair_icon(float x, float y, float size, float r, float g, float b, float a) {
    // Horizontal bar
    draw_rect(x - size, y + size/5.0f, x + size, y - size/5.0f, r, g, b, a);
    // Vertical bar
    draw_rect(x - size/5.0f, y + size, x + size/5.0f, y - size, r, g, b, a);
}

static void draw_save_icon(float x, float y, float size, float r, float g, float b, float a) {
    // Outer floppy outline
    draw_rect(x - size, y + size, x + size, y - size, r, g, b, a);
    // Label paper
    draw_rect(x - size/2.0f, y - size/3.0f, x + size/2.0f, y - size, 0.9f, 0.9f, 0.9f, a);
    // Sliding shutter metal
    draw_rect(x - size/3.0f, y + size, x + size/3.0f, y + size/3.0f, 0.1f, 0.1f, 0.1f, a);
}

static void draw_exit_icon(float x, float y, float size, float r, float g, float b, float a) {
    // Draw a doorway posts
    draw_rect(x - size, y + size, x - size + size/4.0f, y - size, r, g, b, a);
    draw_rect(x - size, y + size, x + size/4.0f, y + size - size/4.0f, r, g, b, a);
    draw_rect(x - size, y - size + size/4.0f, x + size/4.0f, y - size, r, g, b, a);
    // Draw a small exit arrow pointing right
    draw_rect(x - size/4.0f, y + size/8.0f, x + size/2.0f, y - size/8.0f, r, g, b, a);
    draw_play_icon(x + size, y, size/2.0f, r, g, b, a);
}

static void draw_touch_button(TouchTarget *target, bool is_selected) {
    // Convert Vita coordinates (960x544) to NDC (-1.0 to 1.0)
    // Draw button as 120x80 pixels area in Vita coordinate space
    float size_x = 120.0f / 960.0f * 2.0f; // width in NDC
    float size_y = 80.0f / 544.0f * 2.0f;  // height in NDC

    float center_x = ((float)target->x / 960.0f) * 2.0f - 1.0f;
    float center_y = 1.0f - ((float)target->y / 544.0f) * 2.0f;

    float x1 = center_x - size_x / 2.0f;
    float x2 = center_x + size_x / 2.0f;
    float y1 = center_y + size_y / 2.0f;
    float y2 = center_y - size_y / 2.0f;

    if (is_selected) {
        // Selected button: vibrant red/orange border and transparent red fill
        draw_rect(x1 - 0.015f, y1 + 0.015f, x2 + 0.015f, y2 - 0.015f, 1.0f, 0.3f, 0.3f, 0.9f);
        draw_rect(x1, y1, x2, y2, 0.8f, 0.1f, 0.1f, 0.6f);
    } else {
        // Unselected button: cool blue/cyan transparent box
        draw_rect(x1, y1, x2, y2, 0.1f, 0.5f, 0.8f, 0.5f);
    }
}

void varm_menu_render_osd(void) {
    // 1. Ensure OpenGLES shaders are compiled and bound for the UI overlay
    ensure_menu_shader_ready();

    // 2. Fetch the live state tracking statistics from the translation engine
    uint32_t current_pc = varm_jit_get_pc();
    unsigned long long total_cycles = varm_jit_get_cycles();

    // 3. Format the metric strings for the OSD overlay debugging window
    char pc_buffer[64];
    char cycles_buffer[64];
    snprintf(pc_buffer, sizeof(pc_buffer), "GUEST PC: 0x%08X", current_pc);
    snprintf(cycles_buffer, sizeof(cycles_buffer), "CYCLES: %llu", total_cycles);

    // 4. Rate-limited console log mirror so you can audit live values during testing
    // without flooding standard output or hitting carriage-return locks
    static uint32_t diagnostic_frame_ticks = 0;
    if (++diagnostic_frame_ticks % 60 == 0) {
        printf("\033[1;35m[OSD-METRICS]\033[0m %s | %s | State Pipeline: %d\n",
               pc_buffer, cycles_buffer, g_varm_state);
    }

    // 5. --- OPENGL ES 2.0 PRIMITIVE TEXT/WINDOW LAYOUT DRAWING ---
    glUseProgram(s_menu_program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render OSD HUD Top bar backdrop
    draw_rect(-0.95f, 0.95f, 0.95f, 0.75f, 0.15f, 0.15f, 0.2f, 0.85f);
    
    // Cycle Progress Bar inside HUD (based on cycles counter) to visually reflect execution
    float cycle_ratio = (float)(total_cycles % 10000000ULL) / 10000000.0f;
    draw_rect(-0.9f, 0.88f, -0.9f + cycle_ratio * 1.8f, 0.82f, 0.0f, 0.85f, 0.4f, 0.9f);

    // 6. Draw your standard interactive menu array item selections on screen
    if (g_varm_state == VARM_STATE_MENU_ACTIVE) {
        int total_items = sizeof(menu_options) / sizeof(menu_options[0]);
        float box_y_start = 0.4f;
        float box_height = 0.15f;
        float box_spacing = 0.05f;

        // Backdrop for the whole menu
        draw_rect(-0.5f, 0.5f, 0.5f, -0.5f, 0.05f, 0.05f, 0.1f, 0.9f);

        float icon_x = -0.38f;
        for (int i = 0; i < total_items; i++) {
            float y1 = box_y_start - i * (box_height + box_spacing);
            float y2 = y1 - box_height;
            float center_y = (y1 + y2) / 2.0f;

            float r = 1.0f, g = 1.0f, b = 1.0f;
            if (i == s_selected_menu_item) {
                // Highlight active index layout context (vibrant orange-yellow)
                draw_rect(-0.45f, y1, 0.45f, y2, 0.95f, 0.75f, 0.2f, 0.95f);
                r = 0.1f; g = 0.1f; b = 0.15f; // Dark icon for selected item
            } else {
                // Draw regular menu item (darker neutral)
                draw_rect(-0.45f, y1, 0.45f, y2, 0.2f, 0.2f, 0.25f, 0.8f);
            }

            // Draw matching custom retro geometric icon next to the bar
            if (i == 0) {
                draw_play_icon(icon_x, center_y, 0.035f, r, g, b, 1.0f);
            } else if (i == 1) {
                draw_crosshair_icon(icon_x, center_y, 0.035f, r, g, b, 1.0f);
            } else if (i == 2) {
                draw_save_icon(icon_x, center_y, 0.035f, r, g, b, 1.0f);
            } else if (i == 3) {
                draw_exit_icon(icon_x, center_y, 0.035f, r, g, b, 1.0f);
            }
        }
    }
    else if (g_varm_state == VARM_STATE_EDIT_TOUCH) {
        // Visual Touch Calibration Mode
        // Instruction Backdrop at the bottom
        draw_rect(-0.9f, -0.6f, 0.9f, -0.85f, 0.1f, 0.1f, 0.15f, 0.85f);

        // Draw active touch calibration target visual representations
        draw_touch_button(&g_active_profile.l2, s_selected_touch_button == 0);
        draw_touch_button(&g_active_profile.r2, s_selected_touch_button == 1);
        draw_touch_button(&g_active_profile.l3, s_selected_touch_button == 2);
        draw_touch_button(&g_active_profile.r3, s_selected_touch_button == 3);
    }
}

void varm_menu_draw_loading(int completion) {
    // 1. Ensure OpenGLES shaders are compiled and bound
    ensure_menu_shader_ready();
    glUseProgram(s_menu_program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Clear screen to dark background
    glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 2. Draw loading screen backdrop container
    draw_rect(-0.6f, 0.25f, 0.6f, -0.25f, 0.15f, 0.15f, 0.2f, 0.85f);

    // 3. Draw active progress bar (cyan)
    float ratio = (float)completion / 100.0f;
    float fill_x = -0.5f + ratio * 1.0f; // maps from -0.5 to 0.5
    draw_rect(-0.5f, 0.08f, fill_x, -0.08f, 0.0f, 0.85f, 0.85f, 0.95f);

    // 4. Draw dynamic pulsing loading indicator to represent loop activity
    static float spinner_angle = 0.0f;
    spinner_angle += 0.25f;
    float pulse_scale = 0.04f + 0.02f * sinf(spinner_angle);
    draw_rect(-0.04f, -0.16f + pulse_scale, 0.04f, -0.16f - pulse_scale, 0.95f, 0.75f, 0.2f, 0.95f);

    // 5. Swap buffers to show live frame
    extern SDL_Window *g_window;
    if (g_window) {
        SDL_GL_SwapWindow(g_window);
    }
}

void varm_menu_navigate(uint32_t inputs) {
    uint32_t pressed = inputs; // Map inputs to expected format
    uint32_t current_tick = SDL_GetTicks();
    static uint32_t last_nav_tick = 0;
    static uint32_t last_circle_tick = 0;
    static uint32_t last_cross_tick = 0;

    if (g_varm_state == VARM_STATE_EDIT_TOUCH) {
        // Target active profile targets directly
        TouchTarget *targets[4] = {
            &g_active_profile.l2,
            &g_active_profile.r2,
            &g_active_profile.l3,
            &g_active_profile.r3
        };

        TouchTarget *target = targets[s_selected_touch_button];

        // Continuous movement allowed for D-Pad configuration coordinates
        if (pressed & VITA_CTRL_UP)    { if (target->y > 4)   target->y -= 4; }
        if (pressed & VITA_CTRL_DOWN)  { if (target->y < 540) target->y += 4; }
        if (pressed & VITA_CTRL_LEFT)  { if (target->x > 4)   target->x -= 4; }
        if (pressed & VITA_CTRL_RIGHT) { if (target->x < 956) target->x += 4; }

        // Cooldown rate-limiting applied to action target switching and exit:
        if (pressed & VITA_CTRL_CIRCLE) {
            if (current_tick - last_circle_tick >= 250) {
                s_selected_touch_button = (s_selected_touch_button + 1) % 4;
                printf("[VARM-UI] Editing next input target: Index [%d]\n", s_selected_touch_button);
                last_circle_tick = current_tick;
            }
        }
        if (pressed & VITA_CTRL_CROSS) {
            if (current_tick - last_cross_tick >= 250) {
                g_varm_state = VARM_STATE_MENU_ACTIVE;
                printf("[VARM-UI] Configuration layout complete. Returning to main engine menu.\n");
                last_cross_tick = current_tick;
            }
        }
        return;
    }

    int total_items = sizeof(menu_options) / sizeof(menu_options[0]);

    // Apply strict cooldown rate-limiting to menu item navigation and selection
    if (pressed & (VITA_CTRL_DOWN | VITA_CTRL_UP | VITA_CTRL_CIRCLE)) {
        if (current_tick - last_nav_tick < 220) {
            // Ignore during cooldown to prevent double jumps
            return;
        }
        last_nav_tick = current_tick;
    }

    if (pressed & VITA_CTRL_DOWN) {
        s_selected_menu_item = (s_selected_menu_item + 1) % total_items;
    }
    if (pressed & VITA_CTRL_UP) {
        s_selected_menu_item = (s_selected_menu_item - 1 + total_items) % total_items;
    }

    if (pressed & VITA_CTRL_CIRCLE) {
        if (s_selected_menu_item == 0) {
            extern bool g_show_menu;
            g_show_menu = false;
            g_varm_state = VARM_STATE_GAMEPLAY;
        } else if (s_selected_menu_item == 1) {
            g_varm_state = VARM_STATE_EDIT_TOUCH;
        } else if (s_selected_menu_item == 2) {
            varm_input_save_profile(); // Save mapping changes persistently to profile config file
            printf("[VARM-UI] Profiles saved successfully to engine system cache matrices.\n");
        } else if (s_selected_menu_item == 3) {
            g_varm_state = VARM_STATE_EXIT;
        }
    }
}
