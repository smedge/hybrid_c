#ifndef ENEMY_VARIANT_H
#define ENEMY_VARIANT_H

#include "sub_types.h"
#include "enemy_util.h"
#include "color.h"

#define VARIANT_MAX_CARRIED 4

typedef struct {
	/* Visual */
	ColorFloat tint;

	/* Carried subs (what this variant drops/wields) */
	CarriedSubroutine carried[VARIANT_MAX_CARRIED];
	int carried_count;

	/* AI tweaks (1.0 = base behavior) */
	float speed_mult;
	float aggro_range_mult;
	float hp_mult;
	float attack_cadence_mult;
} EnemyVariant;

/* Lookup helpers — each enemy module provides its own variant table.
   These inline helpers select the right carried subs based on theme. */

static inline const CarriedSubroutine *Variant_get_carried(
	const EnemyVariant *variants, ZoneTheme theme,
	const CarriedSubroutine *base_carried, int base_count,
	int *out_count)
{
	if (theme != THEME_NONE && variants && variants[theme].carried_count > 0) {
		*out_count = variants[theme].carried_count;
		return variants[theme].carried;
	}
	*out_count = base_count;
	return base_carried;
}

/* Get render color: themed enemies use variant tint, base enemies use their normal color.
   brightness: 0.0-1.0 modulates the tint for state variation (idle=0.7, aggro=1.0, etc.) */
static inline ColorFloat Variant_get_color(
	const EnemyVariant *variants, ZoneTheme theme,
	const ColorFloat *base_color, float brightness)
{
	if (theme != THEME_NONE && variants && variants[theme].tint.alpha > 0.0f) {
		ColorFloat c = variants[theme].tint;
		c.red *= brightness;
		c.green *= brightness;
		c.blue *= brightness;
		return c;
	}
	return *base_color;
}

#endif
