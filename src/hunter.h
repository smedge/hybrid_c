#ifndef HUNTER_H
#define HUNTER_H

#include "position.h"
#include "entity.h"

void Hunter_initialize(Position position);
void Hunter_cleanup(void);
void Hunter_update(const void *state, const PlaceableComponent *placeable, unsigned int ticks);
void Hunter_render(const void *state, const PlaceableComponent *placeable);
void Hunter_render_bloom_source(void);
Collision Hunter_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Hunter_resolve(const void *state, const Collision collision);
void Hunter_deaggro_all(void);

#endif
