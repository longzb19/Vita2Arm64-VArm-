#ifndef VARM_CTRL_H
#define VARM_CTRL_H

#include <stdint.h>

// Native PlayStation Vita Button Mask Definitions
#define SCE_CTRL_SELECT      0x00000001
#define SCE_CTRL_START       0x00000008
#define SCE_CTRL_UP          0x00000010
#define SCE_CTRL_RIGHT       0x00000020
#define SCE_CTRL_DOWN        0x00000040
#define SCE_CTRL_LEFT        0x00000080
#define SCE_CTRL_LTRIGGER    0x00000100
#define SCE_CTRL_RTRIGGER    0x00000200
#define SCE_CTRL_TRIANGLE    0x00001000
#define SCE_CTRL_CIRCLE      0x00002000
#define SCE_CTRL_CROSS       0x00004000
#define SCE_CTRL_SQUARE      0x00008000

// Struct matching the exact memory layout the game expects when querying input
typedef struct {
    uint32_t timeStamp;
    uint32_t buttons;
    uint8_t  leftX;
    uint8_t  leftY;
    uint8_t  rightX;
    uint8_t  rightY;
    uint8_t  reserved[4];
} SceCtrlData;

// Core HLE function hooks to be called by the binary translation layer
int varm_ctrl_init(void);
int varm_ctrl_peek_buffer_positive(int port, SceCtrlData *pad_data, int count);

#endif // VARM_CTRL_H
