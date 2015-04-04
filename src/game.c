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

	Input input; // TODO initialize input
	
	while(!game.quit) {
		handle_sdl_events(&game, &input);

		update();

		if (!game.iconified)
			render();

		reset_input(&input);

		SDL_Delay(50);
	}

	puts("exiting main loop.");	
}

void reset_input(Input *input) 
{
	input->mouseWheelUp=false;
	input->mouseWheelDown=false;
}

void update() 
{

}

void render()
{

}