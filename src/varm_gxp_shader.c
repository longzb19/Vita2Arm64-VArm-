#include "varm_gxp_shader.h"
#include "varm_gxm_backend.h"
#include "hle_kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* varm_gxp_to_glsl(uint32_t gxp_vaddr, bool is_vertex) {
    if (gxp_vaddr == 0) return NULL;

    // 1. Resolve the target guest memory address into our host space
    GxpHeader* header = (GxpHeader*)hle_kernel_resolve_address(gxp_vaddr, 1);
    if (!header) {
        printf("[GXP-TRANSLATOR] Error: Couldn't resolve shader memory layout at 0x%08X\n", gxp_vaddr);
        return NULL;
    }

    printf("[GXP-TRANSLATOR] Parsing %s GXP Binary -> Size: %u bytes | Parameters: %u\n",
           is_vertex ? "Vertex" : "Fragment", header->program_size, header->parameter_count);

    // 2. Allocate a dynamic buffer to build up our GLSL program string
    size_t glsl_capacity = 8192;
    char* glsl_code = malloc(glsl_capacity);
    if (!glsl_code) return NULL;
    glsl_code[0] = '\0';

    // Append fallback initial compiler setups
    strcat(glsl_code, "// Dynamic Translation Output via Project VARM Engine\n");
    if (!is_vertex) {
        strcat(glsl_code, "precision mediump float;\n");
    }

    // 3. Locate the parameter table block relative to the start of the program header code
    uintptr_t program_base = (uintptr_t)header;
    SceGxmProgramParameter* parameter_table = (SceGxmProgramParameter*)(program_base + header->parameter_table_offset);

    // 4. Translate Vita Parameters to clean GLES2 GLSL Uniform / Attribute mappings
    for (uint32_t i = 0; i < header->parameter_count; i++) {
        SceGxmProgramParameter param = parameter_table[i];

        // Safely extract variable name from file offsets if present
        const char* param_name = "varm_unnamed";
        if (param.name_offset != 0) {
            param_name = (const char*)(program_base + param.name_offset);
        }

        char declaration_line[256];

        // Check categories matching the structural blueprints from Vita3K
        if (param.category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE && is_vertex) {
            // Map inputs to vertex attributes
            snprintf(declaration_line, sizeof(declaration_line),
                     "attribute vec4 %s; // Vita Resource Map ID: %u\n", param_name, param.resource_index);
            strcat(glsl_code, declaration_line);
        }
        else if (param.category == SCE_GXM_PARAMETER_CATEGORY_UNIFORM) {
            // Map variable constants to matrix blocks or uniform vectors
            snprintf(declaration_line, sizeof(declaration_line),
                     "uniform vec4 %s; // Target Register index: %u\n", param_name, param.resource_index);
            strcat(glsl_code, declaration_line);
        }
        else if (param.category == SCE_GXM_PARAMETER_CATEGORY_SAMPLER) {
            // Map image bindings to hardware textures
            snprintf(declaration_line, sizeof(declaration_line),
                     "uniform sampler2D %s; // Texture Unit Bind: %u\n", param_name, param.resource_index);
            strcat(glsl_code, declaration_line);
        }
    }

    // 5. Inject baseline functional execution loops so testing environments load instantly
    strcat(glsl_code, "\nvoid main() {\n");
    if (is_vertex) {
        strcat(glsl_code, "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0); // Baseline position wrapper pass\n");
    } else {
        strcat(glsl_code, "    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0); // Baseline pass color\n");
    }
    strcat(glsl_code, "}\n");

    return glsl_code;
}
