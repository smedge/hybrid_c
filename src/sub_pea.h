#ifndef SUB_PEA_H
#define SUB_PEA_H

#include <stdbool.h>
#include "input.h"
#include "entity.h"
#include "sub_projectile_core.h"

const SubProjectileConfig *Sub_Pea_get_config(void);

void Sub_Pea_initialize(Entity *parent);
void Sub_Pea_cleanup();

void Sub_Pea_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Sub_Pea_render();
void Sub_Pea_render_bloom_source(void);
void Sub_Pea_render_light_source(void);

double Sub_Pea_check_hit(Rectangle target);
bool Sub_Pea_check_nearby(Position pos, double radius);
void Sub_Pea_deactivate_all(void);
float Sub_Pea_get_cooldown_fraction(void);

#endif
