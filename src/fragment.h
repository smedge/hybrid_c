#ifndef FRAGMENT_H
#define FRAGMENT_H

#include "position.h"
#include "screen.h"

typedef enum {
	FRAG_TYPE_MINE = 0,
	FRAG_TYPE_ELITE,
	FRAG_TYPE_HUNTER,
	FRAG_TYPE_SEEKER,
	FRAG_TYPE_MEND,
	FRAG_TYPE_AEGIS,
	FRAG_TYPE_COUNT
} FragmentType;

void Fragment_initialize(void);
void Fragment_cleanup(void);
void Fragment_spawn(Position position, FragmentType type);
void Fragment_update(unsigned int ticks);
void Fragment_render(void);
void Fragment_render_text(const Screen *screen);
void Fragment_render_bloom_source(void);
void Fragment_deactivate_all(void);
int  Fragment_get_count(FragmentType type);
void Fragment_set_count(FragmentType type, int count);

#endif
