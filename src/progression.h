#ifndef PROGRESSION_H
#define PROGRESSION_H

#include <stdbool.h>
#include "screen.h"

typedef enum {
	SUB_ID_PEA = 0,
	SUB_ID_MINE,
	/* SUB_ID_EGRESS, SUB_ID_PLASMA, etc. — add here as new enemies arrive */
	SUB_ID_COUNT
} SubroutineId;

void Progression_initialize(void);
void Progression_cleanup(void);
void Progression_update(unsigned int ticks);
void Progression_render(const Screen *screen);

bool Progression_is_unlocked(SubroutineId id);
int  Progression_get_current(SubroutineId id);
int  Progression_get_threshold(SubroutineId id);
float Progression_get_progress(SubroutineId id);
const char *Progression_get_name(SubroutineId id);

#endif
