#ifndef VARM_INPUT_H
#define VARM_INPUT_H

// RG35XX H Physical Hardware Linux Evdev Key Codes
#define HW_KEY_UP       103
#define HW_KEY_DOWN     108
#define HW_KEY_LEFT     105
#define HW_KEY_RIGHT    106
#define HW_KEY_A        304
#define HW_KEY_B        305
#define HW_KEY_X        307
#define HW_KEY_Y        308
#define HW_KEY_L1       310
#define HW_KEY_R1       311
#define HW_KEY_L2       312
#define HW_KEY_R2       313
#define HW_KEY_SELECT   314
#define HW_KEY_START    315
#define HW_KEY_MENU     139

// Virtual Sony PlayStation Vita Controller Input Masks
typedef enum {
    VITA_CTRL_SELECT      = 0x00000001,
    VITA_CTRL_START       = 0x00000008,
    VITA_CTRL_UP          = 0x00000010,
    VITA_CTRL_RIGHT       = 0x00000020,
    VITA_CTRL_DOWN        = 0x00000040,
    VITA_CTRL_LEFT        = 0x00000080,
    VITA_CTRL_LTRIGGER    = 0x00000100,
    VITA_CTRL_RTRIGGER    = 0x00000200,
    VITA_CTRL_TRIANGLE    = 0x00001000,
    VITA_CTRL_CIRCLE      = 0x00002000,
    VITA_CTRL_CROSS       = 0x00004000,
    VITA_CTRL_SQUARE      = 0x00008000,
} VitaButtons;

typedef struct {
    int physical_code;       // Handheld key
    unsigned int vita_mask;  // Vita Translated value
    const char* button_name; // Debug string
} ControlMap;

void varm_input_init(void);
unsigned int varm_input_get_translated_state(void);

#endif
