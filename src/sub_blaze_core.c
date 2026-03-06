#include "sub_blaze_core.h"
#include "render.h"

/* --- Config singleton --- */

static const SubBlazeConfig blazeConfig = {
	.corridor_life_ms = 3000,
	.corridor_radius = 30.0,
	.burn_interval_ms = 500,
	.segment_spawn_ms = 3,
};

const SubBlazeConfig *SubBlaze_get_config(void)
{
	return &blazeConfig;
}

/* --- Init --- */

void SubBlaze_init(SubBlazeCore *core, BlazeCorridorSegment *buffer, int max_segments)
{
	core->segments = buffer;
	core->max_segments = max_segments;
	core->spawn_timer = 0;
}

/* --- Segment spawning --- */

void SubBlaze_spawn_segment(SubBlazeCore *core, Position pos)
{
	for (int i = 0; i < core->max_segments; i++) {
		if (!core->segments[i].active) {
			const SubBlazeConfig *cfg = SubBlaze_get_config();
			core->segments[i].active = true;
			core->segments[i].position = pos;
			core->segments[i].life_ms = cfg->corridor_life_ms;
			core->segments[i].burn_tick_ms = 0;
			Burn_reset(&core->segments[i].burn);
			Burn_apply(&core->segments[i].burn, cfg->corridor_life_ms);
			return;
		}
	}
}

/* --- Corridor update --- */

void SubBlaze_update_corridor(SubBlazeCore *core, const SubBlazeConfig *cfg, unsigned int ticks)
{
	for (int i = 0; i < core->max_segments; i++) {
		BlazeCorridorSegment *seg = &core->segments[i];
		if (!seg->active)
			continue;

		seg->life_ms -= (int)ticks;
		if (seg->life_ms <= 0) {
			seg->active = false;
			Burn_reset(&seg->burn);
			continue;
		}

		/* Tick the burn visual state to keep stacks alive */
		Burn_update(&seg->burn, ticks);

		/* Re-apply burn stack if it expired (keep 1 stack alive for visuals) */
		if (!Burn_is_active(&seg->burn))
			Burn_apply(&seg->burn, seg->life_ms);

		/* Tick burn interval timer */
		if (seg->burn_tick_ms > 0)
			seg->burn_tick_ms -= (int)ticks;

		/* Register with centralized burn renderer */
		Burn_register(&seg->burn, seg->position);
	}
}

/* --- Corridor burn check --- */

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

int SubBlaze_check_corridor_burn(SubBlazeCore *core, const SubBlazeConfig *cfg, Rectangle target)
{
	int hits = 0;
	for (int i = 0; i < core->max_segments; i++) {
		BlazeCorridorSegment *seg = &core->segments[i];
		if (!seg->active)
			continue;
		if (seg->burn_tick_ms > 0)
			continue;

		if (circle_aabb_overlap(seg->position, cfg->corridor_radius, target)) {
			seg->burn_tick_ms = cfg->burn_interval_ms;
			hits++;
		}
	}
	return hits;
}

/* --- Deactivate --- */

void SubBlaze_deactivate_all(SubBlazeCore *core)
{
	for (int i = 0; i < core->max_segments; i++) {
		core->segments[i].active = false;
		Burn_reset(&core->segments[i].burn);
	}
}

/* --- Rendering --- */

void SubBlaze_render_corridor(const SubBlazeCore *core, const SubBlazeConfig *cfg)
{
	for (int i = 0; i < core->max_segments; i++) {
		if (!core->segments[i].active)
			continue;

		float life_frac = (float)core->segments[i].life_ms / (float)cfg->corridor_life_ms;
		float alpha = 0.25f * life_frac;
		float cx = (float)core->segments[i].position.x;
		float cy = (float)core->segments[i].position.y;

		/* Ground glow circle */
		Render_filled_circle(cx, cy, (float)cfg->corridor_radius, 12,
			1.0f, 0.4f, 0.05f, alpha);
		/* Brighter inner core */
		Render_filled_circle(cx, cy, (float)cfg->corridor_radius * 0.5f, 8,
			1.0f, 0.6f, 0.15f, alpha * 1.5f);
	}
}

void SubBlaze_render_corridor_bloom_source(const SubBlazeCore *core, const SubBlazeConfig *cfg)
{
	for (int i = 0; i < core->max_segments; i++) {
		if (!core->segments[i].active)
			continue;

		float life_frac = (float)core->segments[i].life_ms / (float)cfg->corridor_life_ms;
		float alpha = 0.4f * life_frac;
		float cx = (float)core->segments[i].position.x;
		float cy = (float)core->segments[i].position.y;

		Render_filled_circle(cx, cy, (float)cfg->corridor_radius * 1.3f, 12,
			1.0f, 0.5f, 0.1f, alpha);
	}
}

void SubBlaze_render_corridor_light_source(const SubBlazeCore *core, const SubBlazeConfig *cfg)
{
	(void)cfg;
	for (int i = 0; i < core->max_segments; i++) {
		if (!core->segments[i].active)
			continue;

		float life_frac = (float)core->segments[i].life_ms / (float)SubBlaze_get_config()->corridor_life_ms;
		float alpha = 0.35f * life_frac;
		float cx = (float)core->segments[i].position.x;
		float cy = (float)core->segments[i].position.y;

		Render_filled_circle(cx, cy, 120.0f, 12,
			1.0f, 0.5f, 0.1f, alpha);
	}
}
