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
		SDL_WINDOW_FULLSCREEN_WIDTH, 
		SDL_WINDOW_FULLSCREEN_HEIGHT, 
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
        SDL_WINDOW_WINDOWED_WIDTH, 
        SDL_WINDOW_WINDOWED_HEIGHT,
        SDL_WINDOW_OPENGL
    );
}

static void graphics_destroy_window(Graphics *graphics) 
{
	SDL_DestroyWindow(graphics->window);
}