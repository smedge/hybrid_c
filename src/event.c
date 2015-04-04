#include "event.h"

static SDL_Event event;

void handle_sdl_events(Game *game, Input *input)
{
	while (SDL_PollEvent(&event) != 0) 
	{
		switch (event.type) {
		case SDL_QUIT:
			game->quit = true;
			break;
		
		case SDL_WINDOWEVENT:
			switch(event.window.event) {
			case SDL_WINDOWEVENT_MINIMIZED:
				game->iconified = true;
				break;
			case SDL_WINDOWEVENT_MAXIMIZED:
				game->iconified = false;
				break;
			case SDL_WINDOWEVENT_RESTORED:
				game->iconified = false;
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				game->hasFocus = true;
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				game->hasFocus = false;
				break;
			case SDL_WINDOWEVENT_ENTER:
				SDL_ShowCursor(false);
				input->showMouse = true;
				break;
			case SDL_WINDOWEVENT_LEAVE:
				SDL_ShowCursor(true);
				input->showMouse = false;
				break;
			case SDL_WINDOWEVENT_RESIZED:
            	graphics_resize_window(event.window.data1,
 					event.window.data2);
            	break;
			}
			break;

		case SDL_MOUSEMOTION:
			input->mouseX = event.motion.x;
			input->mouseY = event.motion.y;
			break;
		}
	}
}