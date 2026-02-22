#ifndef SEEKER_H
#define SEEKER_H

#include "position.h"
#include "entity.h"

void Seeker_initialize(Position position);
void Seeker_cleanup(void);
void Seeker_update(void *state, const PlaceableComponent *placeable, unsigned int ticks);
void Seeker_render(const void *state, const PlaceableComponent *placeable);
void Seeker_render_bloom_source(void);
void Seeker_render_light_source(void);
Collision Seeker_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Seeker_resolve(void *state, const Collision collision);
void Seeker_deaggro_all(void);
void Seeker_reset_all(void);
bool Seeker_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index);
bool Seeker_find_aggro(Position from, double range, Position *out_pos);
void Seeker_heal(int index, double amount);
int Seeker_get_count(void);

#endif
