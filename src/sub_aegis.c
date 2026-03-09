#include "sub_aegis.h"

#include "sub_shield_core.h"
#include "skillbar.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"

#define FEEDBACK_COST 30.0

static SubShieldCore core;
static const SubShieldConfig cfg = {
	.duration_ms = 10000,
	.cooldown_ms = 30000,
	.break_grace_ms = 0,
	.ring_radius = 35.0f,
	.ring_thickness = 2.0f,
	.ring_segments = 6,
	.color_r = 0.6f, .color_g = 0.9f, .color_b = 1.0f,
	.pulse_speed = 6.0f,
	.pulse_alpha_min = 0.7f,
	.pulse_alpha_range = 0.3f,
	.radius_pulse_amount = 0.05f,
	.bloom_thickness = 3.0f,
	.bloom_alpha_min = 0.5f,
	.bloom_alpha_range = 0.3f,
	.light_radius = 90.0f,
	.light_segments = 16,
};

void Sub_Aegis_initialize(void)
{
	SubShield_init(&core);
	SubShield_initialize_audio();
}

void Sub_Aegis_cleanup(void)
{
	if (SubShield_is_active(&core))
		PlayerStats_set_shielded(false);

	SubShield_cleanup_audio();
}

void Sub_Aegis_try_activate(void)
{
	if (SubShield_is_active(&core)) return;
	if (SubShield_try_activate(&core, &cfg, Ship_get_position())) {
		PlayerStats_set_shielded(true);
		PlayerStats_add_feedback(FEEDBACK_COST);
	}
}

void Sub_Aegis_update(const Input *input, unsigned int ticks)
{
	(void)input;
	if (Ship_is_destroyed()) return;

	if (!SubShield_is_active(&core)) {
		/* Activation handled by skillbar -> Sub_Aegis_try_activate() */
	} else {
		/* Check if shield was broken externally (e.g. by mine) */
		if (!PlayerStats_is_shielded()) {
			SubShield_break(&core, &cfg, Ship_get_position());
			return;
		}
		/* Tick the core — returns true if shield just expired */
		if (SubShield_update(&core, &cfg, ticks, Ship_get_position())) {
			PlayerStats_set_shielded(false);
			return;
		}
	}

	/* Tick cooldown when not active */
	if (!SubShield_is_active(&core))
		SubShield_update(&core, &cfg, ticks, Ship_get_position());
}

void Sub_Aegis_render(void)
{
	SubShield_render_ring(&core, &cfg, Ship_get_position());
}

void Sub_Aegis_render_bloom_source(void)
{
	SubShield_render_bloom(&core, &cfg, Ship_get_position());
}

void Sub_Aegis_render_light_source(void)
{
	SubShield_render_light(&core, &cfg, Ship_get_position());
}

float Sub_Aegis_get_cooldown_fraction(void)
{
	return SubShield_get_cooldown_fraction(&core, &cfg);
}

void Sub_Aegis_on_hit(void)
{
	SubShield_on_hit(&core, Ship_get_position());
}
