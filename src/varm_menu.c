#include "varm_menu.h"
#include "varm_input.h"
#include "varm_jit.h"   // 🔗 LINKED: Grab Program Counter & cycle metrics
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern VarmRuntimeState g_varm_state;
extern VarmTouchProfile g_active_profile;

// --- Submenu State Management ---
static int s_selected_menu_item = 0;
static int s_selected_touch_button = 0;
static int s_selected_vitagrafix_item = 0;
static int s_selected_cheat_item = 0;

static GLuint s_menu_program = 0;
static GLuint s_text_program = 0;
static GLuint s_font_texture = 0;
static bool s_shader_initialized = false;

// --- Global Hook Configs for VitaGrafix & Cheats ---
bool g_vitagrafix_uncap = false;
int g_vitagrafix_res_preset = 0; // 0: Native (960x544), 1: Performance (720x408), 2: HD Upscale (1280x720)
const char* res_presets[] = { "960x544 (Native)", "720x408 (Perf)", "1280x720 (HD)" };

typedef struct {
    const char* name;
    uint32_t addr;
    uint32_t val;
    bool active;
} VarmCheat;

#define MAX_CHEATS 3
static VarmCheat s_cheats[MAX_CHEATS] = {
    { "Infinite Health (Max HP)", 0x810000A0, 0x000003E7, false },
    { "Infinite Ammo / Focus",   0x810000F4, 0x00000063, false },
    { "Uncap Game Engine Speed",  0x8240B10C, 0x00000001, false }
};

// Embedded 8x8 retro font atlas (ASCII 32 to 127)
static const uint8_t s_font_8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' ' (32)
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00}, // '!'
    {0x24, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00}, // '"'
    {0x24, 0x24, 0x7E, 0x24, 0x7E, 0x24, 0x24, 0x00}, // '#'
    {0x08, 0x3E, 0x1C, 0x08, 0x1C, 0x3E, 0x08, 0x00}, // '$'
    {0x00, 0x62, 0x66, 0x08, 0x10, 0x66, 0x46, 0x00}, // '%'
    {0x3C, 0x66, 0x3C, 0x38, 0x67, 0x66, 0x3F, 0x00}, // '&'
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // '''
    {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00}, // '('
    {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00}, // ')'
    {0x08, 0x2A, 0x1C, 0x08, 0x1C, 0x2A, 0x08, 0x00}, // '*'
    {0x00, 0x10, 0x10, 0x7C, 0x10, 0x10, 0x00, 0x00}, // '+'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30}, // ','
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, // '-'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // '.'
    {0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00}, // '/'
    {0x3E, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3E, 0x00}, // '0'
    {0x18, 0x1C, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // '1'
    {0x3E, 0x66, 0x06, 0x1E, 0x30, 0x62, 0x7E, 0x00}, // '2'
    {0x3E, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3E, 0x00}, // '3'
    {0x1C, 0x3C, 0x6C, 0x6C, 0x7E, 0x0C, 0x1E, 0x00}, // '4'
    {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3E, 0x00}, // '5'
    {0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3E, 0x00}, // '6'
    {0x7E, 0x66, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x00}, // '7'
    {0x3E, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3E, 0x00}, // '8'
    {0x3E, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00}, // '9'
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, // ':'
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00}, // ';'
    {0x00, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x00, 0x00}, // '<'
    {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00}, // '='
    {0x00, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x00, 0x00}, // '>'
    {0x3E, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00}, // '?'
    {0x3E, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00}, // '@'
    {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00}, // 'A'
    {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00}, // 'B'
    {0x3E, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3E, 0x00}, // 'C'
    {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00}, // 'D'
    {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00}, // 'E'
    {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00}, // 'F'
    {0x3E, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00}, // 'G'
    {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00}, // 'H'
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // 'I'
    {0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3E, 0x00}, // 'J'
    {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00}, // 'K'
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00}, // 'L'
    {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, // 'M'
    {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00}, // 'N'
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // 'O'
    {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00}, // 'P'
    {0x3E, 0x66, 0x66, 0x66, 0x6E, 0x3C, 0x0E, 0x00}, // 'Q'
    {0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00}, // 'R'
    {0x3E, 0x66, 0x60, 0x3E, 0x06, 0x66, 0x3E, 0x00}, // 'S'
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // 'T'
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00}, // 'U'
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, // 'V'
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // 'W'
    {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00}, // 'X'
    {0x66, 0x66, 0x66, 0x3E, 0x18, 0x18, 0x18, 0x00}, // 'Y'
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00}, // 'Z'
    {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00}, // '['
    {0x00, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00}, // '\'
    {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00}, // ']'
    {0x08, 0x1C, 0x36, 0x22, 0x00, 0x00, 0x00, 0x00}, // '^'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // '_'
    {0x18, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00}, // '`'
    {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3B, 0x00}, // 'a' (97)
    {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00}, // 'b'
    {0x00, 0x00, 0x3E, 0x60, 0x60, 0x60, 0x3E, 0x00}, // 'c'
    {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3B, 0x00}, // 'd'
    {0x00, 0x00, 0x3E, 0x66, 0x7E, 0x60, 0x3E, 0x00}, // 'e'
    {0x1C, 0x26, 0x20, 0x7C, 0x20, 0x20, 0x20, 0x00}, // 'f'
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x3C}, // 'g'
    {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00}, // 'h'
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 'i'
    {0x0C, 0x00, 0x1C, 0x0C, 0x0C, 0x0C, 0x0C, 0x38}, // 'j'
    {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00}, // 'k'
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 'l'
    {0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // 'm'
    {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00}, // 'n'
    {0x00, 0x00, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00}, // 'o'
    {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60}, // 'p'
    {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06}, // 'q'
    {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00}, // 'r'
    {0x00, 0x00, 0x3E, 0x60, 0x3E, 0x06, 0x3E, 0x00}, // 's'
    {0x10, 0x10, 0x7C, 0x10, 0x10, 0x12, 0x0C, 0x00}, // 't'
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3B, 0x00}, // 'u'
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, // 'v'
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x3E, 0x22, 0x00}, // 'w'
    {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00}, // 'x'
    {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x18, 0x18}, // 'y'
    {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00}, // 'z'
    {0x0C, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0C, 0x00}, // '{'
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // '|'
    {0x30, 0x18, 0x18, 0x0C, 0x18, 0x18, 0x30, 0x00}, // '}'
    {0x38, 0x6C, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00}, // '~'
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}  // Solid Block
};

// Rebranded Main Menu UI options
const char* menu_options[] = {
    "Resume Gameplay",
    "VitaGrafix Settings",
    "Cheat Code Manager",
    "Calibrate Touch Inputs",
    "Save Active Profiles",
    "Exit Translation Env"
};

// --- INITIALIZATION AND SHADERS ---

static void init_font_texture(void) {
    if (s_font_texture != 0) return;

    uint8_t tex_data[128 * 64];
    memset(tex_data, 0, sizeof(tex_data));

    for (int char_idx = 0; char_idx < 97; char_idx++) {
        int font_idx = char_idx;
        if (font_idx >= 97) font_idx = 0;

        int char_col = char_idx % 16;
        int char_row = char_idx / 16;

        int start_x = char_col * 8;
        int start_y = char_row * 8;

        for (int row = 0; row < 8; row++) {
            uint8_t byte = s_font_8x8[font_idx][row];
            for (int col = 0; col < 8; col++) {
                int bit = (byte >> (7 - col)) & 1;
                int px = start_x + col;
                int py = start_y + row;
                tex_data[py * 128 + px] = bit ? 255 : 0;
            }
        }
    }

    glGenTextures(1, &s_font_texture);
    glBindTexture(GL_TEXTURE_2D, s_font_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 128, 64, 0, GL_ALPHA, GL_UNSIGNED_BYTE, tex_data);
}

static void ensure_menu_shaders_ready(void) {
    if (s_shader_initialized) return;

    const char* v_menu_src =
        "attribute vec4 position;\n"
        "attribute vec4 color;\n"
        "varying vec4 v_color;\n"
        "void main() {\n"
        "   gl_Position = position;\n"
        "   v_color = color;\n"
        "}\n";

    const char* f_menu_src =
        "precision mediump float;\n"
        "varying vec4 v_color;\n"
        "void main() {\n"
        "   gl_FragColor = v_color;\n"
        "}\n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &v_menu_src, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &f_menu_src, NULL);
    glCompileShader(fs);

    s_menu_program = glCreateProgram();
    glAttachShader(s_menu_program, vs);
    glAttachShader(s_menu_program, fs);
    glLinkProgram(s_menu_program);

    const char* v_text_src =
        "attribute vec4 position;\n"
        "attribute vec2 tex_coords;\n"
        "varying vec2 v_tex_coords;\n"
        "void main() {\n"
        "   gl_Position = position;\n"
        "   v_tex_coords = tex_coords;\n"
        "}\n";

    const char* f_text_src =
        "precision mediump float;\n"
        "varying vec2 v_tex_coords;\n"
        "uniform sampler2D font_texture;\n"
        "uniform vec4 text_color;\n"
        "void main() {\n"
        "   float alpha = texture2D(font_texture, v_tex_coords).a;\n"
        "   gl_FragColor = vec4(text_color.rgb, text_color.a * alpha);\n"
        "}\n";

    GLuint v_text_sh = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v_text_sh, 1, &v_text_src, NULL);
    glCompileShader(v_text_sh);

    GLuint f_text_sh = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f_text_sh, 1, &f_text_src, NULL);
    glCompileShader(f_text_sh);

    s_text_program = glCreateProgram();
    glAttachShader(s_text_program, v_text_sh);
    glAttachShader(s_text_program, f_text_sh);
    glLinkProgram(s_text_program);

    init_font_texture();
    s_shader_initialized = true;
}

void varm_menu_init(void) {
    s_selected_menu_item = 0;
    s_selected_touch_button = 0;
    s_selected_vitagrafix_item = 0;
    s_selected_cheat_item = 0;
    s_shader_initialized = false;
}

// --- RENDER UTILS & SCREEN COORDINATE MATH ---

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

static void draw_rect_pixels(float px1, float py1, float px2, float py2, float r, float g, float b, float a) {
    float x1 = (px1 / 640.0f) * 2.0f - 1.0f;
    float x2 = (px2 / 640.0f) * 2.0f - 1.0f;
    float y1 = 1.0f - (py1 / 480.0f) * 2.0f;
    float y2 = 1.0f - (py2 / 480.0f) * 2.0f;
    draw_rect(x1, y1, x2, y2, r, g, b, a);
}

static void draw_rect_border_pixels(float px1, float py1, float px2, float py2, float thickness, float r, float g, float b, float a) {
    draw_rect_pixels(px1, py1, px2, py1 + thickness, r, g, b, a);
    draw_rect_pixels(px1, py2 - thickness, px2, py2, r, g, b, a);
    draw_rect_pixels(px1, py1, px1 + thickness, py2, r, g, b, a);
    draw_rect_pixels(px2 - thickness, py1, px2, py2, r, g, b, a);
}

void varm_menu_draw_string(float x, float y, const char* str, float scale, float r, float g, float b, float a) {
    ensure_menu_shaders_ready();
    glUseProgram(s_text_program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_font_texture);
    GLint sampler_loc = glGetUniformLocation(s_text_program, "font_texture");
    if (sampler_loc != -1) glUniform1i(sampler_loc, 0);

    GLint color_loc = glGetUniformLocation(s_text_program, "text_color");
    if (color_loc != -1) glUniform4f(color_loc, r, g, b, a);

    GLint pos_loc = glGetAttribLocation(s_text_program, "position");
    GLint tex_loc = glGetAttribLocation(s_text_program, "tex_coords");

    float char_w = 8.0f * scale;
    float char_h = 8.0f * scale;
    float current_x = x;
    float current_y = y;

    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (c == '\n') {
            current_x = x;
            current_y += char_h + 4.0f * scale;
            continue;
        }

        int ascii = (int)c;
        int font_idx = ascii - 32;
        if (font_idx < 0 || font_idx >= 96) font_idx = 0;

        int col = font_idx % 16;
        int row = font_idx / 16;

        float u1 = (float)(col * 8) / 128.0f;
        float v1 = (float)(row * 8) / 64.0f;
        float u2 = (float)((col + 1) * 8) / 128.0f;
        float v2 = (float)((row + 1) * 8) / 64.0f;

        float x1 = (current_x / 640.0f) * 2.0f - 1.0f;
        float x2 = ((current_x + char_w) / 640.0f) * 2.0f - 1.0f;
        float y1 = 1.0f - (current_y / 480.0f) * 2.0f;
        float y2 = 1.0f - ((current_y + char_h) / 480.0f) * 2.0f;

        GLfloat vertices[] = {
            x1, y1, 0.0f, 1.0f,  u1, v1,
            x1, y2, 0.0f, 1.0f,  u1, v2,
            x2, y2, 0.0f, 1.0f,  u2, v2,

            x1, y1, 0.0f, 1.0f,  u1, v1,
            x2, y2, 0.0f, 1.0f,  u2, v2,
            x2, y1, 0.0f, 1.0f,  u2, v1
        };

        if (pos_loc != -1) {
            glEnableVertexAttribArray(pos_loc);
            glVertexAttribPointer(pos_loc, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), vertices);
        }
        if (tex_loc != -1) {
            glEnableVertexAttribArray(tex_loc);
            glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), &vertices[4]);
        }

        glDrawArrays(GL_TRIANGLES, 0, 6);
        current_x += char_w;
    }

    if (pos_loc != -1) glDisableVertexAttribArray(pos_loc);
    if (tex_loc != -1) glDisableVertexAttribArray(tex_loc);
}

void varm_menu_draw_string_shadow(float x, float y, const char* str, float scale, float r, float g, float b, float a) {
    float offset = (scale > 1.5f) ? 2.0f : 1.0f;
    varm_menu_draw_string(x + offset, y + offset, str, scale, 0.0f, 0.0f, 0.0f, a * 0.85f);
    varm_menu_draw_string(x, y, str, scale, r, g, b, a);
}

// --- PROCEDURAL ICON GENERATORS ---

static void draw_play_icon_pixels(float cx, float cy, float size, float r, float g, float b, float a) {
    float x1 = ((cx - size) / 640.0f) * 2.0f - 1.0f;
    float x2 = ((cx + size) / 640.0f) * 2.0f - 1.0f;
    float y1 = 1.0f - ((cy - size) / 480.0f) * 2.0f;
    float y2 = 1.0f - ((cy + size) / 480.0f) * 2.0f;
    float yc = 1.0f - (cy / 480.0f) * 2.0f;

    GLfloat vertices[] = {
        x1, y1, 0.0f, 1.0f,  r, g, b, a,
        x1, y2, 0.0f, 1.0f,  r, g, b, a,
        x2, yc, 0.0f, 1.0f,  r, g, b, a
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

static void draw_crosshair_icon_pixels(float cx, float cy, float size, float r, float g, float b, float a) {
    draw_rect_pixels(cx - size, cy - size / 4.0f, cx + size, cy + size / 4.0f, r, g, b, a);
    draw_rect_pixels(cx - size / 4.0f, cy - size, cx + size / 4.0f, cy + size, r, g, b, a);
}

static void draw_save_icon_pixels(float cx, float cy, float size, float r, float g, float b, float a) {
    draw_rect_pixels(cx - size, cy - size, cx + size, cy + size, r, g, b, a);
    draw_rect_pixels(cx - size / 1.8f, cy + size / 3.0f, cx + size / 1.8f, cy + size, 0.9f, 0.9f, 0.9f, a);
    draw_rect_pixels(cx - size / 2.5f, cy - size, cx + size / 2.5f, cy - size / 3.0f, 0.15f, 0.15f, 0.15f, a);
}

static void draw_exit_icon_pixels(float cx, float cy, float size, float r, float g, float b, float a) {
    draw_rect_pixels(cx - size, cy - size, cx - size + size / 3.0f, cy + size, r, g, b, a);
    draw_rect_pixels(cx - size, cy - size, cx + size / 3.0f, cy - size + size / 3.0f, r, g, b, a);
    draw_rect_pixels(cx - size, cy + size - size / 3.0f, cx + size / 3.0f, cy + size, r, g, b, a);
    draw_rect_pixels(cx - size / 5.0f, cy - size / 5.0f, cx + size / 1.8f, cy + size / 5.0f, r, g, b, a);
    draw_play_icon_pixels(cx + size / 1.2f, cy, size / 2.5f, r, g, b, a);
}

static void draw_gear_icon_pixels(float cx, float cy, float size, float r, float g, float b, float a) {
    draw_rect_pixels(cx - size / 2.0f, cy - size / 2.0f, cx + size / 2.0f, cy + size / 2.0f, r, g, b, a);
    draw_rect_pixels(cx - size, cy - size / 4.0f, cx + size, cy + size / 4.0f, r, g, b, a);
    draw_rect_pixels(cx - size / 4.0f, cy - size, cx + size / 4.0f, cy + size, r, g, b, a);
}

static void draw_cheat_icon_pixels(float cx, float cy, float size, float r, float g, float b, float a) {
    draw_rect_pixels(cx - size / 4.0f, cy - size, cx + size / 4.0f, cy + size / 2.0f, r, g, b, a);
    draw_rect_pixels(cx - size, cy - size, cx + size, cy - size + size / 2.0f, r, g, b, a);
}

static void draw_touch_button(TouchTarget *target, bool is_selected, const char* label) {
    // Make the touch zones perfect squares on the screen instead of wide rectangles
    float size = 45.0f;
    float x1 = (float)target->x - size;
    float x2 = (float)target->x + size;
    float y1 = (float)target->y - size;
    float y2 = (float)target->y + size;

    if (is_selected) {
        // High-Tech Sniper Corner brackets for the active target
        float thick = 4.0f;
        float len = 15.0f;
        // Top Left Corner
        draw_rect_pixels(x1, y1, x1 + len, y1 + thick, 1.0f, 0.2f, 0.2f, 1.0f);
        draw_rect_pixels(x1, y1, x1 + thick, y1 + len, 1.0f, 0.2f, 0.2f, 1.0f);
        // Top Right Corner
        draw_rect_pixels(x2 - len, y1, x2, y1 + thick, 1.0f, 0.2f, 0.2f, 1.0f);
        draw_rect_pixels(x2 - thick, y1, x2, y1 + len, 1.0f, 0.2f, 0.2f, 1.0f);
        // Bottom Left Corner
        draw_rect_pixels(x1, y2 - thick, x1 + len, y2, 1.0f, 0.2f, 0.2f, 1.0f);
        draw_rect_pixels(x1, y2 - len, x1 + thick, y2, 1.0f, 0.2f, 0.2f, 1.0f);
        // Bottom Right Corner
        draw_rect_pixels(x2 - len, y2 - thick, x2, y2, 1.0f, 0.2f, 0.2f, 1.0f);
        draw_rect_pixels(x2 - thick, y2 - len, x2, y2, 1.0f, 0.2f, 0.2f, 1.0f);

        // Center Dot & Translucent Red Fill
        draw_rect_pixels(target->x - 2, target->y - 2, target->x + 2, target->y + 2, 1.0f, 0.8f, 0.2f, 1.0f);
        draw_rect_pixels(x1, y1, x2, y2, 1.0f, 0.1f, 0.1f, 0.35f);
    } else {
        // Unselected Target: Cool cyan brackets
        draw_rect_border_pixels(x1, y1, x2, y2, 2.0f, 0.2f, 0.8f, 0.8f, 0.6f);
        draw_rect_pixels(x1, y1, x2, y2, 0.05f, 0.3f, 0.5f, 0.15f);
    }

    // Perfectly center the text label above the touch zone
    varm_menu_draw_string_shadow(target->x - 12.0f, y1 - 25.0f, label, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f);
}

// --- MAIN RENDER LOOP ---

void varm_menu_render_osd(void) {
    ensure_menu_shaders_ready();

    uint32_t current_pc = varm_jit_get_pc();
    unsigned long long total_cycles = varm_jit_get_cycles();

    char pc_buffer[64];
    char cycles_buffer[64];
    snprintf(pc_buffer, sizeof(pc_buffer), "GUEST PC: 0x%08X", current_pc);
    snprintf(cycles_buffer, sizeof(cycles_buffer), "CYCLES: %llu", total_cycles);

    // Swap to shader pass
    glUseProgram(s_menu_program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 1. HUD Top Bar (System Telemetry Logs)
    draw_rect_pixels(20.0f, 15.0f, 620.0f, 65.0f, 0.10f, 0.10f, 0.15f, 0.85f);
    draw_rect_border_pixels(20.0f, 15.0f, 620.0f, 65.0f, 2.0f, 0.0f, 0.85f, 0.85f, 0.90f);

    float cycle_ratio = (float)(total_cycles % 10000000ULL) / 10000000.0f;
    draw_rect_pixels(480.0f, 32.0f, 600.0f, 48.0f, 0.20f, 0.20f, 0.25f, 1.0f);
    draw_rect_pixels(482.0f, 34.0f, 482.0f + (cycle_ratio * 116.0f), 46.0f, 0.0f, 0.85f, 0.40f, 1.0f);

    varm_menu_draw_string_shadow(40.0f, 28.0f, pc_buffer, 2.0f, 0.0f, 0.85f, 0.85f, 1.0f);
    varm_menu_draw_string_shadow(250.0f, 28.0f, cycles_buffer, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    // Rebranded dynamic console debug metrics
    static uint32_t diagnostic_frame_ticks = 0;
    if (++diagnostic_frame_ticks % 60 == 0) {
        printf("\033[1;36m[VITA2ARM-OSD]\033[0m %s | %s | State Pipeline: %d\n",
               pc_buffer, cycles_buffer, g_varm_state);
    }

    // 2. REBRANDED MAIN MENU DRAWING
    if (g_varm_state == VARM_STATE_MENU_ACTIVE) {
        glUseProgram(s_menu_program);

        // Menu container
        draw_rect_pixels(120.0f, 40.0f, 520.0f, 440.0f, 0.05f, 0.05f, 0.08f, 0.95f);
        draw_rect_border_pixels(120.0f, 40.0f, 520.0f, 440.0f, 3.0f, 0.0f, 0.85f, 0.85f, 1.0f);

        // 🌟 Rebranded Header to Vita2Arm!
        varm_menu_draw_string_shadow(160.0f, 60.0f, "VITA2ARM MAIN CONFIG MENU", 2.0f, 1.0f, 0.75f, 0.2f, 1.0f);
        draw_rect_pixels(140.0f, 90.0f, 500.0f, 92.0f, 1.0f, 0.75f, 0.2f, 0.6f);

        int total_items = sizeof(menu_options) / sizeof(menu_options[0]);
        for (int i = 0; i < total_items; i++) {
            float box_y = 110.0f + i * 50.0f;
            float icon_cx = 170.0f;
            float icon_cy = box_y + 18.0f;

            glUseProgram(s_menu_program);
            float r = 1.0f, g = 1.0f, b = 1.0f;

            if (i == s_selected_menu_item) {
                draw_rect_pixels(140.0f, box_y, 500.0f, box_y + 38.0f, 0.95f, 0.75f, 0.20f, 0.95f);
                r = 0.05f; g = 0.05f; b = 0.10f;
            } else {
                draw_rect_pixels(140.0f, box_y, 500.0f, box_y + 38.0f, 0.12f, 0.12f, 0.16f, 0.80f);
            }

            // Draw custom vector UI icons
            if (i == 0)       draw_play_icon_pixels(icon_cx, icon_cy, 10.0f, r, g, b, 1.0f);
            else if (i == 1)  draw_gear_icon_pixels(icon_cx, icon_cy, 10.0f, r, g, b, 1.0f);
            else if (i == 2)  draw_cheat_icon_pixels(icon_cx, icon_cy, 10.0f, r, g, b, 1.0f);
            else if (i == 3)  draw_crosshair_icon_pixels(icon_cx, icon_cy, 10.0f, r, g, b, 1.0f);
            else if (i == 4)  draw_save_icon_pixels(icon_cx, icon_cy, 10.0f, r, g, b, 1.0f);
            else if (i == 5)  draw_exit_icon_pixels(icon_cx, icon_cy, 10.0f, r, g, b, 1.0f);

            if (i == s_selected_menu_item) {
                varm_menu_draw_string(210.0f, box_y + 11.0f, menu_options[i], 1.8f, r, g, b, 1.0f);
            } else {
                varm_menu_draw_string_shadow(210.0f, box_y + 11.0f, menu_options[i], 1.8f, 1.0f, 1.0f, 1.0f, 1.0f);
            }
        }
    }
    // 3. VITAGRAFIX SUBMENU RENDER PASS
    else if (g_varm_state == VARM_STATE_VITAGRAFIX) {
        glUseProgram(s_menu_program);

        draw_rect_pixels(120.0f, 40.0f, 520.0f, 440.0f, 0.05f, 0.05f, 0.08f, 0.95f);
        draw_rect_border_pixels(120.0f, 40.0f, 520.0f, 440.0f, 3.0f, 0.0f, 0.85f, 0.85f, 1.0f);

        varm_menu_draw_string_shadow(160.0f, 60.0f, "VITAGRAFIX SETTINGS", 2.0f, 0.0f, 0.85f, 0.85f, 1.0f);
        draw_rect_pixels(140.0f, 90.0f, 500.0f, 92.0f, 0.0f, 0.85f, 0.85f, 0.60f);

        for (int i = 0; i < 3; i++) {
            float box_y = 120.0f + i * 60.0f;
            glUseProgram(s_menu_program);
            float r = 1.0f, g = 1.0f, b = 1.0f;

            if (i == s_selected_vitagrafix_item) {
                draw_rect_pixels(140.0f, box_y, 500.0f, box_y + 48.0f, 0.0f, 0.85f, 0.85f, 0.95f);
                r = 0.05f; g = 0.05f; b = 0.10f;
            } else {
                draw_rect_pixels(140.0f, box_y, 500.0f, box_y + 48.0f, 0.12f, 0.12f, 0.16f, 0.80f);
            }

            char label[128];
            if (i == 0) {
                snprintf(label, sizeof(label), "FRAME UNCAPPER: %s", g_vitagrafix_uncap ? "ENABLED (60 FPS)" : "DISABLED (30 FPS)");
            } else if (i == 1) {
                snprintf(label, sizeof(label), "VIRTUAL RESOLUTION: %s", res_presets[g_vitagrafix_res_preset]);
            } else {
                snprintf(label, sizeof(label), "RETURN TO MAIN MENU");
            }

            if (i == s_selected_vitagrafix_item) {
                varm_menu_draw_string(220.0f, box_y + 16.0f, label, 2.0f, r, g, b, 1.0f);
            } else {
                varm_menu_draw_string_shadow(220.0f, box_y + 16.0f, label, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            }
        }
    }
    // 4. CHEAT SUBMENU RENDER PASS
    else if (g_varm_state == VARM_STATE_CHEATS) {
        glUseProgram(s_menu_program);

        draw_rect_pixels(120.0f, 40.0f, 520.0f, 440.0f, 0.05f, 0.05f, 0.08f, 0.95f);
        draw_rect_border_pixels(120.0f, 40.0f, 520.0f, 440.0f, 3.0f, 1.0f, 0.35f, 0.20f, 1.0f);

        varm_menu_draw_string_shadow(160.0f, 60.0f, "VITA2ARM CHEAT ENGINE", 2.0f, 1.0f, 0.35f, 0.20f, 1.0f);
        draw_rect_pixels(140.0f, 90.0f, 500.0f, 92.0f, 1.0f, 0.35f, 0.20f, 0.60f);

        for (int i = 0; i < MAX_CHEATS + 1; i++) {
            float box_y = 110.0f + i * 55.0f;
            glUseProgram(s_menu_program);
            float r = 1.0f, g = 1.0f, b = 1.0f;

            if (i == s_selected_cheat_item) {
                draw_rect_pixels(140.0f, box_y, 500.0f, box_y + 42.0f, 1.0f, 0.35f, 0.20f, 0.95f);
                r = 0.05f; g = 0.05f; b = 0.10f;
            } else {
                draw_rect_pixels(140.0f, box_y, 500.0f, box_y + 42.0f, 0.12f, 0.12f, 0.16f, 0.80f);
            }

            char label[128];
            if (i < MAX_CHEATS) {
                snprintf(label, sizeof(label), "%s [%s]", s_cheats[i].name, s_cheats[i].active ? "ACTIVE" : "OFF");
            } else {
                snprintf(label, sizeof(label), "RETURN TO MAIN MENU");
            }

            if (i == s_selected_cheat_item) {
                varm_menu_draw_string(170.0f, box_y + 13.0f, label, 1.8f, r, g, b, 1.0f);
            } else {
                varm_menu_draw_string_shadow(170.0f, box_y + 13.0f, label, 1.8f, 1.0f, 1.0f, 1.0f, 1.0f);
            }
        }
    }
    // 5. Touch Mapping Calibrator Layout
    else if (g_varm_state == VARM_STATE_EDIT_TOUCH) {
        draw_rect_pixels(20.0f, 380.0f, 620.0f, 460.0f, 0.08f, 0.08f, 0.12f, 0.90f);
        draw_rect_border_pixels(20.0f, 380.0f, 620.0f, 460.0f, 2.0f, 1.0f, 0.75f, 0.20f, 0.80f);

        varm_menu_draw_string_shadow(40.0f, 400.0f, "USE D-PAD TO ADJUST ACTIVE TARGET COORDINATES", 1.5f, 1.0f, 1.0f, 1.0f, 1.0f);
        varm_menu_draw_string_shadow(40.0f, 430.0f, "(O) NEXT TARGET PROFILE | (X) SAVE & CONFIRM", 1.5f, 1.0f, 0.75f, 0.20f, 1.0f);

        glUseProgram(s_menu_program);
        draw_touch_button(&g_active_profile.l2, s_selected_touch_button == 0, "L2");
        draw_touch_button(&g_active_profile.r2, s_selected_touch_button == 1, "R2");
        draw_touch_button(&g_active_profile.l3, s_selected_touch_button == 2, "L3");
        draw_touch_button(&g_active_profile.r3, s_selected_touch_button == 3, "R3");
    }
}

// --- LOADING SCREEN ---

void varm_menu_draw_loading(int completion) {
    ensure_menu_shaders_ready();

    glUseProgram(s_menu_program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.04f, 0.04f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_rect_pixels(120.0f, 100.0f, 520.0f, 344.0f, 0.10f, 0.10f, 0.15f, 0.85f);
    draw_rect_border_pixels(120.0f, 100.0f, 520.0f, 344.0f, 3.0f, 0.0f, 0.85f, 0.85f, 1.0f);

    varm_menu_draw_string_shadow(160.0f, 130.0f, "REBUILDING TRANSLATION BLOCKS...", 2.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    char pct[16];
    snprintf(pct, sizeof(pct), "%d%%", completion);
    varm_menu_draw_string_shadow(295.0f, 175.0f, pct, 2.5f, 0.0f, 0.85f, 0.85f, 1.0f);

    draw_rect_pixels(160.0f, 220.0f, 480.0f, 245.0f, 0.05f, 0.05f, 0.08f, 1.0f);
    draw_rect_border_pixels(158.0f, 218.0f, 482.0f, 247.0f, 2.0f, 0.20f, 0.20f, 0.25f, 1.0f);

    float ratio = (float)completion / 100.0f;
    draw_rect_pixels(160.0f, 220.0f, 160.0f + ratio * 320.0f, 245.0f, 0.0f, 0.85f, 0.85f, 0.95f);

    static float spinner_angle = 0.0f;
    spinner_angle += 0.25f;
    float pulse_scale = 5.0f + 3.0f * sinf(spinner_angle);
    draw_play_icon_pixels(320.0f, 290.0f, 8.0f + pulse_scale, 1.0f, 0.75f, 0.20f, 1.0f);

    extern SDL_Window *g_window;
    if (g_window) {
        SDL_GL_SwapWindow(g_window);
    }
}

// --- MOCK GAMEPLAY SCENE ---

void varm_menu_draw_mock_game(void) {
    ensure_menu_shaders_ready();
    glUseProgram(s_menu_program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Pulse/rotation angle based on cycles count
    static float angle = 0.0f;
    angle += 0.02f;

    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    // Define 3D vertices of a pyramid (X, Y, Z) and rotate them on Y axis
    // Then project to 2D
    typedef struct { float x, y, z; float r, g, b, a; } Vertex3D;
    Vertex3D p[] = {
        // Base vertices
        {-0.3f, -0.3f, -0.3f, 0.8f, 0.2f, 0.2f, 1.0f},
        { 0.3f, -0.3f, -0.3f, 0.2f, 0.8f, 0.2f, 1.0f},
        { 0.3f, -0.3f,  0.3f, 0.2f, 0.2f, 0.8f, 1.0f},
        {-0.3f, -0.3f,  0.3f, 0.8f, 0.8f, 0.2f, 1.0f},
        // Apex
        { 0.0f,  0.3f,  0.0f, 0.2f, 0.8f, 0.8f, 1.0f}
    };

    // Rotate and project
    GLfloat vertices[12 * 8]; // 4 faces * 3 vertices/face * 8 floats/vertex
    int idx = 0;

    int faces[4][3] = {
        {0, 1, 4},
        {1, 2, 4},
        {2, 3, 4},
        {3, 0, 4}
    };

    for (int f = 0; f < 4; f++) {
        for (int v = 0; v < 3; v++) {
            Vertex3D pt = p[faces[f][v]];
            // Rotate Y axis
            float rx = pt.x * cos_a - pt.z * sin_a;
            float rz = pt.x * sin_a + pt.z * cos_a;
            float ry = pt.y;

            // Simple perspective projection
            float dist = 2.0f;
            float px = rx / (rz + dist);
            float py = ry / (rz + dist);
            
            // Adjust to our screen coordinates and 4:3 aspect ratio (640:480 => 4:3)
            // By making X coordinate multiplied by 480/640 (or 0.75) it stops it from stretching wide
            px *= 0.75f; 

            vertices[idx++] = px;
            vertices[idx++] = py;
            vertices[idx++] = 0.0f;
            vertices[idx++] = 1.0f;
            vertices[idx++] = pt.r;
            vertices[idx++] = pt.g;
            vertices[idx++] = pt.b;
            vertices[idx++] = pt.a;
        }
    }

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

    glDrawArrays(GL_TRIANGLES, 0, 12);

    if (pos_loc != -1) glDisableVertexAttribArray(pos_loc);
    if (col_loc != -1) glDisableVertexAttribArray(col_loc);
}

// --- DYNAMIC SUB-MENU NAVIGATION SYSTEMS ---

void varm_menu_navigate(uint32_t inputs) {
    uint32_t pressed = inputs;
    uint32_t current_tick = SDL_GetTicks();
    static uint32_t last_nav_tick = 0;
    static uint32_t last_circle_tick = 0;
    static uint32_t last_cross_tick = 0;

    // Navigation and Action Button Cooldown filter
    if (pressed & (VITA_CTRL_DOWN | VITA_CTRL_UP | VITA_CTRL_LEFT | VITA_CTRL_RIGHT | VITA_CTRL_CIRCLE | VITA_CTRL_CROSS)) {
        if (current_tick - last_nav_tick < 180) {
            return;
        }
        last_nav_tick = current_tick;
    }

    // A. TOUCH CALIBRATION NAVIGATION
    if (g_varm_state == VARM_STATE_EDIT_TOUCH) {
        TouchTarget *targets[4] = {
            &g_active_profile.l2,
            &g_active_profile.r2,
            &g_active_profile.l3,
            &g_active_profile.r3
        };

        TouchTarget *target = targets[s_selected_touch_button];

        if (pressed & VITA_CTRL_UP)    { if (target->y > 4)   target->y -= 4; }
        if (pressed & VITA_CTRL_DOWN)  { if (target->y < 540) target->y += 4; }
        if (pressed & VITA_CTRL_LEFT)  { if (target->x > 4)   target->x -= 4; }
        if (pressed & VITA_CTRL_RIGHT) { if (target->x < 956) target->x += 4; }

        if (pressed & VITA_CTRL_CIRCLE) {
            s_selected_touch_button = (s_selected_touch_button + 1) % 4;
            printf("[VITA2ARM-UI] Editing next input target: Index [%d]\n", s_selected_touch_button);
        }
        if (pressed & VITA_CTRL_CROSS) {
            g_varm_state = VARM_STATE_MENU_ACTIVE;
            printf("[VITA2ARM-UI] Touch configuration layout saved. Returning to main engine menu.\n");
        }
        return;
    }

    // B. VITAGRAFIX SUBMENU NAVIGATION
    if (g_varm_state == VARM_STATE_VITAGRAFIX) {
        if (pressed & VITA_CTRL_DOWN) {
            s_selected_vitagrafix_item = (s_selected_vitagrafix_item + 1) % 3;
        }
        if (pressed & VITA_CTRL_UP) {
            s_selected_vitagrafix_item = (s_selected_vitagrafix_item - 1 + 3) % 3;
        }
        if (pressed & VITA_CTRL_CIRCLE) {
            if (s_selected_vitagrafix_item == 0) {
                g_vitagrafix_uncap = !g_vitagrafix_uncap;
                printf("[VITA2ARM-VITAGRAFIX] Frame uncapper toggled: %s\n", g_vitagrafix_uncap ? "ON" : "OFF");
            } else if (s_selected_vitagrafix_item == 1) {
                g_vitagrafix_res_preset = (g_vitagrafix_res_preset + 1) % 3;
                printf("[VITA2ARM-VITAGRAFIX] Virtual Resolution switched: %s\n", res_presets[g_vitagrafix_res_preset]);
            } else if (s_selected_vitagrafix_item == 2) {
                g_varm_state = VARM_STATE_MENU_ACTIVE;
            }
        }
        if (pressed & VITA_CTRL_CROSS) {
            g_varm_state = VARM_STATE_MENU_ACTIVE;
        }
        return;
    }

    // C. CHEATS ENGINE SUBMENU NAVIGATION
    if (g_varm_state == VARM_STATE_CHEATS) {
        if (pressed & VITA_CTRL_DOWN) {
            s_selected_cheat_item = (s_selected_cheat_item + 1) % (MAX_CHEATS + 1);
        }
        if (pressed & VITA_CTRL_UP) {
            s_selected_cheat_item = (s_selected_cheat_item - 1 + (MAX_CHEATS + 1)) % (MAX_CHEATS + 1);
        }
        if (pressed & VITA_CTRL_CIRCLE) {
            if (s_selected_cheat_item < MAX_CHEATS) {
                s_cheats[s_selected_cheat_item].active = !s_cheats[s_selected_cheat_item].active;
                printf("[VITA2ARM-CHEATS] %s active state set to %d\n", s_cheats[s_selected_cheat_item].name, s_cheats[s_selected_cheat_item].active);
            } else {
                g_varm_state = VARM_STATE_MENU_ACTIVE;
            }
        }
        if (pressed & VITA_CTRL_CROSS) {
            g_varm_state = VARM_STATE_MENU_ACTIVE;
        }
        return;
    }

    // D. MAIN MENU NAVIGATION
    int total_items = sizeof(menu_options) / sizeof(menu_options[0]);

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
            g_varm_state = VARM_STATE_VITAGRAFIX;
        } else if (s_selected_menu_item == 2) {
            g_varm_state = VARM_STATE_CHEATS;
        } else if (s_selected_menu_item == 3) {
            g_varm_state = VARM_STATE_EDIT_TOUCH;
        } else if (s_selected_menu_item == 4) {
            varm_input_save_profile();
            printf("[VITA2ARM-UI] Configuration & custom mapping profiles saved to disk.\n");
        } else if (s_selected_menu_item == 5) {
            extern bool g_running;
            g_running = false;
        }
    }
}

void varm_menu_shutdown(void) {
    if (s_font_texture != 0) {
        glDeleteTextures(1, &s_font_texture);
        s_font_texture = 0;
    }
    if (s_menu_program != 0) {
        glDeleteProgram(s_menu_program);
        s_menu_program = 0;
    }
    if (s_text_program != 0) {
        glDeleteProgram(s_text_program);
        s_text_program = 0;
    }
    s_shader_initialized = false;
}
