#ifndef SHIP_H
#define SHIP_H

#define DEATH_TIMER 3000
#define SHIP_BB_HALF_SIZE 20.0

#include "input.h"
#include "position.h"
#include "entity.h"

void Ship_initialize();
void Ship_cleanup();

Collision Ship_collide(void* state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Ship_resolve(void *state, const Collision collision);
void Ship_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Ship_render(const void *state, const PlaceableComponent *placeable);
void Ship_render_bloom_source(void);
Position Ship_get_position(void);
double Ship_get_heading(void);
void Ship_force_spawn(Position pos);
void Ship_set_position(Position pos);
void Ship_set_god_mode(bool enabled);
bool Ship_is_destroyed(void);
void Ship_set_pending_cross_zone_respawn(bool pending);
bool Ship_has_pending_cross_zone_respawn(void);

#endif
