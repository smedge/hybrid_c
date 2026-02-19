#include "sub_projectile_core.h"
#include "map.h"
#include "render.h"
#include "view.h"
#include "audio.h"
#include "color.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double get_radians(double degrees)
{
	return degrees * M_PI / 180.0;
}

void SubProjectile_pool_init(SubProjectilePool *pool, int poolSize)
{
	pool->poolSize = poolSize;
	pool->cooldownTimer = 0;
	pool->sparkActive = false;
	pool->sparkTicksLeft = 0;

	for (int i = 0; i < poolSize; i++)
		pool->projectiles[i].active = false;
}

bool SubProjectile_try_fire(SubProjectilePool *pool, const SubProjectileConfig *cfg,
	Position origin, Position target)
{
	if (pool->cooldownTimer > 0)
		return false;

	pool->cooldownTimer = cfg->fire_cooldown_ms;

	/* Find inactive slot */
	int slot = -1;
	for (int i = 0; i < pool->poolSize; i++) {
		if (!pool->projectiles[i].active) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
		int oldest = 0;
		for (int i = 1; i < pool->poolSize; i++) {
			if (pool->projectiles[i].ticksLived > pool->projectiles[oldest].ticksLived)
				oldest = i;
		}
		slot = oldest;
	}

	SubProjectile *p = &pool->projectiles[slot];
	p->active = true;
	p->ticksLived = 0;
	p->position = origin;
	p->prevPosition = origin;

	double heading = Position_get_heading(origin, target);
	double rad = get_radians(heading);
	p->headingSin = sin(rad);
	p->headingCos = cos(rad);

	if (cfg->sample_fire)
		Audio_play_sample(cfg->sample_fire);

	return true;
}

void SubProjectile_update(SubProjectilePool *pool, const SubProjectileConfig *cfg, unsigned int ticks)
{
	if (pool->cooldownTimer > 0)
		pool->cooldownTimer -= (int)ticks;

	for (int i = 0; i < pool->poolSize; i++) {
		SubProjectile *p = &pool->projectiles[i];
		if (!p->active)
			continue;

		p->ticksLived += ticks;
		if (p->ticksLived > cfg->ttl_ms) {
			p->active = false;
			continue;
		}

		p->prevPosition = p->position;

		double dt = ticks / 1000.0;
		double dx = p->headingSin * cfg->velocity * dt;
		double dy = p->headingCos * cfg->velocity * dt;
		p->position.x += dx;
		p->position.y += dy;

		/* Wall collision */
		double hx, hy;
		if (Map_line_test_hit(p->prevPosition.x, p->prevPosition.y,
				p->position.x, p->position.y, &hx, &hy)) {
			pool->sparkActive = true;
			pool->sparkPosition.x = hx;
			pool->sparkPosition.y = hy;
			pool->sparkTicksLeft = cfg->spark_duration_ms;
			p->active = false;
			if (cfg->sample_hit)
				Audio_play_sample(cfg->sample_hit);
		}
	}

	/* Spark decay */
	if (pool->sparkActive) {
		pool->sparkTicksLeft -= (int)ticks;
		if (pool->sparkTicksLeft <= 0)
			pool->sparkActive = false;
	}
}

double SubProjectile_check_hit(SubProjectilePool *pool, const SubProjectileConfig *cfg, Rectangle target)
{
	for (int i = 0; i < pool->poolSize; i++) {
		SubProjectile *p = &pool->projectiles[i];
		if (!p->active)
			continue;
		if (Collision_line_aabb_test(p->prevPosition.x, p->prevPosition.y,
				p->position.x, p->position.y, target, NULL)) {
			p->active = false;
			return cfg->damage;
		}
	}
	return 0.0;
}

bool SubProjectile_check_nearby(const SubProjectilePool *pool, Position pos, double radius)
{
	double r2 = radius * radius;
	for (int i = 0; i < pool->poolSize; i++) {
		const SubProjectile *p = &pool->projectiles[i];
		if (!p->active)
			continue;
		double dx = p->position.x - pos.x;
		double dy = p->position.y - pos.y;
		if (dx * dx + dy * dy < r2)
			return true;
	}
	return false;
}

void SubProjectile_deactivate_all(SubProjectilePool *pool)
{
	for (int i = 0; i < pool->poolSize; i++)
		pool->projectiles[i].active = false;
}

float SubProjectile_get_cooldown_fraction(const SubProjectilePool *pool, const SubProjectileConfig *cfg)
{
	if (pool->cooldownTimer <= 0) return 0.0f;
	return (float)pool->cooldownTimer / cfg->fire_cooldown_ms;
}

void SubProjectile_render(const SubProjectilePool *pool, const SubProjectileConfig *cfg)
{
	View view = View_get_view();

	for (int i = 0; i < pool->poolSize; i++) {
		const SubProjectile *p = &pool->projectiles[i];
		if (!p->active)
			continue;

		/* Motion trail */
		Render_thick_line(
			(float)p->prevPosition.x, (float)p->prevPosition.y,
			(float)p->position.x, (float)p->position.y,
			cfg->trail_thickness, cfg->color_r, cfg->color_g, cfg->color_b, cfg->trail_alpha);

		double size = cfg->point_size * view.scale;
		if (size < cfg->min_point_size)
			size = cfg->min_point_size;
		ColorFloat color = {cfg->color_r, cfg->color_g, cfg->color_b, 1.0f};
		Render_point(&p->position, size, &color);
	}

	if (pool->sparkActive) {
		float fade = (float)pool->sparkTicksLeft / cfg->spark_duration_ms;
		ColorFloat sparkColor = {1.0f, 1.0f, 1.0f, fade};
		Rectangle sparkRect = {-cfg->spark_size, cfg->spark_size, cfg->spark_size, -cfg->spark_size};
		Render_quad(&pool->sparkPosition, 0.0, sparkRect, &sparkColor);
		Render_quad(&pool->sparkPosition, 45.0, sparkRect, &sparkColor);
	}
}

void SubProjectile_render_bloom(const SubProjectilePool *pool, const SubProjectileConfig *cfg)
{
	SubProjectile_render(pool, cfg);
}

void SubProjectile_render_light(const SubProjectilePool *pool, const SubProjectileConfig *cfg)
{
	for (int i = 0; i < pool->poolSize; i++) {
		const SubProjectile *p = &pool->projectiles[i];
		if (!p->active)
			continue;
		Render_filled_circle(
			(float)p->position.x, (float)p->position.y,
			cfg->light_proj_radius, 12,
			cfg->light_proj_r, cfg->light_proj_g, cfg->light_proj_b, cfg->light_proj_a);
	}
	if (pool->sparkActive) {
		Render_filled_circle(
			(float)pool->sparkPosition.x, (float)pool->sparkPosition.y,
			cfg->light_spark_radius, 12,
			cfg->light_spark_r, cfg->light_spark_g, cfg->light_spark_b, cfg->light_spark_a);
	}
}
