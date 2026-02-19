#include "sub_dash_core.h"
#include "audio.h"

#include <SDL2/SDL_mixer.h>

static Mix_Chunk *sampleDash = 0;

void SubDash_initialize_audio(void)
{
	Audio_load_sample(&sampleDash, "resources/sounds/enemy_aggro.wav");
}

void SubDash_cleanup_audio(void)
{
	Audio_unload_sample(&sampleDash);
}

void SubDash_init(SubDashCore *core)
{
	core->active = false;
	core->dashTimeLeft = 0;
	core->cooldownMs = 0;
	core->dirX = 0.0;
	core->dirY = 0.0;
	core->hitThisDash = false;
}

bool SubDash_try_activate(SubDashCore *core, const SubDashConfig *cfg, double dirX, double dirY)
{
	if (core->active || core->cooldownMs > 0)
		return false;
	core->active = true;
	core->dashTimeLeft = cfg->duration_ms;
	core->dirX = dirX;
	core->dirY = dirY;
	core->hitThisDash = false;
	Audio_play_sample(&sampleDash);
	return true;
}

bool SubDash_update(SubDashCore *core, const SubDashConfig *cfg, unsigned int ticks)
{
	if (core->active) {
		core->dashTimeLeft -= (int)ticks;
		if (core->dashTimeLeft <= 0) {
			core->dashTimeLeft = 0;
			core->active = false;
			core->cooldownMs = cfg->cooldown_ms;
			return true;
		}
	} else if (core->cooldownMs > 0) {
		core->cooldownMs -= (int)ticks;
		if (core->cooldownMs < 0)
			core->cooldownMs = 0;
	}
	return false;
}

void SubDash_end_early(SubDashCore *core, const SubDashConfig *cfg)
{
	if (!core->active)
		return;
	core->active = false;
	core->dashTimeLeft = 0;
	core->cooldownMs = cfg->cooldown_ms;
}

bool SubDash_is_active(const SubDashCore *core)
{
	return core->active;
}

float SubDash_get_cooldown_fraction(const SubDashCore *core, const SubDashConfig *cfg)
{
	if (core->active)
		return 1.0f;
	if (core->cooldownMs > 0 && cfg->cooldown_ms > 0)
		return (float)core->cooldownMs / cfg->cooldown_ms;
	return 0.0f;
}
