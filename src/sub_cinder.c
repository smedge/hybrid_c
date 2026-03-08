#include "sub_cinder.h"

#include "sub_mine_core.h"
#include "sub_cinder_core.h"
#include "ship.h"
#include "progression.h"
#include "render.h"
#include "skillbar.h"
#include "sub_stealth.h"
#include "enemy_util.h"
#include "sub_egress.h"
#include "player_stats.h"

#define MAX_CINDER_MINES 3
#define PLACE_COOLDOWN 250

static SubMineCore mines[MAX_CINDER_MINES];
static CinderFirePool firePools[MAX_CINDER_MINES];
static int cooldownTimer;

void Sub_Cinder_initialize(void)
{
	cooldownTimer = 0;

	const SubMineConfig *cfg = SubCinder_get_fire_mine_config();
	(void)cfg;

	for (int i = 0; i < MAX_CINDER_MINES; i++) {
		SubMine_init(&mines[i]);
		SubCinder_init_pool(&firePools[i]);
	}

	SubMine_initialize_audio();
}

void Sub_Cinder_cleanup(void)
{
	SubMine_cleanup_audio();
}

static void handle_detonation(int idx)
{
	PlayerStats_add_feedback(15.0);

	/* Spawn fire pool at mine position */
	SubCinder_spawn_pool(firePools, MAX_CINDER_MINES, mines[idx].position);

	/* Player's own cinder mines hurt the player */
	const SubMineConfig *cfg = SubCinder_get_fire_mine_config();
	Position shipPos = Ship_get_position();
	double dx = shipPos.x - mines[idx].position.x;
	double dy = shipPos.y - mines[idx].position.y;
	float hs = cfg->explosion_half_size;
	if (dx >= -hs && dx <= hs && dy >= -hs && dy <= hs)
		PlayerStats_damage(100.0);
}

void Sub_Cinder_try_deploy(void)
{
	if (cooldownTimer > 0) return;

	const SubMineConfig *cfg = SubCinder_get_fire_mine_config();
	Enemy_break_cloak_attack();
	int slot = -1;
	for (int i = 0; i < MAX_CINDER_MINES; i++) {
		if (mines[i].phase == MINE_PHASE_DORMANT) {
			slot = i;
			break;
		}
	}

	if (slot >= 0) {
		cooldownTimer = PLACE_COOLDOWN;
		SubMine_arm(&mines[slot], cfg, Ship_get_position());
	}
}

void Sub_Cinder_update(const Input *userInput, unsigned int ticks)
{
	(void)userInput;
	if (Ship_is_destroyed()) return;

	const SubMineConfig *cfg = SubCinder_get_fire_mine_config();

	if (cooldownTimer > 0)
		cooldownTimer -= ticks;

	for (int i = 0; i < MAX_CINDER_MINES; i++) {
		SubMineCore *m = &mines[i];
		if (m->phase == MINE_PHASE_DORMANT)
			continue;

		MinePhase prevPhase = m->phase;
		SubMine_update(m, cfg, ticks);

		/* Just transitioned to EXPLODING */
		if (m->phase == MINE_PHASE_EXPLODING && prevPhase == MINE_PHASE_ARMED)
			handle_detonation(i);
	}

	/* Egress dash detonation */
	if (Sub_Egress_is_dashing()) {
		for (int i = 0; i < MAX_CINDER_MINES; i++) {
			SubMineCore *m = &mines[i];
			if (m->phase != MINE_PHASE_ARMED)
				continue;

			float bs = cfg->body_half_size;
			Rectangle mineBody = Collision_transform_bounding_box(m->position,
				(Rectangle){-bs, bs, bs, -bs});

			if (Sub_Egress_check_hit(mineBody) > 0.0) {
				SubMine_detonate(m);
				handle_detonation(i);
			}
		}
	}
}

void Sub_Cinder_tick(unsigned int ticks)
{
	const SubMineConfig *cfg = SubCinder_get_fire_mine_config();
	for (int i = 0; i < MAX_CINDER_MINES; i++) {
		if (mines[i].phase != MINE_PHASE_DORMANT)
			SubMine_update(&mines[i], cfg, ticks);
	}
}

void Sub_Cinder_render(void)
{
	const SubMineConfig *cfg = SubCinder_get_fire_mine_config();
	for (int i = 0; i < MAX_CINDER_MINES; i++) {
		if (mines[i].phase == MINE_PHASE_DORMANT)
			continue;
		SubMine_render(&mines[i], cfg);
	}
}

void Sub_Cinder_render_bloom_source(void)
{
	const SubMineConfig *cfg = SubCinder_get_fire_mine_config();
	for (int i = 0; i < MAX_CINDER_MINES; i++) {
		if (mines[i].phase == MINE_PHASE_DORMANT)
			continue;
		SubMine_render_bloom(&mines[i], cfg);
	}
}

void Sub_Cinder_render_light_source(void)
{
	const SubMineConfig *cfg = SubCinder_get_fire_mine_config();
	for (int i = 0; i < MAX_CINDER_MINES; i++) {
		if (mines[i].phase == MINE_PHASE_DORMANT)
			continue;
		SubMine_render_light(&mines[i], cfg);
	}
}

void Sub_Cinder_deactivate_all(void)
{
	for (int i = 0; i < MAX_CINDER_MINES; i++) {
		SubMine_init(&mines[i]);
		SubCinder_init_pool(&firePools[i]);
	}
}

float Sub_Cinder_get_cooldown_fraction(void)
{
	bool has_free_slot = false;
	for (int i = 0; i < MAX_CINDER_MINES; i++) {
		if (mines[i].phase == MINE_PHASE_DORMANT) {
			has_free_slot = true;
			break;
		}
	}
	if (!has_free_slot) return 1.0f;

	if (cooldownTimer <= 0) return 0.0f;
	return (float)cooldownTimer / PLACE_COOLDOWN;
}

double Sub_Cinder_check_hit(Rectangle target)
{
	const SubMineConfig *cfg = SubCinder_get_fire_mine_config();
	for (int i = 0; i < MAX_CINDER_MINES; i++) {
		if (SubMine_check_explosion_hit(&mines[i], cfg, target))
			return 100.0;
	}
	return 0.0;
}

/* --- Fire pool delegates --- */

void Sub_Cinder_update_pools(unsigned int ticks)
{
	const SubCinderConfig *cfg = SubCinder_get_config();
	SubCinder_update_pools(firePools, MAX_CINDER_MINES, cfg, ticks);
}

int Sub_Cinder_check_pool_burn(Rectangle target)
{
	const SubCinderConfig *cfg = SubCinder_get_config();
	return SubCinder_check_pool_burn(firePools, MAX_CINDER_MINES, cfg, target);
}

void Sub_Cinder_render_pools(void)
{
	const SubCinderConfig *cfg = SubCinder_get_config();
	SubCinder_render_pools(firePools, MAX_CINDER_MINES, cfg);
}

void Sub_Cinder_render_pools_bloom(void)
{
	const SubCinderConfig *cfg = SubCinder_get_config();
	SubCinder_render_pools_bloom(firePools, MAX_CINDER_MINES, cfg);
}

void Sub_Cinder_render_pools_light(void)
{
	const SubCinderConfig *cfg = SubCinder_get_config();
	SubCinder_render_pools_light(firePools, MAX_CINDER_MINES, cfg);
}
