#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "screen.h"

#define SDL_WINDOW_NAME "> - - - #"
#define SDL_WINDOW_FULLSCREEN true 
#define SDL_WINDOW_WINDOWED_WIDTH 640
#define SDL_WINDOW_WINDOWED_HEIGHT 480
#define SDL_WINDOW_FULLSCREEN_WIDTH 1366 
#define SDL_WINDOW_FULLSCREEN_HEIGHT 768

typedef struct {
	SDL_Window *window;
	bool fullScreen;
	SDL_GLContext glcontext;
	Screen screen;
} Graphics;

void Graphics_initialize(void);
void Graphics_cleanup(void);
void Graphics_resize_window(const unsigned int width,
							const unsigned int height);
void Graphics_toggle_fullscreen(void);
void Graphics_clear(void);
void Graphics_flip(void);
void Graphics_set_ui_projection(void);
void Graphics_set_world_projection(void);
const Screen Graphics_get_screen(void);

#endif
