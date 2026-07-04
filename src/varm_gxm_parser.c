#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "varm_gxm_backend.h"
#include "hle_kernel.h"

#define GXM_CMD_SET_VERTEX_STREAM   0x01
#define GXM_CMD_SET_TEXTURE         0x02
#define GXM_CMD_DRAW_PRIMITIVES     0x03
#define GXM_CMD_SET_VERTEX_PROGRAM   0x04
#define GXM_CMD_SET_FRAGMENT_PROGRAM 0x05

typedef struct {
    uint32_t command_id;
    uint32_t data_vaddr;
    uint32_t count;
    uint32_t parameter;
} GxmCommandPacket;

extern V_GxmRendererInterface gxm_interface;

static uint32_t s_current_vertex_vaddr = 0;
static uint32_t s_current_vertex_stride = 0;
static uint32_t s_current_texture_vaddr = 0;
static uint32_t s_current_vshader_vaddr = 0;
static uint32_t s_current_fshader_vaddr = 0;

int varm_gxm_parse_command_buffer(uint32_t buffer_vaddr, uint32_t buffer_size) {
    if (buffer_vaddr == 0 || buffer_size < sizeof(GxmCommandPacket)) {
        return -1;
    }

    void* resolved_addr = hle_kernel_resolve_address(buffer_vaddr, 1);
    if (!resolved_addr) {
        printf("[GXM-PARSER] KERNEL FAULT: Invalid command buffer address 0x%08X\n", buffer_vaddr);
        return -1;
    }

    uint32_t processed_bytes = 0;
    while (processed_bytes + sizeof(GxmCommandPacket) <= buffer_size) {
        GxmCommandPacket *packet = (GxmCommandPacket*)((uint8_t*)resolved_addr + processed_bytes);

        switch (packet->command_id) {
            case GXM_CMD_SET_VERTEX_STREAM:
                s_current_vertex_vaddr = packet->data_vaddr;
                s_current_vertex_stride = packet->parameter;
                break;

            case GXM_CMD_SET_TEXTURE:
                s_current_texture_vaddr = packet->data_vaddr;
                break;

            case GXM_CMD_SET_VERTEX_PROGRAM:
                s_current_vshader_vaddr = packet->data_vaddr;
                if (gxm_interface.bind_program) {
                    gxm_interface.bind_program(s_current_vshader_vaddr, s_current_fshader_vaddr);
                }
                break;

            case GXM_CMD_SET_FRAGMENT_PROGRAM:
                s_current_fshader_vaddr = packet->data_vaddr;
                if (gxm_interface.bind_program) {
                    gxm_interface.bind_program(s_current_vshader_vaddr, s_current_fshader_vaddr);
                }
                break;

            case GXM_CMD_DRAW_PRIMITIVES: {
                void* host_vertex_ptr = hle_kernel_resolve_address(s_current_vertex_vaddr, 1);
                void* host_texture_ptr = hle_kernel_resolve_address(s_current_texture_vaddr, 1);

                if (gxm_interface.draw_primitives) {
                    gxm_interface.draw_primitives(host_vertex_ptr, s_current_vertex_stride,
                                                   host_texture_ptr, packet->parameter, packet->count);
                }
                break;
            }

            default:
                break;
        }
        processed_bytes += sizeof(GxmCommandPacket);
    }
    return 0;
}
