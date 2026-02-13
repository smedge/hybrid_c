#include "mode_gameplay.h"
#include "map.h"
#include "zone.h"
#include "render.h"
#include "mat4.h"
#include "text.h"
#include "background.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

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

/* God mode state */
static bool godModeActive = false;
static int godModeSelectedType = 0;
static int godModeCursorX = 0;
static int godModeCursorY = 0;
static bool godModeCursorValid = false;
#define GOD_CAM_SPEED 800.0

static double ease_in_out_cubic(double t);
static void start_zone_bgm(void);
static void complete_rebirth(void);
static void god_mode_update(const Input *input, const unsigned int ticks);
static void god_mode_render_cursor(void);
static void god_mode_render_hud(const Screen *screen);

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
	Fragment_initialize();
	Progression_initialize();
	Zone_load("./resources/zones/zone_001.zone");

	godModeActive = false;
	godModeSelectedType = 0;

	/* Start rebirth sequence */
	gameplayState = GAMEPLAY_REBIRTH;
	rebirthTimer = 0;
	View_set_scale(REBIRTH_MIN_ZOOM);

	Audio_load_sample(&rebirthSample, REBIRTH_MUSIC_PATH);
	Audio_play_sample_on_channel(&rebirthSample, REBIRTH_CHANNEL);
}

void Mode_Gameplay_cleanup(void)
{
	Progression_cleanup();
	Fragment_cleanup();
	Zone_unload();
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
		Fragment_update(ticks);
		Progression_update(ticks);

		/* Camera stays centered on spawn point */
		Position origin = {0.0, 0.0};
		View_set_position(origin);

		if (rebirthTimer >= REBIRTH_DURATION)
			complete_rebirth();

		return;
	}

	/* Toggle god mode */
	if (input->keyG) {
		godModeActive = !godModeActive;
		Ship_set_god_mode(godModeActive);
	}

	if (godModeActive) {
		god_mode_update(input, ticks);
		return;
	}

	/* Normal gameplay */
	cursor_update(input);
	Entity_user_update_system(input, ticks);
	Entity_ai_update_system(ticks);
	Entity_collision_system();
	Fragment_update(ticks);
	Progression_update(ticks);
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
	Fragment_render();
	if (godModeActive)
		god_mode_render_cursor();
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
		Fragment_render_bloom_source();
		Render_flush(&world_proj, &view);
		Bloom_end_source(bloom, draw_w, draw_h);

		Bloom_blur(bloom);
		Bloom_composite(bloom, draw_w, draw_h);
	}

	/* UI pass */
	Mat4 ui_proj = Graphics_get_ui_projection();
	Mat4 identity = Mat4_identity();
	Hud_render(&screen);
	Progression_render(&screen);
	Fragment_render_text(&screen);
	if (godModeActive)
		god_mode_render_hud(&screen);
	if (gameplayState == GAMEPLAY_ACTIVE && !godModeActive)
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

static void god_mode_update(const Input *input, const unsigned int ticks)
{
	double dt = ticks / 1000.0;
	View view = View_get_view();

	/* Free camera pan with WASD — speed scales with zoom so it feels consistent */
	double camSpeed = GOD_CAM_SPEED / view.scale * dt;
	Position camPos = view.position;
	if (input->keyW) camPos.y += camSpeed;
	if (input->keyS) camPos.y -= camSpeed;
	if (input->keyD) camPos.x += camSpeed;
	if (input->keyA) camPos.x -= camSpeed;
	View_set_position(camPos);

	/* Zoom still works */
	View_update(input, ticks);

	/* Convert mouse to world coordinates */
	Screen screen = Graphics_get_screen();
	Position mousePos = {input->mouseX, input->mouseY};
	Position worldPos = View_get_world_position(&screen, mousePos);

	/* Snap to grid */
	int cell_x = (int)floor(worldPos.x / MAP_CELL_SIZE);
	int cell_y = (int)floor(worldPos.y / MAP_CELL_SIZE);
	int grid_x = cell_x + HALF_MAP_SIZE;
	int grid_y = cell_y + HALF_MAP_SIZE;

	godModeCursorValid = (grid_x >= 0 && grid_x < MAP_SIZE &&
	                      grid_y >= 0 && grid_y < MAP_SIZE);
	godModeCursorX = grid_x;
	godModeCursorY = grid_y;

	/* Place cell on LMB (skip if cell already matches — prevents flood of
	   redundant undo entries when mouse button is held) */
	if (input->mouseLeft && godModeCursorValid) {
		const Zone *z = Zone_get();
		if (z->cell_type_count > 0 &&
		    z->cell_grid[grid_x][grid_y] != godModeSelectedType) {
			Zone_place_cell(grid_x, grid_y, z->cell_types[godModeSelectedType].id);
		}
	}

	/* Remove cell on RMB (Zone_remove_cell already checks for empty) */
	if (input->mouseRight && godModeCursorValid) {
		Zone_remove_cell(grid_x, grid_y);
	}

	/* Undo with Ctrl+Z */
	if (input->keyZ) {
		Zone_undo();
	}

	/* Cycle cell type with Tab */
	if (input->keyTab) {
		const Zone *z = Zone_get();
		if (z->cell_type_count > 0)
			godModeSelectedType = (godModeSelectedType + 1) % z->cell_type_count;
	}

	/* AI still runs so the world feels alive */
	Entity_ai_update_system(ticks);
	Fragment_update(ticks);
	Progression_update(ticks);
}

static void god_mode_render_cursor(void)
{
	if (!godModeCursorValid) return;

	/* Grid to world position */
	float wx = (float)(godModeCursorX - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	float wy = (float)(godModeCursorY - HALF_MAP_SIZE) * MAP_CELL_SIZE;

	/* Get outline color from selected type */
	const Zone *z = Zone_get();
	float r = 1.0f, g = 1.0f, b = 1.0f, a = 0.5f;
	if (z->cell_type_count > 0) {
		ColorRGB c = z->cell_types[godModeSelectedType].outlineColor;
		r = c.red / 255.0f;
		g = c.green / 255.0f;
		b = c.blue / 255.0f;
	}

	/* Draw outline quad */
	float s = MAP_CELL_SIZE;
	Render_line_segment(wx, wy, wx + s, wy, r, g, b, a);
	Render_line_segment(wx + s, wy, wx + s, wy + s, r, g, b, a);
	Render_line_segment(wx + s, wy + s, wx, wy + s, r, g, b, a);
	Render_line_segment(wx, wy + s, wx, wy, r, g, b, a);

	/* Semi-transparent fill */
	Render_quad_absolute(wx, wy, wx + s, wy + s, r, g, b, a * 0.3f);
}

static void god_mode_render_hud(const Screen *screen)
{
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	/* "GOD MODE" top-center */
	float cx = (float)screen->width * 0.5f - 40.0f;
	float ty = 20.0f;
	Text_render(tr, shaders, &proj, &ident,
		"GOD MODE", cx, ty,
		1.0f, 0.3f, 0.3f, 1.0f);

	/* Selected type name */
	const Zone *z = Zone_get();
	if (z->cell_type_count > 0) {
		char buf[64];
		snprintf(buf, sizeof(buf), "Type: %s", z->cell_types[godModeSelectedType].id);
		Text_render(tr, shaders, &proj, &ident,
			buf, cx - 10.0f, ty + 20.0f,
			1.0f, 1.0f, 1.0f, 0.8f);
	}
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
