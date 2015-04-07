#include "game.h"

static Game game;

static void reset_input(Input *input); 
static void update();
static void render();

extern void handle_sdl_events(Game *game, Input *input);

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

	Input input;
	
	while(!game.quit) {
		handle_sdl_events(&game, &input);
		update(&input);
		if (!game.iconified)
			render();
		reset_input(&input);
		SDL_Delay(1000/60);
	}

	puts("exiting main loop.");	
}

void reset_input(Input *input) 
{
	input->mouseWheelUp = false;
	input->mouseWheelDown = false;
}

void update(Input *input) 
{
	cursor_update(input);
}

void render()
{
	graphics_clear();
	graphics_set_ui_projection();
	cursor_render();
	graphics_flip();
}