#include "varm_gxp_shader.h"
#include "varm_gxm_backend.h"
#include "hle_kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* varm_gxp_to_glsl(uint32_t gxp_vaddr, bool is_vertex) {
    if (gxp_vaddr == 0) return NULL;

    GxpHeader* header = (GxpHeader*)hle_kernel_resolve_address(gxp_vaddr, 1);
    if (!header) {
        printf("[GXP-TRANSLATOR] Error: Couldn't resolve shader memory layout at 0x%08X\n", gxp_vaddr);
        return NULL;
    }

    printf("[GXP-TRANSLATOR] Parsing %s GXP Binary -> Size: %u bytes | Parameters: %u\n",
           is_vertex ? "Vertex" : "Fragment", header->size, header->parameter_count);

    size_t glsl_capacity = 8192;
    char* glsl_code = malloc(glsl_capacity);
    if (!glsl_code) return NULL;
    glsl_code[0] = '\0';

    strcat(glsl_code, "// Dynamic Translation Output via Project VARM Engine\n");

    // Setup shared text varying bridges for mapping vertex positions to texture coordinates
    strcat(glsl_code, "varying vec2 v_texcoord;\n");
    if (!is_vertex) {
        strcat(glsl_code, "precision mediump float;\n");
    }

    uintptr_t program_base = (uintptr_t)header;
    GxpParameter* parameter_table = (GxpParameter*)(program_base + header->parameter_table_offset);

    // 🛠️ SMART TRACKING: Containers to remember names for runtime code assignment
    char position_var[64] = "";
    char texcoord_var[64] = "";
    char sampler_var[64] = "";

    for (uint32_t i = 0; i < header->parameter_count; i++) {
        GxpParameter param = parameter_table[i];

        const char* param_name = "varm_unnamed";
        if (param.name_offset != 0) {
            param_name = (const char*)(program_base + param.name_offset);
        }

        char declaration_line[256];

        if (param.category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE && is_vertex) {
            snprintf(declaration_line, sizeof(declaration_line),
                     "attribute vec4 %s; // Vita Resource Map ID: %u\n", param_name, param.resource_index);
            strcat(glsl_code, declaration_line);

            // Dynamically intercept position and texture geometry arrays
            if (strstr(param_name, "position") || strstr(param_name, "pos") || param.resource_index == 0) {
                strncpy(position_var, param_name, sizeof(position_var) - 1);
            } else if (strstr(param_name, "texcoord") || strstr(param_name, "uv") || param.resource_index == 1) {
                strncpy(texcoord_var, param_name, sizeof(texcoord_var) - 1);
            }
        }
        else if (param.category == SCE_GXM_PARAMETER_CATEGORY_UNIFORM) {
            snprintf(declaration_line, sizeof(declaration_line),
                     "uniform vec4 %s; // Target Register index: %u\n", param_name, param.resource_index);
            strcat(glsl_code, declaration_line);
        }
        else if (param.category == SCE_GXM_PARAMETER_CATEGORY_SAMPLER && !is_vertex) {
            snprintf(declaration_line, sizeof(declaration_line),
                     "uniform sampler2D %s; // Texture Unit Bind: %u\n", param_name, param.resource_index);
            strcat(glsl_code, declaration_line);
            strncpy(sampler_var, param_name, sizeof(sampler_var) - 1);
        }
    }

    strcat(glsl_code, "\nvoid main() {\n");
    char execution_line[256];

    if (is_vertex) {
        // Direct the vertex execution loop block dynamically
        if (strlen(position_var) > 0) {
            snprintf(execution_line, sizeof(execution_line), "    gl_Position = %s;\n", position_var);
            strcat(glsl_code, execution_line);
        } else {
            strcat(glsl_code, "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n");
        }

        if (strlen(texcoord_var) > 0) {
            snprintf(execution_line, sizeof(execution_line), "    v_texcoord = %s.xy;\n", texcoord_var);
            strcat(glsl_code, execution_line);
        } else {
            strcat(glsl_code, "    v_texcoord = vec2(0.0, 0.0);\n");
        }
    } else {
        // Direct the color sampling pixel block dynamically
        if (strlen(sampler_var) > 0) {
            snprintf(execution_line, sizeof(execution_line), "    gl_FragColor = texture2D(%s, v_texcoord);\n", sampler_var);
            strcat(glsl_code, execution_line);
        } else {
            strcat(glsl_code, "    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0); // Fallback solid tint\n");
        }
    }
    strcat(glsl_code, "}\n");

    return glsl_code;
}
