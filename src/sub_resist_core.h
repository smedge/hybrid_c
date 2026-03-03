#ifndef SUB_RESIST_CORE_H
#define SUB_RESIST_CORE_H

#include <stdbool.h>
#include "position.h"

typedef struct {
	int duration_ms;
	int cooldown_ms;
	float ring_radius;
	float ring_thickness;
	float color_r, color_g, color_b;
	float pulse_speed;
	float bloom_radius;
	int bloom_segments;
	float bloom_alpha;
} SubResistConfig;

typedef struct {
	bool active;
	int activeTimer;
	int cooldownMs;
} SubResistCore;

void SubResist_init(SubResistCore *core);
bool SubResist_try_activate(SubResistCore *core, const SubResistConfig *cfg);
bool SubResist_update(SubResistCore *core, const SubResistConfig *cfg, unsigned int ticks);
bool SubResist_is_active(const SubResistCore *core);
float SubResist_get_cooldown_fraction(const SubResistCore *core, const SubResistConfig *cfg);

/* Single shared config — same skill, same tuning for player and enemies */
const SubResistConfig *SubResist_get_config(void);

void SubResist_render_ring(const SubResistCore *core, const SubResistConfig *cfg, Position pos);
void SubResist_render_bloom(const SubResistCore *core, const SubResistConfig *cfg, Position pos);

#endif
