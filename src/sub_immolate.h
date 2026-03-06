#ifndef SUB_IMMOLATE_H
#define SUB_IMMOLATE_H

#include "input.h"
#include "collision.h"

void Sub_Immolate_initialize(void);
void Sub_Immolate_cleanup(void);
void Sub_Immolate_update(const Input *input, unsigned int ticks);
void Sub_Immolate_render(void);
void Sub_Immolate_render_bloom_source(void);
void Sub_Immolate_render_light_source(void);
float Sub_Immolate_get_cooldown_fraction(void);
void Sub_Immolate_on_hit(void);
int Sub_Immolate_check_burn(Rectangle target);

#endif
