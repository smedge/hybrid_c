#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#define SDL_WINDOW_NAME "> - - - #"

typedef struct Game 
{
	bool initialized;
	bool running;
	SDL_Window *window;
} Game;

Game *game_create();
void game_delete(Game *game);
static void game_initialize(Game *game); 
static void game_cleanup(Game *game);
void game_loop(Game *game);

#endif
