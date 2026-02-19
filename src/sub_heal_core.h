#ifndef SUB_HEAL_CORE_H
#define SUB_HEAL_CORE_H

#include <stdbool.h>
#include "position.h"

typedef struct {
	int cooldownMs;
	bool beamActive;
	int beamTimerMs;
	Position beamOrigin;
	Position beamTarget;
} SubHealCore;

typedef struct {
	double heal_amount;
	int cooldown_ms;
	int beam_duration_ms;
	float beam_thickness;
	float beam_color_r, beam_color_g, beam_color_b;
	float bloom_beam_thickness;
} SubHealConfig;

void SubHeal_init(SubHealCore *core);
bool SubHeal_try_activate(SubHealCore *core, const SubHealConfig *cfg, Position origin, Position target);
void SubHeal_update(SubHealCore *core, const SubHealConfig *cfg, unsigned int ticks);
bool SubHeal_is_ready(const SubHealCore *core);
float SubHeal_get_cooldown_fraction(const SubHealCore *core, const SubHealConfig *cfg);
void SubHeal_render_beam(const SubHealCore *core, const SubHealConfig *cfg);
void SubHeal_render_beam_bloom(const SubHealCore *core, const SubHealConfig *cfg);

#endif
