#ifndef SUB_SCORCH_H
#define SUB_SCORCH_H

#include <stdbool.h>
#include "input.h"
#include "collision.h"

void Sub_Scorch_initialize(void);
void Sub_Scorch_cleanup(void);
void Sub_Scorch_try_activate(void);
void Sub_Scorch_update(const Input *input, unsigned int ticks);
bool Sub_Scorch_is_sprinting(void);
float Sub_Scorch_get_cooldown_fraction(void);

/* Footprint management — called from mode_gameplay.c */
void Sub_Scorch_update_footprints(unsigned int ticks);
int Sub_Scorch_check_footprint_burn(Rectangle target);
void Sub_Scorch_render_footprints(void);
void Sub_Scorch_render_footprints_bloom(void);
void Sub_Scorch_render_footprints_light(void);

#endif
