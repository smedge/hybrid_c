#ifndef SUB_DISINTEGRATE_H
#define SUB_DISINTEGRATE_H

#include <stdbool.h>
#include "entity.h"
#include "collision.h"

void Sub_Disintegrate_initialize(Entity *p);
void Sub_Disintegrate_cleanup(void);
void Sub_Disintegrate_update(const Input *userInput, unsigned int ticks, PlaceableComponent *placeable);
void Sub_Disintegrate_render(void);
void Sub_Disintegrate_render_bloom_source(void);
void Sub_Disintegrate_render_light_source(void);
bool Sub_Disintegrate_check_hit(Rectangle target);
bool Sub_Disintegrate_check_nearby(Position pos, double radius);
void Sub_Disintegrate_deactivate_all(void);
float Sub_Disintegrate_get_cooldown_fraction(void);

#endif
