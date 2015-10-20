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
	view_update(input, ticks);
}

void mode_gameplay_render(void)
{
	graphics_clear();
	Screen screen = graphics_get_screen();

	graphics_set_world_projection();
	grid_render(&screen);

	graphics_set_ui_projection();
	hud_render(&screen);
	cursor_render();

	graphics_flip();
}