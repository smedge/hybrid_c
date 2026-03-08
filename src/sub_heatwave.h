#ifndef SUB_HEATWAVE_H
#define SUB_HEATWAVE_H

#include <stdbool.h>
#include "input.h"

void Sub_Heatwave_initialize(void);
void Sub_Heatwave_cleanup(void);
void Sub_Heatwave_update(const Input *input, unsigned int ticks);
void Sub_Heatwave_render(void);
void Sub_Heatwave_render_bloom_source(void);
void Sub_Heatwave_render_light_source(void);
float Sub_Heatwave_get_cooldown_fraction(void);
void Sub_Heatwave_try_activate(void);
bool Sub_Heatwave_is_active(void);

#endif
