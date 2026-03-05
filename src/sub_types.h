#ifndef SUB_TYPES_H
#define SUB_TYPES_H

#include "color.h"

typedef enum {
	SUB_TYPE_PROJECTILE,
	SUB_TYPE_DEPLOYABLE,
	SUB_TYPE_MOVEMENT,
	SUB_TYPE_SHIELD,
	SUB_TYPE_HEALING,
	SUB_TYPE_STEALTH,
	SUB_TYPE_CONTROL,
	SUB_TYPE_AREA_EFFECT,
	SUB_TYPE_COUNT
} SubroutineType;

typedef enum {
	TIER_NORMAL,
	TIER_RARE,
	TIER_ELITE
} SubroutineTier;

static const ColorFloat TIER_COLORS[] = {
	[TIER_NORMAL] = {1.0f, 1.0f, 1.0f, 1.0f},
	[TIER_RARE]   = {0.1f, 0.1f, 1.0f, 1.0f},
	[TIER_ELITE]  = {1.0f, 0.84f, 0.0f, 1.0f},
};

typedef enum {
	THEME_NONE = 0,
	THEME_FIRE,
	THEME_ICE,
	THEME_POISON,
	THEME_BLOOD,
	THEME_RADIANCE,
	THEME_VOID,
	THEME_COUNT
} ZoneTheme;

#endif
