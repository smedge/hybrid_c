#ifndef SUB_PEA_H
#define SUB_PEA_H

#include <stdbool.h>
#include "input.h"
#include "entity.h"

void Sub_Pea_initialize(Entity *parent);
void Sub_Pea_cleanup();

void Sub_Pea_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Sub_Pea_render();

bool Sub_Pea_check_hit(Rectangle target);
void Sub_Pea_deactivate_all(void);

#endif
