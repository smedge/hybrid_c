#include "sub_shield_core.h"
#include "render.h"
#include "audio.h"

#include <SDL2/SDL_mixer.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static Mix_Chunk *sampleHit = 0;
static Mix_Chunk *sampleActivate = 0;
static Mix_Chunk *sampleDeactivate = 0;

void SubShield_initialize_audio(void)
{
	Audio_load_sample(&sampleHit, "resources/sounds/ricochet.wav");
	Audio_load_sample(&sampleActivate, "resources/sounds/statue_rise.wav");
	Audio_load_sample(&sampleDeactivate, "resources/sounds/door.wav");
}

void SubShield_cleanup_audio(void)
{
	Audio_unload_sample(&sampleHit);
	Audio_unload_sample(&sampleActivate);
	Audio_unload_sample(&sampleDeactivate);
}

void SubShield_init(SubShieldCore *core)
{
	core->active = false;
	core->activeMs = 0;
	core->cooldownMs = 0;
	core->pulseTimer = 0.0f;
	core->graceMs = 0;
}

bool SubShield_try_activate(SubShieldCore *core, const SubShieldConfig *cfg)
{
	if (core->active || core->cooldownMs > 0)
		return false;
	core->active = true;
	core->activeMs = cfg->duration_ms;
	core->pulseTimer = 0.0f;
	Audio_play_sample(&sampleActivate);
	return true;
}

void SubShield_break(SubShieldCore *core, const SubShieldConfig *cfg)
{
	if (!core->active)
		return;
	core->active = false;
	core->cooldownMs = cfg->cooldown_ms;
	core->graceMs = cfg->break_grace_ms;
	Audio_play_sample(&sampleDeactivate);
}

bool SubShield_update(SubShieldCore *core, const SubShieldConfig *cfg, unsigned int ticks)
{
	if (core->graceMs > 0)
		core->graceMs -= (int)ticks;

	if (core->active) {
		core->pulseTimer += ticks / 1000.0f;
		core->activeMs -= (int)ticks;
		if (core->activeMs <= 0) {
			core->activeMs = 0;
			core->active = false;
			core->cooldownMs = cfg->cooldown_ms;
			Audio_play_sample(&sampleDeactivate);
			return true;
		}
	} else if (core->cooldownMs > 0) {
		core->cooldownMs -= (int)ticks;
		if (core->cooldownMs < 0)
			core->cooldownMs = 0;
	}

	return false;
}

bool SubShield_is_active(const SubShieldCore *core)
{
	return core->active;
}

bool SubShield_in_grace(const SubShieldCore *core)
{
	return core->graceMs > 0;
}

float SubShield_get_cooldown_fraction(const SubShieldCore *core, const SubShieldConfig *cfg)
{
	if (core->active)
		return (float)core->activeMs / cfg->duration_ms;
	if (core->cooldownMs > 0)
		return (float)core->cooldownMs / cfg->cooldown_ms;
	return 0.0f;
}

void SubShield_on_hit(const SubShieldCore *core)
{
	if (core->active || core->graceMs > 0)
		Audio_play_sample(&sampleHit);
}

void SubShield_render_ring(const SubShieldCore *core, const SubShieldConfig *cfg, Position pos)
{
	if (!core->active)
		return;

	float pulse = cfg->pulse_alpha_min + cfg->pulse_alpha_range * sinf(core->pulseTimer * cfg->pulse_speed);
	float r = cfg->ring_radius * (1.0f - cfg->radius_pulse_amount
		+ cfg->radius_pulse_amount * sinf(core->pulseTimer * cfg->pulse_speed * 2.0f));

	for (int i = 0; i < cfg->ring_segments; i++) {
		float a0 = i * (2.0f * (float)M_PI / cfg->ring_segments);
		float a1 = (i + 1) * (2.0f * (float)M_PI / cfg->ring_segments);
		float x0 = (float)pos.x + cosf(a0) * r;
		float y0 = (float)pos.y + sinf(a0) * r;
		float x1 = (float)pos.x + cosf(a1) * r;
		float y1 = (float)pos.y + sinf(a1) * r;
		Render_thick_line(x0, y0, x1, y1, cfg->ring_thickness,
			cfg->color_r, cfg->color_g, cfg->color_b, pulse);
	}
}

void SubShield_render_bloom(const SubShieldCore *core, const SubShieldConfig *cfg, Position pos)
{
	if (!core->active)
		return;

	float pulse = cfg->bloom_alpha_min + cfg->bloom_alpha_range * sinf(core->pulseTimer * cfg->pulse_speed);

	for (int i = 0; i < cfg->ring_segments; i++) {
		float a0 = i * (2.0f * (float)M_PI / cfg->ring_segments);
		float a1 = (i + 1) * (2.0f * (float)M_PI / cfg->ring_segments);
		float x0 = (float)pos.x + cosf(a0) * cfg->ring_radius;
		float y0 = (float)pos.y + sinf(a0) * cfg->ring_radius;
		float x1 = (float)pos.x + cosf(a1) * cfg->ring_radius;
		float y1 = (float)pos.y + sinf(a1) * cfg->ring_radius;
		Render_thick_line(x0, y0, x1, y1, cfg->bloom_thickness,
			cfg->color_r, cfg->color_g, cfg->color_b, pulse);
	}
}

void SubShield_render_light(const SubShieldCore *core, const SubShieldConfig *cfg, Position pos)
{
	if (!core->active || cfg->light_radius <= 0.0f)
		return;

	float pulse = cfg->bloom_alpha_min + cfg->bloom_alpha_range * sinf(core->pulseTimer * cfg->pulse_speed);
	Render_filled_circle(
		(float)pos.x, (float)pos.y,
		cfg->light_radius, cfg->light_segments,
		cfg->color_r, cfg->color_g, cfg->color_b, pulse);
}
