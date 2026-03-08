#include "sub_cauterize.h"

#include "sub_cauterize_core.h"
#include "skillbar.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"
#include "burn.h"

#define FEEDBACK_COST 20.0
#define REGEN_BOOST_DURATION 5000
#define REGEN_BOOST_MULTIPLIER 3.0

static SubCauterizeCore core;

void Sub_Cauterize_initialize(void)
{
	SubCauterize_init(&core);
	SubCauterize_initialize_audio();
}

void Sub_Cauterize_cleanup(void)
{
	SubCauterize_deactivate_all(&core);
	SubCauterize_cleanup_audio();
}

void Sub_Cauterize_try_activate_player(void)
{
	if (!SubCauterize_is_ready(&core)) return;
	const SubCauterizeConfig *cfg = SubCauterize_get_config();
	Position shipPos = Ship_get_position();
	SubCauterize_try_activate(&core, cfg, shipPos, shipPos);
	PlayerStats_heal(cfg->heal_amount);
	PlayerStats_boost_regen(REGEN_BOOST_DURATION, REGEN_BOOST_MULTIPLIER);
	PlayerStats_add_feedback(FEEDBACK_COST);
	Burn_grant_immunity_player(cfg->immunity_duration_ms);
}

void Sub_Cauterize_update(const Input *input, unsigned int ticks)
{
	(void)input;
	if (Ship_is_destroyed()) return;

	const SubCauterizeConfig *cfg = SubCauterize_get_config();

	/* Only tick cooldown here — auras ticked separately */
	SubHealConfig healCfg = { .cooldown_ms = cfg->cooldown_ms };
	SubHeal_update(&core.heal, &healCfg, ticks);
}

void Sub_Cauterize_update_auras(unsigned int ticks)
{
	SubCauterize_update(&core, SubCauterize_get_config(), ticks);
}

int Sub_Cauterize_check_aura_burn(Rectangle target)
{
	return SubCauterize_check_aura_burn(&core, target);
}

void Sub_Cauterize_deactivate_all(void)
{
	SubCauterize_deactivate_all(&core);
}

float Sub_Cauterize_get_cooldown_fraction(void)
{
	return SubCauterize_get_cooldown_fraction(&core);
}

void Sub_Cauterize_render_aura(void)
{
	SubCauterize_render_aura(&core);
}

void Sub_Cauterize_render_aura_bloom_source(void)
{
	SubCauterize_render_aura_bloom(&core);
}

void Sub_Cauterize_render_aura_light_source(void)
{
	SubCauterize_render_aura_light(&core);
}
