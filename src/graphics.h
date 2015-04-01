#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#define SDL_WINDOW_NAME "> - - - #"
#define SDL_WINDOW_WINDOWED_WIDTH 640
#define SDL_WINDOW_WINDOWED_HEIGHT 480
#define SDL_WINDOW_FULLSCREEN_WIDTH 1366
#define SDL_WINDOW_FULLSCREEN_HEIGHT 768

typedef struct {
	SDL_Window *window;
	bool fullScreen;
} Graphics;

void graphics_initialize(Graphics *graphics);
void graphics_cleanup(Graphics *graphics);

static void graphics_create_window(Graphics *graphics);
static void graphics_destroy_window(Graphics *graphics);
static void graphics_create_fullscreen_window(Graphics *graphics);
static void graphics_create_windowed_window(Graphics *graphics);

#endif