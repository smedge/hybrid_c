#include "sub_scorch_core.h"
#include "render.h"
#include "audio.h"

#include <SDL2/SDL_mixer.h>

/* --- Config singleton --- */

static const SubScorchConfig scorchConfig = {
	.sprint_duration_ms = 3000,
	.sprint_cooldown_ms = 15000,
	.footprint_interval_ms = 80,
	.footprint_radius = 25.0,
	.footprint_life_ms = 4000,
	.burn_interval_ms = 500,
};

const SubScorchConfig *SubScorch_get_config(void)
{
	return &scorchConfig;
}

/* --- Legacy global pool (used by corruptor) --- */

static ScorchFootprintPool globalPool;

/* --- Audio --- */

static Mix_Chunk *sampleActivate = 0;

void SubScorch_initialize_audio(void)
{
	Audio_load_sample(&sampleActivate, "resources/sounds/refill_start.wav");
}

void SubScorch_cleanup_audio(void)
{
	Audio_unload_sample(&sampleActivate);
}

/* --- Init --- */

void SubScorch_init(SubScorchCore *core)
{
	SubSprint_init(&core->sprint);
	core->footprint_timer = 0;
}

/* --- Activation --- */

bool SubScorch_try_activate(SubScorchCore *core, const SubScorchConfig *cfg)
{
	SubSprintConfig scfg = {cfg->sprint_duration_ms, cfg->sprint_cooldown_ms};
	if (!SubSprint_try_activate(&core->sprint, &scfg))
		return false;

	core->footprint_timer = 0;
	Audio_play_sample(&sampleActivate);
	return true;
}

/* --- Update --- */

bool SubScorch_update(SubScorchCore *core, const SubScorchConfig *cfg, unsigned int ticks)
{
	SubSprintConfig scfg = {cfg->sprint_duration_ms, cfg->sprint_cooldown_ms};
	return SubSprint_update(&core->sprint, &scfg, ticks);
}

bool SubScorch_is_active(const SubScorchCore *core)
{
	return SubSprint_is_active(&core->sprint);
}

float SubScorch_get_cooldown_fraction(const SubScorchCore *core, const SubScorchConfig *cfg)
{
	SubSprintConfig scfg = {cfg->sprint_duration_ms, cfg->sprint_cooldown_ms};
	return SubSprint_get_cooldown_fraction(&core->sprint, &scfg);
}

/* --- Pool-based footprint API --- */

void SubScorch_pool_init(ScorchFootprintPool *pool, ScorchFootprint *buffer, int max)
{
	pool->data = buffer;
	pool->max = max;
	for (int i = 0; i < max; i++)
		buffer[i].active = false;
}

void SubScorch_pool_spawn(ScorchFootprintPool *pool, const SubScorchConfig *cfg, Position pos)
{
	if (!pool->data)
		return;

	for (int i = 0; i < pool->max; i++) {
		if (!pool->data[i].active) {
			pool->data[i].active = true;
			pool->data[i].position = pos;
			pool->data[i].life_ms = cfg->footprint_life_ms;
			pool->data[i].burn_tick_ms = 0;
			Burn_reset(&pool->data[i].burn);
			Burn_apply(&pool->data[i].burn, cfg->footprint_life_ms);
			return;
		}
	}
}

void SubScorch_pool_update(ScorchFootprintPool *pool, const SubScorchConfig *cfg, unsigned int ticks)
{
	if (!pool->data)
		return;

	(void)cfg;
	for (int i = 0; i < pool->max; i++) {
		ScorchFootprint *fp = &pool->data[i];
		if (!fp->active)
			continue;

		fp->life_ms -= (int)ticks;
		if (fp->life_ms <= 0) {
			fp->active = false;
			Burn_reset(&fp->burn);
			continue;
		}

		Burn_update(&fp->burn, ticks);
		if (!Burn_is_active(&fp->burn))
			Burn_apply(&fp->burn, fp->life_ms);

		if (fp->burn_tick_ms > 0)
			fp->burn_tick_ms -= (int)ticks;

		Burn_register(&fp->burn, fp->position);
	}
}

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

int SubScorch_pool_check_burn(ScorchFootprintPool *pool, const SubScorchConfig *cfg, Rectangle target)
{
	if (!pool->data)
		return 0;

	int hits = 0;
	for (int i = 0; i < pool->max; i++) {
		ScorchFootprint *fp = &pool->data[i];
		if (!fp->active)
			continue;
		if (fp->burn_tick_ms > 0)
			continue;

		if (circle_aabb_overlap(fp->position, cfg->footprint_radius, target)) {
			fp->burn_tick_ms = cfg->burn_interval_ms;
			hits++;
		}
	}
	return hits;
}

void SubScorch_pool_deactivate_all(ScorchFootprintPool *pool)
{
	if (!pool->data)
		return;

	for (int i = 0; i < pool->max; i++) {
		pool->data[i].active = false;
		Burn_reset(&pool->data[i].burn);
	}
}

/* --- Pool rendering --- */

void SubScorch_pool_render(ScorchFootprintPool *pool, const SubScorchConfig *cfg)
{
	if (!pool->data)
		return;

	for (int i = 0; i < pool->max; i++) {
		if (!pool->data[i].active)
			continue;

		float life_frac = (float)pool->data[i].life_ms / (float)cfg->footprint_life_ms;
		float alpha = 0.2f * life_frac;
		float cx = (float)pool->data[i].position.x;
		float cy = (float)pool->data[i].position.y;

		Render_filled_circle(cx, cy, (float)cfg->footprint_radius, 10,
			1.0f, 0.35f, 0.05f, alpha);
		Render_filled_circle(cx, cy, (float)cfg->footprint_radius * 0.4f, 6,
			1.0f, 0.6f, 0.15f, alpha * 1.5f);
	}
}

void SubScorch_pool_render_bloom(ScorchFootprintPool *pool, const SubScorchConfig *cfg)
{
	if (!pool->data)
		return;

	for (int i = 0; i < pool->max; i++) {
		if (!pool->data[i].active)
			continue;

		float life_frac = (float)pool->data[i].life_ms / (float)cfg->footprint_life_ms;
		float alpha = 0.35f * life_frac;
		float cx = (float)pool->data[i].position.x;
		float cy = (float)pool->data[i].position.y;

		Render_filled_circle(cx, cy, (float)cfg->footprint_radius * 1.2f, 10,
			1.0f, 0.45f, 0.08f, alpha);
	}
}

void SubScorch_pool_render_light(ScorchFootprintPool *pool)
{
	if (!pool->data)
		return;

	const SubScorchConfig *cfg = SubScorch_get_config();
	for (int i = 0; i < pool->max; i++) {
		if (!pool->data[i].active)
			continue;

		float life_frac = (float)pool->data[i].life_ms / (float)cfg->footprint_life_ms;
		float alpha = 0.3f * life_frac;
		float cx = (float)pool->data[i].position.x;
		float cy = (float)pool->data[i].position.y;

		Render_filled_circle(cx, cy, 100.0f, 10,
			1.0f, 0.45f, 0.08f, alpha);
	}
}

/* --- Legacy global pool API (delegates to globalPool) --- */

void SubScorch_init_footprints(ScorchFootprint *buffer, int max)
{
	SubScorch_pool_init(&globalPool, buffer, max);
}

void SubScorch_spawn_footprint(SubScorchCore *core, const SubScorchConfig *cfg, Position pos)
{
	(void)core;
	SubScorch_pool_spawn(&globalPool, cfg, pos);
}

void SubScorch_update_footprints(const SubScorchConfig *cfg, unsigned int ticks)
{
	SubScorch_pool_update(&globalPool, cfg, ticks);
}

int SubScorch_check_footprint_burn(const SubScorchConfig *cfg, Rectangle target)
{
	return SubScorch_pool_check_burn(&globalPool, cfg, target);
}

void SubScorch_deactivate_all_footprints(void)
{
	SubScorch_pool_deactivate_all(&globalPool);
}

void SubScorch_render_footprints(const SubScorchConfig *cfg)
{
	SubScorch_pool_render(&globalPool, cfg);
}

void SubScorch_render_footprints_bloom(const SubScorchConfig *cfg)
{
	SubScorch_pool_render_bloom(&globalPool, cfg);
}

void SubScorch_render_footprints_light(void)
{
	SubScorch_pool_render_light(&globalPool);
}
