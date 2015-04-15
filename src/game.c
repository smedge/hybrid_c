#include "game.h"

static Game game;

static void reset_input(Input *input); 
static void update();
static void render();

void game_initialize() 
{
	puts("initializing app.");
	game.iconified = false;
	game.quit = false;
	game.hasFocus = true;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)  {
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
	unsigned int ticks = 0;
	
	while(!game.quit) {
		handle_sdl_events(&game, &input);
		ticks = timer_tick();
		update(&input, ticks);
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

static void update(Input *input, unsigned int ticks) 
{
	cursor_update(input);
}

static void render()
{
	graphics_clear();
	graphics_set_world_projection();
	
	graphics_set_ui_projection();
	cursor_render();

	graphics_flip();
}