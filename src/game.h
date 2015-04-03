#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "graphics.h"

typedef struct {
	bool iconified;
	bool hasFocus;
	bool quit;
} Game;

void game_initialize(); 
void game_cleanup();
void game_loop();

static void update();
static void render();

#endif
