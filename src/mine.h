#ifndef MINE_H
#define MINE_H

#include "position.h"
#include "entity.h"

void Mine_initialize(Position position);
void Mine_cleanup();

Collision Mine_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Mine_resolve(void *state, const Collision collision);
void Mine_update(void *state, const PlaceableComponent *placeable, const unsigned int ticks);
void Mine_render(const void *state, const PlaceableComponent *placeable);
void Mine_render_bloom_source(void);
void Mine_render_light_source(void);
void Mine_reset_all(void);
int Mine_get_count(void);

#endif
