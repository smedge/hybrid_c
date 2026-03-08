#ifndef SUB_TEMPER_H
#define SUB_TEMPER_H

#include <stdbool.h>
#include "input.h"

void Sub_Temper_initialize(void);
void Sub_Temper_cleanup(void);
void Sub_Temper_try_activate(void);
void Sub_Temper_update(const Input *input, unsigned int ticks);
void Sub_Temper_render(void);
void Sub_Temper_render_bloom_source(void);
void Sub_Temper_render_light_source(void);
float Sub_Temper_get_cooldown_fraction(void);
bool Sub_Temper_is_active(void);

#endif
