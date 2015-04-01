#include "event.h"

static SDL_Event event;

void handle_sdl_events(Game *game)
{
	while (SDL_PollEvent(&event) != 0) 
	{
		switch (event.type) {
		case SDL_QUIT:
			game->quit = true;
			break;
		}
	}
}