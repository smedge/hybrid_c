#ifndef SUB_EMBER_H
#define SUB_EMBER_H

#include <stdbool.h>
#include "input.h"
#include "entity.h"

void Sub_Ember_initialize(Entity *parent);
void Sub_Ember_cleanup(void);

void Sub_Ember_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Sub_Ember_tick(unsigned int ticks);
void Sub_Ember_render(void);
void Sub_Ember_render_bloom_source(void);
void Sub_Ember_render_light_source(void);

bool Sub_Ember_check_hit(Rectangle target);
int Sub_Ember_check_hit_burn(Rectangle target);
int Sub_Ember_check_hit_burn_capped(Rectangle target, int max_per_volley);
bool Sub_Ember_check_nearby(Position pos, double radius);
void Sub_Ember_deactivate_all(void);
float Sub_Ember_get_cooldown_fraction(void);

#endif
