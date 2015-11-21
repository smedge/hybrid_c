#include "mode_gameplay.h"

void mode_gameplay_initialize(void)
{
	audio_loop_music(GAMEPLAY_MUSIC_PATH);
}

void mode_gameplay_cleanup(void)
{
	audio_stop_music();
}

void mode_gameplay_update(const Input *input, const unsigned int ticks)
{
	cursor_update(input);

	ship_update(input, ticks);
	view_update(input, ticks);

	// update
	// animate
	// collide
	// resolve

}

void mode_gameplay_render(void)
{
	graphics_clear();
	Screen screen = graphics_get_screen();
	View view = view_get_view();

	graphics_set_world_projection();
	view_transform(&screen);
	grid_render(&screen, &view);
	ship_render();

	graphics_set_ui_projection();
	hud_render(&screen);
	cursor_render();

	graphics_flip();
}