#include "varm_graphics.h"
#include <stdio.h>

static float s_scale_x = 1.0f;
static float s_scale_y = 1.0f;

void varm_graphics_init(void) {
    s_scale_x = 1.0f;
    s_scale_y = 1.0f;
}

void varm_graphics_configure(void) {
    printf("\n[GXM-BRIDGE] Toggling Hardware Resolution Bounds...\n");
    if (s_scale_x == 1.0f) {
        s_scale_x = 0.666f; // Fit original 960x544 geometry down into 4:3 640x480 frame viewports
        s_scale_y = 0.882f;
        printf("[GXM-BRIDGE] Resolution scaling altered for optimized 4:3 Handheld display panels.\n");
    } else {
        s_scale_x = 1.0f;
        s_scale_y = 1.0f;
        printf("[GXM-BRIDGE] Resolution scaling restored to 1:1 native limits.\n");
    }
}

void varm_graphics_get_scale(float *x, float *y) {
    if (x) *x = s_scale_x;
    if (y) *y = s_scale_y;
}
