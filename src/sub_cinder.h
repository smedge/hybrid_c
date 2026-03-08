#ifndef SUB_CINDER_H
#define SUB_CINDER_H

#include <stdbool.h>
#include "input.h"
#include "collision.h"

void Sub_Cinder_initialize(void);
void Sub_Cinder_cleanup(void);
void Sub_Cinder_try_deploy(void);
void Sub_Cinder_update(const Input *userInput, unsigned int ticks);
void Sub_Cinder_tick(unsigned int ticks);
void Sub_Cinder_render(void);
void Sub_Cinder_render_bloom_source(void);
void Sub_Cinder_render_light_source(void);
void Sub_Cinder_deactivate_all(void);
double Sub_Cinder_check_hit(Rectangle target);
float Sub_Cinder_get_cooldown_fraction(void);

/* Fire pool lifecycle (called from mode_gameplay.c) */
void Sub_Cinder_update_pools(unsigned int ticks);
int Sub_Cinder_check_pool_burn(Rectangle target);
void Sub_Cinder_render_pools(void);
void Sub_Cinder_render_pools_bloom(void);
void Sub_Cinder_render_pools_light(void);

#endif
