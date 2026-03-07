#include "sub_cinder_core.h"
#include "render.h"

/* --- Config singletons --- */

static const SubCinderConfig cinderConfig = {
	.pool_radius = 100.0f,
	.pool_duration_ms = 3500,
	.burn_interval_ms = 500,
	.detonation_burn_stacks = 2,
};

static const SubMineConfig fireMineConfig = {
	.armed_duration_ms = 2000,
	.exploding_duration_ms = 100,
	.dead_duration_ms = 0,
	.explosion_half_size = 250.0f,
	.body_half_size = 10.0f,
	/* Orange body */
	.body_r = 0.6f, .body_g = 0.3f, .body_b = 0.05f, .body_a = 1.0f,
	/* Orange blink */
	.blink_r = 1.0f, .blink_g = 0.5f, .blink_b = 0.1f, .blink_a = 1.0f,
	/* Orange-white explosion */
	.explosion_r = 1.0f, .explosion_g = 0.7f, .explosion_b = 0.3f, .explosion_a = 1.0f,
	/* Orange light */
	.light_armed_radius = 180.0f,
	.light_armed_r = 1.0f, .light_armed_g = 0.4f, .light_armed_b = 0.1f, .light_armed_a = 0.4f,
	.light_explode_radius = 750.0f,
	.light_explode_r = 1.0f, .light_explode_g = 0.7f, .light_explode_b = 0.3f, .light_explode_a = 1.0f,
};

const SubCinderConfig *SubCinder_get_config(void)
{
	return &cinderConfig;
}

const SubMineConfig *SubCinder_get_fire_mine_config(void)
{
	return &fireMineConfig;
}

/* --- Pool lifecycle --- */

void SubCinder_init_pool(CinderFirePool *pool)
{
	pool->active = false;
	pool->position = (Position){0, 0};
	pool->life_ms = 0;
	pool->burn_tick_ms = 0;
	Burn_reset(&pool->burn);
}

void SubCinder_spawn_pool(CinderFirePool *pools, int max, Position pos)
{
	for (int i = 0; i < max; i++) {
		if (!pools[i].active) {
			pools[i].active = true;
			pools[i].position = pos;
			pools[i].life_ms = cinderConfig.pool_duration_ms;
			pools[i].burn_tick_ms = 0;
			Burn_reset(&pools[i].burn);
			Burn_apply(&pools[i].burn, cinderConfig.pool_duration_ms);
			return;
		}
	}
}

void SubCinder_update_pools(CinderFirePool *pools, int max, const SubCinderConfig *cfg, unsigned int ticks)
{
	for (int i = 0; i < max; i++) {
		CinderFirePool *p = &pools[i];
		if (!p->active)
			continue;

		p->life_ms -= (int)ticks;
		if (p->life_ms <= 0) {
			p->active = false;
			Burn_reset(&p->burn);
			continue;
		}

		/* Tick burn visual state */
		Burn_update(&p->burn, ticks);

		/* Re-apply burn stack if expired (keep 1 stack alive for visuals) */
		if (!Burn_is_active(&p->burn))
			Burn_apply(&p->burn, p->life_ms);

		/* Tick burn interval timer */
		if (p->burn_tick_ms > 0)
			p->burn_tick_ms -= (int)ticks;

		/* Register with centralized burn renderer */
		Burn_register(&p->burn, p->position);
	}
}

/* --- Circle-AABB overlap --- */

static bool circle_aabb_overlap(Position center, double radius, Rectangle r)
{
	double cx = center.x;
	double cy = center.y;
	double minX = r.aX < r.bX ? r.aX : r.bX;
	double maxX = r.aX > r.bX ? r.aX : r.bX;
	double minY = r.aY < r.bY ? r.aY : r.bY;
	double maxY = r.aY > r.bY ? r.aY : r.bY;

	double nearX = cx < minX ? minX : (cx > maxX ? maxX : cx);
	double nearY = cy < minY ? minY : (cy > maxY ? maxY : cy);

	double dx = cx - nearX;
	double dy = cy - nearY;
	return (dx * dx + dy * dy) <= radius * radius;
}

/* --- Pool burn check --- */

int SubCinder_check_pool_burn(CinderFirePool *pools, int max, const SubCinderConfig *cfg, Rectangle target)
{
	int hits = 0;
	for (int i = 0; i < max; i++) {
		CinderFirePool *p = &pools[i];
		if (!p->active)
			continue;
		if (p->burn_tick_ms > 0)
			continue;

		if (circle_aabb_overlap(p->position, cfg->pool_radius, target)) {
			p->burn_tick_ms = cfg->burn_interval_ms;
			hits++;
		}
	}
	return hits;
}

/* --- Deactivate --- */

void SubCinder_deactivate_pools(CinderFirePool *pools, int max)
{
	for (int i = 0; i < max; i++) {
		pools[i].active = false;
		Burn_reset(&pools[i].burn);
	}
}

/* --- Rendering --- */

void SubCinder_render_pools(const CinderFirePool *pools, int max, const SubCinderConfig *cfg)
{
	for (int i = 0; i < max; i++) {
		if (!pools[i].active)
			continue;

		float life_frac = (float)pools[i].life_ms / (float)cfg->pool_duration_ms;
		float alpha = 0.3f * life_frac;
		float cx = (float)pools[i].position.x;
		float cy = (float)pools[i].position.y;

		/* Ground glow circle */
		Render_filled_circle(cx, cy, cfg->pool_radius, 12,
			1.0f, 0.4f, 0.05f, alpha);
		/* Brighter inner core */
		Render_filled_circle(cx, cy, cfg->pool_radius * 0.5f, 8,
			1.0f, 0.6f, 0.15f, alpha * 1.5f);
	}
}

void SubCinder_render_pools_bloom(const CinderFirePool *pools, int max, const SubCinderConfig *cfg)
{
	for (int i = 0; i < max; i++) {
		if (!pools[i].active)
			continue;

		float life_frac = (float)pools[i].life_ms / (float)cfg->pool_duration_ms;
		float alpha = 0.4f * life_frac;
		float cx = (float)pools[i].position.x;
		float cy = (float)pools[i].position.y;

		Render_filled_circle(cx, cy, cfg->pool_radius * 1.3f, 12,
			1.0f, 0.5f, 0.1f, alpha);
	}
}

void SubCinder_render_pools_light(const CinderFirePool *pools, int max, const SubCinderConfig *cfg)
{
	for (int i = 0; i < max; i++) {
		if (!pools[i].active)
			continue;

		float life_frac = (float)pools[i].life_ms / (float)cfg->pool_duration_ms;
		float alpha = 0.35f * life_frac;
		float cx = (float)pools[i].position.x;
		float cy = (float)pools[i].position.y;

		Render_filled_circle(cx, cy, 150.0f, 12,
			1.0f, 0.5f, 0.1f, alpha);
	}
}
