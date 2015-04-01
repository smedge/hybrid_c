#include <stdio.h>

#include <SDL2/SDL.h>

#include "game.h"

void main() 
{
	game_initialize();
	game_loop();
	game_cleanup();
}