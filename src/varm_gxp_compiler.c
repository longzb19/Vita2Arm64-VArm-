#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "varm_gxm_backend.h"
#include "varm_gxp_shader.h"

// External uniform tracking states inside varm_gxm_backend_gles2.c
extern UniformMapEntry s_vertex_uniform_map[64];
extern int s_vertex_uniform_count;
extern GLuint g_active_program;

// 🛠️ FIX: Explicitly linked to dynamic GLES function pointers to prevent build failures
extern GLuint (*gl_create_shader_ptr)(GLenum type);
extern void   (*gl_shader_source_ptr)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
extern void   (*gl_compile_shader_ptr)(GLuint shader);
extern void   (*gl_get_shader_iv_ptr)(GLuint shader, GLenum pname, GLint* params);
extern void   (*gl_get_shader_info_log_ptr)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
extern void   (*gl_delete_shader_ptr)(GLuint shader);
extern GLint  (*gl_get_uniform_location_ptr)(GLuint program, const GLchar* name);

static GLuint compile_shader_source(GLenum type, const char* source) {
    if (!gl_create_shader_ptr) return 0;

    GLuint shader = gl_create_shader_ptr(type);
    if (!shader) return 0;

    gl_shader_source_ptr(shader, 1, &source, NULL);
    gl_compile_shader_ptr(shader);

    GLint compiled = 0;
    gl_get_shader_iv_ptr(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char info_log[512];
        gl_get_shader_info_log_ptr(shader, sizeof(info_log), NULL, info_log);
        printf("[VARM-COMPILER] Shader Compilation Failure:\n%s\n", info_log);
        if (gl_delete_shader_ptr) gl_delete_shader_ptr(shader);
        return 0;
    }
    return shader;
}

GLuint varm_gxp_translate_and_compile(const uint8_t* gxp_buffer, size_t gxp_size, GLenum shader_type) {
    if (!gxp_buffer || gxp_size < sizeof(GxpHeader)) return 0;

    const GxpHeader* header = (const GxpHeader*)gxp_buffer;
    if (header->magic != GXP_MAGIC) return 0;

    char* dynamic_src = (char*)malloc(16384);
    if (!dynamic_src) return 0;
    dynamic_src[0] = '\0';

    if (shader_type == GL_VERTEX_SHADER) {
        s_vertex_uniform_count = 0;
    }

    if (header->parameter_table_offset > 0 && header->parameter_count > 0) {
        const GxpParameter* params = (const GxpParameter*)(gxp_buffer + header->parameter_table_offset);

        for (uint32_t i = 0; i < header->parameter_count; i++) {
            if (params[i].category == SCE_GXM_PARAMETER_CATEGORY_UNIFORM) {
                char declaration_line[64];
                snprintf(declaration_line, sizeof(declaration_line), "uniform vec4 u_reg_%u;\n", params[i].resource_index);
                strcat(dynamic_src, declaration_line);
            }
        }
    }

    if (shader_type == GL_VERTEX_SHADER) {
        const char* vs_body =
            "attribute vec4 a_position;\n"
            "attribute vec2 a_texcoord;\n"
            "varying vec2 v_uv;\n"
            "void main() {\n"
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

    GLuint shader_handle = compile_shader_source(shader_type, dynamic_src);

    if (shader_handle && shader_type == GL_VERTEX_SHADER && g_active_program > 0 && gl_get_uniform_location_ptr) {
        const GxpParameter* params = (const GxpParameter*)(gxp_buffer + header->parameter_table_offset);

        for (uint32_t i = 0; i < header->parameter_count; i++) {
            if (params[i].category == SCE_GXM_PARAMETER_CATEGORY_UNIFORM && s_vertex_uniform_count < 64) {
                char uniform_name[32];
                snprintf(uniform_name, sizeof(uniform_name), "u_reg_%u", params[i].resource_index);

                GLint loc = gl_get_uniform_location_ptr(g_active_program, uniform_name);
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
