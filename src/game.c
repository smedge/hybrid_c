#include "game.h"

Game *game_create() {
	Game *game = (Game *)calloc(1, sizeof(Game));
	game_initialize(game);
	return game;
}

void game_delete(Game *game) {
	if (game->initialized)
		game_cleanup(game);
	free(game);
}

static void game_initialize(Game *game) 
{
	puts("initializing app.");

	game->running = true;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) 
	{
		puts("error: sdl initialization failed");
		exit(-1);
	}

	game->window = SDL_CreateWindow(
        SDL_WINDOW_NAME,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_OPENGL
    );

    game->initialized = true;
}

static void game_cleanup(Game *game)
{
	puts("cleaning up app.");
	SDL_DestroyWindow(game->window);
	SDL_Quit();
	game->initialized = false;
}

void game_loop(Game *game)
{
	puts("entering main loop.");
	SDL_Delay(5000); // do shit here
	puts("exiting main loop.");	
}