#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include "varm_gxm_backend.h"
#include "varm_graphics.h"
#include "hle_kernel.h"

// Structural layout of a native Sony Vita SceGxmTexture configuration descriptor
typedef struct {
    uint32_t control_word;
    uint32_t data_vaddr; // Pointer to actual raw pixel data inside guest memory space
    uint16_t width;
    uint16_t height;
    uint16_t palette_index;
    uint16_t reserved;
} SceGxmTextureDescriptor;

// Tracking structure to map raw guest addresses to dynamic host GLES texture handles
typedef struct {
    uint32_t guest_vaddr;
    GLuint gles_tex_id;
} TextureCacheEntry;

static TextureCacheEntry s_texture_cache[256];
static int s_texture_cache_count = 0;

// Optimized software converter loop to match explicit Vita swizzles to default standard GLES2 RGBA
static void swizzle_copy_abgr_to_rgba(uint32_t* dest, const uint32_t* src, uint32_t pixel_count) {
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint32_t pixel = src[i];
        uint8_t a = (pixel >> 24) & 0xFF;
        uint8_t b = (pixel >> 16) & 0xFF;
        uint8_t g = (pixel >> 8)  & 0xFF;
        uint8_t r = pixel         & 0xFF;

        // Re-align explicitly to standard RGBA component sequence order
        dest[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }
}

int varm_gxm_parse_command_buffer(uint32_t buffer_vaddr, uint32_t buffer_size);

#define EGL_DRAW 0x3059

V_GxmRendererInterface gxm_interface;

static void* g_cached_egl_display = NULL;
static void* g_cached_egl_surface = NULL;
static GLuint g_active_program     = 0;

// Core Function Pointers (Global Scope)
static void (*gl_draw_arrays_ptr)(GLenum mode, GLint first, GLsizei count) = NULL;
static void (*gl_bind_texture_ptr)(GLenum target, GLuint texture) = NULL;
static void (*gl_clear_color_ptr)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
static void (*gl_clear_ptr)(GLbitfield mask) = NULL;
static void (*gl_viewport_ptr)(GLint x, GLint y, GLsizei width, GLsizei height) = NULL;

// Texture Management Function Pointers (Moved to Global Scope)
static void (*gl_gen_textures_ptr)(GLsizei n, GLuint* textures) = NULL;
static void (*gl_tex_image_2d_ptr)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) = NULL;
static void (*gl_tex_parameter_i_ptr)(GLenum target, GLenum pname, GLint param) = NULL;
static void (*gl_active_texture_ptr)(GLenum texture) = NULL;

// Vertex Attribute Function Pointers for pipeline linking
static void (*gl_vertex_attrib_pointer_ptr)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) = NULL;
static void (*gl_enable_vertex_attrib_array_ptr)(GLuint index) = NULL;
static void (*gl_disable_vertex_attrib_array_ptr)(GLuint index) = NULL;

static void* (*egl_get_current_display_ptr)(void) = NULL;
static void* (*egl_get_current_surface_ptr)(int reftype) = NULL;
static int (*egl_swap_buffers_ptr)(void* dpy, void* surface) = NULL;

static GLuint compile_shader_source(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        printf("[GXM-GLES] Shader Compilation Error: %s\n", info_log);
        return 0;
    }
    return shader;
}

static void generate_glsl_from_gxp(GxpHeader *v_gxp, GxpHeader *f_gxp) {
    const char* v_src =
        "attribute vec4 position;\n"
        "attribute vec2 texcoord;\n"
        "varying vec2 v_texcoord;\n"
        "void main() {\n"
        "  gl_Position = position;\n"
        "  v_texcoord = texcoord;\n"
        "}\n";

    const char* f_src =
        "precision mediump float;\n"
        "varying vec2 v_texcoord;\n"
        "uniform sampler2D texture_unit;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(texture_unit, v_texcoord);\n"
        "}\n";

    if (v_gxp && v_gxp->magic == 0x50584700) {
        SceGxmProgramParameter *params = (SceGxmProgramParameter*)((uint8_t*)v_gxp + v_gxp->parameter_table_offset);
        for (uint32_t i = 0; i < v_gxp->parameter_count; i++) {
            if (params[i].category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) {
                printf("[VARM-COMPILER] Registered Vertex Attribute index: %u\n", params[i].resource_index);
            }
        }
    }

    GLuint vs = compile_shader_source(GL_VERTEX_SHADER, v_src);
    GLuint fs = compile_shader_source(GL_FRAGMENT_SHADER, f_src);

    if (vs && fs) {
        if (g_active_program) glDeleteProgram(g_active_program);
        g_active_program = glCreateProgram();
        glAttachShader(g_active_program, vs);
        glAttachShader(g_active_program, fs);
        glLinkProgram(g_active_program);
        glUseProgram(g_active_program);
    }
}

static void gles_bind_program(uint32_t vshader_vaddr, uint32_t fshader_vaddr) {
    if (vshader_vaddr == 0 || fshader_vaddr == 0) return;

    GxpHeader *v_gxp = (GxpHeader*)hle_kernel_resolve_address(vshader_vaddr, 1);
    GxpHeader *f_gxp = (GxpHeader*)hle_kernel_resolve_address(fshader_vaddr, 1);

    generate_glsl_from_gxp(v_gxp, f_gxp);
}

static int gles_init_display(void) {
    void* gles_handle = dlopen("libGLESv2.so", RTLD_LAZY);
    void* egl_handle = dlopen("libEGL.so", RTLD_LAZY);

    if (!gles_handle || !egl_handle) {
        printf("[GXM-GLES] Fatal: Missing system graphics hardware libraries.\n");
        return -1;
    }

    // Existing bindings
    gl_draw_arrays_ptr = dlsym(gles_handle, "glDrawArrays");
    gl_bind_texture_ptr = dlsym(gles_handle, "glBindTexture");
    gl_clear_color_ptr = dlsym(gles_handle, "glClearColor");
    gl_clear_ptr = dlsym(gles_handle, "glClear");
    gl_viewport_ptr = dlsym(gles_handle, "glViewport");

    // Texture management bindings
    gl_gen_textures_ptr = dlsym(gles_handle, "glGenTextures");
    gl_tex_image_2d_ptr = dlsym(gles_handle, "glTexImage2D");
    gl_tex_parameter_i_ptr = dlsym(gles_handle, "glTexParameteri");
    gl_active_texture_ptr = dlsym(gles_handle, "glActiveTexture");

    // Vertex Array processing bindings
    gl_vertex_attrib_pointer_ptr = dlsym(gles_handle, "glVertexAttribPointer");
    gl_enable_vertex_attrib_array_ptr = dlsym(gles_handle, "glEnableVertexAttribArray");
    gl_disable_vertex_attrib_array_ptr = dlsym(gles_handle, "glDisableVertexAttribArray");

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
    // 1. Texture Binding Logic
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

                if (s_texture_cache_count < 256) {
                    s_texture_cache[s_texture_cache_count].guest_vaddr = tex->data_vaddr;
                    s_texture_cache[s_texture_cache_count].gles_tex_id = target_gl_id;
                    s_texture_cache_count++;
                } else {
                    printf("[GXM-GLES] Warning: Texture Cache full! Performance drop expected.\n");
                }
            }
        } else {
            if (gl_active_texture_ptr) gl_active_texture_ptr(GL_TEXTURE0);
            gl_bind_texture_ptr(GL_TEXTURE_2D, target_gl_id);
        }
    }

    // 2. Vertex Setup Interception (Fixed: Passing the structure data to the Shader)
    if (host_vertex_ptr && gl_vertex_attrib_pointer_ptr && gl_enable_vertex_attrib_array_ptr) {
        // Attribute 0: position (vec4 -> x, y, z, w or just x, y, z depending on intercept layout)
        gl_enable_vertex_attrib_array_ptr(0);
        gl_vertex_attrib_pointer_ptr(0, 3, GL_FLOAT, GL_FALSE, stride, host_vertex_ptr);

        // Attribute 1: texcoord (vec2 -> u, v). Offset assumes UV follows position coordinates
        gl_enable_vertex_attrib_array_ptr(1);
        gl_vertex_attrib_pointer_ptr(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)((uint8_t*)host_vertex_ptr + (sizeof(float) * 3)));
    }

    // 3. Execution Pipeline Draw Trigger
    if (gl_draw_arrays_ptr) {
        gl_draw_arrays_ptr(GL_TRIANGLES, 0, count);
    }

    // Cleanup active states safely
    if (host_vertex_ptr && gl_disable_vertex_attrib_array_ptr) {
        gl_disable_vertex_attrib_array_ptr(0);
        gl_disable_vertex_attrib_array_ptr(1);
    }
}

static void gles_clear_screen(float r, float g, float b, float a) {
    if (gl_viewport_ptr) {
        gl_viewport_ptr(0, 0, 960, 544);
    }
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
    if (g_active_program > 0) glDeleteProgram(g_active_program);
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
        interface->set_uniforms           = NULL;
        return 0;
    }
    return -1;
}
