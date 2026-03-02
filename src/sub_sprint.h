#ifndef SUB_SPRINT_H
#define SUB_SPRINT_H

#include <stdbool.h>
#include "input.h"

void Sub_Sprint_initialize(void);
void Sub_Sprint_cleanup(void);
void Sub_Sprint_update(const Input *input, unsigned int ticks);
bool Sub_Sprint_is_sprinting(void);
float Sub_Sprint_get_cooldown_fraction(void);

#endif
