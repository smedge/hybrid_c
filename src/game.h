#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "graphics.h"

typedef struct {
	bool iconified;
	bool quit;
	Graphics graphics;
} Game;

void game_initialize(Game *game); 
void game_cleanup(Game *game);
void game_loop(Game *game);

#endif
