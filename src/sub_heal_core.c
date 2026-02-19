#include "sub_heal_core.h"
#include "render.h"
#include "audio.h"

#include <SDL2/SDL_mixer.h>

static Mix_Chunk *sampleHeal = 0;

void SubHeal_initialize_audio(void)
{
	Audio_load_sample(&sampleHeal, "resources/sounds/heal.wav");
}

void SubHeal_cleanup_audio(void)
{
	Audio_unload_sample(&sampleHeal);
}

void SubHeal_init(SubHealCore *core)
{
	core->cooldownMs = 0;
	core->beamActive = false;
	core->beamTimerMs = 0;
	core->beamOrigin = (Position){0, 0};
	core->beamTarget = (Position){0, 0};
}

bool SubHeal_try_activate(SubHealCore *core, const SubHealConfig *cfg, Position origin, Position target)
{
	if (core->cooldownMs > 0)
		return false;
	core->cooldownMs = cfg->cooldown_ms;
	if (cfg->beam_duration_ms > 0) {
		core->beamActive = true;
		core->beamTimerMs = cfg->beam_duration_ms;
		core->beamOrigin = origin;
		core->beamTarget = target;
	}
	Audio_play_sample(&sampleHeal);
	return true;
}

void SubHeal_update(SubHealCore *core, const SubHealConfig *cfg, unsigned int ticks)
{
	if (core->cooldownMs > 0) {
		core->cooldownMs -= (int)ticks;
		if (core->cooldownMs < 0)
			core->cooldownMs = 0;
	}
	if (core->beamActive) {
		core->beamTimerMs -= (int)ticks;
		if (core->beamTimerMs <= 0)
			core->beamActive = false;
	}
	(void)cfg;
}

bool SubHeal_is_ready(const SubHealCore *core)
{
	return core->cooldownMs <= 0;
}

float SubHeal_get_cooldown_fraction(const SubHealCore *core, const SubHealConfig *cfg)
{
	if (core->cooldownMs <= 0)
		return 0.0f;
	return (float)core->cooldownMs / cfg->cooldown_ms;
}

void SubHeal_render_beam(const SubHealCore *core, const SubHealConfig *cfg)
{
	if (!core->beamActive || cfg->beam_duration_ms <= 0)
		return;

	float fade = (float)core->beamTimerMs / cfg->beam_duration_ms;
	Render_thick_line(
		(float)core->beamOrigin.x, (float)core->beamOrigin.y,
		(float)core->beamTarget.x, (float)core->beamTarget.y,
		cfg->beam_thickness, cfg->beam_color_r, cfg->beam_color_g, cfg->beam_color_b, fade);
}

void SubHeal_render_beam_bloom(const SubHealCore *core, const SubHealConfig *cfg)
{
	if (!core->beamActive || cfg->beam_duration_ms <= 0)
		return;

	float fade = (float)core->beamTimerMs / cfg->beam_duration_ms;
	Render_thick_line(
		(float)core->beamOrigin.x, (float)core->beamOrigin.y,
		(float)core->beamTarget.x, (float)core->beamTarget.y,
		cfg->bloom_beam_thickness,
		cfg->beam_color_r, cfg->beam_color_g, cfg->beam_color_b, fade * 0.6f);
}
