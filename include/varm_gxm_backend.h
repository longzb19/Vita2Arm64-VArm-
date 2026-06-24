#ifndef VARM_GXM_BACKEND_H
#define VARM_GXM_BACKEND_H

#include <stdint.h>

// Render Core Types
typedef enum {
    VARM_RENDER_CORE_GLES,
    VARM_RENDER_CORE_VULKAN
} V_RenderCoreType;

// Surface Context Structure
typedef struct {
    uint32_t vaddr;
    // Add any other structural alignment properties if needed later
} GxmSurfaceContext;

// Main GXM Renderer Distribution Interface
typedef struct {
    int (*init_display)(void);
    int (*allocate_surface)(GxmSurfaceContext *surface);
    int (*submit_command_buffer)(uint32_t cmd_vaddr, uint32_t size);
    void (*clear_screen)(float r, float g, float b, float a); // 🌟 Added
} V_GxmRendererInterface;

// GLOBAL DECLARATION (No static keyword, so the linker can see it!)
int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface);

#endif // VARM_GXM_BACKEND_H
