// Inside your src/varm_input.c
#include "varm_input.h"
#include <stdbool.h>
#include <time.h>

bool g_show_menu = false; // Default to FALSE so we boot straight to the game
static time_t menu_button_held_since = 0;

void update_varm_input() {
    // Poll your /dev/input/eventX here
    // Let's assume your evdev struct gives you a 'current_key' state

    if (current_key == VITA_KEY_MENU) { // Replace with your actual menu button define
        if (menu_button_held_since == 0) {
            menu_button_held_since = time(NULL); // Start the timer
        } else if (difftime(time(NULL), menu_button_held_since) >= 1.5) { // Held for 1.5 seconds
            g_show_menu = !g_show_menu; // Toggle the menu on/off
            menu_button_held_since = 0; // Reset timer so it doesn't flicker
        }
    } else {
        menu_button_held_since = 0; // Reset timer if the button is released early
    }
}
