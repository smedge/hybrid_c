#include "mode_gameplay.h"

#include <SDL2/SDL_opengl.h>
#include "map.h"

void Mode_Gameplay_initialize(void)
{
	Audio_loop_music(GAMEPLAY_MUSIC_PATH);
	View_initialize();
	Grid_initialize();
	Map_initialize();
	Ship_initialize();
}

void Mode_Gameplay_cleanup(void)
{
	Entity_destroy_all();
	Audio_stop_music();
}

void Mode_Gameplay_update(const Input *input, const unsigned int ticks)
{
	cursor_update(input);
	Entity_user_update_system(input, ticks);
	View_update(input, ticks);
}

void Mode_Gameplay_render(void)
{
	Graphics_clear();
	Screen screen = Graphics_get_screen();

	Graphics_set_world_projection();

	glPushMatrix();

	View_transform(&screen);
	Entity_render_system();

	glPopMatrix();

	Graphics_set_ui_projection();
	hud_render(&screen);
	cursor_render();

	Graphics_flip();
}