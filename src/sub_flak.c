#include "sub_flak.h"

#include "sub_flak_core.h"
#include "sub_projectile_core.h"
#include "burn.h"
#include "graphics.h"
#include "position.h"
#include "render.h"
#include "view.h"
#include "shipstate.h"
#include "skillbar.h"
#include "sub_stealth.h"
#include "enemy_util.h"
#include "player_stats.h"
#include "keybinds.h"

static Entity *parent;
static SubProjectilePool pool;

void Sub_Flak_initialize(Entity *parent_entity)
{
	parent = parent_entity;
	const SubFlakConfig *cfg = SubFlak_get_config();
	SubProjectile_pool_init(&pool, cfg->proj.pool_size);
	SubFlak_initialize_audio();
}

void Sub_Flak_cleanup(void)
{
	SubFlak_cleanup_audio();
}

void Sub_Flak_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable)
{
	const SubFlakConfig *cfg = SubFlak_get_config();
	ShipState *state = (ShipState *)parent->state;

	bool fire = Keybinds_held(BIND_PRIMARY_WEAPON)
		|| (!Keybinds_is_lmb_rebound() && userInput->mouseLeft);
	if (fire && !state->destroyed
			&& Skillbar_is_active(SUB_ID_FLAK)) {
		Enemy_break_cloak_attack();

		Position cursor = {userInput->mouseX, userInput->mouseY};
		Screen screen = Graphics_get_screen();
		Position cursor_world = View_get_world_position(&screen, cursor);

		if (SubFlak_try_fire(&pool, cfg, placeable->position, cursor_world)) {
			PlayerStats_add_feedback(cfg->feedback_cost);
		}
	}

	SubProjectile_update(&pool, &cfg->proj, ticks);
}

void Sub_Flak_tick(unsigned int ticks)
{
	const SubFlakConfig *cfg = SubFlak_get_config();
	SubProjectile_update(&pool, &cfg->proj, ticks);
}

void Sub_Flak_render(void)
{
	const SubFlakConfig *cfg = SubFlak_get_config();
	SubProjectile_render(&pool, &cfg->proj);
}

void Sub_Flak_render_bloom_source(void)
{
	const SubFlakConfig *cfg = SubFlak_get_config();
	SubProjectile_render_bloom(&pool, &cfg->proj);
}

void Sub_Flak_render_light_source(void)
{
	const SubFlakConfig *cfg = SubFlak_get_config();
	SubProjectile_render_light(&pool, &cfg->proj);
}

bool Sub_Flak_check_hit(Rectangle target)
{
	const SubFlakConfig *cfg = SubFlak_get_config();
	SubProjectileHitResult r = SubProjectile_check_hit_multi(&pool, &cfg->proj, target);
	return r.hits > 0;
}

int Sub_Flak_check_hit_burn(Rectangle target)
{
	const SubFlakConfig *cfg = SubFlak_get_config();
	SubProjectileHitResult r = SubProjectile_check_hit_multi(&pool, &cfg->proj, target);
	return r.hits;
}

bool Sub_Flak_check_nearby(Position pos, double radius)
{
	return SubProjectile_check_nearby(&pool, pos, radius);
}

void Sub_Flak_deactivate_all(void)
{
	SubProjectile_deactivate_all(&pool);
}

float Sub_Flak_get_cooldown_fraction(void)
{
	const SubFlakConfig *cfg = SubFlak_get_config();
	return SubProjectile_get_cooldown_fraction(&pool, &cfg->proj);
}
