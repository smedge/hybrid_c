#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#include <SDL2/SDL.h>

typedef struct Game 
{
	bool running;
	SDL_Window *window;
} Game;

void game_initialize(Game *game); 
void game_cleanup(Game *game);

void game_loop(Game *game);

#endif
