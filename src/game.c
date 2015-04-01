#include "game.h"

void game_initialize(Game *game) 
{
	puts("initializing app.");

	game->iconified = false;
	game->quit = false;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) 
	{
		puts("error: sdl initialization failed");
		exit(-1);
	}

	graphics_initialize(&game->graphics);
}

void game_cleanup(Game *game)
{
	puts("cleaning up app.");

	graphics_cleanup(&game->graphics);

	SDL_Quit();
}

void game_loop(Game *game)
{
	puts("entering main loop.");
	
	while(game->quit == false) {
		handle_sdl_events(game);

		// update

		// render

		SDL_Delay(100); // do shit here
	}


	puts("exiting main loop.");	
}