#include "mode_gameplay.h"

#include <SDL2/SDL_opengl.h>
#include "world.h"

void Mode_Gameplay_initialize(void)
{
	audio_loop_music(GAMEPLAY_MUSIC_PATH);
	view_initialize();
	ship_initialize();
}

void Mode_Gameplay_cleanup(void)
{
	audio_stop_music();
}

void Mode_Gameplay_update(const Input *input, const unsigned int ticks)
{
	cursor_update(input);

	ship_update(input, ticks);
	view_update(input, ticks);

	// update
	//
	// animate
	// collide
	// resolve
}

void Mode_Gameplay_render(void)
{
	graphics_clear();
	Screen screen = graphics_get_screen();
	View view = view_get_view();

	graphics_set_world_projection();

	glPushMatrix();
	view_transform(&screen);
	grid_render(&screen, &view);
	World_render(&view);
	ship_render();
	glPopMatrix();

	graphics_set_ui_projection();
	hud_render(&screen);
	cursor_render();

	graphics_flip();
}