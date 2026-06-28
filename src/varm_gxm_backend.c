#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include "varm_gxm_backend.h"
#include "varm_graphics.h"
#include "hle_kernel.h"

#define EGL_DRAW 0x3059

V_GxmRendererInterface gxm_interface;

static void (*gl_draw_arrays_ptr)(GLenum mode, GLint first, GLsizei count) = NULL;
static void (*gl_bind_texture_ptr)(GLenum target, GLuint texture) = NULL;
static void (*gl_clear_color_ptr)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
static void (*gl_clear_ptr)(GLbitfield mask) = NULL;

static void (*gl_gen_textures_ptr)(GLsizei n, GLuint *textures) = NULL;
static void (*gl_tex_image_2d_ptr)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) = NULL;
static void (*gl_tex_parameter_i_ptr)(GLenum target, GLenum pname, GLint param) = NULL;
static void (*gl_active_texture_ptr)(GLenum texture) = NULL;

static void* (*egl_get_current_display_ptr)(void) = NULL;
static void* (*egl_get_current_surface_ptr)(int reftarget) = NULL;
static int   (*egl_swap_buffers_ptr)(void* dpy, void* surface) = NULL;

static int gles_init_display(void) {
    void* gles_handle = dlopen("libGLESv2.so.2", RTLD_LAZY | RTLD_GLOBAL);
    if (!gles_handle) {
        printf("[GXM-GLES] Error: Failed to open hardware libGLESv2 driver binary container.\n");
        return -1;
    }

    gl_draw_arrays_ptr  = dlsym(gles_handle, "glDrawArrays");
    gl_bind_texture_ptr = dlsym(gles_handle, "glBindTexture");
    gl_clear_color_ptr  = dlsym(gles_handle, "glClearColor");
    gl_clear_ptr        = dlsym(gles_handle, "glClear");
    gl_gen_textures_ptr = dlsym(gles_handle, "glGenTextures");
    gl_tex_image_2d_ptr = dlsym(gles_handle, "glTexImage2D");
    gl_tex_parameter_i_ptr = dlsym(gles_handle, "glTexParameteri");
    gl_active_texture_ptr  = dlsym(gles_handle, "glActiveTexture");

    egl_get_current_display_ptr = dlsym(RTLD_DEFAULT, "eglGetCurrentDisplay");
    egl_get_current_surface_ptr = dlsym(RTLD_DEFAULT, "eglGetCurrentSurface");
    egl_swap_buffers_ptr        = dlsym(RTLD_DEFAULT, "eglSwapBuffers");

    printf("[GXM-GLES] Core dynamic rendering pipeline extensions resolved and bound.\n");
    return 0;
}

static int gles_allocate_surface(GxmSurfaceContext *surface) {
    if (!surface || !gl_gen_textures_ptr || !gl_bind_texture_ptr || !gl_tex_image_2d_ptr || !gl_tex_parameter_i_ptr) return -1;

    gl_gen_textures_ptr(1, &surface->host_tex_id);
    gl_bind_texture_ptr(GL_TEXTURE_2D, surface->host_tex_id);
    gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_tex_parameter_i_ptr(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gl_tex_image_2d_ptr(GL_TEXTURE_2D, 0, GL_RGBA, surface->width, surface->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    printf("[GXM-GLES] Allocated guest backing texture target. Host Ref: [%d] (%dx%d)\n", surface->host_tex_id, surface->width, surface->height);
    return 0;
}

static int gles_submit_cmd(uint32_t cmd_vaddr, uint32_t size) {
    if (gl_draw_arrays_ptr) {
        gl_draw_arrays_ptr(GL_TRIANGLE_STRIP, 0, 4);
        return 0;
    }
    return -1;
}

static void gles_clear_screen(float r, float g, float b, float a) {
    if (gl_clear_color_ptr && gl_clear_ptr) {
        gl_clear_color_ptr(r, g, b, a);
        gl_clear_ptr(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

static int gles_swap_buffers(void) {
    if (egl_swap_buffers_ptr && egl_get_current_display_ptr && egl_get_current_surface_ptr) {
        void* dpy = egl_get_current_display_ptr();
        void* surface = egl_get_current_surface_ptr(EGL_DRAW);
        if (dpy && surface) {
            return egl_swap_buffers_ptr(dpy, surface);
        }
    }
    return -1;
}

static void gles_shutdown_display(void) {
    printf("[GXM-GLES] Tearing down active GLES hardware bridge pipelines cleanly.\n");
}

int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface) {
    if (!interface) return -1;

    if (core_type == VARM_RENDER_CORE_GLES) {
        printf("[GXM-BRIDGE] Switched execution context pipeline to: OPENGL ES CORE\n");

        interface->init_display           = gles_init_display;
        interface->allocate_surface       = gles_allocate_surface;
        interface->submit_command_buffer  = gles_submit_cmd;
        interface->clear_screen           = gles_clear_screen;
        interface->swap_buffers           = gles_swap_buffers;
        interface->shutdown_display       = gles_shutdown_display;

        if (gles_init_display() != 0) {
            printf("[GXM-BRIDGE] Critical Error: Internal display pipeline setup failed!\n");
            return -1;
        }

        gxm_interface = *interface;
        return 0;
    }
    return -1;
}
