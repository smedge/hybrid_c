#include "mode_gameplay.h"

void mode_gameplay_update(const Input *input, const unsigned int ticks)
{
	// ecs update
	cursor_update(input);
	view_update(input, ticks);
}

void mode_gameplay_render()
{
	graphics_clear();
	Screen screen = graphics_get_screen();

	graphics_set_world_projection();
	// TODO render ecs

	graphics_set_ui_projection();
	// TODO render ui
	hud_render(&screen);
	cursor_render();

	graphics_flip();
}