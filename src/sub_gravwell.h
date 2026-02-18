#ifndef SUB_GRAVWELL_H
#define SUB_GRAVWELL_H

#include <stdbool.h>
#include "input.h"
#include "position.h"
#include "entity.h"

void Sub_Gravwell_initialize(void);
void Sub_Gravwell_cleanup(void);
void Sub_Gravwell_update(const Input *userInput, unsigned int ticks);
void Sub_Gravwell_render(void);
void Sub_Gravwell_render_bloom_source(void);
void Sub_Gravwell_deactivate_all(void);
float Sub_Gravwell_get_cooldown_fraction(void);
void Sub_Gravwell_try_activate(void);

/* Query API for enemy gravity integration */
bool Sub_Gravwell_is_active(void);
bool Sub_Gravwell_get_pull(Position pos, double dt,
	double *out_dx, double *out_dy, double *out_speed_mult);

#endif
