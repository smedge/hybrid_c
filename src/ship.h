#ifndef SHIP_H
#define SHIP_H

#define STATE_NORMAL 0
#define STATE_DESTROYED 1

#include "input.h"
#include "position.h"
#include "entity.h"

void Ship_initialize();
void Ship_cleanup();

Collision Ship_collide(const void* entity, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Ship_resolve(const void *entity, const Collision collision);
void Ship_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Ship_render(const void *entity, const PlaceableComponent *placeable);
Position Ship_get_position(void);

#endif