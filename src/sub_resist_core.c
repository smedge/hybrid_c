#include "sub_resist_core.h"
#include "render.h"
#include "color.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const SubResistConfig resistCfg = {
	.duration_ms = 5000,
	.cooldown_ms = 10000,
	.ring_radius = 20.0f,
	.ring_thickness = 1.5f,
	.color_r = 1.0f,
	.color_g = 0.7f,
	.color_b = 0.2f,
	.pulse_speed = 0.003f,
	.bloom_radius = 24.0f,
	.bloom_segments = 12,
	.bloom_alpha = 0.3f,
	.beam_thickness = 1.5f,
	.bloom_beam_thickness = 2.5f,
};

const SubResistConfig *SubResist_get_config(void)
{
	return &resistCfg;
}

void SubResist_init(SubResistCore *core)
{
	core->active = false;
	core->activeTimer = 0;
	core->cooldownMs = 0;
}

bool SubResist_try_activate(SubResistCore *core, const SubResistConfig *cfg)
{
	if (core->active || core->cooldownMs > 0)
		return false;
	core->active = true;
	core->activeTimer = cfg->duration_ms;
	return true;
}

bool SubResist_update(SubResistCore *core, const SubResistConfig *cfg, unsigned int ticks)
{
	bool expired = false;

	if (core->active) {
		core->activeTimer -= (int)ticks;
		if (core->activeTimer <= 0) {
			core->active = false;
			core->activeTimer = 0;
			core->cooldownMs = cfg->cooldown_ms;
			expired = true;
		}
	}

	if (core->cooldownMs > 0) {
		core->cooldownMs -= (int)ticks;
		if (core->cooldownMs < 0)
			core->cooldownMs = 0;
	}

	return expired;
}

bool SubResist_is_active(const SubResistCore *core)
{
	return core->active;
}

float SubResist_get_cooldown_fraction(const SubResistCore *core, const SubResistConfig *cfg)
{
	if (core->active)
		return (float)core->activeTimer / cfg->duration_ms;
	if (core->cooldownMs > 0)
		return (float)core->cooldownMs / cfg->cooldown_ms;
	return 0.0f;
}

void SubResist_render_ring(const SubResistCore *core, const SubResistConfig *cfg, Position pos)
{
	if (!core->active)
		return;

	float pulse = 0.7f + 0.3f * sinf((float)core->activeTimer * cfg->pulse_speed);

	for (int i = 0; i < 6; i++) {
		float a0 = i * 60.0f * (float)M_PI / 180.0f;
		float a1 = (i + 1) * 60.0f * (float)M_PI / 180.0f;
		float x0 = (float)pos.x + cosf(a0) * cfg->ring_radius;
		float y0 = (float)pos.y + sinf(a0) * cfg->ring_radius;
		float x1 = (float)pos.x + cosf(a1) * cfg->ring_radius;
		float y1 = (float)pos.y + sinf(a1) * cfg->ring_radius;
		Render_thick_line(x0, y0, x1, y1, cfg->ring_thickness,
			cfg->color_r, cfg->color_g, cfg->color_b, 0.6f * pulse);
	}
}

void SubResist_render_bloom(const SubResistCore *core, const SubResistConfig *cfg, Position pos)
{
	if (!core->active)
		return;

	float pulse = 0.7f + 0.3f * sinf((float)core->activeTimer * cfg->pulse_speed);

	Render_filled_circle(
		(float)pos.x, (float)pos.y,
		cfg->bloom_radius, cfg->bloom_segments,
		cfg->color_r, cfg->color_g, cfg->color_b, cfg->bloom_alpha * pulse);
}

void SubResist_render_beam(const SubResistCore *core, const SubResistConfig *cfg, Position origin, Position target)
{
	if (!core->active)
		return;

	float pulse = 0.7f + 0.3f * sinf((float)core->activeTimer * cfg->pulse_speed);
	Render_thick_line(
		(float)origin.x, (float)origin.y,
		(float)target.x, (float)target.y,
		cfg->beam_thickness, cfg->color_r, cfg->color_g, cfg->color_b, 0.5f * pulse);
}

void SubResist_render_beam_bloom(const SubResistCore *core, const SubResistConfig *cfg, Position origin, Position target)
{
	if (!core->active)
		return;

	float pulse = 0.7f + 0.3f * sinf((float)core->activeTimer * cfg->pulse_speed);
	Render_thick_line(
		(float)origin.x, (float)origin.y,
		(float)target.x, (float)target.y,
		cfg->bloom_beam_thickness, cfg->color_r, cfg->color_g, cfg->color_b, 0.3f * pulse);
}
