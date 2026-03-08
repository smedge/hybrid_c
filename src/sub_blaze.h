#ifndef SUB_BLAZE_H
#define SUB_BLAZE_H

#include <stdbool.h>
#include "input.h"
#include "collision.h"

void Sub_Blaze_initialize(void);
void Sub_Blaze_cleanup(void);
void Sub_Blaze_try_activate(void);
void Sub_Blaze_update(const Input *input, unsigned int ticks);
bool Sub_Blaze_is_dashing(void);
double Sub_Blaze_get_dash_vx(void);
double Sub_Blaze_get_dash_vy(void);
float Sub_Blaze_get_cooldown_fraction(void);

/* Corridor burn — enemies check this per frame */
int Sub_Blaze_check_corridor_burn(Rectangle target);

/* Corridor lifecycle */
void Sub_Blaze_update_corridor(unsigned int ticks);
void Sub_Blaze_deactivate_all(void);

/* Rendering */
void Sub_Blaze_render_corridor(void);
void Sub_Blaze_render_corridor_bloom_source(void);
void Sub_Blaze_render_corridor_light_source(void);

#endif
