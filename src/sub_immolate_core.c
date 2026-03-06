#include "sub_immolate_core.h"
#include "render.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --- Config singleton (shared by player + enemies) --- */

static const SubImmolateConfig immolateConfig = {
	.shield = {
		.duration_ms = 6000,
		.cooldown_ms = 30000,
		.break_grace_ms = 200,
		.ring_radius = 35.0f,
		.ring_thickness = 2.0f,
		.ring_segments = 6,
		.color_r = 1.0f, .color_g = 0.5f, .color_b = 0.1f,
		.pulse_speed = 6.0f,
		.pulse_alpha_min = 0.7f,
		.pulse_alpha_range = 0.3f,
		.radius_pulse_amount = 0.05f,
		.bloom_thickness = 3.0f,
		.bloom_alpha_min = 0.5f,
		.bloom_alpha_range = 0.3f,
		.light_radius = 90.0f,
		.light_segments = 16,
	},
	.aura_radius = 60.0,
	.aura_burn_interval_ms = 500,
};

/* Defender-size ring config (smaller body) */
static const SubShieldConfig defenderRingCfg = {
	.duration_ms = 6000,
	.cooldown_ms = 30000,
	.break_grace_ms = 200,
	.ring_radius = 18.0f,
	.ring_thickness = 1.5f,
	.ring_segments = 6,
	.color_r = 1.0f, .color_g = 0.5f, .color_b = 0.1f,
	.pulse_speed = 5.0f,
	.pulse_alpha_min = 0.6f,
	.pulse_alpha_range = 0.4f,
	.radius_pulse_amount = 0.0f,
	.bloom_thickness = 2.0f,
	.bloom_alpha_min = 0.5f,
	.bloom_alpha_range = 0.0f,
	.light_radius = 0.0f,
	.light_segments = 0,
};

const SubImmolateConfig *SubImmolate_get_config(void)
{
	return &immolateConfig;
}

/* --- Lifecycle --- */

void SubImmolate_init(SubImmolateCore *core)
{
	SubShield_init(&core->shield);
	core->aura_burn_timer = 0;
}

bool SubImmolate_try_activate(SubImmolateCore *core)
{
	return SubShield_try_activate(&core->shield, &immolateConfig.shield);
}

void SubImmolate_break(SubImmolateCore *core)
{
	SubShield_break(&core->shield, &immolateConfig.shield);
}

bool SubImmolate_update(SubImmolateCore *core, unsigned int ticks)
{
	if (core->aura_burn_timer > 0) {
		core->aura_burn_timer -= (int)ticks;
		if (core->aura_burn_timer < 0)
			core->aura_burn_timer = 0;
	}
	return SubShield_update(&core->shield, &immolateConfig.shield, ticks);
}

bool SubImmolate_is_active(const SubImmolateCore *core)
{
	return SubShield_is_active(&core->shield);
}

bool SubImmolate_in_grace(const SubImmolateCore *core)
{
	return SubShield_in_grace(&core->shield);
}

float SubImmolate_get_cooldown_fraction(const SubImmolateCore *core)
{
	return SubShield_get_cooldown_fraction(&core->shield, &immolateConfig.shield);
}

/* --- Burn check --- */

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

int SubImmolate_check_burn(SubImmolateCore *core, Position shield_pos, Rectangle target)
{
	if (!SubShield_is_active(&core->shield))
		return 0;

	if (core->aura_burn_timer > 0)
		return 0;

	if (circle_aabb_overlap(shield_pos, immolateConfig.aura_radius, target)) {
		core->aura_burn_timer = immolateConfig.aura_burn_interval_ms;
		return 1;
	}
	return 0;
}

void SubImmolate_on_hit(SubImmolateCore *core)
{
	SubShield_on_hit(&core->shield);
}

/* --- Rendering --- */

void SubImmolate_render_ring(const SubImmolateCore *core, Position pos)
{
	SubShield_render_ring(&core->shield, &immolateConfig.shield, pos);
}

void SubImmolate_render_ring_small(const SubImmolateCore *core, Position pos)
{
	SubShield_render_ring(&core->shield, &defenderRingCfg, pos);
}

void SubImmolate_render_aura(const SubImmolateCore *core, Position pos)
{
	if (!SubShield_is_active(&core->shield))
		return;

	float pulse = 0.15f + 0.1f * sinf(core->shield.pulseTimer * immolateConfig.shield.pulse_speed);
	float cx = (float)pos.x;
	float cy = (float)pos.y;

	Render_filled_circle(cx, cy, (float)immolateConfig.aura_radius, 16,
		1.0f, 0.3f, 0.0f, pulse);
	Render_filled_circle(cx, cy, (float)immolateConfig.aura_radius * 0.5f, 12,
		1.0f, 0.5f, 0.1f, pulse * 1.5f);
}

void SubImmolate_render_bloom(const SubImmolateCore *core, Position pos)
{
	SubShield_render_bloom(&core->shield, &immolateConfig.shield, pos);
}

void SubImmolate_render_bloom_small(const SubImmolateCore *core, Position pos)
{
	SubShield_render_bloom(&core->shield, &defenderRingCfg, pos);
}

void SubImmolate_render_light(const SubImmolateCore *core, Position pos)
{
	SubShield_render_light(&core->shield, &immolateConfig.shield, pos);

	/* Extra aura light when active */
	if (SubShield_is_active(&core->shield)) {
		float pulse = 0.2f + 0.1f * sinf(core->shield.pulseTimer * 4.0f);
		Render_filled_circle((float)pos.x, (float)pos.y,
			(float)immolateConfig.aura_radius * 1.5f, 12,
			1.0f, 0.4f, 0.05f, pulse);
	}
}

/* --- Audio lifecycle (reuses shield audio) --- */

void SubImmolate_initialize_audio(void)
{
	SubShield_initialize_audio();
}

void SubImmolate_cleanup_audio(void)
{
	SubShield_cleanup_audio();
}
