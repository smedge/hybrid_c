#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "screen.h"
#include "mat4.h"
#include "shader.h"
#include "batch.h"
#include "text.h"

#define SDL_WINDOW_NAME "> - - - #"
#define SDL_WINDOW_FULLSCREEN false
#define SDL_WINDOW_WINDOWED_WIDTH 800
#define SDL_WINDOW_WINDOWED_HEIGHT 600
#define SDL_WINDOW_FULLSCREEN_WIDTH 2560
#define SDL_WINDOW_FULLSCREEN_HEIGHT 1600

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
const Screen Graphics_get_screen(void);
Mat4 Graphics_get_ui_projection(void);
Mat4 Graphics_get_world_projection(void);
Shaders *Graphics_get_shaders(void);
BatchRenderer *Graphics_get_batch(void);
TextRenderer *Graphics_get_text_renderer(void);
TextRenderer *Graphics_get_title_text_renderer(void);

#endif
