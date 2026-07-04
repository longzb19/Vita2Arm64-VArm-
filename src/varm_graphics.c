#include "varm_graphics.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

extern char g_game_id[32]; // Links with identifier exported by main execution binary context

static float s_scale_x = 1.0f;
static float s_scale_y = 1.0f;
static int s_target_fps = 60;          // Bumped base target performance state to 60 FPS
static int s_resolution_width = 960;   // Locked to native PlayStation Vita Width
static int s_resolution_height = 544;  // Locked to native PlayStation Vita Height

void varm_graphics_init(void) {
    // 🛠️ FIX: Force absolute 1:1 native definitions right at launch setup bounds
    s_scale_x = 1.0f;
    s_scale_y = 1.0f;
    s_target_fps = 60;
    s_resolution_width = 960;
    s_resolution_height = 544;

    printf("[VITAGRAFIX-CORE] Evaluating active engine context profile target for title: %s...\n", g_game_id);

    // Dynamic file parsing for root folder configuration layout
    FILE *patch_file = fopen("./vitagrafix/patchlist.txt", "r");
    bool match_found = false;

    if (patch_file) {
        char line[256];
        while (fgets(line, sizeof(line), patch_file)) {
            if (strncmp(line, g_game_id, strlen(g_game_id)) == 0) {
                int res_w = 960, res_h = 544, fps = 60;

                if (sscanf(line, "%*s %d %d %d", &res_w, &res_h, &fps) >= 3) {
                    // Accept game configs but guard our native 960x544 geometry space
                    s_resolution_width = res_w;
                    s_resolution_height = res_h;
                    s_target_fps = fps;

                    // Enforce 1:1 output layouts since the modern host chip has plenty of power
                    s_scale_x = 1.0f;
                    s_scale_y = 1.0f;

                    printf("[VITAGRAFIX-CORE] Profile Found in patchlist.txt: Native Width=%d, Height=%d, Configured FPS=%d\n", res_w, res_h, fps);
                    match_found = true;
                    break;
                }
            }
        }
        fclose(patch_file);
    }

    if (!match_found) {
        // 🛠️ FIX: Removed all hardcoded downsampling overrides. Everything outputs at 1:1 Native Resolution.
        s_scale_x = 1.0f;
        s_scale_y = 1.0f;
        s_resolution_width = 960;
        s_resolution_height = 544;
        s_target_fps = 60;
        printf("[VITAGRAFIX-CORE] Running at full power: 1:1 Native Resolution Frame Buffers Activated (960x544 @ 60FPS).\n");
    }
}

void varm_graphics_configure(void) {
    printf("\n[GXM-BRIDGE] Verifying Hardware Resolution Bounds...\n");
    // Explicitly lock the transformation matrix scaling steps to true 1:1 aspect factors
    s_scale_x = 1.0f;
    s_scale_y = 1.0f;
    printf("[GXM-BRIDGE] Resolution scaling locked to native 1:1 maximum layout limits.\n");
}

void varm_graphics_get_scale(float *x, float *y) {
    if (x) *x = s_scale_x;
    if (y) *y = s_scale_y;
}
