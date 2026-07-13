#ifndef VARM_GRAPHICS_H
#define VARM_GRAPHICS_H

#include <SDL2/SDL.h>

extern SDL_Window *g_window;
extern SDL_GLContext g_gl_context;

void varm_graphics_init(void);
void varm_graphics_configure(void);
void varm_graphics_get_scale(float *x, float *y);
void varm_graphics_shutdown(void);

#endif // VARM_GRAPHICS_H
