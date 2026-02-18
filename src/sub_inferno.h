#ifndef SUB_INFERNO_H
#define SUB_INFERNO_H

#include <stdbool.h>
#include "entity.h"
#include "collision.h"

void Sub_Inferno_initialize(Entity *p);
void Sub_Inferno_cleanup(void);
void Sub_Inferno_update(const Input *userInput, unsigned int ticks, PlaceableComponent *placeable);
void Sub_Inferno_render(void);
void Sub_Inferno_render_bloom_source(void);
void Sub_Inferno_render_light_source(void);
bool Sub_Inferno_check_hit(Rectangle target);
bool Sub_Inferno_check_nearby(Position pos, double radius);
void Sub_Inferno_deactivate_all(void);
float Sub_Inferno_get_cooldown_fraction(void);

#endif
