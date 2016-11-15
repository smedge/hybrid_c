#ifndef MINE_H
#define MINE_H

#include "position.h"
#include "entity.h"
#include "color.h"

void Mine_initialize(Position position);
void Mine_cleanup();

Collision Mine_collide(const Rectangle boundingBox);
void Mine_resolve(Collision collision);
void Mine_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Mine_render(const PlaceableComponent *placeable);

#endif