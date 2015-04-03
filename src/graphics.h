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

void graphics_initialize();
void graphics_cleanup();

static void graphics_create_window();
static void graphics_create_fullscreen_window();
static void graphics_create_windowed_window();
static void graphics_initialize_gl();
static void graphics_destroy_window();

#endif