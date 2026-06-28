#include "varm_graphics.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

extern char g_game_id[32]; // Links with identifier exported by main execution binary context

static float s_scale_x = 1.0f;
static float s_scale_y = 1.0f;
static int s_target_fps = 30;
static int s_resolution_width = 960;
static int s_resolution_height = 544;

void varm_graphics_init(void) {
    s_scale_x = 1.0f;
    s_scale_y = 1.0f;
    s_target_fps = 30;
    s_resolution_width = 960;
    s_resolution_height = 544;

    printf("[VITAGRAFIX-CORE] Evaluating active engine context profile target...\n");

    // Dynamic file parsing for root folder configuration layout
    FILE *patch_file = fopen("./vitagrafix/patchlist.txt", "r");
    bool match_found = false;

    if (patch_file) {
        char line[256];
        while (fgets(line, sizeof(line), patch_file)) {
            // Check if the line begins with the active Game Serial ID
            if (strncmp(line, g_game_id, strlen(g_game_id)) == 0) {
                int res_w = 960, res_h = 544, fps = 30;

                // Expected text line format inside patchlist.txt: [GameID] [Width] [Height] [FPS]
                // Example: PCSA00126 640 480 60
                if (sscanf(line, "%*s %d %d %d", &res_w, &res_h, &fps) >= 3) {
                    s_resolution_width = res_w;
                    s_resolution_height = res_h;
                    s_target_fps = fps;

                    // Compute baseline fractional scaling factor bounds
                    s_scale_x = (float)res_w / 960.0f;
                    s_scale_y = (float)res_h / 544.0f;

                    // Force-fit traditional definitions for standard 4:3 panel setups
                    if (res_w == 640 && res_h == 480) {
                        s_scale_x = 0.666f;
                        s_scale_y = 0.882f;
                    }

                    printf("[VITAGRAFIX-CORE] Dynamic Profile Found in patchlist.txt: Width=%d, Height=%d, FPS=%d\n", res_w, res_h, fps);
                    match_found = true;
                    break;
                }
            }
        }
        fclose(patch_file);
    }

    if (!match_found) {
        // Safe hardcoded profile fallback logic if text map isn't filled out
        if (strncmp(g_game_id, "PCSA00126", 9) == 0) {
            s_scale_x = 0.666f;
            s_scale_y = 0.882f;
            s_resolution_width = 640;
            s_resolution_height = 480;
            s_target_fps = 60;
            printf("[VITAGRAFIX-CORE] Fallback Profile Applied: God of War (60FPS, 4:3 Screen Downsampling)\n");
        } else {
            printf("[VITAGRAFIX-CORE] Warning: No entry found in patchlist.txt for '%s'. Defaulting to 1:1 parameters.\n", g_game_id);
        }
    }
}

void varm_graphics_configure(void) {
    printf("\n[GXM-BRIDGE] Toggling Hardware Resolution Bounds...\n");
    if (s_scale_x == 1.0f) {
        s_scale_x = 0.666f;
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
