#include "sub_smolder.h"

#include "sub_smolder_core.h"
#include "skillbar.h"
#include "player_stats.h"
#include "ship.h"



static SubSmolderCore smolderCore;

void Sub_Smolder_initialize(void)
{
	SubSmolder_init(&smolderCore);
	SubSmolder_initialize_audio();
}

void Sub_Smolder_cleanup(void)
{
	SubSmolder_cleanup_audio();
}

void Sub_Smolder_try_activate(void)
{
	if (Ship_is_destroyed())
		return;
	if (smolderCore.state != SMOLDER_READY)
		return;
	if (Skillbar_find_equipped_slot(SUB_ID_SMOLDER) < 0)
		return;

	/* Like stealth — requires zero feedback to enter */
	if (PlayerStats_get_feedback() > 0.0)
		return;

	SubSmolder_try_activate(&smolderCore, SubSmolder_get_config(), Ship_get_position());
}

void Sub_Smolder_break(void)
{
	SubSmolder_break(&smolderCore, Ship_get_position());
}

void Sub_Smolder_break_attack(void)
{
	if (!SubSmolder_is_active(&smolderCore))
		return;

	SubSmolder_break_attack(&smolderCore, Ship_get_position());
}

void Sub_Smolder_update(unsigned int ticks)
{
	if (Ship_is_destroyed()) {
		if (smolderCore.state == SMOLDER_ACTIVE) {
			smolderCore.state = SMOLDER_READY;
			smolderCore.duration_ms = 0;
		}
		smolderCore.ambush_ms = 0;
		return;
	}

	SubSmolder_update(&smolderCore, SubSmolder_get_config(), ticks, Ship_get_position());
}

bool Sub_Smolder_is_active(void)
{
	return SubSmolder_is_active(&smolderCore);
}

bool Sub_Smolder_is_ambush_active(void)
{
	return SubSmolder_is_ambush_active(&smolderCore);
}

double Sub_Smolder_get_damage_multiplier(double distance)
{
	const SubSmolderConfig *cfg = SubSmolder_get_config();
	if (smolderCore.ambush_ms <= 0)
		return 1.0;
	if (distance > cfg->ambush_burn_radius)
		return 1.0;
	return cfg->ambush_multiplier;
}

void Sub_Smolder_notify_kill(void)
{
	SubSmolder_notify_kill(&smolderCore);
}

float Sub_Smolder_get_ship_alpha(void)
{
	return SubSmolder_get_alpha(&smolderCore, SubSmolder_get_config());
}

float Sub_Smolder_get_cooldown_fraction(void)
{
	return SubSmolder_get_cooldown_fraction(&smolderCore, SubSmolder_get_config());
}
