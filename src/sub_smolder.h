#ifndef SUB_SMOLDER_H
#define SUB_SMOLDER_H

#include <stdbool.h>

void Sub_Smolder_initialize(void);
void Sub_Smolder_cleanup(void);
void Sub_Smolder_update(unsigned int ticks);
void Sub_Smolder_try_activate(void);
void Sub_Smolder_break(void);
void Sub_Smolder_break_attack(void);
bool Sub_Smolder_is_active(void);
bool Sub_Smolder_is_ambush_active(void);
double Sub_Smolder_get_damage_multiplier(double distance);
void Sub_Smolder_notify_kill(void);
float Sub_Smolder_get_ship_alpha(void);
float Sub_Smolder_get_cooldown_fraction(void);

#endif
