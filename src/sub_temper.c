#include "sub_temper.h"

#include "sub_temper_core.h"
#include "skillbar.h"
#include "player_stats.h"
#include "ship.h"
#include "enemy_registry.h"
#include "render.h"
#include "color.h"
#include "burn.h"

#include <math.h>

#define TEMPER_FEEDBACK_COST 20.0
#define TEMPER_REFLECT_RADIUS 120.0
#define TEMPER_BURN_DURATION_MS 2000

static SubTemperCore temperCore;

void Sub_Temper_initialize(void)
{
	SubTemper_init(&temperCore);
}

void Sub_Temper_cleanup(void)
{
}

void Sub_Temper_try_activate(void)
{
	if (SubTemper_is_active(&temperCore))
		return;
	if (SubTemper_get_cooldown_fraction(&temperCore, SubTemper_get_config()) > 0.0f)
		return;
	if (Skillbar_find_equipped_slot(SUB_ID_TEMPER) < 0)
		return;

	if (SubTemper_try_activate(&temperCore, SubTemper_get_config())) {
		PlayerStats_add_feedback(TEMPER_FEEDBACK_COST);
		PlayerStats_set_reflect(true);
	}
}

void Sub_Temper_update(const Input *input, unsigned int ticks)
{
	(void)input;
	if (Ship_is_destroyed()) {
		if (temperCore.resist.active) {
			temperCore.resist.active = false;
			PlayerStats_set_reflect(false);
		}
		return;
	}

	bool expired = SubTemper_update(&temperCore, SubTemper_get_config(), ticks);
	if (expired)
		PlayerStats_set_reflect(false);

	/* Flush reflected damage as burn to nearby enemies */
	double reflected = PlayerStats_flush_reflected_damage();
	if (reflected > 0.0) {
		Position shipPos = Ship_get_position();
		EnemyRegistry_apply_burn(shipPos, TEMPER_REFLECT_RADIUS, TEMPER_BURN_DURATION_MS);
	}
}

bool Sub_Temper_is_active(void)
{
	return SubTemper_is_active(&temperCore);
}

void Sub_Temper_render(void)
{
	Position shipPos = Ship_get_position();
	SubTemper_render_ring(&temperCore, SubTemper_get_config(), shipPos);
}

void Sub_Temper_render_bloom_source(void)
{
	Position shipPos = Ship_get_position();
	SubTemper_render_bloom(&temperCore, SubTemper_get_config(), shipPos);
}

void Sub_Temper_render_light_source(void)
{
	if (!temperCore.resist.active)
		return;

	Position shipPos = Ship_get_position();
	float pulse = 0.7f + 0.3f * sinf((float)temperCore.resist.activeTimer * 0.003f);

	Render_point(&shipPos, 8.0f, &(ColorFloat){1.0f, 0.3f, 0.05f, 0.4f * pulse});
}

float Sub_Temper_get_cooldown_fraction(void)
{
	return SubTemper_get_cooldown_fraction(&temperCore, SubTemper_get_config());
}
