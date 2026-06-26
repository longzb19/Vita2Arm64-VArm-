#ifndef VARM_INPUT_H
#define VARM_INPUT_H

#include <stdint.h>
#include <stdbool.h>

// Physical Key Codes (RG35XX H Evdev Configuration Mapping)
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
#define HW_KEY_L3       317
#define HW_KEY_R3       318
#define HW_KEY_SELECT   314
#define HW_KEY_START    315
#define HW_KEY_MENU     139

// Virtual Sony Vita Controller Input Masks
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
    VITA_CTRL_SQUARE      = 0x00008000
} VitaControlMasks;

// Coordinate Mapping Unit
typedef struct {
    uint16_t x;
    uint16_t y;
    bool is_rear; // true = Rear Touch Pad, false = Front Touch Screen
} TouchTarget;

// Active profile storage layout
typedef struct {
    TouchTarget l2;
    TouchTarget r2;
    TouchTarget l3;
    TouchTarget r3;
} VarmTouchProfile;

typedef struct {
    bool front_touch_active;
    uint16_t front_x;
    uint16_t front_y;

    bool rear_touch_active;
    uint16_t rear_x;
    uint16_t rear_y;
} VarmVirtualTouchState;

typedef struct {
    int hw_code;
    uint32_t vita_mask;
    const char* name;
} ControlMap;

// Globally Linked Context Handles
extern bool g_show_menu;
extern int g_input_fd;
extern VarmVirtualTouchState g_virtual_touch;
extern VarmTouchProfile g_active_profile;
extern char g_game_id[32];

// System Control Routines
void varm_input_init_profile(const char* game_path);
void varm_input_save_profile(void);
uint32_t varm_input_get_translated_state(void);

#endif // VARM_INPUT_H
