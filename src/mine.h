#ifndef MINE_H
#define MINE_H

#include "position.h"
#include "entity.h"

void Mine_initialize(Position position);
void Mine_cleanup();

Collision Mine_collide(const Entity *entity1, const Entity *entity2);
void Mine_resolve(Entity *entity, const Collision collision);
void Mine_update(const void *state, const PlaceableComponent *placeable, const unsigned int ticks);
void Mine_render(const void *state, const PlaceableComponent *placeable);

#endif
