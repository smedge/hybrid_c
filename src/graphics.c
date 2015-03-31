#include "graphics.h"

void graphics_initialize(Graphics *graphics) {
	graphics_create_window(graphics);
}

void graphics_cleanup(Graphics *graphics) {
	graphics_destroy_window(graphics);
}

void graphics_create_window(Graphics *graphics) {
	graphics->window = SDL_CreateWindow(
        SDL_WINDOW_NAME,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_OPENGL
    );
}

static void graphics_create_fullscreen_window(Graphics *graphics) {
	
}

static void graphics_create_windowed_window(Graphics *graphics) {
	
}

void graphics_destroy_window(Graphics *graphics) {
	SDL_DestroyWindow(graphics->window);
}