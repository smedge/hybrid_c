#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

#include <stdbool.h>
#include "screen.h"

void PlayerStats_initialize(void);
void PlayerStats_cleanup(void);
void PlayerStats_update(unsigned int ticks);
void PlayerStats_render(const Screen *screen);
void PlayerStats_add_feedback(double amount);
void PlayerStats_damage(double amount);
bool PlayerStats_is_dead(void);
void PlayerStats_reset(void);

#endif
