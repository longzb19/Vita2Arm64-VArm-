#ifndef VARM_GXM_BACKEND_H
#define VARM_GXM_BACKEND_H

#include <stdint.h>

// Render Core Types
typedef enum {
    VARM_RENDER_CORE_GLES,
    VARM_RENDER_CORE_VULKAN
} V_RenderCoreType;

// Surface Context Structure updated with host-side texture tracking objects
typedef struct {
    uint32_t vaddr;             // Guest video virtual address base
    uint32_t host_tex_id;       // Resolved Host OpenGL ES Texture Handle
    int width;                  // Virtual surface viewport geometry width
    int height;                 // Virtual surface viewport geometry height
} GxmSurfaceContext;

typedef struct {
    int (*init_display)(void);
    int (*allocate_surface)(GxmSurfaceContext *surface);
    int (*submit_command_buffer)(uint32_t cmd_vaddr, uint32_t size);
    void (*clear_screen)(float r, float g, float b, float a);
} V_GxmRendererInterface;

// ADD THIS EXTERN DECLARATION
extern V_GxmRendererInterface gxm_interface;

int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface);

#endif // VARM_GXM_BACKEND_H