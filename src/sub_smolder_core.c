#include "sub_smolder_core.h"
#include "render.h"
#include "audio.h"

#include <math.h>
#include <SDL2/SDL_mixer.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --- Config singleton --- */

static const SubSmolderConfig smolderConfig = {
	.cloak_duration_ms = 8000,
	.cooldown_ms = 12000,
	.ambush_duration_ms = 1000,
	.ambush_multiplier = 5.0,
	.ambush_burn_radius = 80.0,
	.ambush_burn_stacks = 2,
	.alpha_min = 0.15f,
	.alpha_pulse_speed = 6.0f,
};

const SubSmolderConfig *SubSmolder_get_config(void)
{
	return &smolderConfig;
}

/* --- Audio --- */

static Mix_Chunk *sampleActivate = 0;
static Mix_Chunk *sampleBreak = 0;

void SubSmolder_initialize_audio(void)
{
	Audio_load_sample(&sampleActivate, "resources/sounds/refill_start.wav");
	Audio_load_sample(&sampleBreak, "resources/sounds/door.wav");
}

void SubSmolder_cleanup_audio(void)
{
	Audio_unload_sample(&sampleActivate);
	Audio_unload_sample(&sampleBreak);
}

/* --- Init / Reset --- */

void SubSmolder_init(SubSmolderCore *core)
{
	core->state = SMOLDER_READY;
	core->duration_ms = 0;
	core->cooldown_ms = 0;
	core->ambush_ms = 0;
	core->pulse_timer = 0.0f;
}

void SubSmolder_reset(SubSmolderCore *core)
{
	SubSmolder_init(core);
}

/* --- Activation --- */

bool SubSmolder_try_activate(SubSmolderCore *core, const SubSmolderConfig *cfg, Position pos)
{
	if (core->state != SMOLDER_READY)
		return false;

	core->state = SMOLDER_ACTIVE;
	core->duration_ms = cfg->cloak_duration_ms;
	core->cooldown_ms = cfg->cooldown_ms;
	core->pulse_timer = 0.0f;
	Audio_play_sample_at(&sampleActivate, pos);
	return true;
}

void SubSmolder_activate_silent(SubSmolderCore *core, const SubSmolderConfig *cfg)
{
	if (core->state != SMOLDER_READY)
		return;
	core->state = SMOLDER_ACTIVE;
	core->duration_ms = cfg->cloak_duration_ms;
	core->cooldown_ms = cfg->cooldown_ms;
	core->pulse_timer = 0.0f;
}

/* --- Break --- */

void SubSmolder_break(SubSmolderCore *core, Position pos)
{
	if (core->state != SMOLDER_ACTIVE)
		return;

	core->state = SMOLDER_COOLDOWN;
	Audio_play_sample_at(&sampleBreak, pos);
}

void SubSmolder_break_attack(SubSmolderCore *core, Position pos)
{
	if (core->state != SMOLDER_ACTIVE)
		return;

	core->ambush_ms = SubSmolder_get_config()->ambush_duration_ms;
	SubSmolder_break(core, pos);
}

/* --- Update --- */

void SubSmolder_update(SubSmolderCore *core, const SubSmolderConfig *cfg, unsigned int ticks, Position pos)
{
	/* Tick ambush window */
	if (core->ambush_ms > 0) {
		core->ambush_ms -= (int)ticks;
		if (core->ambush_ms < 0) core->ambush_ms = 0;
	}

	switch (core->state) {
	case SMOLDER_READY:
		break;

	case SMOLDER_ACTIVE:
		core->pulse_timer += ticks / 1000.0f;
		core->duration_ms -= (int)ticks;
		if (core->duration_ms <= 0) {
			/* Cloak expired — auto-break (no ambush) */
			core->duration_ms = 0;
			core->state = SMOLDER_COOLDOWN;
			Audio_play_sample_at(&sampleBreak, pos);
		}
		break;

	case SMOLDER_COOLDOWN:
		core->cooldown_ms -= (int)ticks;
		if (core->cooldown_ms <= 0) {
			core->cooldown_ms = 0;
			core->state = SMOLDER_READY;
		}
		break;
	}

	(void)cfg;
}

/* --- Queries --- */

bool SubSmolder_is_active(const SubSmolderCore *core)
{
	return core->state == SMOLDER_ACTIVE;
}

bool SubSmolder_is_ambush_active(const SubSmolderCore *core)
{
	return core->ambush_ms > 0;
}

double SubSmolder_get_damage_multiplier(const SubSmolderCore *core, const SubSmolderConfig *cfg)
{
	if (core->ambush_ms > 0)
		return cfg->ambush_multiplier;
	return 1.0;
}

void SubSmolder_notify_kill(SubSmolderCore *core)
{
	if (core->ambush_ms > 0) {
		core->cooldown_ms = 0;
		core->state = SMOLDER_READY;
	}
}

float SubSmolder_get_alpha(const SubSmolderCore *core, const SubSmolderConfig *cfg)
{
	if (core->state != SMOLDER_ACTIVE)
		return 1.0f;

	/* Heat shimmer — pulsing semi-transparency */
	float shimmer = sinf(core->pulse_timer * cfg->alpha_pulse_speed);
	return cfg->alpha_min + 0.10f * (0.5f + 0.5f * shimmer);
}

float SubSmolder_get_cooldown_fraction(const SubSmolderCore *core, const SubSmolderConfig *cfg)
{
	if (core->state == SMOLDER_ACTIVE)
		return 0.0f;
	if (core->state == SMOLDER_COOLDOWN)
		return (float)core->cooldown_ms / (float)cfg->cooldown_ms;
	return 0.0f;
}

/* --- Rendering --- */

void SubSmolder_render_shimmer(const SubSmolderCore *core, const SubSmolderConfig *cfg,
	float x, float y)
{
	if (core->state != SMOLDER_ACTIVE)
		return;

	/* Heat shimmer — drifting ember points around cloaked entity */
	float alpha = SubSmolder_get_alpha(core, cfg);
	float t = core->pulse_timer;

	for (int i = 0; i < 6; i++) {
		float angle = (float)i * (float)(M_PI / 3.0) + t * 1.5f;
		float r = 18.0f + 6.0f * sinf(t * 3.0f + (float)i);
		float pa = alpha * (0.4f + 0.3f * sinf(t * 5.0f + (float)i * 1.7f));
		Position pt = {x + r * cosf(angle), y + r * sinf(angle)};
		ColorFloat c = {1.0f, 0.5f, 0.1f, pa};
		Render_point(&pt, 3.0f, &c);
	}
}

void SubSmolder_render_shimmer_bloom(const SubSmolderCore *core, const SubSmolderConfig *cfg,
	float x, float y)
{
	if (core->state != SMOLDER_ACTIVE)
		return;

	float alpha = SubSmolder_get_alpha(core, cfg) * 0.5f;
	Render_filled_circle(x, y, 25.0f, 8, 1.0f, 0.4f, 0.05f, alpha);
}
