#ifndef GRAPHICS_H
#define GRAPHICS_H

#define SDL_WINDOW_NAME "> - - - #"

#include <stdbool.h>

#include <SDL2/SDL.h>

typedef struct {
	SDL_Window *window;
	bool fullScreen;
} Graphics;

void graphics_initialize(Graphics *graphics);
void graphics_cleanup(Graphics *graphics);
void graphics_create_window(Graphics *graphics);
void graphics_destroy_window(Graphics *graphics);

#endif