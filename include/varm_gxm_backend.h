#ifndef VARM_GXM_BACKEND_H
#define VARM_GXM_BACKEND_H

#include <stdint.h>

typedef enum {
    VARM_RENDER_CORE_GLES,
    VARM_RENDER_CORE_VULKAN
} V_RenderCoreType;

// GXM Parameter categories mirroring the structures inside types.h
typedef enum {
    SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE = 0,
    SCE_GXM_PARAMETER_CATEGORY_UNIFORM   = 1,
    SCE_GXM_PARAMETER_CATEGORY_SAMPLER   = 2
} SceGxmParameterCategory;

typedef enum {
    SCE_GXM_PARAMETER_SEMANTIC_ATTR     = 1,
    SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD = 4
} SceGxmParameterSemantic;

// Extracted from Vita3K types.h alignment mappings
typedef struct {
    uint32_t name_offset;
    uint16_t type;
    uint8_t  category;
    uint8_t  container_index;
    uint32_t semantic;
    uint8_t  semantic_index;
    uint16_t padding;
    uint32_t array_size;
    uint32_t resource_index;
} SceGxmProgramParameter;

typedef struct {
    uint32_t magic; // "GXP\0"
    uint32_t version;
    uint32_t program_size;
    uint32_t parameter_count;
    uint32_t parameter_table_offset;
} GxpHeader;

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
    void (*draw_primitives)(void* host_vertex_ptr, uint32_t stride, void* host_texture_ptr, uint32_t topology, uint32_t count);
    void (*clear_screen)(float r, float g, float b, float a);
    int (*swap_buffers)(void);
    void (*shutdown_display)(void);

    // Dynamic Pipeline Controls
    void (*bind_program)(uint32_t vshader_vaddr, uint32_t fshader_vaddr);
    void (*set_uniforms)(uint32_t buffer_vaddr, uint32_t size);
} V_GxmRendererInterface;

extern V_GxmRendererInterface gxm_interface;

int varm_gxm_init_renderer(V_RenderCoreType core_type, V_GxmRendererInterface *interface);

#endif // VARM_GXM_BACKEND_H
