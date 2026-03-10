#ifndef SUB_FLAK_H
#define SUB_FLAK_H

#include <stdbool.h>
#include "input.h"
#include "entity.h"

void Sub_Flak_initialize(Entity *parent);
void Sub_Flak_cleanup(void);

void Sub_Flak_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Sub_Flak_tick(unsigned int ticks);
void Sub_Flak_render(void);
void Sub_Flak_render_bloom_source(void);
void Sub_Flak_render_light_source(void);

bool Sub_Flak_check_hit(Rectangle target);
int Sub_Flak_check_hit_burn(Rectangle target); /* returns hit count for burn application */
int Sub_Flak_check_hit_burn_capped(Rectangle target, int max_per_volley);
bool Sub_Flak_check_nearby(Position pos, double radius);
void Sub_Flak_deactivate_all(void);
float Sub_Flak_get_cooldown_fraction(void);

#endif
