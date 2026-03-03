#include "sub_sprint_core.h"

static const SubSprintConfig sprintCfg = {
	.duration_ms = 5000,
	.cooldown_ms = 15000,
};

const SubSprintConfig *SubSprint_get_config(void)
{
	return &sprintCfg;
}

void SubSprint_init(SubSprintCore *core)
{
	core->active = false;
	core->sprintTimer = 0;
	core->cooldownMs = 0;
}

bool SubSprint_try_activate(SubSprintCore *core, const SubSprintConfig *cfg)
{
	if (core->active || core->cooldownMs > 0)
		return false;
	core->active = true;
	core->sprintTimer = cfg->duration_ms;
	return true;
}

bool SubSprint_update(SubSprintCore *core, const SubSprintConfig *cfg, unsigned int ticks)
{
	bool expired = false;

	if (core->active) {
		core->sprintTimer -= (int)ticks;
		if (core->sprintTimer <= 0) {
			core->active = false;
			core->sprintTimer = 0;
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

bool SubSprint_is_active(const SubSprintCore *core)
{
	return core->active;
}

float SubSprint_get_cooldown_fraction(const SubSprintCore *core, const SubSprintConfig *cfg)
{
	if (core->active)
		return (float)core->sprintTimer / cfg->duration_ms;
	if (core->cooldownMs > 0)
		return (float)core->cooldownMs / cfg->cooldown_ms;
	return 0.0f;
}
