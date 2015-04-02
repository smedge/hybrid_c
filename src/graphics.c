#include "graphics.h"

static Graphics graphics;

void graphics_initialize() 
{
	graphics.fullScreen = false;
	graphics_create_window();
}

void graphics_cleanup() 
{
	graphics_destroy_window();
}

static void graphics_create_window() 
{
	if (graphics.fullScreen)
		graphics_create_fullscreen_window();
	else
		graphics_create_windowed_window();

	graphics_initialize_gl();
}

static void graphics_create_fullscreen_window() 
{
	graphics.window = SDL_CreateWindow(
		SDL_WINDOW_NAME, 
		SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOW_FULLSCREEN_WIDTH, 
		SDL_WINDOW_FULLSCREEN_HEIGHT, 
		SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN
	);
}

static void graphics_create_windowed_window() 
{
	graphics.window = SDL_CreateWindow(
        SDL_WINDOW_NAME,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOW_WINDOWED_WIDTH, 
        SDL_WINDOW_WINDOWED_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
}

static void graphics_initialize_gl()
{
	graphics.glcontext = SDL_GL_CreateContext(graphics.window);

	//initializeMultisampling();

	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glClearColor(0,0,0,1);
}

static void graphics_destroy_window() 
{
	SDL_GL_DeleteContext(graphics.glcontext);
	SDL_DestroyWindow(graphics.window);
}