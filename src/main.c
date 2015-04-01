#include <stdio.h>

#include <SDL2/SDL.h>

#include "game.h"

void main() 
{
	Game game;
	game_initialize(&game);
	game_loop(&game);
	game_cleanup(&game);
}