#ifndef SHIP_H
#define SHIP_H

#include "input.h"
#include "position.h"
#include "entity.h"

void Ship_initialize();
void Ship_cleanup();

void Ship_collide();
void Ship_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Ship_render(const PlaceableComponent *placeable);

#endif