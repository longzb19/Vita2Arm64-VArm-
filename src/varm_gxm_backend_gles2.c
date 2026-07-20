#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include "varm_gxm_backend.h"
#include "varm_graphics.h"
#include "hle_kernel.h"
#include "varm_gxp_shader.h" // 🔗 Links your dynamic translation function directly
#include "varm_input.h"      // 🔗 Added to track the menu state

extern _Bool g_show_menu; // Added to know when to render the visual overlay[cite: 11]

// Structural layout mirroring s_vertex_uniform_map inside the shader compiler[cite: 11]
typedef struct {
    uint16_t register_index;
    GLint gles_location;
} UniformMapEntry;

// Instantiated here so varm_gxp_shader_compiler_example.c can link successfully[cite: 11]
UniformMapEntry s_vertex_uniform_map[64];
int s_vertex_uniform_count = 0;

typedef struct {
    uint32_t control_word;
    uint32_t data_vaddr;
    uint16_t width;
    uint16_t height;
    uint16_t palette_index;
    uint16_t reserved;
} SceGxmTextureDescriptor;

typedef struct {
    uint32_t guest_vaddr;
    GLuint gles_tex_id;
} TextureCacheEntry;

static TextureCacheEntry s_texture_cache[256];
static int s_texture_cache_count = 0;

// Struct layout mirroring the guest SceDisplayFrameBuf structure
typedef struct {
    uint32_t size;
    uint32_t base;         // Guest virtual memory address of raw pixel layout
    uint32_t pitch;
    uint32_t pixelformat;
    uint32_t width;
    uint32_t height;
} SceDisplayFrameBuf;

static void swizzle_copy_abgr_to_rgba(uint32_t* dest, const uint32_t* src, uint32_t pixel_count) {
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint32_t pixel = src[i];
        uint8_t a = (pixel >> 24) & 0xFF;
        uint8_t b = (pixel >> 16) & 0xFF;
        uint8_t g = (pixel >> 8)  & 0xFF;
        uint8_t r = pixel         & 0xFF;
        dest[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }
}

int varm_gxm_parse_command_buffer(uint32_t buffer_vaddr, uint32_t buffer_size);

#define EGL_DRAW 0x3059

V_GxmRendererInterface gxm_interface;

static void* g_cached_egl_display = NULL;
static void* g_cached_egl_surface = NULL;
GLuint g_active_program           = 0; // Global exposure for compiler linking[cite: 11]

// Core Function Pointers (Global Scope)[cite: 11]
static void (*gl_draw_arrays_ptr)(GLenum mode, GLint first, GLsizei count) = NULL;
static void (*gl_bind_texture_ptr)(GLenum target, GLuint texture) = NULL;
static void (*gl_clear_color_ptr)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
static void (*gl_clear_ptr)(GLbitfield mask) = NULL;
static void (*gl_viewport_ptr)(GLint x, GLint y, GLsizei width, GLsizei height) = NULL;

// Texture Management Function Pointers[cite: 11]
void (*gl_gen_textures_ptr)(GLsizei n, GLuint* textures) = NULL;
void (*gl_delete_textures_ptr)(GLsizei n, const GLuint* textures) = NULL;
void (*gl_tex_image_2d_ptr)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) = NULL;
void (*gl_tex_parameter_i_ptr)(GLenum target, GLenum pname, GLint param) = NULL;
void (*gl_active_texture_ptr)(GLenum texture) = NULL;

// Vertex Attribute Function Pointers[cite: 11]
static void (*gl_vertex_attrib_pointer_ptr)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) = NULL;
static void (*gl_enable_vertex_attrib_array_ptr)(GLuint index) = NULL;
static void (*gl_disable_vertex_attrib_array_ptr)(GLuint index) = NULL;
static void (*gl_bind_attrib_location_ptr)(GLuint program, GLuint index, const GLchar* name) = NULL;

// Uniform Management Function Pointers[cite: 11]
static void (*gl_uniform_4fv_ptr)(GLint location, GLsizei count, const GLfloat* value) = NULL;
GLint (*gl_get_uniform_location_ptr)(GLuint program, const GLchar* name) = NULL;

// Shader Core Function Pointers[cite: 11]
GLuint (*gl_create_shader_ptr)(GLenum type) = NULL;
void   (*gl_shader_source_ptr)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) = NULL;
void   (*gl_compile_shader_ptr)(GLuint shader) = NULL;
void   (*gl_get_shader_iv_ptr)(GLuint shader, GLenum pname, GLint* params) = NULL;
void   (*gl_get_shader_info_log_ptr)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) = NULL;
GLuint (*gl_create_program_ptr)(void) = NULL;
void   (*gl_attach_shader_ptr)(GLuint program, GLuint shader) = NULL;
void   (*gl_link_program_ptr)(GLuint program) = NULL;
void   (*gl_use_program_ptr)(GLuint program) = NULL;
void   (*gl_delete_program_ptr)(GLuint program) = NULL;
void   (*gl_delete_shader_ptr)(GLuint shader) = NULL;

// Additional dynamically resolved calls for the Blitter Engine
static void (*gl_bind_framebuffer_ptr)(GLenum target, GLuint framebuffer) = NULL;
static void (*gl_uniform_1i_ptr)(GLint location, GLint v0) = NULL;

static void* (*egl_get_current_display_ptr)(void) = NULL;
static void* (*egl_get_current_surface_ptr)(int reftype) = NULL;
static int (*egl_swap_buffers_ptr)(void* dpy, void* surface) = NULL;

// ========================================================================
// 🛠️ DYNAMIC BLITTER ENGINE (GLES2 Dynamic Call Compliant)
// ========================================================================

static GLuint s_blit_program = 0;
static GLuint s_fallback_texture = 0;

static void init_blit_shader(void) {
    if (s_blit_program != 0) return;
    if (!gl_create_shader_ptr || !gl_shader_source_ptr || !gl_compile_shader_ptr ||
        !gl_create_program_ptr || !gl_attach_shader_ptr || !gl_bind_attrib_location_ptr ||
        !gl_link_program_ptr || !gl_delete_shader_ptr) {
        return;
    }

    const char* v_src =
        "attribute vec2 position;\n"
        "attribute vec2 texcoord;\n"
        "varying vec2 v_texcoord;\n"
        "void main() {\n"
        "   gl_Position = vec4(position, 0.0, 1.0);\n"
        "   v_texcoord = texcoord;\n"
        "}\n";

    const char* f_src =
        "precision mediump float;\n"
        "varying vec2 v_texcoord;\n"
        "uniform sampler2D u_texture;\n"
        "void main() {\n"
        "   gl_FragColor = texture2D(u_texture, v_texcoord);\n"
        "}\n";

    GLuint vs = gl_create_shader_ptr(GL_VERTEX_SHADER);
    gl_shader_source_ptr(vs, 1, &v_src, NULL);
    gl_compile_shader_ptr(vs);

    GLuint fs = gl_create_shader_ptr(GL_FRAGMENT_SHADER);
    gl_shader_source_ptr(fs, 1, &f_src, NULL);
    gl_compile_shader_ptr(fs);

    s_blit_program = gl_create_program_ptr();
    gl_attach_shader_ptr(s_blit_program, vs);
    gl_attach_shader_ptr(s_blit_program, fs);
    gl_bind_attrib_location_ptr(s_blit_program, 0, "position");
    gl_bind_attrib_location_ptr(s_blit_program, 1, "texcoord");
    gl_link_program_ptr(s_blit_program);

    gl_delete_shader_ptr(vs);
    gl_delete_shader_ptr(fs);
}

static void blit_texture_to_screen(GLuint tex_id) {
    init_blit_shader();

    if (s_blit_program == 0) return;
    if (!gl_use_program_ptr || !gl_viewport_ptr || !gl_active_texture_ptr ||
        !gl_bind_texture_ptr || !gl_get_uniform_location_ptr ||
        !gl_vertex_attrib_pointer_ptr || !gl_enable_vertex_attrib_array_ptr ||
        !gl_draw_arrays_ptr || !gl_disable_vertex_attrib_array_ptr) {
        return;
    }

    gl_use_program_ptr(s_blit_program);
    if (gl_bind_framebuffer_ptr) {
        gl_bind_framebuffer_ptr(GL_FRAMEBUFFER, 0); // Target standard screen backbuffer
    }
    gl_viewport_ptr(0, 0, 960, 544); // Force native Vita scale context mapping

    // Fullscreen quad vertices (Y inverted to correct Vita-to-GL coordinate mapping)
    GLfloat vertices[] = {
        -1.0f,  1.0f,  0.0f, 0.0f, // Top-Left
        -1.0f, -1.0f,  0.0f, 1.0f, // Bottom-Left
         1.0f, -1.0f,  1.0f, 1.0f, // Bottom-Right

        -1.0f,  1.0f,  0.0f, 0.0f, // Top-Left
         1.0f, -1.0f,  1.0f, 1.0f, // Bottom-Right
         1.0f,  1.0f,  1.0f, 0.0f  // Top-Right
    };

    gl_active_texture_ptr(GL_TEXTURE0);
    gl_bind_texture_ptr(GL_TEXTURE_2D, tex_id);

    GLint u_tex = gl_get_uniform_location_ptr(s_blit_program, "u_texture");
    if (u_tex != -1 && gl_uniform_1i_ptr) {
        gl_uniform_1i_ptr(u_tex, 0);
    }

    gl_vertex_attrib_pointer_ptr(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);
    gl_enable_vertex_attrib_array_ptr(0);
    gl_vertex_attrib_pointer_ptr(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), &vertices[2]);
    gl_enable_vertex_attrib_array_ptr(1);

    gl_draw_arrays_ptr(GL_TRIANGLES, 0, 6);

    gl_disable_vertex_attrib_array_ptr(0);
    gl_disable_vertex_attrib_array_ptr(1);
    gl_use_program_ptr(0);
}

static GLuint get_or_create_fallback_texture(void *pixels, int width, int height) {
    if (!gl_gen_textures_ptr || !gl_bind_texture_ptr || !gl_tex_parameter_i_ptr || !gl_tex_image_2d_ptr) return 0;

    if (s_fallback_texture == 0) {
        gl_gen_textures_ptr(1, &s_fallback_texture);
        gl_bind_texture_ptr(GL_TEXTURE_2D, s_fallback_texture);
        gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    gl_bind_texture_ptr(GL_TEXTURE_2D, s_fallback_texture);

    // Dynamic swizzle from guest ABGR to standard GLES RGBA on the fly
    uint32_t *swizzled = malloc(width * height * sizeof(uint32_t));
    if (swizzled) {
        swizzle_copy_abgr_to_rgba(swizzled, (const uint32_t*)pixels, width * height);
        gl_tex_image_2d_ptr(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, swizzled);
        free(swizzled);
    }
    return s_fallback_texture;
}

// ========================================================================
// 🛠️ SHADER TRANSLATION COMPILER PIPELINES
// ========================================================================

static GLuint compile_shader_source(GLenum type, const char* source) {
    if (!gl_create_shader_ptr) return 0;

    GLuint shader = gl_create_shader_ptr(type);
    gl_shader_source_ptr(shader, 1, &source, NULL);
    gl_compile_shader_ptr(shader);

    GLint success;
    gl_get_shader_iv_ptr(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        gl_get_shader_info_log_ptr(shader, 512, NULL, info_log);
        printf("[GXM-GLES] Shader Compilation Error: %s\n", info_log);
        return 0;
    }
    return shader;
}

static void generate_glsl_from_gxp(uint32_t vshader_vaddr, uint32_t fshader_vaddr) {
    if (!gl_create_program_ptr) return;

    // Call your translator engine to process the real virtual address structures
    char* v_src = varm_gxp_to_glsl(vshader_vaddr, true);
    char* f_src = varm_gxp_to_glsl(fshader_vaddr, false);

    if (!v_src || !f_src) {
        printf("[GXM-GLES] Pipeline Error: Dynamic GXP shader translation failed.\n");
        if (v_src) free(v_src);
        if (f_src) free(f_src);
        return;
    }

    GLuint vs = compile_shader_source(GL_VERTEX_SHADER, v_src);
    GLuint fs = compile_shader_source(GL_FRAGMENT_SHADER, f_src);

    if (vs && fs) {
        if (g_active_program && gl_delete_program_ptr) gl_delete_program_ptr(g_active_program);
        g_active_program = gl_create_program_ptr();
        gl_attach_shader_ptr(g_active_program, vs);
        gl_attach_shader_ptr(g_active_program, fs);

        // Enforce layout bindings BEFORE mapping program execution states
        if (gl_bind_attrib_location_ptr) {
            gl_bind_attrib_location_ptr(g_active_program, 0, "position");
            gl_bind_attrib_location_ptr(g_active_program, 1, "texcoord");
        }

        gl_link_program_ptr(g_active_program);
        gl_use_program_ptr(g_active_program);
    }

    // Free the dynamic code strings generated by malloc inside varm_gxp_shader.c
    free(v_src);
    free(f_src);
}

static void gles_bind_program(uint32_t vshader_vaddr, uint32_t fshader_vaddr) {
    if (vshader_vaddr == 0 || fshader_vaddr == 0) return;

    // Bypass old static hooks and process raw virtual addresses straight down the pipe
    generate_glsl_from_gxp(vshader_vaddr, fshader_vaddr);
}

static void gles_set_uniforms(uint32_t buffer_vaddr, uint32_t size) {
    if (g_active_program == 0 || !gl_uniform_4fv_ptr || !gl_get_uniform_location_ptr) return;

    void* raw_uniforms = hle_kernel_resolve_address(buffer_vaddr, 1);
    if (!raw_uniforms) return;

    // Fix applied to uniform lookup query
    GLint loc = gl_get_uniform_location_ptr(g_active_program, "u_v_uniforms");
    if (loc != -1) {
        gl_uniform_4fv_ptr(loc, size / 16, (const GLfloat*)raw_uniforms);
    }
}

// ========================================================================
// 🛠️ SYSTEM GRAPHICS INTERFACE & CONTEXTS
// ========================================================================

static int gles_init_display(void) {
    void* gles_handle = dlopen("libGLESv2.so", RTLD_LAZY);
    void* egl_handle = dlopen("libEGL.so", RTLD_LAZY);

    if (!gles_handle || !egl_handle) {
        printf("[GXM-GLES] Fatal: Missing system graphics hardware libraries.\n");
        return -1;
    }

    gl_draw_arrays_ptr = dlsym(gles_handle, "glDrawArrays");
    gl_bind_texture_ptr = dlsym(gles_handle, "glBindTexture");
    gl_clear_color_ptr = dlsym(gles_handle, "glClearColor");
    gl_clear_ptr = dlsym(gles_handle, "glClear");
    gl_viewport_ptr = dlsym(gles_handle, "glViewport");

    gl_gen_textures_ptr = dlsym(gles_handle, "glGenTextures");
    gl_delete_textures_ptr = dlsym(gles_handle, "glDeleteTextures");
    gl_tex_image_2d_ptr = dlsym(gles_handle, "glTexImage2D");
    gl_tex_parameter_i_ptr = dlsym(gles_handle, "glTexParameteri");
    gl_active_texture_ptr = dlsym(gles_handle, "glActiveTexture");

    gl_vertex_attrib_pointer_ptr = dlsym(gles_handle, "glVertexAttribPointer");
    gl_enable_vertex_attrib_array_ptr = dlsym(gles_handle, "glEnableVertexAttribArray");
    gl_disable_vertex_attrib_array_ptr = dlsym(gles_handle, "glDisableVertexAttribArray");
    gl_bind_attrib_location_ptr = dlsym(gles_handle, "glBindAttribLocation");

    gl_uniform_4fv_ptr = dlsym(gles_handle, "glUniform4fv");
    gl_get_uniform_location_ptr = dlsym(gles_handle, "glGetUniformLocation");

    gl_create_shader_ptr = dlsym(gles_handle, "glCreateShader");
    gl_shader_source_ptr = dlsym(gles_handle, "glShaderSource");
    gl_compile_shader_ptr = dlsym(gles_handle, "glCompileShader");
    gl_get_shader_iv_ptr = dlsym(gles_handle, "glGetShaderiv");
    gl_get_shader_info_log_ptr = dlsym(gles_handle, "glGetShaderInfoLog");
    gl_create_program_ptr = dlsym(gles_handle, "glCreateProgram");
    gl_attach_shader_ptr = dlsym(gles_handle, "glAttachShader");
    gl_link_program_ptr = dlsym(gles_handle, "glLinkProgram");
    gl_use_program_ptr = dlsym(gles_handle, "glUseProgram");
    gl_delete_program_ptr = dlsym(gles_handle, "glDeleteProgram");
    gl_delete_shader_ptr = dlsym(gles_handle, "glDeleteShader");

    // Resolvers for blitting
    gl_bind_framebuffer_ptr = dlsym(gles_handle, "glBindFramebuffer");
    gl_uniform_1i_ptr = dlsym(gles_handle, "glUniform1i");

    egl_get_current_display_ptr = dlsym(egl_handle, "eglGetCurrentDisplay");
    egl_get_current_surface_ptr = dlsym(egl_handle, "eglGetCurrentSurface");
    egl_swap_buffers_ptr = dlsym(egl_handle, "eglSwapBuffers");

    printf("[GXM-GLES] Core OpenGLES Interface Drivers Linked Successfully.\n");
    return 0;
}

static int gles_allocate_surface(GxmSurfaceContext *surface) {
    if (!surface) return -1;
    if (egl_get_current_display_ptr) g_cached_egl_display = egl_get_current_display_ptr();
    if (egl_get_current_surface_ptr) g_cached_egl_surface = egl_get_current_surface_ptr(EGL_DRAW);
    return 0;
}

static int gles_submit_command_buffer(uint32_t cmd_vaddr, uint32_t size) {
    return varm_gxm_parse_command_buffer(cmd_vaddr, size);
}

static void gles_draw_primitives(void* host_vertex_ptr, uint32_t stride, void* host_texture_ptr, uint32_t topology, uint32_t count) {
    if (host_texture_ptr != NULL && gl_bind_texture_ptr != NULL) {
        SceGxmTextureDescriptor* tex = (SceGxmTextureDescriptor*)host_texture_ptr;
        GLuint target_gl_id = 0;

        for (int i = 0; i < s_texture_cache_count; i++) {
            if (s_texture_cache[i].guest_vaddr == tex->data_vaddr) {
                target_gl_id = s_texture_cache[i].gles_tex_id;
                break;
            }
        }

        if (target_gl_id == 0 && gl_gen_textures_ptr && gl_tex_image_2d_ptr && gl_tex_parameter_i_ptr) {
            if (s_texture_cache_count >= 256) {
                printf("[GXM-GLES] Reclaiming active texture units via flush routine...\n");
                if (gl_delete_textures_ptr) {
                    for (int i = 0; i < s_texture_cache_count; i++) {
                        gl_delete_textures_ptr(1, &s_texture_cache[i].gles_tex_id);
                    }
                }
                s_texture_cache_count = 0;
            }

            void* raw_pixels = hle_kernel_resolve_address(tex->data_vaddr, 1);
            if (raw_pixels) {
                gl_gen_textures_ptr(1, &target_gl_id);
                if (gl_active_texture_ptr) gl_active_texture_ptr(GL_TEXTURE0);
                gl_bind_texture_ptr(GL_TEXTURE_2D, target_gl_id);

                gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                uint32_t total_pixels = tex->width * tex->height;
                uint32_t* conversion_buffer = (uint32_t*)malloc(total_pixels * sizeof(uint32_t));

                if (conversion_buffer) {
                    uint32_t format_type = (tex->control_word >> 24) & 0x1F;
                    if (format_type == 0x14) {
                        swizzle_copy_abgr_to_rgba(conversion_buffer, (const uint32_t*)raw_pixels, total_pixels);
                    } else {
                        memcpy(conversion_buffer, raw_pixels, total_pixels * sizeof(uint32_t));
                    }

                    gl_tex_image_2d_ptr(GL_TEXTURE_2D, 0, GL_RGBA, tex->width, tex->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, conversion_buffer);
                    free(conversion_buffer);
                }

                s_texture_cache[s_texture_cache_count].guest_vaddr = tex->data_vaddr;
                s_texture_cache[s_texture_cache_count].gles_tex_id = target_gl_id;
                s_texture_cache_count++;
            }
        } else {
            if (gl_active_texture_ptr) gl_active_texture_ptr(GL_TEXTURE0);
            gl_bind_texture_ptr(GL_TEXTURE_2D, target_gl_id);
        }
    }

    if (host_vertex_ptr && gl_vertex_attrib_pointer_ptr && gl_enable_vertex_attrib_array_ptr) {
        gl_enable_vertex_attrib_array_ptr(0);
        gl_vertex_attrib_pointer_ptr(0, 3, GL_FLOAT, GL_FALSE, stride, host_vertex_ptr);

        gl_enable_vertex_attrib_array_ptr(1);
        gl_vertex_attrib_pointer_ptr(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)((uint8_t*)host_vertex_ptr + (sizeof(float) * 3)));
    }

    if (gl_draw_arrays_ptr) {
        gl_draw_arrays_ptr(GL_TRIANGLES, 0, count);
    }

    if (host_vertex_ptr && gl_disable_vertex_attrib_array_ptr) {
        gl_disable_vertex_attrib_array_ptr(0);
        gl_disable_vertex_attrib_array_ptr(1);
    }
}

static void gles_clear_screen(float r, float g, float b, float a) {
    if (gl_viewport_ptr) gl_viewport_ptr(0, 0, 960, 544);
    if (gl_clear_color_ptr && gl_clear_ptr) {
        gl_clear_color_ptr(r, g, b, a);
        gl_clear_ptr(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

static int gles_swap_buffers(void) {
    extern SDL_Window *g_window;
    if (g_window) {
        SDL_GL_SwapWindow(g_window);
        return 0;
    }
    if (egl_swap_buffers_ptr && g_cached_egl_display && g_cached_egl_surface) {
        return egl_swap_buffers_ptr(g_cached_egl_display, g_cached_egl_surface);
    }
    return -1;
}

static void gles_shutdown_display(void) {
    if (g_active_program > 0 && gl_delete_program_ptr) {
        gl_delete_program_ptr(g_active_program);
    }
    if (s_blit_program > 0 && gl_delete_program_ptr) {
        gl_delete_program_ptr(s_blit_program);
        s_blit_program = 0;
    }
    if (s_fallback_texture > 0 && gl_delete_textures_ptr) {
        gl_delete_textures_ptr(1, &s_fallback_texture);
        s_fallback_texture = 0;
    }
    if (gl_delete_textures_ptr) {
        for (int i = 0; i < s_texture_cache_count; i++) {
            gl_delete_textures_ptr(1, &s_texture_cache[i].gles_tex_id);
        }
    }
    s_texture_cache_count = 0;
}

int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface) {
    if (!interface) return -1;

    if (core_type == VARM_RENDER_CORE_GLES) {
        printf("[GXM-BRIDGE] Switched execution context pipeline to: OPENGL ES CORE\n");
        interface->init_display           = gles_init_display;
        interface->allocate_surface       = gles_allocate_surface;
        interface->submit_command_buffer  = gles_submit_command_buffer;
        interface->draw_primitives        = gles_draw_primitives;
        interface->clear_screen           = gles_clear_screen;
        interface->swap_buffers           = gles_swap_buffers;
        interface->shutdown_display       = gles_shutdown_display;
        interface->bind_program           = gles_bind_program;
        interface->set_uniforms           = gles_set_uniforms;
        return 0;
    }
    return -1;
}

/**
 * 🛠️ THE FIX: varm_display_set_framebuf
 * HLE hook intercepted when the game binary requests to flip the display buffer.[cite: 11]
 */
int varm_display_set_framebuf(uint32_t pParam_vaddr, int sync) {
    if (pParam_vaddr == 0) return -1;

    // 1. Resolve guest virtual struct address into host space
    SceDisplayFrameBuf *pParam = (SceDisplayFrameBuf *)hle_kernel_resolve_address(pParam_vaddr, 1);
    if (!pParam || pParam->base == 0) return -1;

    GLuint tex_id = 0;

    // 2. Scan texture cache to locate GLES handle mapped to this guest address space
    for (int i = 0; i < s_texture_cache_count; i++) {
        if (s_texture_cache[i].guest_vaddr == pParam->base) {
            tex_id = s_texture_cache[i].gles_tex_id;
            break;
        }
    }

    // 3. Fallback: If not in cache (CPU/software rendering), resolve direct RAM pointer and upload on the fly
    if (tex_id == 0) {
        void *host_pixels = hle_kernel_resolve_address(pParam->base, 1);
        if (host_pixels) {
            tex_id = get_or_create_fallback_texture(host_pixels, pParam->width, pParam->height);
        }
    }

    // 4. Blit the game frame to the host's backbuffer!
    if (tex_id > 0) {
        blit_texture_to_screen(tex_id);
    }

    return 0; // Let main.c render overlays and swap normally!
}
