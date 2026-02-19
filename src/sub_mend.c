#include "sub_mend.h"

#include "sub_heal_core.h"
#include "skillbar.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"

#define FEEDBACK_COST 20.0
#define REGEN_BOOST_DURATION 5000
#define REGEN_BOOST_MULTIPLIER 3.0

static SubHealCore core;
static const SubHealConfig cfg = {
	.heal_amount = 50.0,
	.cooldown_ms = 10000,
	.beam_duration_ms = 0,
	.beam_thickness = 0.0f,
	.beam_color_r = 0.0f, .beam_color_g = 0.0f, .beam_color_b = 0.0f,
	.bloom_beam_thickness = 0.0f,
};

void Sub_Mend_initialize(void)
{
	SubHeal_init(&core);
	SubHeal_initialize_audio();
}

void Sub_Mend_cleanup(void)
{
	SubHeal_cleanup_audio();
}

void Sub_Mend_update(const Input *input, unsigned int ticks)
{
	if (Ship_is_destroyed()) return;

	SubHeal_update(&core, &cfg, ticks);

	if (input->keyG && Skillbar_is_active(SUB_ID_MEND) && SubHeal_is_ready(&core)) {
		Position shipPos = Ship_get_position();
		SubHeal_try_activate(&core, &cfg, shipPos, shipPos);
		PlayerStats_heal(cfg.heal_amount);
		PlayerStats_boost_regen(REGEN_BOOST_DURATION, REGEN_BOOST_MULTIPLIER);
		PlayerStats_add_feedback(FEEDBACK_COST);
	}
}

float Sub_Mend_get_cooldown_fraction(void)
{
	return SubHeal_get_cooldown_fraction(&core, &cfg);
}
