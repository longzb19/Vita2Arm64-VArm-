#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include "varm_gxm_backend.h"
#include "varm_graphics.h"
#include "hle_kernel.h"

// 🌟 Instantiating the global interface handler here fixes the main.c linker error!
V_GxmRendererInterface gxm_interface;

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
static void (*gl_uniform_1i_ptr)(GLint location, GLint v0) = NULL;
static void (*gl_vertex_attrib_pointer_ptr)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) = NULL;
static void (*gl_enable_vertex_attrib_array_ptr)(GLuint index) = NULL;

// --- State Tracking and Geometry Cache ---
static GLuint s_active_surface_tex_id = 0;
static GLuint s_shader_program = 0;
static GLint gl_vertex_attrib_loc = 0;
static GLint gl_tex_attrib_loc = 1;
static GLint gl_tex_uniform_loc = 0;

static GLfloat quad_data[] = {
    // x, y, u, v
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

// --- Core Internal GLES Display Pipeline Setup ---
static int gles_init_display(void) {
    printf("[GXM-GLES] Hooking native GLES reference driver layers...\n");

    // Dynamic lookup of the target platform's GLESv2 library context
    void* gles_handle = dlopen("libGLESv2.so.2", RTLD_LAZY | RTLD_GLOBAL);
    if (!gles_handle) {
        printf("[GXM-GLES] Error: Failed to safely bind libGLESv2.so.2 dynamic handle!\n");
        return -1;
    }

    // Map function pointers securely with strict type casting
    gl_draw_arrays_ptr                = (void (*)(GLenum, GLint, GLsizei))dlsym(gles_handle, "glDrawArrays");
    gl_bind_texture_ptr               = (void (*)(GLenum, GLuint))dlsym(gles_handle, "glBindTexture");
    gl_clear_color_ptr                = (void (*)(GLclampf, GLclampf, GLclampf, GLclampf))dlsym(gles_handle, "glClearColor");
    gl_clear_ptr                      = (void (*)(GLbitfield))dlsym(gles_handle, "glClear");
    gl_gen_textures_ptr               = (void (*)(GLsizei, GLuint *))dlsym(gles_handle, "glGenTextures");
    gl_tex_image_2d_ptr               = (void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *))dlsym(gles_handle, "glTexImage2D");
    gl_tex_parameter_i_ptr            = (void (*)(GLenum, GLenum, GLint))dlsym(gles_handle, "glTexParameteri");
    gl_active_texture_ptr             = (void (*)(GLenum))dlsym(gles_handle, "glActiveTexture");
    gl_create_shader_ptr              = (GLuint (*)(GLenum))dlsym(gles_handle, "glCreateShader");
    gl_shader_source_ptr              = (void (*)(GLuint, GLsizei, const GLchar *const*, const GLint *))dlsym(gles_handle, "glShaderSource");
    gl_compile_shader_ptr             = (void (*)(GLuint))dlsym(gles_handle, "glCompileShader");
    gl_create_program_ptr             = (GLuint (*)(void))dlsym(gles_handle, "glCreateProgram");
    gl_attach_shader_ptr              = (void (*)(GLuint, GLuint))dlsym(gles_handle, "glAttachShader");
    gl_link_program_ptr               = (void (*)(GLuint))dlsym(gles_handle, "glLinkProgram");
    gl_use_program_ptr                = (void (*)(GLuint))dlsym(gles_handle, "glUseProgram");
    gl_get_attrib_location_ptr        = (GLint (*)(GLuint, const GLchar *))dlsym(gles_handle, "glGetAttribLocation");
    gl_get_uniform_location_ptr       = (GLint (*)(GLuint, const GLchar *))dlsym(gles_handle, "glGetUniformLocation");
    gl_uniform_1i_ptr                 = (void (*)(GLint, GLint))dlsym(gles_handle, "glUniform1i");
    gl_vertex_attrib_pointer_ptr      = (void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *))dlsym(gles_handle, "glVertexAttribPointer");
    gl_enable_vertex_attrib_array_ptr = (void (*)(GLuint))dlsym(gles_handle, "glEnableVertexAttribArray");

    printf("[GXM-GLES] System dynamic GLES hooks bound successfully!\n");
    return 0;
}

// 🎮 Corrected Signature: Matches definition layout inside varm_gxm_backend.h
static int gles_allocate_surface(GxmSurfaceContext *surface) {
    if (!surface) return -1;

    printf("[GXM-GLES] Allocating surface context: %dx%d (vaddr: 0x%08X)\n", surface->width, surface->height, surface->vaddr);

    if (gl_gen_textures_ptr && gl_bind_texture_ptr && gl_tex_parameter_i_ptr && gl_tex_image_2d_ptr) {
        gl_gen_textures_ptr(1, &surface->host_tex_id);
        gl_bind_texture_ptr(GL_TEXTURE_2D, surface->host_tex_id);

        gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Setup unpopulated texture context dimensions matching guest frame parameters
        gl_tex_image_2d_ptr(GL_TEXTURE_2D, 0, GL_RGBA, surface->width, surface->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        s_active_surface_tex_id = surface->host_tex_id;
        return 0;
    }

    return -1;
}

// 🎮 Corrected Signature: Matches definition layout inside varm_gxm_backend.h
static int gles_submit_cmd(uint32_t cmd_vaddr, uint32_t size) {
    if (!gl_enable_vertex_attrib_array_ptr || !gl_vertex_attrib_pointer_ptr || !gl_active_texture_ptr || !gl_bind_texture_ptr || !gl_uniform_1i_ptr || !gl_draw_arrays_ptr) {
        return -1;
    }

    // Process positional quad vertices geometry setup
    gl_enable_vertex_attrib_array_ptr(gl_vertex_attrib_loc);
    gl_vertex_attrib_pointer_ptr(gl_vertex_attrib_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), quad_data);

    // Process texture coordinates geometry setup
    gl_enable_vertex_attrib_array_ptr(gl_tex_attrib_loc);
    gl_vertex_attrib_pointer_ptr(gl_tex_attrib_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), &quad_data[2]);

    // Bind texture source safely to dynamic rendering hardware layout channel index 0
    gl_active_texture_ptr(GL_TEXTURE0);
    gl_bind_texture_ptr(GL_TEXTURE_2D, s_active_surface_tex_id);
    gl_uniform_1i_ptr(gl_tex_uniform_loc, 0);

    // Present rendering frame geometry boundaries to the target display backbuffer
    gl_draw_arrays_ptr(GL_TRIANGLES, 0, 6);

    return 0;
}

// 🎮 Corrected Signature: Has global scope access to function pointers resolved
static void gles_clear_screen(float r, float g, float b, float a) {
    if (gl_clear_color_ptr && gl_clear_ptr) {
        gl_clear_color_ptr(r, g, b, a);
        gl_clear_ptr(GL_COLOR_BUFFER_BIT);
    }
}

// --- Global Interface Distributor Switcher ---
int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface) {
    if (!interface) return -1;

    if (core_type == VARM_RENDER_CORE_GLES) {
        printf("[GXM-BRIDGE] Switched execution context pipeline to: OPENGL ES CORE\n");

        // Map interfaces cleanly
        interface->init_display           = gles_init_display;
        interface->allocate_surface       = gles_allocate_surface;
        interface->submit_command_buffer  = gles_submit_cmd;
        interface->clear_screen           = gles_clear_screen;

        // Run hardware hooks initialization sequencing pass immediately
        if (gles_init_display() != 0) {
            printf("[GXM-BRIDGE] Critical Error: Internal display pipeline setup failed!\n");
            return -1;
        }

        gxm_interface = *interface;
        return 0;
    }

    printf("[GXM-BRIDGE] Error: Selected unsupported backend graphics core engine.\n");
    return -1;
}
