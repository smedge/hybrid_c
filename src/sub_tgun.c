#include "sub_tgun.h"

#include "sub_projectile_core.h"
#include "graphics.h"
#include "position.h"
#include "render.h"
#include "view.h"
#include "shipstate.h"
#include "skillbar.h"
#include "sub_stealth.h"
#include "player_stats.h"

#include <math.h>

#define BARREL_OFFSET 8.0
#define EXTERNAL_COOLDOWN_MS 100

static Entity *parent;
static SubProjectilePool poolLeft;
static SubProjectilePool poolRight;
static bool nextBarrelLeft;
static int tgunCooldown;

static const SubProjectileConfig cfg = {
	.fire_cooldown_ms = 200,
	.velocity = 3500.0,
	.ttl_ms = 1000,
	.pool_size = 16,
	.damage = 25.0,
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

void Sub_Tgun_initialize(Entity *p)
{
	parent = p;
	SubProjectile_pool_init(&poolLeft, cfg.pool_size);
	SubProjectile_pool_init(&poolRight, cfg.pool_size);
	SubProjectile_initialize_audio();
	nextBarrelLeft = true;
	tgunCooldown = 0;
}

void Sub_Tgun_cleanup()
{
	SubProjectile_cleanup_audio();
}

void Sub_Tgun_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable)
{
	ShipState *state = (ShipState*)parent->state;

	if (tgunCooldown > 0)
		tgunCooldown -= (int)ticks;

	if (userInput->mouseLeft && tgunCooldown <= 0 && !state->destroyed
			&& Skillbar_is_active(SUB_ID_TGUN)) {
		Sub_Stealth_break_attack();

		Position position_cursor = {userInput->mouseX, userInput->mouseY};
		Screen screen = Graphics_get_screen();
		Position position_cursor_world = View_get_world_position(&screen, position_cursor);

		/* Compute aim direction and perpendicular offset */
		double dx = position_cursor_world.x - placeable->position.x;
		double dy = position_cursor_world.y - placeable->position.y;
		double dist = sqrt(dx * dx + dy * dy);

		Position origin = placeable->position;
		if (dist > 0.001) {
			double nx = dx / dist;
			double ny = dy / dist;
			/* Perpendicular: (-ny, nx) */
			double px = -ny;
			double py = nx;
			double offset = nextBarrelLeft ? -BARREL_OFFSET : BARREL_OFFSET;
			origin.x += px * offset;
			origin.y += py * offset;
		}

		SubProjectilePool *targetPool = nextBarrelLeft ? &poolLeft : &poolRight;
		targetPool->cooldownTimer = 0;

		if (SubProjectile_try_fire(targetPool, &cfg, origin, position_cursor_world)) {
			PlayerStats_add_feedback(1.0);
			tgunCooldown = EXTERNAL_COOLDOWN_MS;
			nextBarrelLeft = !nextBarrelLeft;
		}
	}

	SubProjectile_update(&poolLeft, &cfg, ticks);
	SubProjectile_update(&poolRight, &cfg, ticks);
}

void Sub_Tgun_render()
{
	SubProjectile_render(&poolLeft, &cfg);
	SubProjectile_render(&poolRight, &cfg);
}

void Sub_Tgun_render_bloom_source(void)
{
	SubProjectile_render_bloom(&poolLeft, &cfg);
	SubProjectile_render_bloom(&poolRight, &cfg);
}

void Sub_Tgun_render_light_source(void)
{
	SubProjectile_render_light(&poolLeft, &cfg);
	SubProjectile_render_light(&poolRight, &cfg);
}

double Sub_Tgun_check_hit(Rectangle target)
{
	double dmg = SubProjectile_check_hit(&poolLeft, &cfg, target);
	dmg += SubProjectile_check_hit(&poolRight, &cfg, target);
	return dmg;
}

bool Sub_Tgun_check_nearby(Position pos, double radius)
{
	return SubProjectile_check_nearby(&poolLeft, pos, radius)
		|| SubProjectile_check_nearby(&poolRight, pos, radius);
}

void Sub_Tgun_deactivate_all(void)
{
	SubProjectile_deactivate_all(&poolLeft);
	SubProjectile_deactivate_all(&poolRight);
}

float Sub_Tgun_get_cooldown_fraction(void)
{
	if (tgunCooldown <= 0)
		return 0.0f;
	return (float)tgunCooldown / (float)EXTERNAL_COOLDOWN_MS;
}
