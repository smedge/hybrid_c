#ifndef VIEW_H
#define VIEW_H

#include "input.h"
#include "position.h"

void view_update(const Input *input, const unsigned int ticks);
void view_render();

#endif