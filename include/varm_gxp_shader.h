#ifndef VARM_GXP_SHADER_H
#define VARM_GXP_SHADER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * varm_gxp_to_glsl
 * Transpiles parsed Vita GXP binary shader blocks directly into host-compatible GLSL code loops.
 */
char* varm_gxp_to_glsl(uint32_t gxp_vaddr, bool is_vertex);

#endif // VARM_GXP_SHADER_H
