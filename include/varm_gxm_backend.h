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
    uint32_t vaddr;         // Guest video virtual address base
    uint32_t host_tex_id;   // Resolved Host OpenGL ES Texture Handle
    uint32_t width;         // Virtual surface viewport geometry width
    uint32_t height;        // Virtual surface viewport geometry height
    uint32_t stride;        // Row stride alignment
    int format;             // Pixel color format tracking
} GxmSurfaceContext;

// Driver interface functions mapping to host graphics libraries
typedef struct {
    int (*init_display)(void);
    int (*allocate_surface)(GxmSurfaceContext *surface);
    int (*submit_command_buffer)(uint32_t cmd_vaddr, uint32_t size);
    void (*clear_screen)(float r, float g, float b, float a);
    void (*swap_buffers)(void);       // Added to support the main render loop
    void (*shutdown_display)(void);   // Added to support clean environment exits
} V_GxmRendererInterface;

// Exported initialization interface instance
extern V_GxmRendererInterface gxm_interface;

int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface);

#endif // VARM_GXM_BACKEND_H
