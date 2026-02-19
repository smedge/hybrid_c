#ifndef SUB_SHIELD_CORE_H
#define SUB_SHIELD_CORE_H

#include <stdbool.h>
#include "position.h"

typedef struct {
	bool active;
	int activeMs;
	int cooldownMs;
	float pulseTimer;
	int graceMs;
} SubShieldCore;

typedef struct {
	int duration_ms;
	int cooldown_ms;
	int break_grace_ms;
	float ring_radius;
	float ring_thickness;
	int ring_segments;
	float color_r, color_g, color_b;
	float pulse_speed;
	float pulse_alpha_min;
	float pulse_alpha_range;
	float radius_pulse_amount;
	float bloom_thickness;
	float bloom_alpha_min;
	float bloom_alpha_range;
	float light_radius;
	int light_segments;
} SubShieldConfig;

void SubShield_init(SubShieldCore *core);
bool SubShield_try_activate(SubShieldCore *core, const SubShieldConfig *cfg);
void SubShield_break(SubShieldCore *core, const SubShieldConfig *cfg);
bool SubShield_update(SubShieldCore *core, const SubShieldConfig *cfg, unsigned int ticks);
bool SubShield_is_active(const SubShieldCore *core);
bool SubShield_in_grace(const SubShieldCore *core);
float SubShield_get_cooldown_fraction(const SubShieldCore *core, const SubShieldConfig *cfg);

void SubShield_render_ring(const SubShieldCore *core, const SubShieldConfig *cfg, Position pos);
void SubShield_render_bloom(const SubShieldCore *core, const SubShieldConfig *cfg, Position pos);
void SubShield_render_light(const SubShieldCore *core, const SubShieldConfig *cfg, Position pos);

#endif
