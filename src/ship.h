#ifndef SHIP_H
#define SHIP_H

#include "input.h"
#include "position.h"
#include "entity.h"

void Ship_initialize();
void Ship_cleanup();

bool Ship_collide(const Rectangle boundingBox);
void Ship_resolve(void);
void Ship_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Ship_render(const PlaceableComponent *placeable);
Position Ship_get_position(void);

#endif