#ifndef VARM_GXM_BACKEND_H
#define VARM_GXM_BACKEND_H

#include <stdint.h>

typedef enum {
    VARM_RENDER_CORE_GLES,
    VARM_RENDER_CORE_VULKAN
} V_RenderCoreType;

typedef struct {
    uint32_t vaddr;
    uint32_t host_tex_id;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int format;
} GxmSurfaceContext;

typedef struct {
    int (*init_display)(void);
    int (*allocate_surface)(GxmSurfaceContext *surface);
    int (*submit_command_buffer)(uint32_t cmd_vaddr, uint32_t size);
    void (*clear_screen)(float r, float g, float b, float a);
    int (*swap_buffers)(void);
    void (*shutdown_display)(void);
} V_GxmRendererInterface;

extern V_GxmRendererInterface gxm_interface;

int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface);

#endif // VARM_GXM_BACKEND_H
