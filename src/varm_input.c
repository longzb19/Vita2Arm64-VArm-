// src/varm_input.c
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

void varm_input_init() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~ICANON; // Disable buffering
    t.c_lflag &= ~ECHO;   // Disable echoing
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

int varm_input_poll() {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) > 0) {
        if (ch == 27) { // ANSI escape sequences (Arrow keys)
            read(STDIN_FILENO, &ch, 1);
            read(STDIN_FILENO, &ch, 1);
            if (ch == 'A') return 1; // UP
            if (ch == 'B') return 2; // DOWN
        }
        if (ch == 10) return 3; // ENTER
    }
    return 0; // No input
}
