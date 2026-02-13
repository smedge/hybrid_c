#ifndef SUB_EGRESS_H
#define SUB_EGRESS_H

#include <stdbool.h>
#include "input.h"

void Sub_Egress_initialize(void);
void Sub_Egress_cleanup(void);
void Sub_Egress_update(const Input *input, unsigned int ticks);
bool Sub_Egress_is_dashing(void);
double Sub_Egress_get_dash_vx(void);
double Sub_Egress_get_dash_vy(void);
float Sub_Egress_get_cooldown_fraction(void);

#endif
