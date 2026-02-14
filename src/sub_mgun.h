#ifndef SUB_MGUN_H
#define SUB_MGUN_H

#include <stdbool.h>
#include "input.h"
#include "entity.h"

void Sub_Mgun_initialize(Entity *parent);
void Sub_Mgun_cleanup();

void Sub_Mgun_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Sub_Mgun_render();
void Sub_Mgun_render_bloom_source(void);

bool Sub_Mgun_check_hit(Rectangle target);
void Sub_Mgun_deactivate_all(void);
float Sub_Mgun_get_cooldown_fraction(void);

#endif
