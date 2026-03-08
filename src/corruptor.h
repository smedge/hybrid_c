#ifndef CORRUPTOR_H
#define CORRUPTOR_H

#include "position.h"
#include "entity.h"
#include "sub_types.h"

void Corruptor_initialize(Position position, ZoneTheme theme);
void Corruptor_cleanup(void);
void Corruptor_update(void *state, const PlaceableComponent *placeable, unsigned int ticks);
void Corruptor_render(const void *state, const PlaceableComponent *placeable);
void Corruptor_render_bloom_source(void);
void Corruptor_render_light_source(void);
Collision Corruptor_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Corruptor_resolve(void *state, const Collision collision);
void Corruptor_deaggro_all(void);
void Corruptor_reset_all(void);
bool Corruptor_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index);
bool Corruptor_find_aggro(Position from, double range, Position *out_pos);
void Corruptor_heal(int index, double amount);
void Corruptor_alert_nearby(Position origin, double radius, Position threat);
void Corruptor_apply_emp(Position center, double half_size, unsigned int duration_ms);
void Corruptor_apply_heatwave(Position center, double half_size, double multiplier, unsigned int duration_ms);
void Corruptor_cleanse_burn(Position center, double radius, int immunity_ms);
void Corruptor_apply_burn(Position center, double radius, int duration_ms);
int Corruptor_get_count(void);

/* Returns true if a corruptor with active resist/temper aura covers this position */
bool Corruptor_is_resist_buffing(Position pos);

/* Scorch footprint public API */
void Corruptor_update_footprints(unsigned int ticks);
void Corruptor_check_footprint_burn_player(void);
void Corruptor_render_footprints(void);
void Corruptor_render_footprint_bloom_source(void);
void Corruptor_render_footprint_light_source(void);

#endif
