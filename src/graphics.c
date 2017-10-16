#include "graphics.h"

#include <SDL2/SDL_opengl.h>

static Graphics graphics = {0, false};

static void create_window(void);
static void create_fullscreen_window(void);
static void create_windowed_window(void);
static void initialize_gl(void);
static void destroy_window(void);

void Graphics_initialize(void) 
{
	create_window();
	initialize_gl();
	Graphics_clear();
	Graphics_flip();
}

void Graphics_cleanup(void) 
{
	destroy_window();
}

void Graphics_resize_window(const unsigned int width,
		const unsigned int height) 
{
	graphics.screen.width = width;
	graphics.screen.height = height;
	glViewport(0, 0, width, height);
}

void Graphics_toggle_fullscreen(void) 
{
	Graphics_cleanup();
	graphics.fullScreen = !graphics.fullScreen;
	Graphics_initialize();
}

void Graphics_clear(void) 
{
	glClear(GL_COLOR_BUFFER_BIT);
}

void Graphics_flip(void) 
{
	SDL_GL_SwapWindow(graphics.window);
}

void Graphics_set_ui_projection(void) 
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, graphics.screen.width, graphics.screen.height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void Graphics_set_world_projection(void)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, graphics.screen.width, 0, graphics.screen.height, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

const Screen Graphics_get_screen(void)
{
	return graphics.screen;
}

static void create_window(void) 
{
	if (graphics.fullScreen)
		create_fullscreen_window();
	else
		create_windowed_window();
}

static void create_fullscreen_window(void) 
{
	graphics.window = SDL_CreateWindow(
		SDL_WINDOW_NAME, 
		SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOWPOS_UNDEFINED, 
		SDL_WINDOW_FULLSCREEN_WIDTH, 
		SDL_WINDOW_FULLSCREEN_HEIGHT, 
		SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | 
			SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN
	);

	graphics.screen.width = SDL_WINDOW_FULLSCREEN_WIDTH;
	graphics.screen.height = SDL_WINDOW_FULLSCREEN_HEIGHT;
}

static void create_windowed_window(void) 
{
	graphics.window = SDL_CreateWindow(
        SDL_WINDOW_NAME,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOW_WINDOWED_WIDTH, 
        SDL_WINDOW_WINDOWED_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | 
        	SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );

    graphics.screen.width = SDL_WINDOW_WINDOWED_WIDTH;
	graphics.screen.height = SDL_WINDOW_WINDOWED_HEIGHT;
}

static void initialize_gl(void)
{
	graphics.glcontext = SDL_GL_CreateContext(graphics.window);
	SDL_GL_SetSwapInterval(1);

	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0,0,0,1);
	glViewport(0, 0, graphics.screen.width, graphics.screen.height);
}

static void destroy_window(void) 
{
	SDL_GL_DeleteContext(graphics.glcontext);
	SDL_DestroyWindow(graphics.window);
}
