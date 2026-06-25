#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include "varm_gxm_backend.h"
#include "varm_graphics.h"
#include "hle_kernel.h"

// 1. Define the Global Interface (The linker will find this here)
V_GxmRendererInterface gxm_interface;

// 2. File-scope static GLES function pointers
static void (*gl_clear_color_ptr)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
static void (*gl_clear_ptr)(GLbitfield mask) = NULL;
static void (*gl_draw_arrays_ptr)(GLenum mode, GLint first, GLsizei count) = NULL;

// 3. Initialization of Display and Function Pointers
static int gles_init_display(void) {
    printf("[GXM-GLES] Hooking GLES driver layers...\n");
    
    void* gles_handle = dlopen("libGLESv2.so.2", RTLD_LAZY | RTLD_GLOBAL);
    if (!gles_handle) {
        printf("[GXM-GLES] Failed to load libGLESv2.so.2\n");
        return -1;
    }

    // Bind pointers
    gl_clear_color_ptr = dlsym(gles_handle, "glClearColor");
    gl_clear_ptr       = dlsym(gles_handle, "glClear");
    gl_draw_arrays_ptr = dlsym(gles_handle, "glDrawArrays");

    if (!gl_clear_color_ptr || !gl_clear_ptr) {
        printf("[GXM-GLES] Failed to map essential GL functions.\n");
        return -1;
    }

    return 0;
}

// 4. Corrected Surface Allocation (Matches Header Struct)
static int gles_allocate_surface(GxmSurfaceContext *surface) {
    if (!surface) return -1;
    printf("[GXM-GLES] Allocating surface context: %dx%d\n", surface->width, surface->height);
    // Add your actual surface allocation logic here
    return 0;
}

// 5. Corrected Command Buffer Submission (Matches Header Struct)
static int gles_submit_cmd(uint32_t cmd_vaddr, uint32_t size) {
    printf("[GXM-GLES] Submitting command buffer at 0x%08X (Size: %u)\n", cmd_vaddr, size);
    // Add your actual submission logic here
    return 0;
}

// 6. Corrected Clear Screen (Has visibility to global pointers now)
static void gles_clear_screen(float r, float g, float b, float a) {
    if (gl_clear_color_ptr && gl_clear_ptr) {
        gl_clear_color_ptr(r, g, b, a);
        gl_clear_ptr(GL_COLOR_BUFFER_BIT);
    }
}

// 7. Global Distributor Selector
int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface) {
    if (!interface) return -1;

    if (core_type == VARM_RENDER_CORE_GLES) {
        printf("[GXM-BRIDGE] Initializing GLES Render Pipeline...\n");
        
        interface->init_display           = gles_init_display;
        interface->allocate_surface       = gles_allocate_surface; // Now matches GxmSurfaceContext *
        interface->submit_command_buffer  = gles_submit_cmd;       // Now matches uint32_t, uint32_t
        interface->clear_screen           = gles_clear_screen;
        
        return 0;
    }

    return -1;
}