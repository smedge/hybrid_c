#ifndef DEFENDER_H
#define DEFENDER_H

#include "position.h"
#include "entity.h"
#include "sub_types.h"

void Defender_initialize(Position position, ZoneTheme theme);
void Defender_cleanup(void);
void Defender_update(void *state, const PlaceableComponent *placeable, unsigned int ticks);
void Defender_render(const void *state, const PlaceableComponent *placeable);
Collision Defender_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Defender_resolve(void *state, const Collision collision);
void Defender_deaggro_all(void);
void Defender_reset_all(void);
bool Defender_is_protecting(Position pos, bool ambush);
void Defender_notify_shield_hit(Position pos);
bool Defender_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index);
bool Defender_find_aggro(Position from, double range, Position *out_pos);
void Defender_heal(int index, double amount);
void Defender_alert_nearby(Position origin, double radius, Position threat);
void Defender_apply_emp(Position center, double half_size, unsigned int duration_ms);
void Defender_apply_heatwave(Position center, double half_size, double multiplier, unsigned int duration_ms);
void Defender_cleanse_burn(Position center, double radius, int immunity_ms);
void Defender_apply_burn(Position center, double radius, int duration_ms);
int Defender_get_count(void);

/* Fire defender aura lifecycle (called from mode_gameplay) */
void Defender_update_fire_auras(unsigned int ticks);
void Defender_render_fire_auras(void);
void Defender_render_fire_aura_bloom(void);
void Defender_render_fire_aura_light(void);

#endif
