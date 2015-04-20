#include "graphics.h"

static Graphics graphics;

static void graphics_create_window();
static void graphics_create_fullscreen_window();
static void graphics_create_windowed_window();
static void graphics_initialize_gl();
static void graphics_destroy_window();

void graphics_initialize() 
{
	graphics_create_window();
	graphics_initialize_gl();
	graphics_clear();
	graphics_flip();
}

void graphics_cleanup() 
{
	graphics_destroy_window();
}

void graphics_resize_window(const unsigned int width,
		const unsigned int height) 
{
	graphics.screen.width = width;
	graphics.screen.height = height;
	glViewport(0, 0, width, height);
}

void graphics_toggle_fullscreen() {
	graphics_cleanup();
	graphics.fullScreen = !graphics.fullScreen;
	graphics_initialize();
}

void graphics_clear() {
	glClear(GL_COLOR_BUFFER_BIT);
}

void graphics_flip() {
	SDL_GL_SwapWindow(graphics.window);
}

void graphics_set_ui_projection() 
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, graphics.screen.width, graphics.screen.height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void graphics_set_world_projection()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, graphics.screen.width, 0, graphics.screen.height, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

static void graphics_create_window() 
{
	if (graphics.fullScreen)
		graphics_create_fullscreen_window();
	else
		graphics_create_windowed_window();
}

static void graphics_create_fullscreen_window() 
{
	graphics.window = SDL_CreateWindow(
		SDL_WINDOW_NAME, 
		SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOW_FULLSCREEN_WIDTH, 
		SDL_WINDOW_FULLSCREEN_HEIGHT, 
		SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN
	);

	graphics.screen.width = SDL_WINDOW_FULLSCREEN_WIDTH;
	graphics.screen.height = SDL_WINDOW_FULLSCREEN_HEIGHT;
}

static void graphics_create_windowed_window() 
{
	graphics.window = SDL_CreateWindow(
        SDL_WINDOW_NAME,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOW_WINDOWED_WIDTH, 
        SDL_WINDOW_WINDOWED_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );

    graphics.screen.width = SDL_WINDOW_WINDOWED_WIDTH;
	graphics.screen.height = SDL_WINDOW_WINDOWED_HEIGHT;
}

static void graphics_initialize_gl()
{
	graphics.glcontext = SDL_GL_CreateContext(graphics.window);

	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0,0,0,1);
	glViewport(0, 0, graphics.screen.width, graphics.screen.height);
}

static void graphics_destroy_window() 
{
	SDL_GL_DeleteContext(graphics.glcontext);
	SDL_DestroyWindow(graphics.window);
}