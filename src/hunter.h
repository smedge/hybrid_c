#ifndef HUNTER_H
#define HUNTER_H

#include "position.h"
#include "entity.h"

void Hunter_initialize(Position position);
void Hunter_cleanup(void);
void Hunter_update(void *state, const PlaceableComponent *placeable, unsigned int ticks);
void Hunter_render(const void *state, const PlaceableComponent *placeable);
void Hunter_render_bloom_source(void);
Collision Hunter_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Hunter_resolve(void *state, const Collision collision);
void Hunter_deaggro_all(void);
void Hunter_reset_all(void);
bool Hunter_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index);
bool Hunter_find_aggro(Position from, double range, Position *out_pos);
void Hunter_heal(int index, double amount);

#endif
