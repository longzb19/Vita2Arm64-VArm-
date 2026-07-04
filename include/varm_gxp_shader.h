#ifndef VARM_GXP_SHADER_H
#define VARM_GXP_SHADER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Parses a Sony GXP binary file structure and translates it into standard GLSL.
 * @param gxp_vaddr The guest virtual address pointing to the GXP binary inside mapped memory.
 * @param is_vertex Set to true if parsing a vertex program, false for a fragment program.
 * @return A heap-allocated string containing valid, clean OpenGL ES 2.0 GLSL shader code.
 */
char* varm_gxp_to_glsl(uint32_t gxp_vaddr, bool is_vertex);

#endif // VARM_GXP_SHADER_H
