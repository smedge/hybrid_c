#include "sub_heatwave.h"

#include "sub_heatwave_core.h"
#include "skillbar.h"
#include "player_stats.h"
#include "ship.h"
#include "enemy_registry.h"

#define HEATWAVE_FEEDBACK_COST 25.0

static SubHeatwaveCore hwCore;
static bool keyWasDown;

void Sub_Heatwave_initialize(void)
{
	SubHeatwave_init(&hwCore);
	keyWasDown = false;
	SubHeatwave_initialize_audio();
}

void Sub_Heatwave_cleanup(void)
{
	SubHeatwave_cleanup_audio();
}

void Sub_Heatwave_try_activate(void)
{
	if (Ship_is_destroyed())
		return;
	if (hwCore.cooldownMs > 0)
		return;
	if (Skillbar_find_equipped_slot(SUB_ID_HEATWAVE) < 0)
		return;

	Position center = Ship_get_position();
	const SubHeatwaveConfig *cfg = SubHeatwave_get_config();
	if (SubHeatwave_try_activate(&hwCore, cfg, center)) {
		PlayerStats_add_feedback(HEATWAVE_FEEDBACK_COST);
		EnemyRegistry_apply_heatwave(center, cfg->half_size,
			cfg->feedback_multiplier, cfg->debuff_duration_ms);
	}
}

void Sub_Heatwave_update(const Input *input, unsigned int ticks)
{
	if (Ship_is_destroyed()) {
		hwCore.visualActive = false;
		return;
	}

	SubHeatwave_update(&hwCore, SubHeatwave_get_config(), ticks);

	/* V key activation (edge-detect) */
	bool keyDown = input->keyV && Skillbar_find_equipped_slot(SUB_ID_HEATWAVE) >= 0;
	if (keyDown && !keyWasDown && hwCore.cooldownMs <= 0)
		Sub_Heatwave_try_activate();
	keyWasDown = keyDown;
}

bool Sub_Heatwave_is_active(void)
{
	return SubHeatwave_is_active(&hwCore);
}

void Sub_Heatwave_render(void)
{
	SubHeatwave_render_ring(&hwCore, SubHeatwave_get_config());
}

void Sub_Heatwave_render_bloom_source(void)
{
	SubHeatwave_render_bloom(&hwCore, SubHeatwave_get_config());
}

void Sub_Heatwave_render_light_source(void)
{
	SubHeatwave_render_light(&hwCore, SubHeatwave_get_config());
}

float Sub_Heatwave_get_cooldown_fraction(void)
{
	return SubHeatwave_get_cooldown_fraction(&hwCore, SubHeatwave_get_config());
}
