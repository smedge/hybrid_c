#include "mode_gameplay.h"

#include <SDL2/SDL_opengl.h>
#include "world.h"

void Mode_Gameplay_initialize(void)
{
	Audio_loop_music(GAMEPLAY_MUSIC_PATH);
	View_initialize();
	Ship_initialize();
}

void Mode_Gameplay_cleanup(void)
{
	Audio_stop_music();
}

void Mode_Gameplay_update(const Input *input, const unsigned int ticks)
{
	cursor_update(input);

	Ship_update(input, ticks);

	Ship_collide();
	World_collide();
	
	Ship_resolve();
	World_resolve();

	View_update(input, ticks);
}

void Mode_Gameplay_render(void)
{
	Graphics_clear();
	Screen screen = Graphics_get_screen();
	View view = View_get_view();

	Graphics_set_world_projection();

	glPushMatrix();
	View_transform(&screen);
	Grid_render(&screen, &view);
	World_render(&view);
	Ship_render();
	glPopMatrix();

	Graphics_set_ui_projection();
	hud_render(&screen);
	cursor_render();

	Graphics_flip();
}