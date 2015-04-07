#include <stdio.h>

#include <SDL2/SDL.h>

#include "game.h"

int main() 
{
	game_initialize();
	game_loop();
	game_cleanup();
	return 0;
}