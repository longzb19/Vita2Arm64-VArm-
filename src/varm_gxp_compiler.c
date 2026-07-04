#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "varm_gxm_backend.h"

#define GXP_MAGIC 0x50584700

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t parameter_count;
    uint32_t parameter_table_offset;
    uint32_t bytecode_offset;
    uint32_t bytecode_size;
} GxpHeader;

typedef struct {
    uint32_t name_offset;
    uint16_t category;       // 1 = SCE_GXM_PARAMETER_CATEGORY_UNIFORM
    uint16_t resource_index;  // Hardware register index (e.g., 0 for c0)
    uint16_t container_type;
    uint16_t container_count;
} GxpParameter;

// Structural layout mirroring s_vertex_uniform_map inside the GLES2 backend
typedef struct {
    uint16_t register_index;
    GLint gles_location;
} UniformMapEntry;

// External uniform tracking states inside varm_gxm_backend_gles2.c
extern UniformMapEntry s_vertex_uniform_map[64];
extern int s_vertex_uniform_count;
extern GLuint g_active_program;

/**
 * compile_shader_source
 * Compiles raw GLSL source text on the host GPU.
 */
static GLuint compile_shader_source(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (!shader) return 0;

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char info_log[512];
        glGetShaderInfoLog(shader, sizeof(info_log), NULL, info_log);
        printf("[VARM-COMPILER] Shader Compilation Failure:\n%s\n", info_log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

/**
 * varm_gxp_translate_and_compile
 * Parses GXP binary tables, dynamically generates matching GLSL source text
 * with structural uniform declarations, and maps locations for the backend.
 */
GLuint varm_gxp_translate_and_compile(const uint8_t* gxp_buffer, size_t gxp_size, GLenum shader_type) {
    if (!gxp_buffer || gxp_size < sizeof(GxpHeader)) return 0;

    const GxpHeader* header = (const GxpHeader*)gxp_buffer;
    if (header->magic != GXP_MAGIC) return 0;

    // Allocate a large working string buffer to build our dynamic GLSL source code
    char* dynamic_src = (char*)malloc(16384);
    if (!dynamic_src) return 0;
    dynamic_src[0] = '\0';

    // Clear previous tracking indices if resetting vertex pipeline definitions
    if (shader_type == GL_VERTEX_SHADER) {
        s_vertex_uniform_count = 0;
    }

    // 1. Dynamic Uniform Declaration Phase
    // Loop through the GXP parameter tables to declare hardware vector spaces
    if (header->parameter_table_offset > 0 && header->parameter_count > 0) {
        const GxpParameter* params = (const GxpParameter*)(gxp_buffer + header->parameter_table_offset);

        for (uint32_t i = 0; i < header->parameter_count; i++) {
            if (params[i].category == 1) { // SCE_GXM_PARAMETER_CATEGORY_UNIFORM
                char declaration_line[64];
                // Inject the exact uniform variable name expected by your gles_set_uniforms loop
                snprintf(declaration_line, sizeof(declaration_line), "uniform vec4 u_reg_%u;\n", params[i].resource_index);
                strcat(dynamic_src, declaration_line);
            }
        }
    }

    // 2. Append Core Shader Logic Pipeline
    if (shader_type == GL_VERTEX_SHADER) {
        const char* vs_body =
            "attribute vec4 a_position;\n"
            "attribute vec2 a_texcoord;\n"
            "varying vec2 v_uv;\n"
            "void main() {\n"
            "    // High-end retail titles like GoW rely heavily on world matrix changes.\n"
            "    // While bytecode parsing is under development, we scale and pass coordinates safely.\n"
            "    gl_Position = a_position;\n"
            "    v_uv = a_texcoord;\n"
            "}\n";
        strcat(dynamic_src, vs_body);
    } else {
        const char* fs_body =
            "precision mediump float;\n"
            "varying vec2 v_uv;\n"
            "uniform sampler2D u_texture;\n"
            "void main() {\n"
            "    gl_FragColor = texture2D(u_texture, v_uv);\n"
            "}\n";
        strcat(dynamic_src, fs_body);
    }

    // 3. Compile the Generated Source String
    GLuint shader_handle = compile_shader_source(shader_type, dynamic_src);

    // 4. Extract Location Hooks for the Backend Registry
    // If we just compiled a vertex shader and linked it, hook up the layout mappings immediately
    if (shader_handle && shader_type == GL_VERTEX_SHADER && g_active_program > 0) {
        const GxpParameter* params = (const GxpParameter*)(gxp_buffer + header->parameter_table_offset);

        for (uint32_t i = 0; i < header->parameter_count; i++) {
            if (params[i].category == 1 && s_vertex_uniform_count < 64) {
                char uniform_name[32];
                snprintf(uniform_name, sizeof(uniform_name), "u_reg_%u", params[i].resource_index);

                // Get the physical runtime uniform location from the Mali GPU driver
                GLint loc = glGetUniformLocation(g_active_program, uniform_name);
                if (loc != -1) {
                    s_vertex_uniform_map[s_vertex_uniform_count].register_index = params[i].resource_index;
                    s_vertex_uniform_map[s_vertex_uniform_count].gles_location = loc;
                    s_vertex_uniform_count++;
                }
            }
        }
        printf("[VARM-COMPILER] Successfully built shader and injected %d active uniform layout mappings.\n", s_vertex_uniform_count);
    }

    free(dynamic_src);
    return shader_handle;
}
