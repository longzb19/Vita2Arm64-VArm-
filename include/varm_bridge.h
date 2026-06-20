/* Foundations for the Vita2Arm Translation Core Bridge */
#pragma once
#include <stdint.h>

// CPU context structure tracking the 32-bit ARM hardware registers
typedef struct {
    uint32_t r[16];     // General Purpose Registers R0 - R15
                        // R13 = Stack Pointer (SP)
                        // R14 = Link Register (LR)
                        // R15 = Program Counter (PC)
    uint32_t cpsr;      // Current Program Status Register
} V_ARMRegisters;

// Initializes the virtual execution state of the game binary
void varm_bridge_setup_env(V_ARMRegisters *regs, uint32_t entrypoint_addr);

// Triggers an interception when the game calls a Vita OS native NID function
void varm_bridge_trigger_syscall(V_ARMRegisters *regs, uint32_t nid);
