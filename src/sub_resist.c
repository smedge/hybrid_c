#include "sub_resist.h"

#include "sub_resist_core.h"
#include "skillbar.h"
#include "progression.h"
#include "ship.h"
#include "player_stats.h"
#include "render.h"
#include "color.h"

#include <math.h>

#define RESIST_FEEDBACK_COST 25.0


static SubResistCore resistCore;

void Sub_Resist_initialize(void)
{
	SubResist_init(&resistCore);
}

void Sub_Resist_cleanup(void)
{
}

void Sub_Resist_try_activate(void)
{
	if (resistCore.active || resistCore.cooldownMs > 0) return;
	if (SubResist_try_activate(&resistCore, SubResist_get_config())) {
		PlayerStats_add_feedback(RESIST_FEEDBACK_COST);
		PlayerStats_set_resist(true, SubResist_get_config()->duration_ms);
	}
}

void Sub_Resist_update(const Input *input, unsigned int ticks)
{
	(void)input;
	if (Ship_is_destroyed()) {
		if (resistCore.active) {
			resistCore.active = false;
			PlayerStats_set_resist(false, 0);
		}
		return;
	}

	bool expired = SubResist_update(&resistCore, SubResist_get_config(), ticks);
	if (expired)
		PlayerStats_set_resist(false, 0);
}

bool Sub_Resist_is_active(void)
{
	return SubResist_is_active(&resistCore);
}

void Sub_Resist_render(void)
{
	Position shipPos = Ship_get_position();
	SubResist_render_ring(&resistCore, SubResist_get_config(), shipPos);
}

void Sub_Resist_render_bloom_source(void)
{
	Position shipPos = Ship_get_position();
	SubResist_render_bloom(&resistCore, SubResist_get_config(), shipPos);
}

void Sub_Resist_render_light_source(void)
{
	if (!resistCore.active)
		return;

	Position shipPos = Ship_get_position();
	float pulse = 0.7f + 0.3f * sinf((float)resistCore.activeTimer * 0.003f);

	Render_point(&shipPos, 8.0f, &(ColorFloat){1.0f, 0.7f, 0.2f, 0.4f * pulse});
}

float Sub_Resist_get_cooldown_fraction(void)
{
	return SubResist_get_cooldown_fraction(&resistCore, SubResist_get_config());
}
