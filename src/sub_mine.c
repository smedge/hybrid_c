#include "sub_mine.h"

#include "ship.h"
#include "progression.h"
#include "render.h"
#include "audio.h"
#include "shipstate.h"
#include "color.h"
#include "view.h"

#include <SDL2/SDL_mixer.h>

#define MAX_PLAYER_MINES 3
#define ARMED_TIME 2000
#define EXPLODING_TIME 100
#define PLACE_COOLDOWN 250
#define EXPLOSION_SIZE 250.0
#define MINE_SIZE 10.0

typedef enum {
	MINE_INACTIVE,
	MINE_ARMED,
	MINE_EXPLODING
} PlayerMinePhase;

typedef struct {
	bool active;
	PlayerMinePhase phase;
	Position position;
	unsigned int phaseTicks;
} PlayerMine;

static PlayerMine mines[MAX_PLAYER_MINES];
static int cooldownTimer;

static Mix_Chunk *samplePlace = 0;
static Mix_Chunk *sampleExplode = 0;

static const ColorRGB COLOR_DARK_RGB = {50, 50, 50, 255};
static const ColorRGB COLOR_ACTIVE_RGB = {255, 0, 0, 255};
static const ColorRGB COLOR_WHITE_RGB = {255, 255, 255, 255};

static ColorFloat colorDark, colorActive, colorWhite;

void Sub_Mine_initialize(void)
{
	cooldownTimer = 0;

	for (int i = 0; i < MAX_PLAYER_MINES; i++)
		mines[i].active = false;

	Audio_load_sample(&samplePlace, "resources/sounds/bomb_set.wav");
	Audio_load_sample(&sampleExplode, "resources/sounds/bomb_explode.wav");

	colorDark = Color_rgb_to_float(&COLOR_DARK_RGB);
	colorActive = Color_rgb_to_float(&COLOR_ACTIVE_RGB);
	colorWhite = Color_rgb_to_float(&COLOR_WHITE_RGB);
}

void Sub_Mine_cleanup(void)
{
	Audio_unload_sample(&samplePlace);
	Audio_unload_sample(&sampleExplode);
}

void Sub_Mine_update(const Input *userInput, const unsigned int ticks)
{
	if (cooldownTimer > 0)
		cooldownTimer -= ticks;

	if (userInput->keySpace && cooldownTimer <= 0
			&& Progression_is_unlocked(SUB_ID_MINE)) {
		/* Only place if there's a free slot — 3 max, no overwriting */
		int slot = -1;
		for (int i = 0; i < MAX_PLAYER_MINES; i++) {
			if (!mines[i].active) {
				slot = i;
				break;
			}
		}

		if (slot >= 0) {
			cooldownTimer = PLACE_COOLDOWN;

			PlayerMine *m = &mines[slot];
			m->active = true;
			m->phase = MINE_ARMED;
			m->position = Ship_get_position();
			m->phaseTicks = 0;

			Audio_play_sample(&samplePlace);
		}
	}

	for (int i = 0; i < MAX_PLAYER_MINES; i++) {
		PlayerMine *m = &mines[i];
		if (!m->active)
			continue;

		m->phaseTicks += ticks;

		switch (m->phase) {
		case MINE_ARMED:
			if (m->phaseTicks >= ARMED_TIME) {
				m->phase = MINE_EXPLODING;
				m->phaseTicks = 0;
				Audio_play_sample(&sampleExplode);
			}
			break;

		case MINE_EXPLODING:
			if (m->phaseTicks >= EXPLODING_TIME)
				m->active = false;
			break;

		case MINE_INACTIVE:
			break;
		}
	}
}

void Sub_Mine_render(void)
{
	for (int i = 0; i < MAX_PLAYER_MINES; i++) {
		PlayerMine *m = &mines[i];
		if (!m->active)
			continue;

		switch (m->phase) {
		case MINE_ARMED: {
			Rectangle rect = {-MINE_SIZE, MINE_SIZE, MINE_SIZE, -MINE_SIZE};
			View view = View_get_view();
			if (view.scale > 0.09)
				Render_quad(&m->position, 45.0, rect, &colorDark);
			Render_point(&m->position, 3.0, &colorActive);
			break;
		}

		case MINE_EXPLODING: {
			Rectangle explosion = {-EXPLOSION_SIZE, EXPLOSION_SIZE,
				EXPLOSION_SIZE, -EXPLOSION_SIZE};
			Render_quad(&m->position, 22.5, explosion, &colorWhite);
			Render_quad(&m->position, 67.5, explosion, &colorWhite);
			break;
		}

		case MINE_INACTIVE:
			break;
		}
	}
}

void Sub_Mine_render_bloom_source(void)
{
	for (int i = 0; i < MAX_PLAYER_MINES; i++) {
		PlayerMine *m = &mines[i];
		if (!m->active)
			continue;

		switch (m->phase) {
		case MINE_ARMED:
			Render_point(&m->position, 3.0, &colorActive);
			break;

		case MINE_EXPLODING: {
			Rectangle explosion = {-EXPLOSION_SIZE, EXPLOSION_SIZE,
				EXPLOSION_SIZE, -EXPLOSION_SIZE};
			Render_quad(&m->position, 22.5, explosion, &colorWhite);
			Render_quad(&m->position, 67.5, explosion, &colorWhite);
			break;
		}

		case MINE_INACTIVE:
			break;
		}
	}
}

void Sub_Mine_deactivate_all(void)
{
	for (int i = 0; i < MAX_PLAYER_MINES; i++)
		mines[i].active = false;
}

bool Sub_Mine_check_hit(Rectangle target)
{
	for (int i = 0; i < MAX_PLAYER_MINES; i++) {
		PlayerMine *m = &mines[i];
		if (!m->active || m->phase != MINE_EXPLODING)
			continue;

		Rectangle blastArea = Collision_transform_bounding_box(m->position,
			(Rectangle){-EXPLOSION_SIZE, EXPLOSION_SIZE,
				EXPLOSION_SIZE, -EXPLOSION_SIZE});

		if (Collision_aabb_test(blastArea, target))
			return true;
	}
	return false;
}
