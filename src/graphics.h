#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#define SDL_WINDOW_NAME "> - - - #"

typedef struct {
	SDL_Window *window;
	bool fullScreen;
} Graphics;

void graphics_initialize(Graphics *graphics);
void graphics_cleanup(Graphics *graphics);
void graphics_destroy_window(Graphics *graphics);

static void graphics_create_window(Graphics *graphics);
static void graphics_create_fullscreen_window(Graphics *graphics);
static void graphics_create_windowed_window(Graphics *graphics);

#endif