#include "varm_cheats.h"
#include "hle_kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define VITA_USER_BASE_VADDR 0x81000000

typedef struct {
    char name[64];
    uint32_t offset;
    uint32_t value;
    int type; // 1 = 8-bit, 2 = 16-bit, 4 = 32-bit
    int enabled;
} VarmCheatCode;

static VarmCheatCode s_cheat_db[128];
static int s_cheat_count = 0;

void varm_cheats_init(void) {
    s_cheat_count = 0;
    memset(s_cheat_db, 0, sizeof(s_cheat_db));
    printf("[VARM CHEAT] Subsystem initialized. Parsed baseline offset: 0x%08X\n", VITA_USER_BASE_VADDR);
}

// Parses standard r0ah/vitacheat syntax lines natively (e.g., "_V0 0x0001A2C0 0x0000FFFF")
int varm_cheats_parse_line(const char *line) {
    if (!line || s_cheat_count >= 128) return -1;

    // Skip empty lines or comment tags
    if (line[0] == '#' || line[0] == '\0' || line[0] == '\n') return 0;

    char token[8];
    uint32_t raw_offset = 0;
    uint32_t target_value = 0;

    // Scan standard cheat format string sequences
    if (sscanf(line, "%7s 0x%X 0x%X", token, &raw_offset, &target_value) == 3) {
        if (strcmp(token, "_V0") == 0) {
            s_cheat_db[s_cheat_count].offset = raw_offset;
            s_cheat_db[s_cheat_count].value = target_value;
            s_cheat_db[s_cheat_count].type = 4; // Default to 32-bit memory word alteration
            s_cheat_db[s_cheat_count].enabled = 1;
            strcpy(s_cheat_db[s_cheat_count].name, "Project VArm Code Injector");
            s_cheat_count++;
            return 1;
        }
    }
    return 0;
}

// Scans memory segments and locks targeted values down in memory
void varm_cheats_inject(void) {
    if (s_cheat_count == 0) return;

    int active_injections = 0;

    for (int i = 0; i < s_cheat_count; i++) {
        if (!s_cheat_db[i].enabled) continue;

        // Compute memory footprint location relative to the game runtime application boundaries
        uint32_t runtime_vaddr = VITA_USER_BASE_VADDR + s_cheat_db[i].offset;

        // Verify the memory layout addresses safely via core MMU segment lookup tables
        uint32_t resolved_host_ptr = hle_kernel_resolve_address(runtime_vaddr, 2); // 2 = Write permissions required

        if (resolved_host_ptr) {
            // Write memory data word into game address space
            volatile uint32_t *dest = (volatile uint32_t*)((uintptr_t)resolved_host_ptr);
            *dest = s_cheat_db[i].value;
            active_injections++;
        }
    }

    if (active_injections > 0) {
        printf("[VARM CHEAT] Dynamic Injection: Cleanly maintained %d live memory patches.\n", active_injections);
    }
}
