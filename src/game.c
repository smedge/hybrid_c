#include "game.h"

static Game game;

void game_initialize() 
{
	puts("initializing app.");

	game.iconified = false;
	game.quit = false;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) 
	{
		puts("error: sdl initialization failed");
		exit(-1);
	}

	graphics_initialize();
}

void game_cleanup()
{
	puts("cleaning up app.");

	graphics_cleanup();

	SDL_Quit();
}

void game_loop()
{
	puts("entering main loop.");
	
	while(!game.quit) {
		handle_sdl_events(&game);

		//update_entities();

		if (!game.iconified)
			//render_entities();

		SDL_Delay(50); // do shit here
	}


	puts("exiting main loop.");	
}