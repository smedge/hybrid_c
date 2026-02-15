#ifndef DEFENDER_H
#define DEFENDER_H

#include "position.h"
#include "entity.h"

void Defender_initialize(Position position);
void Defender_cleanup(void);
void Defender_update(void *state, const PlaceableComponent *placeable, unsigned int ticks);
void Defender_render(const void *state, const PlaceableComponent *placeable);
void Defender_render_bloom_source(void);
Collision Defender_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Defender_resolve(void *state, const Collision collision);
void Defender_deaggro_all(void);
void Defender_reset_all(void);
bool Defender_is_protecting(Position pos);

#endif
