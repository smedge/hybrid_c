#include "sdlapp.h"

static SdlApp sdlApp;

static void update();
static void render();
static void reset_input(Input *input); 

static void handle_sdl_events(Input *input);
static void handle_sdl_event(Input *input, const SDL_Event *event);
static void handle_sdl_window_event(Input *input, const SDL_Event *event);
static void handle_sdl_mousebuttondown_event(Input *input, const SDL_Event *event);
static void handle_sdl_mousebuttonup_event(Input *input, const SDL_Event *event);
static void handle_sdl_keydown_event(Input *input, const SDL_Event *event);
static void handle_sdl_keyup_event(Input *input, const SDL_Event *event);

void sdlapp_initialize() 
{
	puts("initializing app.");
	sdlApp.iconified = false;
	sdlApp.quit = false;
	sdlApp.hasFocus = true;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)  {
		puts("error: sdl initialization failed");
		exit(-1);
	}

	graphics_initialize();
}

void sdlapp_cleanup()
{
	puts("cleaning up app.");
	graphics_cleanup();
	SDL_Quit();
}

void sdlapp_loop()
{
	puts("entering main loop.");

	Input input;
	input.showMouse = true;
	unsigned int ticks = 0;
	
	while(!sdlApp.quit) {
		handle_sdl_events(&input);
		ticks = timer_tick();
		update(&input, ticks);
		if (!sdlApp.iconified)
			render();
		reset_input(&input);
		SDL_Delay(1000/60);
	}

	puts("exiting main loop.");	
}

static void reset_input(Input *input) 
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

static void handle_sdl_events(Input *input)
{
	SDL_Event event;

	while (SDL_PollEvent(&event) != 0) 
	{
		handle_sdl_event(input, &event);
	}
}

static void handle_sdl_event(Input *input, const SDL_Event *event) 
{
	switch (event->type) {
		
	case SDL_QUIT:
		sdlApp.quit = true;
		break;
	
	case SDL_WINDOWEVENT:
		handle_sdl_window_event(input, event);
		break;

	case SDL_MOUSEWHEEL:
		if (event->wheel.y > 0) {
			input->mouseWheelUp = true;
		}
		else if (event->wheel.y < 0) {
			input->mouseWheelDown = true;
		}
		break;
	
	case SDL_MOUSEMOTION:
		input->mouseX = event->motion.x;
		input->mouseY = event->motion.y;
		break;

	case SDL_MOUSEBUTTONDOWN:
		handle_sdl_mousebuttondown_event(input, event);
		break;

	case SDL_MOUSEBUTTONUP:
		handle_sdl_mousebuttonup_event(input, event);
		break;

	case SDL_KEYDOWN:
		handle_sdl_keydown_event(input, event);
		break;
	
	case SDL_KEYUP:
		handle_sdl_keyup_event(input, event);
		break;
	}
}

static void handle_sdl_window_event(Input *input, const SDL_Event *event)
{
	switch(event->window.event) {
			
	case SDL_WINDOWEVENT_MINIMIZED:
		sdlApp.iconified = true;
		break;
			
	case SDL_WINDOWEVENT_MAXIMIZED:
		sdlApp.iconified = false;
		break;
			
	case SDL_WINDOWEVENT_RESTORED:
		sdlApp.iconified = false;
		break;
			
	case SDL_WINDOWEVENT_FOCUS_GAINED:
		sdlApp.hasFocus = true;
		break;
			
	case SDL_WINDOWEVENT_FOCUS_LOST:
		sdlApp.hasFocus = false;
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
        graphics_resize_window(event->window.data1,
 		event->window.data2);
        break;
	}
}

static void handle_sdl_mousebuttondown_event(Input *input, const SDL_Event *event)
{
	switch(event->button.button) {
			
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
}

static void handle_sdl_mousebuttonup_event(Input *input, const SDL_Event *event)
{
	switch(event->button.button) {
			
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
}

static void handle_sdl_keydown_event(Input *input, const SDL_Event *event)
{
	switch (event->key.keysym.sym) {
			
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
}

static void handle_sdl_keyup_event(Input *input, const SDL_Event *event)
{
	switch (event->key.keysym.sym) {
			
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
		sdlApp.quit = true;
		break;
			
	default:
		break;
	}
}