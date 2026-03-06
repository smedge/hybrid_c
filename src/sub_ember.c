#include "sub_ember.h"

#include "sub_ember_core.h"
#include "sub_projectile_core.h"
#include "graphics.h"
#include "position.h"
#include "render.h"
#include "view.h"
#include "shipstate.h"
#include "skillbar.h"
#include "sub_stealth.h"
#include "player_stats.h"

static Entity *parent;
static SubProjectilePool pool;

/* Snapshot active states to detect wall impacts */
static bool prevActive[SUB_PROJ_MAX_POOL];

void Sub_Ember_initialize(Entity *parent_entity)
{
	parent = parent_entity;
	const SubEmberConfig *cfg = SubEmber_get_config();
	SubProjectile_pool_init(&pool, cfg->proj.pool_size);
	SubEmber_initialize_audio();
}

void Sub_Ember_cleanup(void)
{
	SubEmber_cleanup_audio();
}

void Sub_Ember_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable)
{
	const SubEmberConfig *cfg = SubEmber_get_config();
	ShipState *state = (ShipState *)parent->state;

	if (userInput->mouseLeft && !state->destroyed
			&& Skillbar_is_active(SUB_ID_EMBER)) {
		Sub_Stealth_break_attack();

		Position cursor = {userInput->mouseX, userInput->mouseY};
		Screen screen = Graphics_get_screen();
		Position cursor_world = View_get_world_position(&screen, cursor);

		if (SubProjectile_try_fire(&pool, &cfg->proj, placeable->position, cursor_world)) {
			PlayerStats_add_feedback(cfg->feedback_cost);
		}
	}

	/* Snapshot before update to detect wall hits */
	for (int i = 0; i < pool.poolSize; i++)
		prevActive[i] = pool.projectiles[i].active;

	SubProjectile_update(&pool, &cfg->proj, ticks);

	/* Wall impacts — spark means a projectile hit a wall this frame */
	for (int i = 0; i < pool.poolSize; i++) {
		if (prevActive[i] && !pool.projectiles[i].active && pool.sparkActive)
			SubEmber_add_burst(pool.sparkPosition);
	}
}

void Sub_Ember_tick(unsigned int ticks)
{
	const SubEmberConfig *cfg = SubEmber_get_config();

	for (int i = 0; i < pool.poolSize; i++)
		prevActive[i] = pool.projectiles[i].active;

	SubProjectile_update(&pool, &cfg->proj, ticks);

	for (int i = 0; i < pool.poolSize; i++) {
		if (prevActive[i] && !pool.projectiles[i].active && pool.sparkActive)
			SubEmber_add_burst(pool.sparkPosition);
	}
}

void Sub_Ember_render(void)
{
	const SubEmberConfig *cfg = SubEmber_get_config();
	SubProjectile_render(&pool, &cfg->proj);
}

void Sub_Ember_render_bloom_source(void)
{
	const SubEmberConfig *cfg = SubEmber_get_config();
	SubProjectile_render_bloom(&pool, &cfg->proj);
}

void Sub_Ember_render_light_source(void)
{
	const SubEmberConfig *cfg = SubEmber_get_config();
	SubProjectile_render_light(&pool, &cfg->proj);
}

double Sub_Ember_check_hit(Rectangle target)
{
	const SubEmberConfig *cfg = SubEmber_get_config();
	SubProjectileHitResult r = SubProjectile_check_hit_multi(&pool, &cfg->proj, target);
	return r.damage;
}

int Sub_Ember_check_hit_burn(Rectangle target)
{
	const SubEmberConfig *cfg = SubEmber_get_config();
	SubProjectileHitResult r = SubProjectile_check_hit_multi(&pool, &cfg->proj, target);

	/* Enemy impact — burst ignites nearby enemies */
	if (r.hits > 0) {
		Position impact;
		impact.x = (target.aX + target.bX) * 0.5;
		impact.y = (target.aY + target.bY) * 0.5;
		SubEmber_add_burst(impact);
	}

	return r.hits;
}

bool Sub_Ember_check_nearby(Position pos, double radius)
{
	return SubProjectile_check_nearby(&pool, pos, radius);
}

void Sub_Ember_deactivate_all(void)
{
	SubProjectile_deactivate_all(&pool);
}

float Sub_Ember_get_cooldown_fraction(void)
{
	const SubEmberConfig *cfg = SubEmber_get_config();
	return SubProjectile_get_cooldown_fraction(&pool, &cfg->proj);
}
