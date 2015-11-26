#include "sdlapp.h"

static SdlApp sdlApp;

static void initialize(void); 
static void cleanup(void);
static void loop(void);
static void update(const Input *input, const unsigned int ticks);
static void render(void);
static void reset_input(Input *input);

static void change_mode(const Mode mode);
static void initialize_mode(void);
static void cleanup_mode(void);

static void handle_sdl_events(Input *input);
static void handle_sdl_event(Input *input, const SDL_Event *event);
static void handle_sdl_window_event(Input *input, const SDL_Event *event);
static void handle_sdl_mousebuttondown_event(Input *input, const SDL_Event *event);
static void handle_sdl_mousebuttonup_event(Input *input, const SDL_Event *event);
static void handle_sdl_keydown_event(Input *input, const SDL_Event *event);
static void handle_sdl_keyup_event(Input *input, const SDL_Event *event);

static void quit_callback(void);
static void gameplay_mode_callback(void);

void sdlapp_run(void)
{
	initialize();
	loop();
	cleanup();
}

static void initialize(void) 
{
	sdlApp.iconified = false;
	sdlApp.quit = false;
	sdlApp.hasFocus = true;
	
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)  {
		puts("error: sdl initialization failed");
		exit(-1);
	}

	graphics_initialize();
	audio_initialize();

	mode_mainmenu_initialize(&quit_callback, &gameplay_mode_callback);
	
	sdlApp.mode = MAINMENU;
}

static void cleanup(void)
{
	cleanup_mode();
	audio_cleanup();
	graphics_cleanup();
	SDL_Quit();
}

static void loop(void)
{
	Input input;
	input_initialize(&input);
	unsigned int ticks = 0;
	const unsigned int delay = 1000 / 60;
	
	while(!sdlApp.quit) {
		ticks = timer_tick();
		reset_input(&input);
		handle_sdl_events(&input);
		update(&input, ticks);
		render();
		SDL_Delay(delay);
	}
}

static void reset_input(Input *input) 
{
	input->mouseWheelUp = false;
	input->mouseWheelDown = false;
}

static void quit_callback(void) 
{
	sdlApp.quit = true;
}

static void gameplay_mode_callback(void)
{
	change_mode(GAMEPLAY);
}

static void initialize_mode(void) 
{
	switch (sdlApp.mode) {
	case INTRO:
		break;
	case MAINMENU:
		mode_mainmenu_initialize(&quit_callback, &gameplay_mode_callback);
		break;
	case GAMEPLAY:
		mode_gameplay_initialize();
		break;
	};
}

static void cleanup_mode(void)
{
	switch (sdlApp.mode) {
	case INTRO:
		break;
	case MAINMENU:
		mode_mainmenu_cleanup();
		break;
	case GAMEPLAY:
		mode_gameplay_cleanup();
		break;
	};
}

static void change_mode(const Mode mode)
{
	if (mode == sdlApp.mode)
		return;

	cleanup_mode();
	sdlApp.mode = mode;
	initialize_mode();
}

static void update(const Input* const input, const unsigned int ticks) 
{
	switch (sdlApp.mode) {
	case INTRO:
		break;
	case MAINMENU:
		mode_mainmenu_update(input, ticks);
		break;
	case GAMEPLAY:
		mode_gameplay_update(input, ticks);
		break;
	};
}

static void render(void)
{
	if (sdlApp.iconified)
			return;

	switch (sdlApp.mode) {
	case INTRO:
		break;
	case MAINMENU:
		mode_mainmenu_render();
		break;
	case GAMEPLAY:
		mode_gameplay_render();
		break;
	};
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
	
	case SDLK_LCTRL:
		input->keyLControl = true;
		break;

	case SDLK_LSHIFT:
		input->keyLShift = true;
		break;

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

	case SDLK_ESCAPE:
		break;
			
	default:
		break;
	}
}

static void handle_sdl_keyup_event(Input *input, const SDL_Event *event)
{
	switch (event->key.keysym.sym) {

	case SDLK_LCTRL:
		input->keyLControl = false;
		break;

	case SDLK_LSHIFT:
		input->keyLShift = false;
		break;
			
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

	case SDLK_ESCAPE:
		change_mode(MAINMENU);
		break;
			
	default:
		break;
	}
}