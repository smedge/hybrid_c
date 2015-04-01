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
	SDL_Delay(5000); // do shit here
	puts("exiting main loop.");	
}

/*Game *game_create() {
	Game *game = (Game *)calloc(1, sizeof(Game));
	game_initialize(game);
	return game;
}

void game_delete(Game *game) {
	game_cleanup(game);
	free(game);
}*/