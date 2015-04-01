#include "graphics.h"

void graphics_initialize(Graphics *graphics) 
{
	graphics->fullScreen = false;
	graphics_create_window(graphics);
}

void graphics_cleanup(Graphics *graphics) 
{
	graphics_destroy_window(graphics);
}

static void graphics_create_window(Graphics *graphics) 
{
	if (graphics->fullScreen)
		graphics_create_fullscreen_window(graphics);
	else
		graphics_create_windowed_window(graphics);
}

static void graphics_create_fullscreen_window(Graphics *graphics) 
{
	graphics->window = SDL_CreateWindow(
		SDL_WINDOW_NAME, 
		SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOWPOS_UNDEFINED, 
		1366, 768, 
		SDL_WINDOW_OPENGL | 
			SDL_WINDOW_FULLSCREEN
	);
}

static void graphics_create_windowed_window(Graphics *graphics) 
{
	graphics->window = SDL_CreateWindow(
        SDL_WINDOW_NAME,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_OPENGL
    );
}

void graphics_destroy_window(Graphics *graphics) 
{
	SDL_DestroyWindow(graphics->window);
}