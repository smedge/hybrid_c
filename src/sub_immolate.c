#include "sub_immolate.h"

#include "sub_immolate_core.h"
#include "skillbar.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"

#define FEEDBACK_COST 30.0

static SubImmolateCore core;

void Sub_Immolate_initialize(void)
{
	SubImmolate_init(&core);
	SubImmolate_initialize_audio();
}

void Sub_Immolate_cleanup(void)
{
	if (SubImmolate_is_active(&core))
		PlayerStats_set_shielded(false);

	SubImmolate_cleanup_audio();
}

void Sub_Immolate_update(const Input *input, unsigned int ticks)
{
	if (Ship_is_destroyed()) return;

	if (!SubImmolate_is_active(&core)) {
		/* Try to activate */
		if (input->keyF && Skillbar_is_active(SUB_ID_IMMOLATE)) {
			if (SubImmolate_try_activate(&core)) {
				PlayerStats_set_shielded(true);
				PlayerStats_add_feedback(FEEDBACK_COST);
			}
		}
	} else {
		/* Check if shield was broken externally */
		if (!PlayerStats_is_shielded()) {
			SubImmolate_break(&core);
			return;
		}
		/* Tick the core — returns true if shield just expired */
		if (SubImmolate_update(&core, ticks)) {
			PlayerStats_set_shielded(false);
			return;
		}
	}

	/* Tick cooldown when not active */
	if (!SubImmolate_is_active(&core))
		SubImmolate_update(&core, ticks);
}

void Sub_Immolate_render(void)
{
	Position pos = Ship_get_position();
	SubImmolate_render_ring(&core, pos);
	SubImmolate_render_aura(&core, pos);
}

void Sub_Immolate_render_bloom_source(void)
{
	SubImmolate_render_bloom(&core, Ship_get_position());
}

void Sub_Immolate_render_light_source(void)
{
	SubImmolate_render_light(&core, Ship_get_position());
}

float Sub_Immolate_get_cooldown_fraction(void)
{
	return SubImmolate_get_cooldown_fraction(&core);
}

void Sub_Immolate_on_hit(void)
{
	SubImmolate_on_hit(&core);
}

int Sub_Immolate_check_burn(Rectangle target)
{
	return SubImmolate_check_burn(&core, Ship_get_position(), target);
}
