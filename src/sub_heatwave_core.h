#ifndef SUB_HEATWAVE_CORE_H
#define SUB_HEATWAVE_CORE_H

#include <stdbool.h>
#include "position.h"

/*
 * Sub Heatwave Core — Expanding heat ring (fire variant of EMP)
 *
 * Instead of disabling feedback decay, applies 3x feedback accumulation
 * for 10s. Anything caught in the ring overheats — skill spam becomes
 * extremely dangerous.
 * Used by both player (sub_heatwave.c) and Fire Corruptor enemy.
 */

typedef struct {
	int cooldown_ms;
	int visual_ms;          /* ring expansion duration */
	float half_size;        /* AABB half-size for hit detection */
	float ring_max_radius;
	double feedback_multiplier;
	int debuff_duration_ms;
	/* Ring visuals */
	float inner_ring_ratio;
	int segments;
	float outer_r, outer_g, outer_b;
	float outer_thickness;
	float inner_r, inner_g, inner_b;
	float inner_thickness;
	float inner_alpha_mult;
} SubHeatwaveConfig;

typedef struct {
	int cooldownMs;
	bool visualActive;
	int visualTimer;
	Position visualCenter;
} SubHeatwaveCore;

const SubHeatwaveConfig *SubHeatwave_get_config(void);

void SubHeatwave_init(SubHeatwaveCore *core);
bool SubHeatwave_try_activate(SubHeatwaveCore *core, const SubHeatwaveConfig *cfg, Position center);
void SubHeatwave_update(SubHeatwaveCore *core, const SubHeatwaveConfig *cfg, unsigned int ticks);
bool SubHeatwave_is_active(const SubHeatwaveCore *core);
float SubHeatwave_get_cooldown_fraction(const SubHeatwaveCore *core, const SubHeatwaveConfig *cfg);

void SubHeatwave_initialize_audio(void);
void SubHeatwave_cleanup_audio(void);

void SubHeatwave_render_ring(const SubHeatwaveCore *core, const SubHeatwaveConfig *cfg);
void SubHeatwave_render_bloom(const SubHeatwaveCore *core, const SubHeatwaveConfig *cfg);
void SubHeatwave_render_light(const SubHeatwaveCore *core, const SubHeatwaveConfig *cfg);

#endif
