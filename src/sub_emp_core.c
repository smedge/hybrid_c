#include "sub_emp_core.h"
#include "render.h"
#include "audio.h"

#include <math.h>
#include <SDL2/SDL_mixer.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static Mix_Chunk *sampleFire = 0;

void SubEmp_initialize_audio(void)
{
	Audio_load_sample(&sampleFire, "resources/sounds/bomb_explode.wav");
}

void SubEmp_cleanup_audio(void)
{
	Audio_unload_sample(&sampleFire);
}

void SubEmp_init(SubEmpCore *core)
{
	core->cooldownMs = 0;
	core->visualActive = false;
	core->visualTimer = 0;
	core->visualCenter = (Position){0, 0};
}

bool SubEmp_try_activate(SubEmpCore *core, const SubEmpConfig *cfg, Position center)
{
	if (core->cooldownMs > 0)
		return false;

	core->cooldownMs = cfg->cooldown_ms;
	core->visualActive = true;
	core->visualTimer = 0;
	core->visualCenter = center;
	Audio_play_sample(&sampleFire);
	return true;
}

void SubEmp_update(SubEmpCore *core, const SubEmpConfig *cfg, unsigned int ticks)
{
	if (core->cooldownMs > 0) {
		core->cooldownMs -= (int)ticks;
		if (core->cooldownMs < 0)
			core->cooldownMs = 0;
	}

	if (core->visualActive) {
		core->visualTimer += ticks;
		if (core->visualTimer >= cfg->visual_ms)
			core->visualActive = false;
	}
}

bool SubEmp_is_active(const SubEmpCore *core)
{
	return core->visualActive;
}

float SubEmp_get_cooldown_fraction(const SubEmpCore *core, const SubEmpConfig *cfg)
{
	if (core->cooldownMs > 0)
		return (float)core->cooldownMs / cfg->cooldown_ms;
	return 0.0f;
}

void SubEmp_render_ring(const SubEmpCore *core, const SubEmpConfig *cfg)
{
	if (!core->visualActive)
		return;

	float t = (float)core->visualTimer / cfg->visual_ms;
	float radius = cfg->ring_max_radius * t;
	float alpha = 1.0f - t;

	/* Outer expanding ring */
	for (int i = 0; i < cfg->segments; i++) {
		float a0 = i * (2.0f * (float)M_PI / cfg->segments);
		float a1 = (i + 1) * (2.0f * (float)M_PI / cfg->segments);
		float x0 = (float)core->visualCenter.x + cosf(a0) * radius;
		float y0 = (float)core->visualCenter.y + sinf(a0) * radius;
		float x1 = (float)core->visualCenter.x + cosf(a1) * radius;
		float y1 = (float)core->visualCenter.y + sinf(a1) * radius;
		Render_thick_line(x0, y0, x1, y1, cfg->outer_thickness,
			cfg->outer_r, cfg->outer_g, cfg->outer_b, alpha);
	}

	/* Inner bright ring */
	float innerRadius = radius * cfg->inner_ring_ratio;
	for (int i = 0; i < cfg->segments; i++) {
		float a0 = i * (2.0f * (float)M_PI / cfg->segments);
		float a1 = (i + 1) * (2.0f * (float)M_PI / cfg->segments);
		float x0 = (float)core->visualCenter.x + cosf(a0) * innerRadius;
		float y0 = (float)core->visualCenter.y + sinf(a0) * innerRadius;
		float x1 = (float)core->visualCenter.x + cosf(a1) * innerRadius;
		float y1 = (float)core->visualCenter.y + sinf(a1) * innerRadius;
		Render_thick_line(x0, y0, x1, y1, cfg->inner_thickness,
			cfg->inner_r, cfg->inner_g, cfg->inner_b, alpha * cfg->inner_alpha_mult);
	}
}

void SubEmp_render_bloom(const SubEmpCore *core, const SubEmpConfig *cfg)
{
	if (!core->visualActive)
		return;

	float t = (float)core->visualTimer / cfg->visual_ms;
	float radius = cfg->ring_max_radius * t;
	float alpha = 1.0f - t;

	Render_filled_circle(
		(float)core->visualCenter.x, (float)core->visualCenter.y,
		radius, cfg->segments,
		cfg->outer_r, cfg->outer_g, cfg->outer_b, alpha * 0.5f);
}

void SubEmp_render_light(const SubEmpCore *core, const SubEmpConfig *cfg)
{
	if (!core->visualActive)
		return;

	float t = (float)core->visualTimer / cfg->visual_ms;
	float radius = cfg->ring_max_radius * t * 1.5f;
	float alpha = (1.0f - t) * 0.8f;

	Render_filled_circle(
		(float)core->visualCenter.x, (float)core->visualCenter.y,
		radius, cfg->segments,
		cfg->outer_r, cfg->outer_g, cfg->outer_b, alpha);
}
