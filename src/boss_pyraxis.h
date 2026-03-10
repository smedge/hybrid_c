#ifndef BOSS_PYRAXIS_H
#define BOSS_PYRAXIS_H

#include "position.h"
#include "entity.h"
#include "sub_types.h"

void BossPyraxis_initialize(Position position);
void BossPyraxis_cleanup(void);
void BossPyraxis_update(void *state, const PlaceableComponent *placeable, unsigned int ticks);
void BossPyraxis_render(const void *state, const PlaceableComponent *placeable);
Collision BossPyraxis_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void BossPyraxis_resolve(void *state, const Collision collision);

/* Query for intro speech — movement is locked via DataNode reading overlay */
bool BossPyraxis_is_active(void);

#endif
