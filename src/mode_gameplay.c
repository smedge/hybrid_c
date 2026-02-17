#include "mode_gameplay.h"
#include "map.h"
#include "zone.h"
#include "render.h"
#include "mat4.h"
#include "text.h"
#include "background.h"
#include "skillbar.h"
#include "catalog.h"
#include "destructible.h"
#include "player_stats.h"
#include "sub_pea.h"
#include "sub_mgun.h"
#include "sub_mine.h"
#include "hunter.h"
#include "seeker.h"
#include "defender.h"
#include "sub_aegis.h"
#include "savepoint.h"
#include "portal.h"
#include "fragment.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

typedef enum {
	GAMEPLAY_REBIRTH,
	GAMEPLAY_ACTIVE,
	WARP_PULL,
	WARP_ACCEL,
	WARP_FLASH,
	WARP_ARRIVE,
	WARP_RESUME
} GameplayState;

#define REBIRTH_DURATION 13000
#define REBIRTH_FADE_MS 2000
#define REBIRTH_MIN_ZOOM 0.05
#define REBIRTH_DEFAULT_ZOOM 0.5
#define REBIRTH_CHANNEL 1

/* Warp timing (ms) */
#define WARP_PULL_MS    600
#define WARP_ACCEL_MS   400
#define WARP_FLASH_MS   200
#define WARP_ARRIVE_MS  600
#define WARP_RESUME_MS  200

static GameplayState gameplayState;
static int rebirthTimer;
static Mix_Chunk *rebirthSample = 0;
static int selectedBgm;

/* Zone music playlist */
static int zoneMusicIndex = 0;
static int zoneMusicCount = 0;
static char zoneMusicPaths[ZONE_MAX_MUSIC][256];
static bool useZoneMusic = false;
static volatile bool zoneMusicAdvance = false;

/* Warp transition state */
static int warpTimer;
static Position warpPortalPos;      /* portal center being warped to */
static Position warpShipStartPos;   /* ship position when warp began */
static char warpDestZone[256];
static char warpDestPortalId[32];
static PlayerStatsSnapshot warpStatsSnap;
static SkillbarSnapshot warpSkillSnap;
static double warpStartZoom;

/* Data stream effect */
#define WARP_STREAM_COUNT 32
static struct {
	float angle;
	float speed;
	float length;
} warpStreams[WARP_STREAM_COUNT];

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
static bool godSnapToGrid = true;
static double godFreeWorldX = 0.0;
static double godFreeWorldY = 0.0;
static bool godMouseLeftConsumed = false;
static bool godMouseRightConsumed = false;
#define GOD_CAM_SPEED 800.0

typedef enum {
	GOD_MODE_CELLS,
	GOD_MODE_ENEMIES,
	GOD_MODE_SAVEPOINTS,
	GOD_MODE_PORTALS,
	GOD_MODE_COUNT
} GodPlacementMode;

static GodPlacementMode godPlacementMode = GOD_MODE_CELLS;

static const char *ENEMY_TYPES[] = {"mine", "hunter", "seeker", "defender"};
#define ENEMY_TYPE_COUNT 4
static int godEnemyType = 0;

static const char *GOD_MODE_NAMES[] = {"Cells", "Enemies", "Savepoints", "Portals"};

/* Zone jump menu */
static bool godZoneMenuOpen = false;
static char godZoneFiles[16][256];
static char godZoneNames[16][64];
static int godZoneFileCount = 0;

static bool escConsumed = false;

/* Load-from-save state */
static bool loadFromSave = false;
static Position loadSpawnPos = {0.0, 0.0};


/* FPS counter */
static bool fpsVisible = false;
static double fpsValue = 0.0;
static unsigned int fpsAccum = 0;
static int fpsFrames = 0;

static double ease_in_out_cubic(double t);
static void start_zone_bgm(void);
static void complete_rebirth(void);
static void god_mode_update(const Input *input, const unsigned int ticks);
static void god_mode_render_cursor(void);
static void god_mode_render_hud(const Screen *screen);
static void god_mode_render_spawn_labels(void);
static void god_mode_render_spawn_markers(void);
static void zone_teardown_and_load(const char *zone_path);
static void god_mode_jump_to_zone(const char *zone_path);
static void god_mode_scan_zones(void);
static void god_mode_create_zone(void);
static void begin_warp(void);
static void warp_update(unsigned int ticks);
static void warp_do_zone_swap(void);
static void warp_render_effects(const Screen *screen);

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
	PlayerStats_initialize();
	Fragment_initialize();
	Progression_initialize();
	Skillbar_initialize();
	Catalog_initialize();
	Zone_load("./resources/zones/zone_001.zone");
	Destructible_initialize();

	godModeActive = false;
	godModeSelectedType = 0;

	/* Start rebirth sequence */
	gameplayState = GAMEPLAY_REBIRTH;
	rebirthTimer = 0;
	View_set_scale(REBIRTH_MIN_ZOOM);

	Audio_load_sample(&rebirthSample, REBIRTH_MUSIC_PATH);
	Audio_play_sample_on_channel(&rebirthSample, REBIRTH_CHANNEL);
}

void Mode_Gameplay_initialize_from_save(void)
{
	const SaveCheckpoint *ckpt = Savepoint_get_checkpoint();
	if (!ckpt->valid) {
		/* Fallback to normal init */
		Mode_Gameplay_initialize();
		return;
	}

	Entity_destroy_all();

	selectedBgm = rand() % 7;

	View_initialize();
	Hud_initialize();
	Background_initialize();
	Grid_initialize();
	Map_initialize();
	Ship_initialize();
	PlayerStats_initialize();
	Fragment_initialize();
	Progression_initialize();
	Skillbar_initialize();
	Catalog_initialize();
	Zone_load(ckpt->zone_path);
	Destructible_initialize();

	/* Restore progression + skillbar + fragment counts from checkpoint */
	for (int i = 0; i < FRAG_TYPE_COUNT; i++)
		Fragment_set_count(i, ckpt->fragment_counts[i]);
	Progression_restore(ckpt->unlocked, ckpt->discovered);
	Skillbar_restore(ckpt->skillbar);
	Catalog_mark_all_seen();

	godModeActive = false;
	godModeSelectedType = 0;
	loadFromSave = true;
	loadSpawnPos = ckpt->position;

	Savepoint_suppress_by_id(ckpt->savepoint_id);

	/* Start rebirth sequence */
	gameplayState = GAMEPLAY_REBIRTH;
	rebirthTimer = 0;
	View_set_scale(REBIRTH_MIN_ZOOM);

	Audio_load_sample(&rebirthSample, REBIRTH_MUSIC_PATH);
	Audio_play_sample_on_channel(&rebirthSample, REBIRTH_CHANNEL);
}

void Mode_Gameplay_cleanup(void)
{
	Catalog_cleanup();
	Skillbar_cleanup();
	Progression_cleanup();
	Fragment_cleanup();
	Zone_unload();
	Ship_cleanup();
	PlayerStats_cleanup();
	Entity_destroy_all();
	Hud_cleanup();
	Mix_HookMusicFinished(NULL);
	Audio_stop_music();
	Mix_HaltChannel(REBIRTH_CHANNEL);
	Audio_unload_sample(&rebirthSample);
}

void Mode_Gameplay_update(const Input *input, const unsigned int ticks)
{
	escConsumed = false;

	/* FPS counter */
	if (input->keyL)
		fpsVisible = !fpsVisible;
	fpsAccum += ticks;
	fpsFrames++;
	if (fpsAccum >= 500) {
		fpsValue = (double)fpsFrames / (fpsAccum / 1000.0);
		fpsFrames = 0;
		fpsAccum = 0;
	}

	Background_update(ticks * 3);

	/* Advance zone music playlist (deferred from callback) */
	if (zoneMusicAdvance) {
		zoneMusicAdvance = false;
		Audio_play_music(zoneMusicPaths[zoneMusicIndex]);
	}

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
		Destructible_update(ticks);
		Fragment_update(ticks);
		Progression_update(ticks);

		/* Camera stays centered on spawn point */
		if (loadFromSave)
			View_set_position(loadSpawnPos);
		else {
			Position origin = {0.0, 0.0};
			View_set_position(origin);
		}

		if (rebirthTimer >= REBIRTH_DURATION)
			complete_rebirth();

		return;
	}

	/* Handle warp transition states */
	if (gameplayState >= WARP_PULL && gameplayState <= WARP_RESUME) {
		warp_update(ticks);
		return;
	}

	/* Toggle god mode */
	if (input->keyO) {
		godModeActive = !godModeActive;
		Ship_set_god_mode(godModeActive);
		if (godModeActive) {
			View_set_min_zoom(0.01);
		} else {
			Zone_save_if_dirty();
			View_set_min_zoom(0.25);
			View_set_scale(0.5);
		}
	}

	if (godModeActive) {
		god_mode_update(input, ticks);
		return;
	}

	/* Toggle catalog */
	if (input->keyP && !godModeActive)
		Catalog_toggle();

	if (Catalog_is_open()) {
		if (input->keyEsc)
			escConsumed = true;
		Catalog_update(input, ticks);
	}

	/* UI gets raw input; gameplay gets mouse stripped when UI consumes it */
	cursor_update(input);
	Skillbar_update(input, ticks);

	bool ui_wants_mouse = Catalog_is_open() || Skillbar_consumed_click();
	Input filtered = *input;
	if (ui_wants_mouse) {
		filtered.mouseLeft = false;
		filtered.mouseRight = false;
		filtered.mouseMiddle = false;
	}
	Entity_user_update_system(&filtered, ticks);
	PlayerStats_update(ticks);
	Entity_ai_update_system(ticks);
	Entity_collision_system();
	Portal_update_all(ticks);
	Savepoint_update_all(ticks);
	Destructible_update(ticks);
	Fragment_update(ticks);
	Progression_update(ticks);
	View_update(input, ticks);

	View_set_position(Ship_get_position());

	/* Check for portal transition trigger */
	if (Portal_has_pending_transition()) {
		begin_warp();
	}

	/* Check for cross-zone death respawn */
	if (Ship_has_pending_cross_zone_respawn()) {
		Ship_set_pending_cross_zone_respawn(false);

		const SaveCheckpoint *ckpt = Savepoint_get_checkpoint();
		if (ckpt->valid) {
			SkillbarSnapshot skillSnap = ckpt->skillbar;

			/* Zone swap (no cinematic) */
			zone_teardown_and_load(ckpt->zone_path);

			Ship_force_spawn(ckpt->position);
			Savepoint_suppress_by_id(ckpt->savepoint_id);

			/* Restore progression + fragment counts */
			for (int i = 0; i < FRAG_TYPE_COUNT; i++)
				Fragment_set_count(i, ckpt->fragment_counts[i]);
			Progression_restore(ckpt->unlocked, ckpt->discovered);
			Skillbar_restore(skillSnap);

			start_zone_bgm();
		}
	}
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

	Portal_render_deactivated();
	Entity_render_system();
	Fragment_render();
	if (godModeActive) {
		god_mode_render_spawn_markers();
		god_mode_render_cursor();
	}
	Render_flush(&world_proj, &view);

	/* God mode labels (world-space text) */
	if (godModeActive) {
		god_mode_render_spawn_labels();
		Portal_render_god_labels();
		Savepoint_render_god_labels();
	}

	/* FBO bloom pass */
	{
		int draw_w, draw_h;
		Graphics_get_drawable_size(&draw_w, &draw_h);
		Bloom *bloom = Graphics_get_bloom();

		Bloom_begin_source(bloom);
		Map_render(NULL, NULL);
		Ship_render_bloom_source();
		Mine_render_bloom_source();
		Hunter_render_bloom_source();
		Seeker_render_bloom_source();
		Defender_render_bloom_source();
		Sub_Aegis_render_bloom_source();
		Portal_render_bloom_source();
		Savepoint_render_bloom_source();
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
	PlayerStats_render(&screen);
	Progression_render(&screen);
	Savepoint_render_notification(&screen);
	Fragment_render_text(&screen);
	Catalog_render(&screen);
	if (godModeActive)
		god_mode_render_hud(&screen);

	/* FPS counter */
	if (fpsVisible) {
		char fpsBuf[32];
		snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.1f", fpsValue);
		TextRenderer *tr = Graphics_get_text_renderer();
		Shaders *shaders = Graphics_get_shaders();
		Render_flush(&ui_proj, &identity);
		Text_render(tr, shaders, &ui_proj, &identity,
			fpsBuf, screen.width - 100.0f, 15.0f,
			0.0f, 1.0f, 1.0f, 0.8f);
	}

	/* Warp visual effects overlay */
	if (gameplayState >= WARP_PULL && gameplayState <= WARP_RESUME)
		warp_render_effects(&screen);

	/* Flush everything so far, then render cursor on top */
	Render_flush(&ui_proj, &identity);
	if (gameplayState == GAMEPLAY_ACTIVE && !godModeActive)
		cursor_render();
	Render_flush(&ui_proj, &identity);

	Graphics_flip();
}

static void complete_rebirth(void)
{
	gameplayState = GAMEPLAY_ACTIVE;

	/* Spawn ship — at checkpoint if loading, otherwise origin */
	Position spawn = {0.0, 0.0};
	if (loadFromSave) {
		spawn = loadSpawnPos;
		loadFromSave = false;
	}
	Ship_force_spawn(spawn);

	/* Start zone BGM */
	start_zone_bgm();

	/* Fade out Memory Man over 2 seconds */
	Audio_fade_out_channel(REBIRTH_CHANNEL, REBIRTH_FADE_MS);
}

static void on_music_finished(void)
{
	if (!useZoneMusic || zoneMusicCount == 0) return;
	zoneMusicIndex = (zoneMusicIndex + 1) % zoneMusicCount;
	zoneMusicAdvance = true;
}

static void start_zone_bgm(void)
{
	const Zone *z = Zone_get();
	if (z->music_count > 0) {
		useZoneMusic = true;
		zoneMusicCount = z->music_count;
		for (int i = 0; i < z->music_count; i++)
			strncpy(zoneMusicPaths[i], z->music_paths[i], 255);
		zoneMusicIndex = 0;
		zoneMusicAdvance = false;
		Mix_HookMusicFinished(on_music_finished);
		Audio_play_music(zoneMusicPaths[0]);
	} else {
		useZoneMusic = false;
		Mix_HookMusicFinished(NULL);
		selectedBgm = rand() % 7;
		Audio_loop_music(bgm_paths[selectedBgm]);
	}
}

static void god_mode_update(const Input *input, const unsigned int ticks)
{
	/* Handle zone jump menu */
	if (godZoneMenuOpen) {
		if (input->keyEsc) {
			godZoneMenuOpen = false;
			escConsumed = true;
			return;
		}
		if (input->keySlot >= 0 && input->keySlot < godZoneFileCount) {
			god_mode_jump_to_zone(godZoneFiles[input->keySlot]);
			godZoneMenuOpen = false;
			return;
		}
		return;
	}

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

	/* Toggle snap-to-grid with X */
	if (input->keyX)
		godSnapToGrid = !godSnapToGrid;

	/* Store free world position for non-snapped placement */
	godFreeWorldX = worldPos.x;
	godFreeWorldY = worldPos.y;

	/* Snap to grid */
	int cell_x = (int)floor(worldPos.x / MAP_CELL_SIZE);
	int cell_y = (int)floor(worldPos.y / MAP_CELL_SIZE);
	int grid_x = cell_x + HALF_MAP_SIZE;
	int grid_y = cell_y + HALF_MAP_SIZE;

	godModeCursorValid = (grid_x >= 0 && grid_x < MAP_SIZE &&
	                      grid_y >= 0 && grid_y < MAP_SIZE);
	godModeCursorX = grid_x;
	godModeCursorY = grid_y;

	/* Mode switching: Q = prev, E = next */
	if (input->keyQ) {
		godPlacementMode = (godPlacementMode + GOD_MODE_COUNT - 1) % GOD_MODE_COUNT;
	}
	if (input->keyE) {
		godPlacementMode = (godPlacementMode + 1) % GOD_MODE_COUNT;
	}

	/* Tab cycles sub-type within mode */
	if (input->keyTab) {
		const Zone *z = Zone_get();
		switch (godPlacementMode) {
		case GOD_MODE_CELLS:
			if (z->cell_type_count > 0)
				godModeSelectedType = (godModeSelectedType + 1) % z->cell_type_count;
			break;
		case GOD_MODE_ENEMIES:
			godEnemyType = (godEnemyType + 1) % ENEMY_TYPE_COUNT;
			break;
		default:
			break;
		}
	}

	/* Reset consumed flags when buttons released */
	if (!input->mouseLeft) godMouseLeftConsumed = false;
	if (!input->mouseRight) godMouseRightConsumed = false;

	/* Single-click for entity modes, continuous for cells */
	bool lmbClick = input->mouseLeft && godModeCursorValid;
	bool lmbSingle = lmbClick && !godMouseLeftConsumed;
	bool rmbClick = input->mouseRight && godModeCursorValid;
	bool rmbSingle = rmbClick && !godMouseRightConsumed;

	/* LMB placement per mode */
	if (lmbClick && godPlacementMode == GOD_MODE_CELLS) {
		const Zone *z = Zone_get();
		if (z->cell_type_count > 0 &&
		    z->cell_grid[grid_x][grid_y] != godModeSelectedType) {
			Zone_place_cell(grid_x, grid_y, z->cell_types[godModeSelectedType].id);
		}
	}
	if (lmbSingle) {
		switch (godPlacementMode) {
		case GOD_MODE_ENEMIES: {
			double wx, wy;
			if (godSnapToGrid) {
				wx = (grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
				wy = (grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
			} else {
				wx = godFreeWorldX;
				wy = godFreeWorldY;
			}
			Zone_place_spawn(ENEMY_TYPES[godEnemyType], wx, wy);
			godMouseLeftConsumed = true;
			break;
		}
		case GOD_MODE_SAVEPOINTS: {
			const Zone *z = Zone_get();
			char id[32];
			snprintf(id, sizeof(id), "save_%d", z->savepoint_count + 1);
			Zone_place_savepoint(grid_x, grid_y, id);
			godMouseLeftConsumed = true;
			break;
		}
		case GOD_MODE_PORTALS: {
			const Zone *z = Zone_get();
			char id[32];
			snprintf(id, sizeof(id), "portal_%d", z->portal_count + 1);
			Zone_place_portal(grid_x, grid_y, id, "", "");
			godMouseLeftConsumed = true;
			break;
		}
		default:
			break;
		}
	}

	/* RMB removal per mode */
	if (rmbClick && godPlacementMode == GOD_MODE_CELLS) {
		Zone_remove_cell(grid_x, grid_y);
	}
	if (rmbSingle) {
		switch (godPlacementMode) {
		case GOD_MODE_ENEMIES: {
			double wx, wy;
			if (godSnapToGrid) {
				wx = (grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
				wy = (grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
			} else {
				wx = godFreeWorldX;
				wy = godFreeWorldY;
			}
			int idx = Zone_find_spawn_near(wx, wy, MAP_CELL_SIZE);
			if (idx >= 0) Zone_remove_spawn(idx);
			godMouseRightConsumed = true;
			break;
		}
		case GOD_MODE_SAVEPOINTS:
			Zone_remove_savepoint(grid_x, grid_y);
			godMouseRightConsumed = true;
			break;
		case GOD_MODE_PORTALS:
			Zone_remove_portal(grid_x, grid_y);
			godMouseRightConsumed = true;
			break;
		default:
			break;
		}
	}

	/* Undo with Ctrl+Z */
	if (input->keyZ) {
		Zone_undo();
	}

	/* Zone jump: J key */
	if (input->keyJ) {
		god_mode_scan_zones();
		godZoneMenuOpen = true;
	}

	/* New zone: N key */
	if (input->keyN) {
		god_mode_create_zone();
	}

	/* AI still runs so the world feels alive */
	Entity_ai_update_system(ticks);
	Destructible_update(ticks);
	Fragment_update(ticks);
	Progression_update(ticks);

	/* Flush pending zone saves at most once per frame */
	Zone_save_if_dirty();
}

static void god_mode_render_cursor(void)
{
	if (!godModeCursorValid) return;

	float wx = (float)(godModeCursorX - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	float wy = (float)(godModeCursorY - HALF_MAP_SIZE) * MAP_CELL_SIZE;

	float r, g, b, a = 0.5f;

	switch (godPlacementMode) {
	case GOD_MODE_CELLS: {
		/* Cell mode: always snapped, colored cell outline + fill */
		const Zone *z = Zone_get();
		r = 1.0f; g = 1.0f; b = 1.0f;
		if (z->cell_type_count > 0) {
			ColorRGB c = z->cell_types[godModeSelectedType].outlineColor;
			r = c.red / 255.0f;
			g = c.green / 255.0f;
			b = c.blue / 255.0f;
		}
		float s = MAP_CELL_SIZE;
		Render_line_segment(wx, wy, wx + s, wy, r, g, b, a);
		Render_line_segment(wx + s, wy, wx + s, wy + s, r, g, b, a);
		Render_line_segment(wx + s, wy + s, wx, wy + s, r, g, b, a);
		Render_line_segment(wx, wy + s, wx, wy, r, g, b, a);
		Render_quad_absolute(wx, wy, wx + s, wy + s, r, g, b, a * 0.3f);
		break;
	}
	case GOD_MODE_ENEMIES:
		r = 1.0f; g = 0.3f; b = 0.2f;
		goto render_crosshair;
	case GOD_MODE_SAVEPOINTS:
		r = 0.2f; g = 1.0f; b = 0.3f;
		goto render_crosshair;
	case GOD_MODE_PORTALS:
		r = 0.5f; g = 0.3f; b = 1.0f;
		goto render_crosshair;
	render_crosshair: {
		/* Entity modes: use free position when snap is off */
		float cx, cy;
		if (godSnapToGrid) {
			cx = wx;
			cy = wy;
		} else {
			cx = (float)godFreeWorldX;
			cy = (float)godFreeWorldY;
		}
		float cs = 15.0f;
		Render_thick_line(cx - cs, cy, cx + cs, cy, 2.0f, r, g, b, a);
		Render_thick_line(cx, cy - cs, cx, cy + cs, 2.0f, r, g, b, a);
		/* Small diamond */
		float ds = 6.0f;
		Render_thick_line(cx - ds, cy, cx, cy - ds, 1.5f, r, g, b, 0.8f);
		Render_thick_line(cx, cy - ds, cx + ds, cy, 1.5f, r, g, b, 0.8f);
		Render_thick_line(cx + ds, cy, cx, cy + ds, 1.5f, r, g, b, 0.8f);
		Render_thick_line(cx, cy + ds, cx - ds, cy, 1.5f, r, g, b, 0.8f);
		break;
	}
	default:
		break;
	}
}

static void god_mode_render_hud(const Screen *screen)
{
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	float cx = (float)screen->width * 0.5f - 60.0f;
	float ty = 20.0f;
	float line_h = 18.0f;
	char buf[128];

	Render_flush(&proj, &ident);

	/* "GOD MODE" top-center */
	Text_render(tr, shaders, &proj, &ident,
		"GOD MODE", cx, ty,
		1.0f, 0.3f, 0.3f, 1.0f);

	/* Mode name */
	snprintf(buf, sizeof(buf), "Mode: %s (Q/E)", GOD_MODE_NAMES[godPlacementMode]);
	Text_render(tr, shaders, &proj, &ident,
		buf, cx, ty + line_h,
		1.0f, 1.0f, 1.0f, 0.8f);

	/* Sub-type (cells and enemies only) */
	const Zone *z = Zone_get();
	switch (godPlacementMode) {
	case GOD_MODE_CELLS:
		if (z->cell_type_count > 0) {
			snprintf(buf, sizeof(buf), "Type: %s (Tab)", z->cell_types[godModeSelectedType].id);
			Text_render(tr, shaders, &proj, &ident,
				buf, cx, ty + line_h * 2,
				1.0f, 1.0f, 1.0f, 0.8f);
		}
		break;
	case GOD_MODE_ENEMIES:
		snprintf(buf, sizeof(buf), "Type: %s (Tab)", ENEMY_TYPES[godEnemyType]);
		Text_render(tr, shaders, &proj, &ident,
			buf, cx, ty + line_h * 2,
			1.0f, 0.5f, 0.3f, 0.8f);
		break;
	default:
		break;
	}

	/* Grid coordinates */
	if (godModeCursorValid) {
		snprintf(buf, sizeof(buf), "Grid: (%d, %d)", godModeCursorX, godModeCursorY);
		Text_render(tr, shaders, &proj, &ident,
			buf, cx, ty + line_h * 3,
			0.7f, 0.7f, 0.7f, 0.8f);
	}

	/* Snap toggle */
	snprintf(buf, sizeof(buf), "Snap: %s (X)", godSnapToGrid ? "ON" : "OFF");
	Text_render(tr, shaders, &proj, &ident,
		buf, cx, ty + line_h * 4,
		godSnapToGrid ? 0.3f : 1.0f,
		godSnapToGrid ? 1.0f : 0.6f,
		godSnapToGrid ? 0.3f : 0.2f, 0.8f);

	/* Zone name */
	snprintf(buf, sizeof(buf), "Zone: %s", z->name);
	Text_render(tr, shaders, &proj, &ident,
		buf, cx, ty + line_h * 5,
		0.7f, 0.7f, 0.7f, 0.8f);

	/* Zone jump menu */
	if (godZoneMenuOpen) {
		float mx = (float)screen->width * 0.5f - 100.0f;
		float my = (float)screen->height * 0.3f;

		/* Dark background */
		Render_quad_absolute(mx - 10, my - 10,
			mx + 250, my + 20 + godZoneFileCount * line_h,
			0.0f, 0.0f, 0.0f, 0.8f);
		Render_flush(&proj, &ident);

		Text_render(tr, shaders, &proj, &ident,
			"=== ZONE JUMP ===", mx, my,
			1.0f, 1.0f, 0.3f, 1.0f);

		for (int i = 0; i < godZoneFileCount && i < 10; i++) {
			snprintf(buf, sizeof(buf), "%d: %s", (i + 1) % 10, godZoneNames[i]);
			Text_render(tr, shaders, &proj, &ident,
				buf, mx, my + line_h * (i + 1),
				1.0f, 1.0f, 1.0f, 0.8f);
		}
	}
}

/* --- Spawn labels in godmode --- */

static void god_mode_render_spawn_labels(void)
{
	const Zone *z = Zone_get();
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Screen screen = Graphics_get_screen();
	Mat4 ui_proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();
	Mat4 view = View_get_transform(&screen);

	for (int i = 0; i < z->spawn_count; i++) {
		/* World to screen coordinates */
		float sx, sy;
		Mat4_transform_point(&view, (float)z->spawns[i].world_x,
			(float)z->spawns[i].world_y, &sx, &sy);
		sy = (float)screen.height - sy;

		Text_render(tr, shaders, &ui_proj, &ident,
			z->spawns[i].enemy_type, sx - 20.0f, sy - 30.0f,
			1.0f, 0.5f, 0.3f, 0.7f);
	}
}

static void god_mode_render_spawn_markers(void)
{
	const Zone *z = Zone_get();
	float ds = 4.0f;

	/* Enemy spawn dots — red/orange */
	for (int i = 0; i < z->spawn_count; i++) {
		float wx = (float)z->spawns[i].world_x;
		float wy = (float)z->spawns[i].world_y;
		Render_quad_absolute(wx - ds, wy - ds, wx + ds, wy + ds,
			1.0f, 0.4f, 0.2f, 0.9f);
	}

	/* Portal dots — purple */
	for (int i = 0; i < z->portal_count; i++) {
		float wx = (float)(z->portals[i].grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		float wy = (float)(z->portals[i].grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		Render_quad_absolute(wx - ds, wy - ds, wx + ds, wy + ds,
			0.5f, 0.3f, 1.0f, 0.9f);
	}

	/* Savepoint dots — green */
	for (int i = 0; i < z->savepoint_count; i++) {
		float wx = (float)(z->savepoints[i].grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		float wy = (float)(z->savepoints[i].grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		Render_quad_absolute(wx - ds, wy - ds, wx + ds, wy + ds,
			0.2f, 1.0f, 0.3f, 0.9f);
	}
}

/* --- Zone teardown/load helper --- */

static void zone_teardown_and_load(const char *zone_path)
{
	Zone_save_if_dirty();
	Sub_Pea_cleanup();
	Sub_Mgun_cleanup();
	Sub_Mine_cleanup();
	Fragment_deactivate_all();
	Zone_unload();
	Entity_destroy_all();
	Grid_initialize();
	Map_initialize();
	Ship_initialize();
	Zone_load(zone_path);
	Destructible_initialize();
}

/* --- Zone navigation --- */

static void god_mode_scan_zones(void)
{
	godZoneFileCount = 0;
	DIR *dir = opendir("./resources/zones");
	if (!dir) return;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL && godZoneFileCount < 16) {
		size_t len = strlen(entry->d_name);
		if (len < 6) continue;
		if (strcmp(entry->d_name + len - 5, ".zone") != 0) continue;

		snprintf(godZoneFiles[godZoneFileCount], 256,
			"./resources/zones/%s", entry->d_name);

		/* Parse zone name from file */
		FILE *f = fopen(godZoneFiles[godZoneFileCount], "r");
		if (f) {
			char line[512];
			godZoneNames[godZoneFileCount][0] = '\0';
			while (fgets(line, sizeof(line), f)) {
				if (strncmp(line, "name ", 5) == 0) {
					size_t nlen = strlen(line + 5);
					if (nlen > 0 && line[5 + nlen - 1] == '\n')
						line[5 + nlen - 1] = '\0';
					snprintf(godZoneNames[godZoneFileCount], 64, "%s (%s)",
						line + 5, entry->d_name);
					break;
				}
			}
			fclose(f);
			if (godZoneNames[godZoneFileCount][0] == '\0')
				snprintf(godZoneNames[godZoneFileCount], 64, "%s", entry->d_name);
		}
		godZoneFileCount++;
	}
	closedir(dir);
}

static void god_mode_jump_to_zone(const char *zone_path)
{
	zone_teardown_and_load(zone_path);

	/* Spawn at first savepoint or origin */
	Position spawn = {0.0, 0.0};
	const Zone *z = Zone_get();
	if (z->savepoint_count > 0) {
		spawn.x = (z->savepoints[0].grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		spawn.y = (z->savepoints[0].grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	}
	Ship_force_spawn(spawn);

	Ship_set_god_mode(true);
	godPlacementMode = GOD_MODE_CELLS;
	godModeSelectedType = 0;

	start_zone_bgm();
}

static void god_mode_create_zone(void)
{
	/* Find next available zone number */
	int next_num = 1;
	DIR *dir = opendir("./resources/zones");
	if (dir) {
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL) {
			int num;
			if (sscanf(entry->d_name, "zone_%d.zone", &num) == 1) {
				if (num >= next_num)
					next_num = num + 1;
			}
		}
		closedir(dir);
	}

	char path[256];
	snprintf(path, sizeof(path), "./resources/zones/zone_%03d.zone", next_num);

	/* Write minimal zone file, copying cell types from current zone */
	FILE *f = fopen(path, "w");
	if (!f) {
		printf("god_mode_create_zone: failed to create '%s'\n", path);
		return;
	}

	char zone_name[64];
	snprintf(zone_name, sizeof(zone_name), "New Zone %d", next_num);

	fprintf(f, "# Zone: %s\n", zone_name);
	fprintf(f, "name %s\n", zone_name);
	fprintf(f, "size %d\n", MAP_SIZE);

	/* Copy background colors from current zone */
	const Zone *z = Zone_get();
	if (z->has_bg_colors) {
		for (int i = 0; i < 4; i++) {
			fprintf(f, "bgcolor %d %d %d %d\n", i,
				z->bg_colors[i].red, z->bg_colors[i].green,
				z->bg_colors[i].blue);
		}
	}

	fprintf(f, "\n");

	/* Copy cell types from current zone for palette continuity */
	for (int i = 0; i < z->cell_type_count; i++) {
		const ZoneCellType *ct = &z->cell_types[i];
		fprintf(f, "celltype %s %d %d %d %d %d %d %d %d %s\n",
			ct->id,
			ct->primaryColor.red, ct->primaryColor.green,
			ct->primaryColor.blue, ct->primaryColor.alpha,
			ct->outlineColor.red, ct->outlineColor.green,
			ct->outlineColor.blue, ct->outlineColor.alpha,
			ct->pattern);
	}

	fclose(f);
	printf("Created new zone: %s\n", path);

	/* Jump to the new zone */
	god_mode_jump_to_zone(path);
}

/* --- Warp transition --- */

static void begin_warp(void)
{
	strncpy(warpDestZone, Portal_get_pending_dest_zone(), sizeof(warpDestZone) - 1);
	warpDestZone[sizeof(warpDestZone) - 1] = '\0';
	strncpy(warpDestPortalId, Portal_get_pending_dest_portal_id(), sizeof(warpDestPortalId) - 1);
	warpDestPortalId[sizeof(warpDestPortalId) - 1] = '\0';
	Portal_clear_pending_transition();

	/* Snapshot player state */
	warpStatsSnap = PlayerStats_snapshot();
	warpSkillSnap = Skillbar_snapshot();

	/* Record ship start position and portal position */
	warpShipStartPos = Ship_get_position();
	/* Use ship position as portal center (ship is standing on it) */
	warpPortalPos = warpShipStartPos;
	warpStartZoom = View_get_view().scale;

	/* Init data stream effects */
	for (int i = 0; i < WARP_STREAM_COUNT; i++) {
		warpStreams[i].angle = (float)i * (360.0f / WARP_STREAM_COUNT) +
			((float)(rand() % 100) / 100.0f) * 10.0f;
		warpStreams[i].speed = 0.5f + (float)(rand() % 100) / 100.0f;
		warpStreams[i].length = 30.0f + (float)(rand() % 50);
	}

	gameplayState = WARP_PULL;
	warpTimer = 0;

	printf("Warp: begin -> %s @ %s\n", warpDestZone, warpDestPortalId);
}

static void warp_update(unsigned int ticks)
{
	warpTimer += ticks;
	Background_update(ticks * 3);

	switch (gameplayState) {
	case WARP_PULL: {
		/* Check if ship died during pull */
		if (Ship_is_destroyed()) {
			gameplayState = GAMEPLAY_ACTIVE;
			return;
		}

		/* Ship interpolates toward portal center, input disabled */
		float t = (float)warpTimer / WARP_PULL_MS;
		if (t > 1.0f) t = 1.0f;
		float eased = t * t; /* ease-in */

		Position pos;
		pos.x = warpShipStartPos.x + (warpPortalPos.x - warpShipStartPos.x) * eased;
		pos.y = warpShipStartPos.y + (warpPortalPos.y - warpShipStartPos.y) * eased;
		Ship_set_position(pos);
		View_set_position(pos);

		if (warpTimer >= WARP_PULL_MS) {
			gameplayState = WARP_ACCEL;
			warpTimer = 0;
		}
		break;
	}
	case WARP_ACCEL: {
		/* Camera zooms in rapidly */
		float t = (float)warpTimer / WARP_ACCEL_MS;
		if (t > 1.0f) t = 1.0f;

		double zoom = warpStartZoom + (2.0 - warpStartZoom) * t * t;
		View_set_scale(zoom);
		View_set_position(warpPortalPos);

		if (warpTimer >= WARP_ACCEL_MS) {
			gameplayState = WARP_FLASH;
			warpTimer = 0;
			/* Zone swap happens at start of flash */
			warp_do_zone_swap();
		}
		break;
	}
	case WARP_FLASH:
		/* Full white screen, zone already swapped */
		if (warpTimer >= WARP_FLASH_MS) {
			gameplayState = WARP_ARRIVE;
			warpTimer = 0;
		}
		break;
	case WARP_ARRIVE: {
		/* Camera zooms back out to normal */
		float t = (float)warpTimer / WARP_ARRIVE_MS;
		if (t > 1.0f) t = 1.0f;
		float eased = 1.0f - (1.0f - t) * (1.0f - t); /* ease-out */

		double zoom = 2.0 + (REBIRTH_DEFAULT_ZOOM - 2.0) * eased;
		View_set_scale(zoom);
		View_set_position(Ship_get_position());

		Entity_ai_update_system(ticks);

		if (warpTimer >= WARP_ARRIVE_MS) {
			gameplayState = WARP_RESUME;
			warpTimer = 0;
		}
		break;
	}
	case WARP_RESUME:
		/* Brief pause before returning control */
		View_set_scale(REBIRTH_DEFAULT_ZOOM);
		View_set_position(Ship_get_position());

		if (warpTimer >= WARP_RESUME_MS) {
			gameplayState = GAMEPLAY_ACTIVE;
			printf("Warp: complete\n");
		}
		break;
	default:
		break;
	}
}

static void warp_do_zone_swap(void)
{
	zone_teardown_and_load(warpDestZone);

	/* 6. Find destination portal and spawn ship there */
	Position arrival = {0.0, 0.0};
	if (!Portal_get_position_by_id(warpDestPortalId, &arrival)) {
		printf("Warp: destination portal '%s' not found, spawning at origin\n",
			warpDestPortalId);
	}
	Ship_force_spawn(arrival);

	/* 7. Restore player state */
	PlayerStats_restore(warpStatsSnap);
	Skillbar_restore(warpSkillSnap);

	/* 8. Suppress re-trigger at arrival portal */
	Portal_suppress_arrival(warpDestPortalId);

	/* 9. New BGM */
	start_zone_bgm();

	printf("Warp: zone swap complete, arrived at (%.0f, %.0f)\n",
		arrival.x, arrival.y);
}

static void warp_render_effects(const Screen *screen)
{
	float sw = (float)screen->width;
	float sh = (float)screen->height;

	switch (gameplayState) {
	case WARP_PULL: {
		/* Gentle fade to white over pull duration */
		float t = (float)warpTimer / WARP_PULL_MS;
		float alpha = t * 0.2f;
		Render_quad_absolute(0, 0, sw, sh, 1.0f, 1.0f, 1.0f, alpha);
		break;
	}
	case WARP_ACCEL: {
		/* Continue fade to white */
		float t = (float)warpTimer / WARP_ACCEL_MS;
		float alpha = 0.2f + t * 0.8f;
		Render_quad_absolute(0, 0, sw, sh, 1.0f, 1.0f, 1.0f, alpha);
		break;
	}
	case WARP_FLASH:
		/* Full white screen */
		Render_quad_absolute(0, 0, sw, sh, 1.0f, 1.0f, 1.0f, 1.0f);
		break;
	case WARP_ARRIVE: {
		/* White flash fading out + converging data streams */
		float t = (float)warpTimer / WARP_ARRIVE_MS;
		float flashAlpha = 1.0f - t;
		if (flashAlpha > 0.0f)
			Render_quad_absolute(0, 0, sw, sh, 1.0f, 1.0f, 1.0f, flashAlpha);

		/* Converging streams (reverse of accel) */
		float cx = sw * 0.5f;
		float cy = sh * 0.5f;
		float streamAlpha = (1.0f - t) * 0.6f;
		if (streamAlpha > 0.0f) {
			for (int i = 0; i < WARP_STREAM_COUNT; i++) {
				float angle = warpStreams[i].angle * M_PI / 180.0f;
				float outerR = 400.0f * (1.0f - t);
				float innerR = outerR - warpStreams[i].length * (1.0f - t);
				if (innerR < 0.0f) innerR = 0.0f;
				float x0 = cx + cosf(angle) * innerR;
				float y0 = cy + sinf(angle) * innerR;
				float x1 = cx + cosf(angle) * outerR;
				float y1 = cy + sinf(angle) * outerR;
				Render_thick_line(x0, y0, x1, y1,
					1.5f, 0.7f, 0.9f, 1.0f, streamAlpha);
			}
		}
		break;
	}
	case WARP_RESUME:
		/* Nothing — just a brief pause */
		break;
	default:
		break;
	}
}

bool Mode_Gameplay_consumed_esc(void)
{
	return escConsumed;
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
