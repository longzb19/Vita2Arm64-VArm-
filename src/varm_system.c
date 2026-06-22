#include "varm_system.h"
#include <stdio.h>

static int s_cpu_mhz = 500;
static int s_gpu_mhz = 222;

void varm_system_init(void) {
    s_cpu_mhz = 500;
    s_gpu_mhz = 222;
}

void varm_system_set_cpu_clock(int mhz) {
    s_cpu_mhz = mhz;
    printf("[SYSTEM] Internal CPU operational frequency cap set to %d MHz.\n", s_cpu_mhz);
}

void varm_system_set_gpu_clock(int mhz) {
    s_gpu_mhz = mhz;
    printf("[SYSTEM] Internal GPU operational frequency cap set to %d MHz.\n", s_gpu_mhz);
}

int varm_system_get_cpu_clock(void) {
    return s_cpu_mhz;
}

int varm_system_get_gpu_clock(void) {
    return s_gpu_mhz;
}
