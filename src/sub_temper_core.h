#ifndef SUB_TEMPER_CORE_H
#define SUB_TEMPER_CORE_H

#include <stdbool.h>
#include "position.h"
#include "sub_resist_core.h"

/*
 * Sub Temper Core — Fire damage reflection aura (fire variant of resist)
 *
 * Instead of 50% damage reduction, reflects 50% of fire and burn damage
 * back to the attacker. Forces player to switch off fire loadouts or
 * focus the corruptor first.
 * Used by both player (sub_temper.c) and Fire Corruptor enemy.
 */

typedef struct {
	int duration_ms;
	int cooldown_ms;
	double reflect_percent;     /* 0.0–1.0 (0.5 = 50% reflection) */
	/* Ring visuals — deep orange-red */
	float ring_radius;
	float ring_thickness;
	float color_r, color_g, color_b;
	float pulse_speed;
	float bloom_radius;
	int bloom_segments;
	float bloom_alpha;
	float beam_thickness;
	float bloom_beam_thickness;
} SubTemperConfig;

typedef struct {
	SubResistCore resist;       /* embedded resist for timer logic */
} SubTemperCore;

const SubTemperConfig *SubTemper_get_config(void);

void SubTemper_init(SubTemperCore *core);
bool SubTemper_try_activate(SubTemperCore *core, const SubTemperConfig *cfg);
bool SubTemper_update(SubTemperCore *core, const SubTemperConfig *cfg, unsigned int ticks);
bool SubTemper_is_active(const SubTemperCore *core);
float SubTemper_get_cooldown_fraction(const SubTemperCore *core, const SubTemperConfig *cfg);

/* Returns reflected damage amount. Pass is_fire=true for fire/burn damage.
   Returns 0.0 if temper not active or damage is not fire-typed. */
double SubTemper_check_reflect(const SubTemperCore *core, const SubTemperConfig *cfg,
	double damage, bool is_fire);

/* Rendering — same ring/beam pattern as resist, different colors */
void SubTemper_render_ring(const SubTemperCore *core, const SubTemperConfig *cfg, Position pos);
void SubTemper_render_bloom(const SubTemperCore *core, const SubTemperConfig *cfg, Position pos);
void SubTemper_render_beam(const SubTemperCore *core, const SubTemperConfig *cfg, Position origin, Position target);
void SubTemper_render_beam_bloom(const SubTemperCore *core, const SubTemperConfig *cfg, Position origin, Position target);

#endif
