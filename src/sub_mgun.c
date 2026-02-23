#include "sub_mgun.h"

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

static const SubProjectileConfig cfg = {
	.fire_cooldown_ms = 200,
	.velocity = 3500.0,
	.ttl_ms = 1000,
	.pool_size = 16,
	.damage = 20.0,
	.color_r = 1.0f, .color_g = 1.0f, .color_b = 1.0f,
	.trail_thickness = 3.0f,
	.trail_alpha = 0.6f,
	.point_size = 8.0,
	.min_point_size = 2.0,
	.spark_duration_ms = 80,
	.spark_size = 15.0f,
	.light_proj_radius = 60.0f,
	.light_proj_r = 1.0f, .light_proj_g = 1.0f, .light_proj_b = 1.0f, .light_proj_a = 0.6f,
	.light_spark_radius = 90.0f,
	.light_spark_r = 1.0f, .light_spark_g = 1.0f, .light_spark_b = 0.8f, .light_spark_a = 0.5f,
};

void Sub_Mgun_initialize(Entity *p)
{
	parent = p;
	SubProjectile_pool_init(&pool, cfg.pool_size);
	SubProjectile_initialize_audio();
}

void Sub_Mgun_cleanup()
{
	SubProjectile_cleanup_audio();
}

void Sub_Mgun_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable)
{
	ShipState *state = (ShipState*)parent->state;

	if (userInput->mouseLeft && pool.cooldownTimer <= 0 && !state->destroyed
			&& Skillbar_is_active(SUB_ID_MGUN)) {
		Sub_Stealth_break_attack();

		Position position_cursor = {userInput->mouseX, userInput->mouseY};
		Screen screen = Graphics_get_screen();
		Position position_cursor_world = View_get_world_position(&screen, position_cursor);

		if (SubProjectile_try_fire(&pool, &cfg, placeable->position, position_cursor_world))
			PlayerStats_add_feedback(1.0);
	}

	SubProjectile_update(&pool, &cfg, ticks);
}

void Sub_Mgun_render()
{
	SubProjectile_render(&pool, &cfg);
}

void Sub_Mgun_render_bloom_source(void)
{
	SubProjectile_render_bloom(&pool, &cfg);
}

void Sub_Mgun_render_light_source(void)
{
	SubProjectile_render_light(&pool, &cfg);
}

double Sub_Mgun_check_hit(Rectangle target)
{
	return SubProjectile_check_hit(&pool, &cfg, target);
}

bool Sub_Mgun_check_nearby(Position pos, double radius)
{
	return SubProjectile_check_nearby(&pool, pos, radius);
}

void Sub_Mgun_deactivate_all(void)
{
	SubProjectile_deactivate_all(&pool);
}

float Sub_Mgun_get_cooldown_fraction(void)
{
	return SubProjectile_get_cooldown_fraction(&pool, &cfg);
}
