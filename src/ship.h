#ifndef SHIP_H
#define SHIP_H

#include "input.h"
#include "position.h"

void ship_update(const Input *input, const unsigned int ticks);
void ship_render(void);

#endif