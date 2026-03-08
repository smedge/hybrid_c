#include "sub_scorch.h"

#include "sub_scorch_core.h"
#include "skillbar.h"
#include "player_stats.h"
#include "ship.h"

#define SCORCH_FEEDBACK_COST 15.0
#define SCORCH_MAX_FOOTPRINTS 64

static SubScorchCore scorchCore;
static ScorchFootprintPool footprintPool;
static ScorchFootprint footprintBuffer[SCORCH_MAX_FOOTPRINTS];

void Sub_Scorch_initialize(void)
{
	SubScorch_init(&scorchCore);
	SubScorch_pool_init(&footprintPool, footprintBuffer, SCORCH_MAX_FOOTPRINTS);
	SubScorch_initialize_audio();
}

void Sub_Scorch_cleanup(void)
{
	SubScorch_cleanup_audio();
}

void Sub_Scorch_try_activate(void)
{
	if (Ship_is_destroyed())
		return;
	if (SubScorch_is_active(&scorchCore))
		return;
	if (SubScorch_get_cooldown_fraction(&scorchCore, SubScorch_get_config()) > 0.0f)
		return;
	if (Skillbar_find_equipped_slot(SUB_ID_SCORCH) < 0)
		return;

	if (SubScorch_try_activate(&scorchCore, SubScorch_get_config()))
		PlayerStats_add_feedback(SCORCH_FEEDBACK_COST);
}

void Sub_Scorch_update(const Input *input, unsigned int ticks)
{
	(void)input;
	if (Ship_is_destroyed()) {
		scorchCore.sprint.active = false;
		return;
	}

	SubScorch_update(&scorchCore, SubScorch_get_config(), ticks);

	/* Deposit footprints while sprinting */
	if (SubScorch_is_active(&scorchCore)) {
		const SubScorchConfig *cfg = SubScorch_get_config();
		scorchCore.footprint_timer += (int)ticks;
		if (scorchCore.footprint_timer >= cfg->footprint_interval_ms) {
			scorchCore.footprint_timer -= cfg->footprint_interval_ms;
			SubScorch_pool_spawn(&footprintPool, cfg, Ship_get_position());
		}
	}
}

bool Sub_Scorch_is_sprinting(void)
{
	return SubScorch_is_active(&scorchCore);
}

float Sub_Scorch_get_cooldown_fraction(void)
{
	return SubScorch_get_cooldown_fraction(&scorchCore, SubScorch_get_config());
}

/* Footprint management — called from mode_gameplay.c */

void Sub_Scorch_update_footprints(unsigned int ticks)
{
	SubScorch_pool_update(&footprintPool, SubScorch_get_config(), ticks);
}

int Sub_Scorch_check_footprint_burn(Rectangle target)
{
	return SubScorch_pool_check_burn(&footprintPool, SubScorch_get_config(), target);
}

void Sub_Scorch_render_footprints(void)
{
	SubScorch_pool_render(&footprintPool, SubScorch_get_config());
}

void Sub_Scorch_render_footprints_bloom(void)
{
	SubScorch_pool_render_bloom(&footprintPool, SubScorch_get_config());
}

void Sub_Scorch_render_footprints_light(void)
{
	SubScorch_pool_render_light(&footprintPool);
}
