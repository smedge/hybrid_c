#ifndef SHIP_H
#define SHIP_H

#define DEATH_TIMER 3000

#include "input.h"
#include "position.h"
#include "entity.h"

void Ship_initialize();
void Ship_cleanup();

Collision Ship_collide(const Entity *entity1, const Entity *entity2);
void Ship_resolve(const Entity *entity, const Collision collision);
void Ship_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Ship_render(const void *state, const PlaceableComponent *placeable);
Position Ship_get_position(void);

#endif
