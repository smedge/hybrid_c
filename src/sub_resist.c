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

static const SubResistConfig playerResistCfg = {
	.duration_ms = 5000,
	.cooldown_ms = 15000,
	.ring_radius = 20.0f,
	.ring_thickness = 1.5f,
	.color_r = 1.0f,
	.color_g = 0.7f,
	.color_b = 0.2f,
	.pulse_speed = 0.003f,
	.bloom_radius = 24.0f,
	.bloom_segments = 12,
	.bloom_alpha = 0.3f,
};

static SubResistCore resistCore;

void Sub_Resist_initialize(void)
{
	SubResist_init(&resistCore);
}

void Sub_Resist_cleanup(void)
{
}

void Sub_Resist_update(const Input *input, unsigned int ticks)
{
	if (Ship_is_destroyed()) {
		if (resistCore.active) {
			resistCore.active = false;
			PlayerStats_set_resist(false, 0);
		}
		return;
	}

	bool expired = SubResist_update(&resistCore, &playerResistCfg, ticks);
	if (expired)
		PlayerStats_set_resist(false, 0);

	/* F key activation (same key as aegis — type exclusivity handles conflict) */
	if (input->keyF && !resistCore.active && resistCore.cooldownMs <= 0 &&
		Skillbar_is_active(SUB_ID_RESIST)) {
		if (SubResist_try_activate(&resistCore, &playerResistCfg)) {
			PlayerStats_add_feedback(RESIST_FEEDBACK_COST);
			PlayerStats_set_resist(true, playerResistCfg.duration_ms);
		}
	}
}

bool Sub_Resist_is_active(void)
{
	return SubResist_is_active(&resistCore);
}

void Sub_Resist_render(void)
{
	Position shipPos = Ship_get_position();
	SubResist_render_ring(&resistCore, &playerResistCfg, shipPos);
}

void Sub_Resist_render_bloom_source(void)
{
	Position shipPos = Ship_get_position();
	SubResist_render_bloom(&resistCore, &playerResistCfg, shipPos);
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
	return SubResist_get_cooldown_fraction(&resistCore, &playerResistCfg);
}
