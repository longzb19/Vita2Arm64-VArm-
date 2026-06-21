#ifndef VARM_CPU_H
#define VARM_CPU_H

#include <stdint.h>

typedef struct {
    uint32_t r[16];    // GPRs: r0 - r12, r13 (SP), r14 (LR), r15 (PC)
    uint32_t cpsr;     // Current Program Status Register
    float    fpscr;    // Floating point status/control
    uint64_t d[32];    // VFP/NEON Extension registers
} Varm_CPU_Context;

extern Varm_CPU_Context g_cpu_regs;

#endif
