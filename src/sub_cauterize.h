#ifndef SUB_CAUTERIZE_H
#define SUB_CAUTERIZE_H

#include "input.h"
#include "collision.h"

void Sub_Cauterize_initialize(void);
void Sub_Cauterize_cleanup(void);
void Sub_Cauterize_try_activate_player(void);
void Sub_Cauterize_update(const Input *input, unsigned int ticks);
void Sub_Cauterize_update_auras(unsigned int ticks);
int Sub_Cauterize_check_aura_burn(Rectangle target);
void Sub_Cauterize_deactivate_all(void);
float Sub_Cauterize_get_cooldown_fraction(void);
void Sub_Cauterize_render_aura(void);
void Sub_Cauterize_render_aura_bloom_source(void);
void Sub_Cauterize_render_aura_light_source(void);

#endif
