#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "graphics.h"

typedef struct {
	bool running;
	Graphics graphics;
} Game;

Game *game_create();
void game_delete(Game *game);
void game_initialize(Game *game); 
void game_cleanup(Game *game);
void game_loop(Game *game);

#endif
