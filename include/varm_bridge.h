#pragma once
#include <stdint.h>
#include <stddef.h>

// 1. The Virtual ARM Register State Core
typedef struct {
    uint32_t r[13];  // General Purpose Registers r0 - r12
    uint32_t sp;     // r13 Stack Pointer
    uint32_t lr;     // r14 Link Register
    uint32_t pc;     // r15 Program Counter / Entrypoint
    uint32_t cpsr;   // Current Program Status Register
} V_ARMRegisters;

// 2. Master Module Directory Structure inside every Vita binary
typedef struct {
    uint16_t attributes;
    uint8_t major_version;
    uint8_t minor_version;
    char name[27];           // Executable name (e.g., "VitaTester")
    uint8_t type;
    uint32_t gp_value;
    uint32_t ent_top;
    uint32_t ent_end;
    uint32_t stub_top;       // Virtual address offset to where libraries start
    uint32_t stub_end;       // Virtual address offset to where libraries end
} __attribute__((packed)) SceModuleInfo;

// 3. Blueprint for a Vita Import Library block
typedef struct {
    uint16_t struct_size;
    uint16_t version;
    uint16_t flags;
    uint16_t num_functions;
    uint32_t libname_nid;
    uint32_t libname_ptr;
    uint32_t nid_table_ptr;
    uint32_t func_table_ptr;
} __attribute__((packed)) SceLibStubTable;

// Forward declarations for kernel functions
void varm_bridge_setup_env(V_ARMRegisters *regs, uint32_t entrypoint_addr);
void varm_bridge_trigger_syscall(V_ARMRegisters *regs, uint32_t nid);
