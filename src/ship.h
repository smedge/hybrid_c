#ifndef SHIP_H
#define SHIP_H

#include "input.h"
#include "position.h"
#include "render.h"

void Ship_initialize();
void Ship_collide();
void Ship_resolve();
void Ship_update(const Input *input, const unsigned int ticks);
void Ship_render(void);

#endif