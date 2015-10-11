#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "screen.h"

#define SDL_WINDOW_NAME "> - - - #"
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

void graphics_initialize(void);
void graphics_cleanup(void);
void graphics_resize_window(const unsigned int width,
							const unsigned int height);
void graphics_toggle_fullscreen(void);
void graphics_clear(void);
void graphics_flip(void);
void graphics_set_ui_projection(void);
void graphics_set_world_projection(void);
const Screen graphics_get_screen(void);

#endif