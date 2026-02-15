#ifndef SUB_AEGIS_H
#define SUB_AEGIS_H

#include "input.h"

void Sub_Aegis_initialize(void);
void Sub_Aegis_cleanup(void);
void Sub_Aegis_update(const Input *input, unsigned int ticks);
void Sub_Aegis_render(void);
void Sub_Aegis_render_bloom_source(void);
float Sub_Aegis_get_cooldown_fraction(void);

#endif
