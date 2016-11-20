#ifndef MINE_H
#define MINE_H

#include "position.h"
#include "entity.h"

void Mine_initialize(Position position);
void Mine_cleanup();

Collision Mine_collide(const void *entity, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Mine_resolve(const void *entity, const Collision collision);
void Mine_update(const void *entity, const PlaceableComponent *placeable, const unsigned int ticks);
void Mine_render(const void *entity, const PlaceableComponent *placeable);

#endif