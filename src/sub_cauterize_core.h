#ifndef SUB_CAUTERIZE_CORE_H
#define SUB_CAUTERIZE_CORE_H

#include <stdbool.h>
#include "sub_heal_core.h"
#include "burn.h"
#include "collision.h"

typedef struct {
	double heal_amount;
	int cooldown_ms;
	int immunity_duration_ms;
	double cleanse_radius;
	double aura_radius;
	int aura_duration_ms;
	int aura_burn_interval_ms;
	/* Heal beam visuals (0 = no beam for player self-heal) */
	int beam_duration_ms;
	float beam_thickness;
	float beam_color_r, beam_color_g, beam_color_b;
	float bloom_beam_thickness;
} SubCauterizeConfig;

typedef struct {
	bool active;
	Position position;
	int life_ms;
	int burn_tick_ms;
	BurnState burn;  /* for visual registration with burn system */
} CauterizeAura;

#define CAUTERIZE_AURA_MAX 4

typedef struct {
	SubHealCore heal;
	CauterizeAura auras[CAUTERIZE_AURA_MAX];
} SubCauterizeCore;

const SubCauterizeConfig *SubCauterize_get_config(void);
const SubCauterizeConfig *SubCauterize_get_defender_config(void);

void SubCauterize_init(SubCauterizeCore *core);
bool SubCauterize_is_ready(const SubCauterizeCore *core);
float SubCauterize_get_cooldown_fraction(const SubCauterizeCore *core);
bool SubCauterize_try_activate(SubCauterizeCore *core, const SubCauterizeConfig *cfg,
	Position origin, Position target);
void SubCauterize_update(SubCauterizeCore *core, const SubCauterizeConfig *cfg, unsigned int ticks);
int SubCauterize_check_aura_burn(SubCauterizeCore *core, Rectangle target);
void SubCauterize_deactivate_all(SubCauterizeCore *core);

/* Rendering */
void SubCauterize_render_aura(const SubCauterizeCore *core);
void SubCauterize_render_beam(const SubCauterizeCore *core, const SubCauterizeConfig *cfg);
void SubCauterize_render_aura_bloom(const SubCauterizeCore *core);
void SubCauterize_render_beam_bloom(const SubCauterizeCore *core, const SubCauterizeConfig *cfg);
void SubCauterize_render_aura_light(const SubCauterizeCore *core);

/* Audio lifecycle */
void SubCauterize_initialize_audio(void);
void SubCauterize_cleanup_audio(void);

#endif
