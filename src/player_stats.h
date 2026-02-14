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
void PlayerStats_force_kill(void);
bool PlayerStats_is_dead(void);
void PlayerStats_reset(void);

/* Snapshot/restore for zone transitions */
typedef struct {
	double integrity;
	double feedback;
} PlayerStatsSnapshot;

PlayerStatsSnapshot PlayerStats_snapshot(void);
void PlayerStats_restore(PlayerStatsSnapshot snap);

#endif
