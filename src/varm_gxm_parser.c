#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "varm_gxm_backend.h"
#include "hle_kernel.h"

#define GXM_CMD_SET_VERTEX_STREAM   0x01
#define GXM_CMD_SET_TEXTURE         0x02
#define GXM_CMD_DRAW_PRIMITIVES     0x03

typedef struct {
    uint32_t command_id;
    uint32_t data_vaddr;
    uint32_t count;
    uint32_t parameter;
} GxmCommandPacket;

extern V_GxmRendererInterface gxm_interface;

int varm_gxm_parse_command_buffer(uint32_t buffer_vaddr, uint32_t buffer_size) {
    if (buffer_vaddr == 0 || buffer_size < sizeof(GxmCommandPacket)) {
        return -1;
    }

    uint32_t resolved_addr = hle_kernel_resolve_address(buffer_vaddr, 1);
    if (!resolved_addr) {
        printf("[GXM-PARSER] KERNEL FAULT: Invalid command buffer address 0x%08X\n", buffer_vaddr);
        return -1;
    }

    GxmCommandPacket *packet_stream = (GxmCommandPacket*)((uintptr_t)resolved_addr);
    uint32_t max_packets = buffer_size / sizeof(GxmCommandPacket);

    printf("[GXM-PARSER] Beginning processing pass of %d hardware command streams...\n", max_packets);

    for (uint32_t i = 0; i < max_packets; i++) {
        GxmCommandPacket *packet = &packet_stream[i];

        if (packet->command_id == 0x00) break;

        switch (packet->command_id) {
            case GXM_CMD_SET_VERTEX_STREAM:
                printf("[GXM-PARSER] Translating Vertex Stream Array (Address: 0x%08X, Stride: %d bytes)\n",
                        packet->data_vaddr, packet->parameter);
                break;

            case GXM_CMD_SET_TEXTURE:
                printf("[GXM-PARSER] Mapping Vita VRAM Texture Reference (Address: 0x%08X, Format Block: 0x%X)\n",
                        packet->data_vaddr, packet->parameter);
                break;

            case GXM_CMD_DRAW_PRIMITIVES:
                printf("[GXM-PARSER] Intercepted Draw Token! Topology Mode: %d | Index Count: %d\n",
                        packet->parameter, packet->count);

                if (gxm_interface.submit_command_buffer) {
                    gxm_interface.submit_command_buffer(packet->data_vaddr, packet->count);
                }
                break;

            default:
                printf("[GXM-PARSER] Warning: Skipping unrecognized Vita GXM operational packet ID: 0x%X\n", packet->command_id);
                break;
        }
    }

    // FIXED: REMOVED gxm_interface.swap_buffers() from here!
    // Let SceDisplay manage display synchronization instead.

    return 0;
}
