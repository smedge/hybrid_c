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
				//userInput.showMouse = true;
				break;
			case SDL_WINDOWEVENT_LEAVE:
				SDL_ShowCursor(true);
				//userInput.showMouse = false;
				break;
			/*case SDL_WINDOWEVENT_RESIZED:
            	std::cout << "SDL_WINDOWEVENT_RESIZED: " << 
            		event.window.data1 << "," << 
            		event.window.data2 << 
            		std::endl;
            	
            	graphicsManager.resizeWindow(event.window.data1,
 					event.window.data2);
            	break;*/
			}
			break;
		}
	}
}