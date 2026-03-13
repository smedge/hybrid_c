#ifndef PROGRESSION_H
#define PROGRESSION_H

#include <stdbool.h>
#include "screen.h"
#include "fragment.h"
#include "color.h"

typedef enum {
	SUB_ID_PEA = 0,
	SUB_ID_MINE,
	SUB_ID_BOOST,
	SUB_ID_EGRESS,
	SUB_ID_MGUN,
	SUB_ID_MEND,
	SUB_ID_AEGIS,
	SUB_ID_STEALTH,
	SUB_ID_INFERNO,
	SUB_ID_DISINTEGRATE,
	SUB_ID_GRAVWELL,
	SUB_ID_TGUN,
	SUB_ID_SPRINT,
	SUB_ID_EMP,
	SUB_ID_RESIST,
	/* Fire zone (The Crucible) */
	SUB_ID_EMBER,
	SUB_ID_FLAK,
	SUB_ID_BLAZE,
	SUB_ID_SCORCH,
	SUB_ID_CINDER,
	SUB_ID_CAUTERIZE,
	SUB_ID_IMMOLATE,
	SUB_ID_SMOLDER,
	SUB_ID_HEATWAVE,
	SUB_ID_TEMPER,
	SUB_ID_COUNT
} SubroutineId;

void Progression_initialize(void);
void Progression_cleanup(void);
void Progression_update(unsigned int ticks);
void Progression_render(const Screen *screen);

void Progression_unlock_all(void);
bool Progression_is_unlocked(SubroutineId id);
bool Progression_is_discovered(SubroutineId id);
void Progression_restore(const bool *unlocked, const bool *discovered);
int  Progression_get_current(SubroutineId id);
int  Progression_get_threshold(SubroutineId id);
float Progression_get_progress(SubroutineId id);
const char *Progression_get_name(SubroutineId id);
const char *Progression_get_enemy_name(SubroutineId id);
ColorFloat Progression_get_enemy_color(SubroutineId id);
bool Progression_would_complete(FragmentType frag_type, int new_count);
FragmentType Progression_get_frag_type(SubroutineId id);

#endif
