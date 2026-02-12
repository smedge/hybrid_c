#include "mode_gameplay.h"
#include "map.h"
#include "render.h"
#include "mat4.h"
#include "background.h"

#include <math.h>
#include <stdlib.h>

typedef enum {
	GAMEPLAY_REBIRTH,
	GAMEPLAY_ACTIVE
} GameplayState;

#define REBIRTH_DURATION 13000
#define REBIRTH_FADE_MS 2000
#define REBIRTH_MIN_ZOOM 0.01
#define REBIRTH_DEFAULT_ZOOM 0.5
#define REBIRTH_CHANNEL 1

static GameplayState gameplayState;
static int rebirthTimer;
static Mix_Chunk *rebirthSample = 0;
static int selectedBgm;

static const char *bgm_paths[] = {
	GAMEPLAY_MUSIC_01_PATH,
	GAMEPLAY_MUSIC_02_PATH,
	GAMEPLAY_MUSIC_03_PATH,
	GAMEPLAY_MUSIC_04_PATH,
	GAMEPLAY_MUSIC_05_PATH,
	GAMEPLAY_MUSIC_06_PATH,
	GAMEPLAY_MUSIC_07_PATH
};

static double ease_in_out_cubic(double t);
static void start_zone_bgm(void);
static void complete_rebirth(void);

void Mode_Gameplay_initialize(void)
{
	Entity_destroy_all();

	selectedBgm = rand() % 7;

	View_initialize();
	Hud_initialize();
	Background_initialize();
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

	/* Start rebirth sequence */
	gameplayState = GAMEPLAY_REBIRTH;
	rebirthTimer = 0;
	View_set_scale(REBIRTH_MIN_ZOOM);

	Audio_load_sample(&rebirthSample, REBIRTH_MUSIC_PATH);
	Audio_play_sample_on_channel(&rebirthSample, REBIRTH_CHANNEL);
}

void Mode_Gameplay_cleanup(void)
{
	Mine_cleanup();
	Ship_cleanup();
	Entity_destroy_all();
	Hud_cleanup();
	Audio_stop_music();
	Mix_HaltChannel(REBIRTH_CHANNEL);
	Audio_unload_sample(&rebirthSample);
}

void Mode_Gameplay_update(const Input *input, const unsigned int ticks)
{
	Background_update(ticks * 3);

	if (gameplayState == GAMEPLAY_REBIRTH) {
		rebirthTimer += ticks;

		/* Animate zoom */
		double t = (double)rebirthTimer / REBIRTH_DURATION;
		if (t > 1.0) t = 1.0;

		double progress = ease_in_out_cubic(t);
		double scale = REBIRTH_MIN_ZOOM + (REBIRTH_DEFAULT_ZOOM - REBIRTH_MIN_ZOOM) * progress;
		View_set_scale(scale);

		/* AI still runs so the world feels alive */
		Entity_ai_update_system(ticks);

		/* Camera stays centered on spawn point */
		Position origin = {0.0, 0.0};
		View_set_position(origin);

		if (rebirthTimer >= REBIRTH_DURATION)
			complete_rebirth();

		return;
	}

	/* Normal gameplay */
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

	/* Background bloom pass (blurred only — no raw polygon render) */
	{
		int draw_w, draw_h;
		Graphics_get_drawable_size(&draw_w, &draw_h);
		Bloom *bg_bloom = Graphics_get_bg_bloom();

		Bloom_begin_source(bg_bloom);
		Background_render();
		Render_flush(&world_proj, &view);
		Bloom_end_source(bg_bloom, draw_w, draw_h);

		Bloom_blur(bg_bloom);
		Bloom_composite(bg_bloom, draw_w, draw_h);
	}

	Entity_render_system();
	Render_flush(&world_proj, &view);

	/* FBO bloom pass */
	{
		int draw_w, draw_h;
		Graphics_get_drawable_size(&draw_w, &draw_h);
		Bloom *bloom = Graphics_get_bloom();

		Bloom_begin_source(bloom);
		Map_render();
		Ship_render_bloom_source();
		Mine_render_bloom_source();
		Render_flush(&world_proj, &view);
		Bloom_end_source(bloom, draw_w, draw_h);

		Bloom_blur(bloom);
		Bloom_composite(bloom, draw_w, draw_h);
	}

	/* UI pass */
	Mat4 ui_proj = Graphics_get_ui_projection();
	Mat4 identity = Mat4_identity();
	Hud_render(&screen);
	if (gameplayState == GAMEPLAY_ACTIVE)
		cursor_render();
	Render_flush(&ui_proj, &identity);

	Graphics_flip();
}

static void complete_rebirth(void)
{
	gameplayState = GAMEPLAY_ACTIVE;

	/* Spawn ship */
	Position origin = {0.0, 0.0};
	Ship_force_spawn(origin);

	/* Start zone BGM */
	start_zone_bgm();

	/* Fade out Memory Man over 2 seconds */
	Audio_fade_out_channel(REBIRTH_CHANNEL, REBIRTH_FADE_MS);
}

static void start_zone_bgm(void)
{
	Audio_loop_music(bgm_paths[selectedBgm]);
}

static double ease_in_out_cubic(double t)
{
	if (t < 0.5)
		return 4.0 * t * t * t;
	else {
		double f = -2.0 * t + 2.0;
		return 1.0 - (f * f * f) / 2.0;
	}
}
