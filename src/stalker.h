#ifndef STALKER_H
#define STALKER_H

#include "position.h"
#include "entity.h"
#include "sub_types.h"

void Stalker_initialize(Position position, ZoneTheme theme);
void Stalker_cleanup(void);
void Stalker_update(void *state, const PlaceableComponent *placeable, unsigned int ticks);
void Stalker_render(const void *state, const PlaceableComponent *placeable);
void Stalker_render_bloom_source(void);
void Stalker_render_light_source(void);
Collision Stalker_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Stalker_resolve(void *state, const Collision collision);
void Stalker_deaggro_all(void);
void Stalker_reset_all(void);
bool Stalker_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index);
bool Stalker_find_aggro(Position from, double range, Position *out_pos);
void Stalker_heal(int index, double amount);
void Stalker_alert_nearby(Position origin, double radius, Position threat);
void Stalker_apply_emp(Position center, double half_size, unsigned int duration_ms);
void Stalker_apply_heatwave(Position center, double half_size, double multiplier, unsigned int duration_ms);
void Stalker_cleanse_burn(Position center, double radius, int immunity_ms);
void Stalker_apply_burn(Position center, double radius, int duration_ms);
int Stalker_get_count(void);
void Stalker_update_projectiles(unsigned int ticks);

/* Fire stalker corridor (blaze dash trail) */
void Stalker_update_corridors(unsigned int ticks);
void Stalker_check_corridor_burn_player(void);
void Stalker_render_corridors(void);
void Stalker_render_corridor_bloom_source(void);
void Stalker_render_corridor_light_source(void);

#endif
