#ifndef SUB_RESIST_H
#define SUB_RESIST_H

#include <stdbool.h>
#include "input.h"

void Sub_Resist_initialize(void);
void Sub_Resist_cleanup(void);
void Sub_Resist_try_activate(void);
void Sub_Resist_update(const Input *input, unsigned int ticks);
void Sub_Resist_render(void);
void Sub_Resist_render_bloom_source(void);
void Sub_Resist_render_light_source(void);
float Sub_Resist_get_cooldown_fraction(void);
bool Sub_Resist_is_active(void);

#endif
