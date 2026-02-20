#ifndef SUB_EGRESS_H
#define SUB_EGRESS_H

#include <stdbool.h>
#include "input.h"
#include "collision.h"
#include "sub_dash_core.h"

const SubDashConfig *Sub_Egress_get_config(void);

void Sub_Egress_initialize(void);
void Sub_Egress_cleanup(void);
void Sub_Egress_update(const Input *input, unsigned int ticks);
bool Sub_Egress_is_dashing(void);
double Sub_Egress_get_dash_vx(void);
double Sub_Egress_get_dash_vy(void);
float Sub_Egress_get_cooldown_fraction(void);
double Sub_Egress_check_hit(Rectangle target);

#endif
