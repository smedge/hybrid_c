#ifndef SUB_STEALTH_H
#define SUB_STEALTH_H

#include <stdbool.h>

void Sub_Stealth_initialize(void);
void Sub_Stealth_cleanup(void);
void Sub_Stealth_update(unsigned int ticks);

/* Called by skillbar when slot is pressed */
void Sub_Stealth_try_activate(void);

/* Called by non-attack breaks (mine collision, enemy collision, manual toggle) */
void Sub_Stealth_break(void);

/* Called by attack subs (pea, mgun, mine) — triggers ambush damage bonus */
void Sub_Stealth_break_attack(void);

/* Returns damage multiplier (5x if within 500 units during 1s ambush window) */
double Sub_Stealth_get_damage_multiplier(double distance);

/* Call when an enemy is killed — resets stealth cooldown if it was an ambush kill */
void Sub_Stealth_notify_kill(void);

bool Sub_Stealth_is_stealthed(void);
bool Sub_Stealth_is_available(void);
float Sub_Stealth_get_cooldown_fraction(void);

/* Returns alpha multiplier for ship rendering (1.0 = normal, pulsing when stealthed) */
float Sub_Stealth_get_ship_alpha(void);

#endif
