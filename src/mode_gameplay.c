#include "mode_gameplay.h"

void mode_gameplay_update(const Input *input, const unsigned int ticks)
{
	cursor_update(input);
}

void mode_gameplay_render()
{
	graphics_clear();

	graphics_set_world_projection();
	// TODO render ecs

	graphics_set_ui_projection();
	cursor_render();

	graphics_flip();
}