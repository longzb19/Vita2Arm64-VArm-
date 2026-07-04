#ifndef VARM_INPUT_H
#define VARM_INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "varm_menu.h" // Safely provides VarmRuntimeState structure definition globally

// Virtual Sony Vita Controller Input Masks (Matches official SceCtrl constants)
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

// Coordinate Mapping Unit for Virtual Touch Zones
typedef struct {
    uint16_t x;
    uint16_t y;
    bool is_rear; // true = Rear Touch Pad, false = Front Touch Screen
} TouchTarget;

// Active Profile Storage Layout
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

// Structural mapping format to loop through buttons cleanly
typedef struct {
    SDL_GameControllerButton sdl_btn;
    uint32_t vita_mask;
} SdlControlMap;

// Global Exported Interfaces
extern bool g_running;
extern VarmVirtualTouchState g_virtual_touch;
extern VarmTouchProfile g_active_profile;
extern VarmRuntimeState g_varm_state;
extern bool g_show_menu;

// Option C: Global Analog Stick State Variables (Native Vita expects 0 - 255)
extern uint8_t g_stick_lx;
extern uint8_t g_stick_ly;
extern uint8_t g_stick_rx;
extern uint8_t g_stick_ry;

// --- Explicit Subsystem Function Declarations ---
void varm_input_init(void);
uint32_t varm_input_poll(void);
void varm_input_shutdown(void);     // 👈 ADD THIS
void varm_input_save_profile(void); // 👈 ADD THIS
void varm_input_load_profile(void); // 👈 ADD THIS

#endif // VARM_INPUT_H
