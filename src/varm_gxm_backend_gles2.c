#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include "varm_gxm_backend.h"
#include "varm_graphics.h"
#include "hle_kernel.h"

#define EGL_DRAW 0x3059

// ============================================================================
// STRUCTS & GLOBALS
// ============================================================================

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

typedef struct {
    uint16_t register_index;
    GLint gles_location;
} UniformMapEntry;

static TextureCacheEntry s_texture_cache[256];
static int s_texture_cache_count = 0;
static UniformMapEntry s_vertex_uniform_map[64];
static int s_vertex_uniform_count = 0;

static void* g_cached_egl_display = NULL;
static void* g_cached_egl_surface = NULL;
static GLuint g_active_program     = 0;

// GLES/EGL Function Pointers
static void (*gl_draw_arrays_ptr)(GLenum mode, GLint first, GLsizei count) = NULL;
static void (*gl_bind_texture_ptr)(GLenum target, GLuint texture) = NULL;
static void (*gl_clear_color_ptr)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
static void (*gl_clear_ptr)(GLbitfield mask) = NULL;
static void (*gl_viewport_ptr)(GLint x, GLint y, GLsizei width, GLsizei height) = NULL;
static void (*gl_gen_textures_ptr)(GLsizei n, GLuint* textures) = NULL;
static void (*gl_tex_image_2d_ptr)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) = NULL;
static void (*gl_tex_parameter_i_ptr)(GLenum target, GLenum pname, GLint param) = NULL;
static void (*gl_active_texture_ptr)(GLenum texture) = NULL;
static void (*gl_vertex_attrib_pointer_ptr)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) = NULL;
static void (*gl_enable_vertex_attrib_array_ptr)(GLuint index) = NULL;
static void (*gl_disable_vertex_attrib_array_ptr)(GLuint index) = NULL;
static void (*gl_uniform_4fv_ptr)(GLint location, GLsizei count, const GLfloat* value) = NULL;
static GLint (*gl_get_uniform_location_ptr)(GLuint program, const GLchar* name) = NULL;
static void (*gl_delete_program_ptr)(GLuint program) = NULL; // FIX: Added missing pointer

static void* (*egl_get_current_display_ptr)(void) = NULL;
static void* (*egl_get_current_surface_ptr)(int reftype) = NULL;
static int (*egl_swap_buffers_ptr)(void* dpy, void* surface) = NULL;

// ============================================================================
// TEXTURE ENGINE
// ============================================================================

static void swizzle_copy_abgr_to_rgba(uint32_t* dest, const uint32_t* src, uint32_t pixel_count) {
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint32_t pixel = src[i];
        uint8_t a = (pixel >> 24) & 0xFF;
        uint8_t b = (pixel >> 16) & 0xFF;
        uint8_t g = (pixel >> 8)  & 0xFF;
        uint8_t r = pixel         & 0xFF;
        dest[i] = r | (g << 8) | (b << 16) | (a << 24);
    }
}

static GLuint varm_gles_get_or_create_texture(SceGxmTextureDescriptor* tex) {
    if (!tex || tex->data_vaddr == 0) return 0;
    for (int i = 0; i < s_texture_cache_count; i++) {
        if (s_texture_cache[i].guest_vaddr == tex->data_vaddr) return s_texture_cache[i].gles_tex_id;
    }

    void* raw_pixels = hle_kernel_resolve_address(tex->data_vaddr, 1);
    if (!raw_pixels) return 0;

    GLuint new_gl_id = 0;
    gl_gen_textures_ptr(1, &new_gl_id);
    gl_active_texture_ptr(GL_TEXTURE0);
    gl_bind_texture_ptr(GL_TEXTURE_2D, new_gl_id);
    gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    uint32_t total_pixels = tex->width * tex->height;
    uint32_t* conversion_buffer = (uint32_t*)malloc(total_pixels * sizeof(uint32_t));
    if (conversion_buffer) {
        swizzle_copy_abgr_to_rgba(conversion_buffer, (const uint32_t*)raw_pixels, total_pixels);
        gl_tex_image_2d_ptr(GL_TEXTURE_2D, 0, GL_RGBA, tex->width, tex->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, conversion_buffer);
        free(conversion_buffer);
    }

    if (s_texture_cache_count < 256) {
        s_texture_cache[s_texture_cache_count].guest_vaddr = tex->data_vaddr;
        s_texture_cache[s_texture_cache_count].gles_tex_id = new_gl_id;
        s_texture_cache_count++;
    }
    return new_gl_id;
}

// ============================================================================
// RENDERER INTERFACE IMPLEMENTATION
// ============================================================================

static int gles_init_display(void) {
    void* gles_handle = dlopen("libGLESv2.so", RTLD_LAZY);
    void* egl_handle = dlopen("libEGL.so", RTLD_LAZY);
    if (!gles_handle || !egl_handle) return -1;

    gl_draw_arrays_ptr = dlsym(gles_handle, "glDrawArrays");
    gl_bind_texture_ptr = dlsym(gles_handle, "glBindTexture");
    gl_clear_color_ptr = dlsym(gles_handle, "glClearColor");
    gl_clear_ptr = dlsym(gles_handle, "glClear");
    gl_viewport_ptr = dlsym(gles_handle, "glViewport");
    gl_gen_textures_ptr = dlsym(gles_handle, "glGenTextures");
    gl_tex_image_2d_ptr = dlsym(gles_handle, "glTexImage2D");
    gl_tex_parameter_i_ptr = dlsym(gles_handle, "glTexParameteri");
    gl_active_texture_ptr = dlsym(gles_handle, "glActiveTexture");
    gl_vertex_attrib_pointer_ptr = dlsym(gles_handle, "glVertexAttribPointer");
    gl_enable_vertex_attrib_array_ptr = dlsym(gles_handle, "glEnableVertexAttribArray");
    gl_disable_vertex_attrib_array_ptr = dlsym(gles_handle, "glDisableVertexAttribArray");
    gl_uniform_4fv_ptr = dlsym(gles_handle, "glUniform4fv");
    gl_get_uniform_location_ptr = dlsym(gles_handle, "glGetUniformLocation");
    gl_delete_program_ptr = dlsym(gles_handle, "glDeleteProgram"); // FIX: Resolved pointer

    egl_get_current_display_ptr = dlsym(egl_handle, "eglGetCurrentDisplay");
    egl_get_current_surface_ptr = dlsym(egl_handle, "eglGetCurrentSurface");
    egl_swap_buffers_ptr = dlsym(egl_handle, "eglSwapBuffers");

    return 0;
}

static void gles_draw_primitives(void* host_vertex_ptr, uint32_t stride, void* host_texture_ptr, uint32_t topology, uint32_t count) {
    if (host_texture_ptr) {
        GLuint tex_id = varm_gles_get_or_create_texture((SceGxmTextureDescriptor*)host_texture_ptr);
        gl_bind_texture_ptr(GL_TEXTURE_2D, tex_id);
    }

    if (host_vertex_ptr) {
        gl_enable_vertex_attrib_array_ptr(0);
        gl_vertex_attrib_pointer_ptr(0, 3, GL_FLOAT, GL_FALSE, stride, host_vertex_ptr);
        gl_enable_vertex_attrib_array_ptr(1);
        gl_vertex_attrib_pointer_ptr(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)((uint8_t*)host_vertex_ptr + (sizeof(float) * 3)));
    }

    gl_draw_arrays_ptr(GL_TRIANGLES, 0, count);

    gl_disable_vertex_attrib_array_ptr(0);
    gl_disable_vertex_attrib_array_ptr(1);
}

static void gles_set_uniforms(uint32_t buffer_vaddr, uint32_t size) {
    float* raw_uniform_buffer = (float*)hle_kernel_resolve_address(buffer_vaddr, 1);
    if (!raw_uniform_buffer || !gl_uniform_4fv_ptr) return;

    for (int i = 0; i < s_vertex_uniform_count; i++) {
        uint16_t reg = s_vertex_uniform_map[i].register_index;
        gl_uniform_4fv_ptr(s_vertex_uniform_map[i].gles_location, 1, &raw_uniform_buffer[reg * 4]);
    }
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
    if (egl_swap_buffers_ptr && g_cached_egl_display && g_cached_egl_surface) {
        return egl_swap_buffers_ptr(g_cached_egl_display, g_cached_egl_surface);
    }
    return -1;
}

static void gles_shutdown_display(void) {
    if (g_active_program > 0 && gl_delete_program_ptr) gl_delete_program_ptr(g_active_program); // FIX: Safe dynamic call
}

int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface) {
    if (!interface) return -1;
    if (core_type == VARM_RENDER_CORE_GLES) {
        interface->init_display           = gles_init_display;
        interface->allocate_surface       = gles_allocate_surface;
        interface->submit_command_buffer  = gles_submit_command_buffer;
        interface->draw_primitives        = gles_draw_primitives;
        interface->clear_screen           = gles_clear_screen;
        interface->swap_buffers           = gles_swap_buffers;
        interface->shutdown_display       = gles_shutdown_display;
        interface->set_uniforms           = gles_set_uniforms;
        interface->bind_program           = NULL;
        return 0;
    }
    return -1;
}
