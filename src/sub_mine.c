#include "sub_mine.h"

#include "sub_mine_core.h"
#include "ship.h"
#include "progression.h"
#include "render.h"
#include "audio.h"
#include "skillbar.h"
#include "sub_stealth.h"
#include "player_stats.h"

#include <SDL2/SDL_mixer.h>

#define MAX_PLAYER_MINES 3
#define PLACE_COOLDOWN 250

static const SubMineConfig playerMineCfg = {
	.armed_duration_ms = 2000,
	.exploding_duration_ms = 100,
	.dead_duration_ms = 0,
	.explosion_half_size = 250.0f,
	.body_half_size = 10.0f,
	.body_r = 0.196f, .body_g = 0.196f, .body_b = 0.196f, .body_a = 1.0f,
	.blink_r = 1.0f, .blink_g = 0.0f, .blink_b = 0.0f, .blink_a = 1.0f,
	.explosion_r = 1.0f, .explosion_g = 1.0f, .explosion_b = 1.0f, .explosion_a = 1.0f,
	.light_armed_radius = 180.0f,
	.light_armed_r = 1.0f, .light_armed_g = 0.2f, .light_armed_b = 0.1f, .light_armed_a = 0.4f,
	.light_explode_radius = 750.0f,
	.light_explode_r = 1.0f, .light_explode_g = 0.9f, .light_explode_b = 0.7f, .light_explode_a = 1.0f,
};

static SubMineCore mines[MAX_PLAYER_MINES];
static int cooldownTimer;

static Mix_Chunk *samplePlace = 0;
static Mix_Chunk *sampleExplode = 0;

void Sub_Mine_initialize(void)
{
	cooldownTimer = 0;

	for (int i = 0; i < MAX_PLAYER_MINES; i++)
		SubMine_init(&mines[i]);

	Audio_load_sample(&samplePlace, "resources/sounds/bomb_set.wav");
	Audio_load_sample(&sampleExplode, "resources/sounds/bomb_explode.wav");
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
			&& Skillbar_is_active(SUB_ID_MINE)) {
		Sub_Stealth_break_attack();
		/* Only place if there's a free slot */
		int slot = -1;
		for (int i = 0; i < MAX_PLAYER_MINES; i++) {
			if (mines[i].phase == MINE_PHASE_DORMANT) {
				slot = i;
				break;
			}
		}

		if (slot >= 0) {
			cooldownTimer = PLACE_COOLDOWN;
			SubMine_arm(&mines[slot], &playerMineCfg, Ship_get_position());
			Audio_play_sample(&samplePlace);
		}
	}

	for (int i = 0; i < MAX_PLAYER_MINES; i++) {
		SubMineCore *m = &mines[i];
		if (m->phase == MINE_PHASE_DORMANT)
			continue;

		MinePhase prevPhase = m->phase;
		SubMine_update(m, &playerMineCfg, ticks);

		/* Just transitioned to EXPLODING */
		if (m->phase == MINE_PHASE_EXPLODING && prevPhase == MINE_PHASE_ARMED) {
			Audio_play_sample(&sampleExplode);
			PlayerStats_add_feedback(15.0);

			/* Player's own mines hurt the player */
			Position shipPos = Ship_get_position();
			double dx = shipPos.x - m->position.x;
			double dy = shipPos.y - m->position.y;
			float hs = playerMineCfg.explosion_half_size;
			if (dx >= -hs && dx <= hs && dy >= -hs && dy <= hs)
				PlayerStats_damage(100.0);
		}
	}
}

void Sub_Mine_render(void)
{
	for (int i = 0; i < MAX_PLAYER_MINES; i++) {
		if (mines[i].phase == MINE_PHASE_DORMANT)
			continue;
		SubMine_render(&mines[i], &playerMineCfg);
	}
}

void Sub_Mine_render_bloom_source(void)
{
	for (int i = 0; i < MAX_PLAYER_MINES; i++) {
		if (mines[i].phase == MINE_PHASE_DORMANT)
			continue;
		SubMine_render_bloom(&mines[i], &playerMineCfg);
	}
}

void Sub_Mine_render_light_source(void)
{
	for (int i = 0; i < MAX_PLAYER_MINES; i++) {
		if (mines[i].phase == MINE_PHASE_DORMANT)
			continue;
		SubMine_render_light(&mines[i], &playerMineCfg);
	}
}

void Sub_Mine_deactivate_all(void)
{
	for (int i = 0; i < MAX_PLAYER_MINES; i++)
		SubMine_init(&mines[i]);
}

float Sub_Mine_get_cooldown_fraction(void)
{
	/* All slots occupied — show fully disabled until one frees up */
	bool has_free_slot = false;
	for (int i = 0; i < MAX_PLAYER_MINES; i++) {
		if (mines[i].phase == MINE_PHASE_DORMANT) {
			has_free_slot = true;
			break;
		}
	}
	if (!has_free_slot) return 1.0f;

	if (cooldownTimer <= 0) return 0.0f;
	return (float)cooldownTimer / PLACE_COOLDOWN;
}

double Sub_Mine_check_hit(Rectangle target)
{
	for (int i = 0; i < MAX_PLAYER_MINES; i++) {
		if (SubMine_check_explosion_hit(&mines[i], &playerMineCfg, target))
			return 100.0;
	}
	return 0.0;
}
