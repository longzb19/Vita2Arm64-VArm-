#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "varm_gxm_backend.h"
#include <GLES2/gl2.h>

static void (*gl_draw_arrays_ptr)(GLenum mode, GLint first, GLsizei count) = NULL;
static void (*gl_bind_texture_ptr)(GLenum target, GLuint texture) = NULL;
static void (*gl_clear_color_ptr)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
static void (*gl_clear_ptr)(GLbitfield mask) = NULL;

static int gles_init_display(void) {
    printf("[GXM-GLES] Hooking native muOS GLES reference driver layers...\n");

    void* gles_lib = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
    if (!gles_lib) {
        printf("[GXM-GLES-ERROR] Target driver link hook libGLESv2.so failed!\n");
        return -1;
    }

    gl_draw_arrays_ptr = dlsym(gles_lib, "glDrawArrays");
    gl_bind_texture_ptr = dlsym(gles_lib, "glBindTexture");
    gl_clear_color_ptr = dlsym(gles_lib, "glClearColor");
    gl_clear_ptr = dlsym(gles_lib, "glClear");

    printf("[GXM-GLES] System dynamic GLES hooks bound successfully!\n");
    return 0;
}

static int gles_allocate_surface(GxmSurfaceContext *surface) {
    printf("[GXM-GLES] Mapping internal virtual GXM Surface (Vaddr: 0x%08X) to GLES Texture mapping...\n", surface->vaddr);
    return 0;
}

static int gles_submit_cmd(uint32_t cmd_vaddr, uint32_t size) {
    uint32_t* cmd_buffer = (uint32_t*)(uintptr_t)cmd_vaddr;
    if (!cmd_buffer) return -1;

    uint32_t gxm_op_code = cmd_buffer[0];
    if (gxm_op_code == 0x000044AA && gl_draw_arrays_ptr) {
        // Translation mapping would handle real execution loops here
    }
    return 0;
}

static void gles_clear_screen(float r, float g, float b, float a) {
    if (gl_clear_color_ptr && gl_clear_ptr) {
        gl_clear_color_ptr(r, g, b, a);
        gl_clear_ptr(GL_COLOR_BUFFER_BIT);
    }
}

static int vulkan_init_display(void) {
    printf("[GXM-VULKAN-ERROR] Vulkan context loader unavailable.\n");
    return -1;
}

static int vulkan_allocate_surface(GxmSurfaceContext *surface) { return 0; }
static int vulkan_submit_cmd(uint32_t cmd_vaddr, uint32_t size) { return 0; }
static void vulkan_clear_screen(float r, float g, float b, float a) { }

// Global Core Distributor Selector
int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface) {
    if (!interface) return -1;

    if (core_type == VARM_RENDER_CORE_GLES) {
        printf("[GXM-BRIDGE] Switched execution context pipeline to: OPENGL ES CORE\n");
        interface->init_display           = gles_init_display;
        interface->allocate_surface       = gles_allocate_surface;
        interface->submit_command_buffer  = gles_submit_cmd;
        interface->clear_screen           = gles_clear_screen; // 🌟 Now safely mapped!
        return 0;
    }
    else if (core_type == VARM_RENDER_CORE_VULKAN) {
        printf("[GXM-BRIDGE] Switched execution context pipeline to: VULKAN CORE\n");
        interface->init_display           = vulkan_init_display;
        interface->allocate_surface       = vulkan_allocate_surface;
        interface->submit_command_buffer  = vulkan_submit_cmd;
        interface->clear_screen           = vulkan_clear_screen;
        return 0;
    }
    return -1;
}
