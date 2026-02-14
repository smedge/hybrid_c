#ifndef SHIP_H
#define SHIP_H

#define DEATH_TIMER 3000

#include "input.h"
#include "position.h"
#include "entity.h"

void Ship_initialize();
void Ship_cleanup();

Collision Ship_collide(const void* state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Ship_resolve(const void *state, const Collision collision);
void Ship_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Ship_render(const void *state, const PlaceableComponent *placeable);
void Ship_render_bloom_source(void);
Position Ship_get_position(void);
double Ship_get_heading(void);
void Ship_force_spawn(Position pos);
void Ship_set_position(Position pos);
void Ship_set_god_mode(bool enabled);
bool Ship_is_destroyed(void);

#endif
