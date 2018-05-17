#ifndef SUB_PEA_H
#define SUB_PEA_H

#include "input.h"
#include "entity.h"

void Sub_Pea_initialize(Entity *parent);
void Sub_Pea_cleanup();

void Sub_Pea_collide(const void* state, const PlaceableComponent *placeable, const Rectangle boundingBox,
	void (*resolve)(const void *state, const Collision collision), void *stateOther);
void Sub_Pea_resolve(const void *state, const Collision collision);
void Sub_Pea_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Sub_Pea_render();

#endif
