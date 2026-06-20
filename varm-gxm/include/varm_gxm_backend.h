#ifndef VARM_GXM_BACKEND_H
#define VARM_GXM_BACKEND_H

#include <stdint.h>

// Selectable rendering cores
typedef enum {
    VARM_RENDER_CORE_GLES,
    VARM_RENDER_CORE_VULKAN
} V_RenderCoreType;

// Standardized structure to hold resolved GXM textures/framebuffers
typedef struct {
    uint32_t vaddr;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int format;
} GxmSurfaceContext;

// Driver interface functions mapping to muOS stock libraries
typedef struct {
    int (*init_display)(void);
    int (*allocate_surface)(GxmSurfaceContext *surface);
    int (*submit_command_buffer)(uint32_t cmd_vaddr, uint32_t size);
    void (*swap_buffers)(void);
    void (*shutdown_display)(void);
} V_GxmRendererInterface;

// Exported initialization function
int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface);

#endif // VARM_GXM_BACKEND_H
