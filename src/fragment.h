#ifndef FRAGMENT_H
#define FRAGMENT_H

#include "position.h"
#include "screen.h"
#include "sub_types.h"
#include "color.h"
#include <stdbool.h>

typedef enum {
	FRAG_TYPE_MINE = 0,
	FRAG_TYPE_BOOST,
	FRAG_TYPE_MGUN,
	FRAG_TYPE_EGRESS,
	FRAG_TYPE_MEND,
	FRAG_TYPE_AEGIS,
	FRAG_TYPE_GRAVWELL,
	FRAG_TYPE_STEALTH,
	FRAG_TYPE_INFERNO,
	FRAG_TYPE_DISINTEGRATE,
	FRAG_TYPE_TGUN,
	FRAG_TYPE_SPRINT,
	FRAG_TYPE_EMP,
	FRAG_TYPE_RESIST,
	/* Fire zone (The Crucible) */
	FRAG_TYPE_EMBER,
	FRAG_TYPE_FLAK,
	FRAG_TYPE_BLAZE,
	FRAG_TYPE_SCORCH,
	FRAG_TYPE_CINDER,
	FRAG_TYPE_CAUTERIZE,
	FRAG_TYPE_IMMOLATE,
	FRAG_TYPE_SMOLDER,
	FRAG_TYPE_HEATWAVE,
	FRAG_TYPE_TEMPER,
	FRAG_TYPE_COUNT
} FragmentType;

void Fragment_initialize(void);
void Fragment_cleanup(void);
void Fragment_spawn(Position position, FragmentType type, SubroutineTier tier);
void Fragment_spawn_persistent(Position position, FragmentType type, SubroutineTier tier);
void Fragment_update(unsigned int ticks);
void Fragment_render(void);
void Fragment_render_text(const Screen *screen);
void Fragment_render_bloom_source(void);
void Fragment_deactivate_all(void);
int  Fragment_get_count(FragmentType type);
void Fragment_set_count(FragmentType type, int count);
bool Fragment_any_persistent_active(void);

/* Source enemy info — maps fragment types back to the enemy that drops them */
const char *Fragment_get_source_enemy_name(FragmentType type);
ColorFloat Fragment_get_source_enemy_color(FragmentType type);

#endif
