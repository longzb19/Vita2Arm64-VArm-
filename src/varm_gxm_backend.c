#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include "varm_gxm_backend.h"
#include "varm_graphics.h"
#include "hle_kernel.h"

// --- Extended GLES 2.0 Dynamic Function Pointers ---
static void (*gl_draw_arrays_ptr)(GLenum mode, GLint first, GLsizei count) = NULL;
static void (*gl_bind_texture_ptr)(GLenum target, GLuint texture) = NULL;
static void (*gl_clear_color_ptr)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
static void (*gl_clear_ptr)(GLbitfield mask) = NULL;

static void (*gl_gen_textures_ptr)(GLsizei n, GLuint *textures) = NULL;
static void (*gl_tex_image_2d_ptr)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) = NULL;
static void (*gl_tex_parameter_i_ptr)(GLenum target, GLenum pname, GLint param) = NULL;
static void (*gl_active_texture_ptr)(GLenum texture) = NULL;

static GLuint (*gl_create_shader_ptr)(GLenum type) = NULL;
static void (*gl_shader_source_ptr)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) = NULL;
static void (*gl_compile_shader_ptr)(GLuint shader) = NULL;
static GLuint (*gl_create_program_ptr)(void) = NULL;
static void (*gl_attach_shader_ptr)(GLuint program, GLuint shader) = NULL;
static void (*gl_link_program_ptr)(GLuint program) = NULL;
static void (*gl_use_program_ptr)(GLuint program) = NULL;

static GLint (*gl_get_attrib_location_ptr)(GLuint program, const GLchar *name) = NULL;
static GLint (*gl_get_uniform_location_ptr)(GLuint program, const GLchar *name) = NULL;
static void (*gl_uniform_2f_ptr)(GLint location, GLfloat v0, GLfloat v1) = NULL;
static void (*gl_uniform_1i_ptr)(GLint location, GLint v0) = NULL;
static void (*gl_enable_vertex_attrib_array_ptr)(GLuint index) = NULL;
static void (*gl_vertex_attrib_pointer_ptr)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) = NULL;

// --- Presentation Pipeline Shaders & Program State ---
static GLuint gl_program = 0;
static GLint gl_scale_uniform_loc = -1;
static GLint gl_tex_uniform_loc = -1;
static GLint gl_pos_attrib_loc = -1;
static GLint gl_tex_attrib_loc = -1;

// Global track container of active guest display textures to sync frames
static GLuint s_active_surface_tex_id = 0;
static uint32_t s_active_surface_vaddr = 0;

// Embedded core shaders to scale the original 16:9 960x544 viewport down natively onto the 4:3 hardware screen
const char* vert_shader_src =
    "attribute vec2 position;\n"
    "attribute vec2 tex_coord;\n"
    "varying vec2 v_tex_coord;\n"
    "uniform vec2 u_scale;\n"
    "void main() {\n"
    "    v_tex_coord = tex_coord;\n"
    "    gl_Position = vec4(position * u_scale, 0.0, 1.0);\n"
    "}\n";

const char* frag_shader_src =
    "precision mediump float;\n"
    "varying vec2 v_tex_coord;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_texture, v_tex_coord);\n"
    "}\n";

static int compile_presentation_pipeline(void) {
    GLuint vs = gl_create_shader_ptr(GL_VERTEX_SHADER);
    gl_shader_source_ptr(vs, 1, &vert_shader_src, NULL);
    gl_compile_shader_ptr(vs);

    GLuint fs = gl_create_shader_ptr(GL_FRAGMENT_SHADER);
    gl_shader_source_ptr(fs, 1, &frag_shader_src, NULL);
    gl_compile_shader_ptr(fs);

    gl_program = gl_create_program_ptr();
    gl_attach_shader_ptr(gl_program, vs);
    gl_attach_shader_ptr(gl_program, fs);
    gl_link_program_ptr(gl_program);

    gl_scale_uniform_loc = gl_get_uniform_location_ptr(gl_program, "u_scale");
    gl_tex_uniform_loc   = gl_get_uniform_location_ptr(gl_program, "u_texture");
    gl_pos_attrib_loc    = gl_get_attrib_location_ptr(gl_program, "position");
    gl_tex_attrib_loc    = gl_get_attrib_location_ptr(gl_program, "tex_coord");

    return 0;
}

static int gles_init_display(void) {
    printf("[GXM-GLES] Hooking native muOS GLES reference driver layers...\n");

    void* gles_lib = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
    if (!gles_lib) {
        printf("[GXM-GLES-ERROR] Target driver link hook libGLESv2.so failed!\n");
        return -1;
    }

    // Resolve structural draw symbols
    gl_draw_arrays_ptr = dlsym(gles_lib, "glDrawArrays");
    gl_bind_texture_ptr = dlsym(gles_lib, "glBindTexture");
    gl_clear_color_ptr = dlsym(gles_lib, "glClearColor");
    gl_clear_ptr = dlsym(gles_lib, "glClear");

    // Resolve extended texture management hooks
    gl_gen_textures_ptr = dlsym(gles_lib, "glGenTextures");
    gl_tex_image_2d_ptr = dlsym(gles_lib, "glTexImage2D");
    gl_tex_parameter_i_ptr = dlsym(gles_lib, "glTexParameteri");
    gl_active_texture_ptr = dlsym(gles_lib, "glActiveTexture");

    // Resolve shader processing infrastructure hooks
    gl_create_shader_ptr = dlsym(gles_lib, "glCreateShader");
    gl_shader_source_ptr = dlsym(gles_lib, "glShaderSource");
    gl_compile_shader_ptr = dlsym(gles_lib, "glCompileShader");
    gl_create_program_ptr = dlsym(gles_lib, "glCreateProgram");
    gl_attach_shader_ptr = dlsym(gles_lib, "glAttachShader");
    gl_link_program_ptr = dlsym(gles_lib, "glLinkProgram");
    gl_use_program_ptr = dlsym(gles_lib, "glUseProgram");

    gl_get_attrib_location_ptr = dlsym(gles_lib, "glGetAttribLocation");
    gl_get_uniform_location_ptr = dlsym(gles_lib, "glGetUniformLocation");
    gl_uniform_2f_ptr = dlsym(gles_lib, "glUniform2f");
    gl_uniform_1i_ptr = dlsym(gles_lib, "glUniform1i");
    gl_enable_vertex_attrib_array_ptr = dlsym(gles_lib, "glEnableVertexAttribArray");
    gl_vertex_attrib_pointer_ptr = dlsym(gles_lib, "glVertexAttribPointer");

    printf("[GXM-GLES] System dynamic GLES hooks bound successfully!\n");

    // Initialize the background presentation pipeline compiler immediately
    compile_presentation_pipeline();
    return 0;
}

static int gles_allocate_surface(GxmSurfaceContext *surface) {
    if (!surface) return -1;

    // Default to standard Vita Hardware profile restrictions
    surface->width = 960;
    surface->height = 544;

    // Generate physical host texture space containers
    gl_gen_textures_ptr(1, &surface->host_tex_id);
    gl_bind_texture_ptr(GL_TEXTURE_2D, surface->host_tex_id);

    // Enforce optimized handheld display alignment parameters
    gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Initialize the structural container allocation buffer empty
    gl_tex_image_2d_ptr(GL_TEXTURE_2D, 0, GL_RGBA, surface->width, surface->height,
                        0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // Track active rendering bindings for main presentation pipeline passes
    s_active_surface_tex_id = surface->host_tex_id;
    s_active_surface_vaddr  = surface->vaddr;

    printf("[GXM-GLES] Texture mapping instantiated. Handle: %u allocated for Guest VAddr: 0x%08X\n",
           surface->host_tex_id, surface->vaddr);

    return 0;
}

static int gles_submit_cmd(uint32_t cmd_vaddr, uint32_t size) {
    if (!gl_program || s_active_surface_tex_id == 0) return -1;

    // 1. High-Level Emulation Sync: Extract raw pixel changes inside Guest Video Memory
    if (s_active_surface_vaddr != 0) {
        // Resolve the guest surface virtual buffer directly into host space permissions
        uint32_t host_ptr = hle_kernel_resolve_address(s_active_surface_vaddr, 0);
        if (host_ptr != 0) {
            // Push modifications directly into the host GPU texture layout
            gl_bind_texture_ptr(GL_TEXTURE_2D, s_active_surface_tex_id);
            gl_tex_image_2d_ptr(GL_TEXTURE_2D, 0, GL_RGBA, 960, 544, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)(uintptr_t)host_ptr);
        }
    }

    // 2. Hardware Layout Presentation Block
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    varm_graphics_get_scale(&scale_x, &scale_y);

    // Clear background
    gl_clear_color_ptr(0.0f, 0.0f, 0.0f, 1.0f);
    gl_clear_ptr(GL_COLOR_BUFFER_BIT);

    // Invoke presentation quad configuration variables
    gl_use_program_ptr(gl_program);
    gl_uniform_2f_ptr(gl_scale_uniform_loc, scale_x, scale_y);

    // Quad geometry arrays layout: Position(X, Y), TexCoord(U, V)
    static const float quad_data[] = {
        -1.0f,  1.0f,  0.0f, 0.0f, // Top-Left
        -1.0f, -1.0f,  0.0f, 1.0f, // Bottom-Left
         1.0f, -1.0f,  1.0f, 1.0f, // Bottom-Right

        -1.0f,  1.0f,  0.0f, 0.0f, // Top-Left
         1.0f, -1.0f,  1.0f, 1.0f, // Bottom-Right
         1.0f,  1.0f,  1.0f, 0.0f  // Top-Right
    };

    // Safely inject vertex position properties
    gl_enable_vertex_attrib_array_ptr(gl_pos_attrib_loc);
    gl_vertex_attrib_pointer_ptr(gl_pos_attrib_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), quad_data);

    // Inject texture scaling tracking coordinates
    gl_enable_vertex_attrib_array_ptr(gl_tex_attrib_loc);
    gl_vertex_attrib_pointer_ptr(gl_tex_attrib_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), &quad_data[2]);

    // Bind texture source safely to texture layer index 0
    gl_active_texture_ptr(GL_TEXTURE0);
    gl_bind_texture_ptr(GL_TEXTURE_2D, s_active_surface_tex_id);
    gl_uniform_1i_ptr(gl_tex_uniform_loc, 0);

    // Present presentation geometry to screen backbuffer natively on the Allwinner SoC Mali hardware
    gl_draw_arrays_ptr(GL_TRIANGLES, 0, 6);

    return 0;
}

static void gles_clear_screen(float r, float g, float b, float a) {
    if (gl_clear_color_ptr && gl_clear_ptr) {
        gl_clear_color_ptr(r, g, b, a);
        gl_clear_ptr(GL_COLOR_BUFFER_BIT);
    }
}

// --- Global Core Distributor Selector ---
int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface) {
    if (!interface) return -1;

    if (core_type == VARM_RENDER_CORE_GLES) {
        printf("[GXM-BRIDGE] Switched execution context pipeline to: OPENGL ES CORE\n");
        interface->init_display           = gles_init_display;
        interface->allocate_surface       = gles_allocate_surface;
        interface->submit_command_buffer  = gles_submit_cmd;
        interface->clear_screen           = gles_clear_screen;
        return 0;
    }
    else if (core_type == VARM_RENDER_CORE_VULKAN) {
        printf("[GXM-BRIDGE] Switched execution context pipeline to: VULKAN CORE\n");
        // Vulkan context remains uninstantiated for portability fallback profiles
        return 0;
    }
    return -1;
}
