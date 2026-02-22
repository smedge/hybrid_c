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
bool Defender_is_protecting(Position pos, bool ambush);
void Defender_notify_shield_hit(Position pos);
bool Defender_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index);
bool Defender_find_aggro(Position from, double range, Position *out_pos);
void Defender_heal(int index, double amount);
int Defender_get_count(void);

#endif
