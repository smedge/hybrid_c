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

		case SDL_MOUSEWHEEL:
			if (event.wheel.y > 0) {
				input->mouseWheelUp = true;
			}
			else if (event.wheel.y < 0) {
				input->mouseWheelDown = true;
			}
			break;
		
		case SDL_MOUSEMOTION:
			input->mouseX = event.motion.x;
			input->mouseY = event.motion.y;
			break;

		case SDL_MOUSEBUTTONDOWN:
			switch(event.button.button) {
			
			case SDL_BUTTON_LEFT:
				input->mouseLeft = true;
				break;
			
			case SDL_BUTTON_MIDDLE:
				input->mouseMiddle = true;
				break;
			
			case SDL_BUTTON_RIGHT:
				input->mouseRight = true;
				break;
			
			case 6:
				break;
			
			case 7:
				break;
			
			case 8:
				break;
			
			case 9:
				break;
			}
			break;

		case SDL_MOUSEBUTTONUP:
			switch(event.button.button) {
			
			case SDL_BUTTON_LEFT:
				input->mouseLeft = false;
				break;
			
			case SDL_BUTTON_MIDDLE:
				input->mouseMiddle = false;
				break;
			
			case SDL_BUTTON_RIGHT:
				input->mouseRight = false;
				break;
			
			case 6:
				break;
			
			case 7:
				break;
			
			case 8:
				break;
			
			case 9:
				break;
			}
			break;

		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) {
			
			case SDLK_a:
				input->keyA = true;
				break;
			
			case SDLK_d:
				input->keyD = true;
				break;
			
			case SDLK_w:
				input->keyW = true;
				break;
			
			case SDLK_s:
				input->keyS = true;
				break;
			
			case SDLK_F10:
				break;
			
			case SDLK_F11:
				break;
			
			case SDLK_F12:
				break;
			
			default:
				break;
			}
			break;
		
		case SDL_KEYUP:
			switch (event.key.keysym.sym) {
			
			case SDLK_a:
				input->keyA = false;
				break;
			
			case SDLK_d:
				input->keyD = false;
				break;
			
			case SDLK_w:
				input->keyW = false;
				break;
			
			case SDLK_s:
				input->keyS = false;
				break;
			
			case SDLK_F10:
				// graphics_toggle_multisampling();
				break;
			
			case SDLK_F11:
				graphics_toggle_fullscreen();
				break;
			
			case SDLK_F12:
				game->quit = true;
				break;
			
			default:
				break;
			}
			break;
		}
	}
}