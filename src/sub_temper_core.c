#include "sub_temper_core.h"
#include "render.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --- Config singleton --- */

static const SubTemperConfig temperCfg = {
	.duration_ms = 5000,
	.cooldown_ms = 10000,
	.reflect_percent = 0.50,
	/* Deep orange-red visuals */
	.ring_radius = 20.0f,
	.ring_thickness = 1.8f,
	.color_r = 1.0f,
	.color_g = 0.3f,
	.color_b = 0.05f,
	.pulse_speed = 0.004f,
	.bloom_radius = 24.0f,
	.bloom_segments = 12,
	.bloom_alpha = 0.35f,
	.beam_thickness = 1.5f,
	.bloom_beam_thickness = 2.5f,
};

const SubTemperConfig *SubTemper_get_config(void)
{
	return &temperCfg;
}

/* --- Lifecycle --- */

void SubTemper_init(SubTemperCore *core)
{
	SubResist_init(&core->resist);
}

bool SubTemper_try_activate(SubTemperCore *core, const SubTemperConfig *cfg)
{
	SubResistConfig rcfg = {
		.duration_ms = cfg->duration_ms,
		.cooldown_ms = cfg->cooldown_ms,
	};
	return SubResist_try_activate(&core->resist, &rcfg);
}

bool SubTemper_update(SubTemperCore *core, const SubTemperConfig *cfg, unsigned int ticks)
{
	SubResistConfig rcfg = {
		.duration_ms = cfg->duration_ms,
		.cooldown_ms = cfg->cooldown_ms,
	};
	return SubResist_update(&core->resist, &rcfg, ticks);
}

bool SubTemper_is_active(const SubTemperCore *core)
{
	return SubResist_is_active(&core->resist);
}

float SubTemper_get_cooldown_fraction(const SubTemperCore *core, const SubTemperConfig *cfg)
{
	SubResistConfig rcfg = {
		.duration_ms = cfg->duration_ms,
		.cooldown_ms = cfg->cooldown_ms,
	};
	return SubResist_get_cooldown_fraction(&core->resist, &rcfg);
}

/* --- Reflection --- */

double SubTemper_check_reflect(const SubTemperCore *core, const SubTemperConfig *cfg,
	double damage, bool is_fire)
{
	if (!SubResist_is_active(&core->resist))
		return 0.0;
	if (!is_fire)
		return 0.0;
	return damage * cfg->reflect_percent;
}

/* --- Rendering --- */

void SubTemper_render_ring(const SubTemperCore *core, const SubTemperConfig *cfg, Position pos)
{
	if (!core->resist.active)
		return;

	float pulse = 0.7f + 0.3f * sinf((float)core->resist.activeTimer * cfg->pulse_speed);

	for (int i = 0; i < 6; i++) {
		float a0 = i * 60.0f * (float)M_PI / 180.0f;
		float a1 = (i + 1) * 60.0f * (float)M_PI / 180.0f;
		float x0 = (float)pos.x + cosf(a0) * cfg->ring_radius;
		float y0 = (float)pos.y + sinf(a0) * cfg->ring_radius;
		float x1 = (float)pos.x + cosf(a1) * cfg->ring_radius;
		float y1 = (float)pos.y + sinf(a1) * cfg->ring_radius;
		Render_thick_line(x0, y0, x1, y1, cfg->ring_thickness,
			cfg->color_r, cfg->color_g, cfg->color_b, 0.7f * pulse);
	}
}

void SubTemper_render_bloom(const SubTemperCore *core, const SubTemperConfig *cfg, Position pos)
{
	if (!core->resist.active)
		return;

	float pulse = 0.7f + 0.3f * sinf((float)core->resist.activeTimer * cfg->pulse_speed);

	Render_filled_circle(
		(float)pos.x, (float)pos.y,
		cfg->bloom_radius, cfg->bloom_segments,
		cfg->color_r, cfg->color_g, cfg->color_b, cfg->bloom_alpha * pulse);
}

void SubTemper_render_beam(const SubTemperCore *core, const SubTemperConfig *cfg, Position origin, Position target)
{
	if (!core->resist.active)
		return;

	float pulse = 0.7f + 0.3f * sinf((float)core->resist.activeTimer * cfg->pulse_speed);
	Render_thick_line(
		(float)origin.x, (float)origin.y,
		(float)target.x, (float)target.y,
		cfg->beam_thickness, cfg->color_r, cfg->color_g, cfg->color_b, 0.5f * pulse);
}

void SubTemper_render_beam_bloom(const SubTemperCore *core, const SubTemperConfig *cfg, Position origin, Position target)
{
	if (!core->resist.active)
		return;

	float pulse = 0.7f + 0.3f * sinf((float)core->resist.activeTimer * cfg->pulse_speed);
	Render_thick_line(
		(float)origin.x, (float)origin.y,
		(float)target.x, (float)target.y,
		cfg->bloom_beam_thickness, cfg->color_r, cfg->color_g, cfg->color_b, 0.3f * pulse);
}
