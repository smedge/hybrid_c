#ifndef SUB_TGUN_H
#define SUB_TGUN_H

#include <stdbool.h>
#include "input.h"
#include "entity.h"

void Sub_Tgun_initialize(Entity *parent);
void Sub_Tgun_cleanup();

void Sub_Tgun_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Sub_Tgun_render();
void Sub_Tgun_render_bloom_source(void);
void Sub_Tgun_render_light_source(void);

double Sub_Tgun_check_hit(Rectangle target);
bool Sub_Tgun_check_nearby(Position pos, double radius);
void Sub_Tgun_deactivate_all(void);
float Sub_Tgun_get_cooldown_fraction(void);

#endif
