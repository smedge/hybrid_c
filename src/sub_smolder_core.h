#ifndef SUB_SMOLDER_CORE_H
#define SUB_SMOLDER_CORE_H

#include <stdbool.h>
#include "position.h"

/*
 * Sub Smolder Core — Heat shimmer stealth (fire variant of stealth)
 *
 * Time-limited cloak with heat shimmer visual (semi-transparent, not invisible).
 * Ambush attack ignites nearby targets with burn stacks.
 * Used by both player (sub_smolder.c) and Fire Stalker enemy.
 */

typedef enum {
	SMOLDER_READY,
	SMOLDER_ACTIVE,
	SMOLDER_COOLDOWN
} SmolderState;

typedef struct {
	int cloak_duration_ms;      /* how long cloak lasts (not toggle — timed) */
	int cooldown_ms;            /* cooldown after cloak ends */
	int ambush_duration_ms;     /* damage bonus window after attack break */
	double ambush_multiplier;   /* damage multiplier during ambush */
	double ambush_burn_radius;  /* radius for burn ignition on ambush break */
	int ambush_burn_stacks;     /* burn stacks applied in radius on ambush */
	float alpha_min;            /* base transparency when cloaked */
	float alpha_pulse_speed;    /* shimmer pulse rate */
} SubSmolderConfig;

typedef struct {
	SmolderState state;
	int duration_ms;            /* remaining cloak time */
	int cooldown_ms;            /* remaining cooldown */
	int ambush_ms;              /* remaining ambush window */
	float pulse_timer;          /* shimmer animation */
} SubSmolderCore;

const SubSmolderConfig *SubSmolder_get_config(void);

void SubSmolder_init(SubSmolderCore *core);
bool SubSmolder_try_activate(SubSmolderCore *core, const SubSmolderConfig *cfg, Position pos);
void SubSmolder_activate_silent(SubSmolderCore *core, const SubSmolderConfig *cfg);
void SubSmolder_break(SubSmolderCore *core, Position pos);
void SubSmolder_break_attack(SubSmolderCore *core, Position pos);
void SubSmolder_update(SubSmolderCore *core, const SubSmolderConfig *cfg, unsigned int ticks, Position pos);
void SubSmolder_reset(SubSmolderCore *core);

bool SubSmolder_is_active(const SubSmolderCore *core);
bool SubSmolder_is_ambush_active(const SubSmolderCore *core);
double SubSmolder_get_damage_multiplier(const SubSmolderCore *core, const SubSmolderConfig *cfg);
void SubSmolder_notify_kill(SubSmolderCore *core);
float SubSmolder_get_alpha(const SubSmolderCore *core, const SubSmolderConfig *cfg);
float SubSmolder_get_cooldown_fraction(const SubSmolderCore *core, const SubSmolderConfig *cfg);

/* Audio lifecycle — call from both player wrapper and enemy module */
void SubSmolder_initialize_audio(void);
void SubSmolder_cleanup_audio(void);

/* Render — heat shimmer particles around cloaked entity */
void SubSmolder_render_shimmer(const SubSmolderCore *core, const SubSmolderConfig *cfg, float x, float y);
void SubSmolder_render_shimmer_bloom(const SubSmolderCore *core, const SubSmolderConfig *cfg, float x, float y);

#endif
