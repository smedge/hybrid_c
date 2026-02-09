#include "mode_gameplay.h"
#include "map.h"
#include "render.h"
#include "mat4.h"


void Mode_Gameplay_initialize(void)
{
	Entity_destroy_all();

	int num_between_1_and_7 = (rand() % 7) + 1;

	switch (num_between_1_and_7) {
	case 1:
		Audio_loop_music(GAMEPLAY_MUSIC_01_PATH);
		break;
	case 2:
		Audio_loop_music(GAMEPLAY_MUSIC_02_PATH);
		break;
	case 3:
		Audio_loop_music(GAMEPLAY_MUSIC_03_PATH);
		break;
	case 4:
		Audio_loop_music(GAMEPLAY_MUSIC_04_PATH);
		break;
	case 5:
		Audio_loop_music(GAMEPLAY_MUSIC_05_PATH);
		break;
	case 6:
		Audio_loop_music(GAMEPLAY_MUSIC_06_PATH);
		break;
	case 7:
		Audio_loop_music(GAMEPLAY_MUSIC_07_PATH);
		break;
	}


	View_initialize();
	Hud_initialize();
	Grid_initialize();
	Map_initialize();
	Ship_initialize();

	Position position;

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



	position.x = -3200.0;
	position.y = 3200.0;
	Mine_initialize(position);

	position.x = -1600.0;
	position.y = 3200.0;
	Mine_initialize(position);

	position.x = 0.0;
	position.y = 3200.0;
	Mine_initialize(position);

	position.x = 1600.0;
	position.y = 3200.0;
	Mine_initialize(position);

	position.x = 3200.0;
	position.y = 3200.0;
	Mine_initialize(position);



	position.x = -3200.0;
	position.y = -3200.0;
	Mine_initialize(position);

	position.x = -1600.0;
	position.y = -3200.0;
	Mine_initialize(position);

	position.x = 0.0;
	position.y = -3200.0;
	Mine_initialize(position);

	position.x = 1600.0;
	position.y = -3200.0;
	Mine_initialize(position);

	position.x = 3200.0;
	position.y = -3200.0;
	Mine_initialize(position);



	position.x = -3200.0;
	position.y = 1600.0;
	Mine_initialize(position);

	position.x = -3200.0;
	position.y = 0.0;
	Mine_initialize(position);

	position.x = -3200.0;
	position.y = -1600.0;
	Mine_initialize(position);



	position.x = 3200.0;
	position.y = 1600.0;
	Mine_initialize(position);

	position.x = 3200.0;
	position.y = 0.0;
	Mine_initialize(position);

	position.x = 3200.0;
	position.y = -1600.0;
	Mine_initialize(position);



	position.x = -2400.0;
	position.y = 2400.0;
	Mine_initialize(position);

	position.x = -800.0;
	position.y = 2400.0;
	Mine_initialize(position);

	position.x = 800.0;
	position.y = 2400.0;
	Mine_initialize(position);

	position.x = 2400.0;
	position.y = 2400.0;
	Mine_initialize(position);



	position.x = -2400.0;
	position.y = -2400.0;
	Mine_initialize(position);

	position.x = -800.0;
	position.y = -2400.0;
	Mine_initialize(position);

	position.x = 800.0;
	position.y = -2400.0;
	Mine_initialize(position);

	position.x = 2400.0;
	position.y = -2400.0;
	Mine_initialize(position);



	position.x = -2400.0;
	position.y = 800.0;
	Mine_initialize(position);

	position.x = -2400.0;
	position.y = -800.0;
	Mine_initialize(position);

	position.x = 2400.0;
	position.y = 800.0;
	Mine_initialize(position);

	position.x = 2400.0;
	position.y = -800.0;
	Mine_initialize(position);
}

void Mode_Gameplay_cleanup(void)
{
	Mine_cleanup();
	Ship_cleanup();
	Entity_destroy_all();
	Hud_cleanup();
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

	/* World pass */
	Mat4 world_proj = Graphics_get_world_projection();
	Mat4 view = View_get_transform(&screen);
	Entity_render_system();
	Render_flush(&world_proj, &view);

	/* UI pass */
	Mat4 ui_proj = Graphics_get_ui_projection();
	Mat4 identity = Mat4_identity();
	Hud_render(&screen);
	cursor_render();
	Render_flush(&ui_proj, &identity);

	Graphics_flip();
}
