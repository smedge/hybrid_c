#ifndef SUB_IMMOLATE_CORE_H
#define SUB_IMMOLATE_CORE_H

#include <stdbool.h>
#include "sub_shield_core.h"
#include "collision.h"

typedef struct {
	SubShieldConfig shield;
	double aura_radius;
	int aura_burn_interval_ms;
} SubImmolateConfig;

typedef struct {
	SubShieldCore shield;
	int aura_burn_timer;
} SubImmolateCore;

const SubImmolateConfig *SubImmolate_get_config(void);

void SubImmolate_init(SubImmolateCore *core);
bool SubImmolate_try_activate(SubImmolateCore *core, Position pos);
void SubImmolate_break(SubImmolateCore *core, Position pos);
bool SubImmolate_update(SubImmolateCore *core, unsigned int ticks, Position pos);
bool SubImmolate_is_active(const SubImmolateCore *core);
bool SubImmolate_in_grace(const SubImmolateCore *core);
float SubImmolate_get_cooldown_fraction(const SubImmolateCore *core);
int SubImmolate_check_burn(SubImmolateCore *core, Position shield_pos, Rectangle target);
void SubImmolate_on_hit(SubImmolateCore *core, Position pos);

/* Rendering */
void SubImmolate_render_ring(const SubImmolateCore *core, Position pos);
void SubImmolate_render_ring_small(const SubImmolateCore *core, Position pos);
void SubImmolate_render_aura(const SubImmolateCore *core, Position pos);
void SubImmolate_render_bloom(const SubImmolateCore *core, Position pos);
void SubImmolate_render_bloom_small(const SubImmolateCore *core, Position pos);
void SubImmolate_render_light(const SubImmolateCore *core, Position pos);

/* Audio lifecycle */
void SubImmolate_initialize_audio(void);
void SubImmolate_cleanup_audio(void);

#endif
