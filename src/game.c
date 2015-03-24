#include "game.h"

void game_initialize(Game *game) 
{
	puts("initializing app.");

	game->running = true;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) 
	{
		puts("error: sdl initialization failed");

		exit(-1);
	}

	game->window = SDL_CreateWindow(
        "> - - - #",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_OPENGL
    );
}

void game_cleanup(Game *game)
{
	puts("cleaning up app.");

	SDL_DestroyWindow(game->window);

	SDL_Quit();
}

void game_loop(Game *game)
{
	puts("entering main loop.");

	SDL_Delay(5000); // do shit here

	puts("exiting main loop.");	
}