#include "sub_sprint.h"

#include "sub_sprint_core.h"
#include "skillbar.h"
#include "progression.h"
#include "ship.h"
#include "player_stats.h"

#define FEEDBACK_COST 0.0


static SubSprintCore sprintCore;

void Sub_Sprint_initialize(void)
{
	SubSprint_init(&sprintCore);
}

void Sub_Sprint_cleanup(void)
{
}

void Sub_Sprint_try_activate(void)
{
	if (sprintCore.active || sprintCore.cooldownMs > 0) return;
	SubSprint_try_activate(&sprintCore, SubSprint_get_config());
	if (FEEDBACK_COST > 0.0)
		PlayerStats_add_feedback(FEEDBACK_COST);
}

void Sub_Sprint_update(const Input *input, unsigned int ticks)
{
	(void)input;
	if (Ship_is_destroyed()) {
		sprintCore.active = false;
		return;
	}

	SubSprint_update(&sprintCore, SubSprint_get_config(), ticks);
}

bool Sub_Sprint_is_sprinting(void)
{
	return SubSprint_is_active(&sprintCore);
}

float Sub_Sprint_get_cooldown_fraction(void)
{
	return SubSprint_get_cooldown_fraction(&sprintCore, SubSprint_get_config());
}
