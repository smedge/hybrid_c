#ifndef SKILLBAR_H
#define SKILLBAR_H

#include <stdbool.h>
#include "input.h"
#include "screen.h"
#include "progression.h"

#define SKILLBAR_SLOTS 10
#define SUB_NONE -1

typedef enum {
	SUB_TYPE_PROJECTILE,
	SUB_TYPE_DEPLOYABLE,
	SUB_TYPE_MOVEMENT,
	SUB_TYPE_SHIELD,
	SUB_TYPE_HEALING,
	SUB_TYPE_COUNT
} SubroutineType;

void Skillbar_initialize(void);
void Skillbar_cleanup(void);
void Skillbar_update(const Input *input, const unsigned int ticks);
void Skillbar_render(const Screen *screen);

bool Skillbar_is_active(SubroutineId id);
bool Skillbar_consumed_click(void);
void Skillbar_auto_equip(SubroutineId id);
void Skillbar_equip(int slot, SubroutineId id);
void Skillbar_clear_slot(int slot);

const char *Skillbar_get_sub_name(SubroutineId id);
const char *Skillbar_get_sub_description(SubroutineId id);
SubroutineType Skillbar_get_sub_type(SubroutineId id);
int Skillbar_get_slot_sub(int slot);
int Skillbar_find_equipped_slot(SubroutineId id);
void Skillbar_swap_slots(int a, int b);
int Skillbar_slot_at_position(float x, float y, const Screen *screen);
void Skillbar_render_icon_at(SubroutineId id, float cx, float cy, float alpha);
bool Skillbar_is_elite(SubroutineId id);

#endif
