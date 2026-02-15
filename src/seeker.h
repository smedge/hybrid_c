#ifndef SEEKER_H
#define SEEKER_H

#include "position.h"
#include "entity.h"

void Seeker_initialize(Position position);
void Seeker_cleanup(void);
void Seeker_update(const void *state, const PlaceableComponent *placeable, unsigned int ticks);
void Seeker_render(const void *state, const PlaceableComponent *placeable);
void Seeker_render_bloom_source(void);
Collision Seeker_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Seeker_resolve(const void *state, const Collision collision);
void Seeker_deaggro_all(void);
void Seeker_reset_all(void);

#endif
