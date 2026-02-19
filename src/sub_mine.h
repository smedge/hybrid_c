#ifndef SUB_MINE_H
#define SUB_MINE_H

#include <stdbool.h>
#include "input.h"
#include "collision.h"

void Sub_Mine_initialize(void);
void Sub_Mine_cleanup(void);
void Sub_Mine_update(const Input *userInput, const unsigned int ticks);
void Sub_Mine_render(void);
void Sub_Mine_render_bloom_source(void);
void Sub_Mine_render_light_source(void);
void Sub_Mine_deactivate_all(void);
double Sub_Mine_check_hit(Rectangle target);
float Sub_Mine_get_cooldown_fraction(void);

#endif
