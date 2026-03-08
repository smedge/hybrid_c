#ifndef MINE_H
#define MINE_H

#include "position.h"
#include "entity.h"
#include "sub_types.h"

void Mine_initialize(Position position, ZoneTheme theme);
void Mine_cleanup();

Collision Mine_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Mine_resolve(void *state, const Collision collision);
void Mine_update(void *state, const PlaceableComponent *placeable, const unsigned int ticks);
void Mine_render(const void *state, const PlaceableComponent *placeable);
void Mine_reset_all(void);
int Mine_get_count(void);

/* Fire mine pools */
void Mine_update_fire_pools(unsigned int ticks);
void Mine_render_fire_pools(void);
void Mine_render_fire_pool_bloom(void);
void Mine_render_fire_pool_light(void);

#endif
