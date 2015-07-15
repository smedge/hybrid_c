#include "mode_mainmenu.h"

void mode_mainmenu_update(const Input *input, const unsigned int ticks)
{
	cursor_update(input);
}

void mode_mainmenu_render()
{
	graphics_clear();
	graphics_set_ui_projection();
	
	cursor_render();

	graphics_flip();
}