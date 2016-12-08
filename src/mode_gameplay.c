#include "mode_gameplay.h"

#include <SDL2/SDL_opengl.h>
#include "map.h"

void Mode_Gameplay_initialize(void)
{
	Entity_destroy_all();

	Audio_loop_music(GAMEPLAY_MUSIC_PATH);
	View_initialize();
	Grid_initialize();
	Map_initialize();
	Ship_initialize();

	Position position;
	
	position.x = 420.0;
	position.y = 420.0;
	Mine_initialize(position);

	position.x = -420.0;
	position.y = -420.0;
	Mine_initialize(position);




	position.x = -1600.0;
	position.y = 1600.0;
	Mine_initialize(position);

	position.x = 1600.0;
	position.y = 1600.0;
	Mine_initialize(position);

	position.x = 1600.0;
	position.y = -1600.0;
	Mine_initialize(position);

	position.x = -1600.0;
	position.y = -1600.0;
	Mine_initialize(position);



	position.x = -1600.0;
	position.y = 0.0;
	Mine_initialize(position);

	position.x = 1600.0;
	position.y = 0.0;
	Mine_initialize(position);

	position.x = 0.0;
	position.y = 1600.0;
	Mine_initialize(position);

	position.x = 0.0;
	position.y = -1600.0;
	Mine_initialize(position);
}

void Mode_Gameplay_cleanup(void)
{
	Mine_cleanup();
	Ship_cleanup();
	Entity_destroy_all();
	Audio_stop_music();
}

void Mode_Gameplay_update(const Input *input, const unsigned int ticks)
{
	cursor_update(input);
	Entity_user_update_system(input, ticks);
	Entity_ai_update_system(ticks);
	Entity_collision_system();
	View_update(input, ticks);

	View_set_position(Ship_get_position());
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
	Hud_render(&screen);
	cursor_render();

	Graphics_flip();
}