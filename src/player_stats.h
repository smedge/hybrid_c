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
void PlayerStats_heal(double amount);
bool PlayerStats_is_dead(void);
bool PlayerStats_is_shielded(void);
void PlayerStats_set_shielded(bool shielded);
void PlayerStats_reset(void);
double PlayerStats_get_feedback(void);
void PlayerStats_set_iframes(int ms);
bool PlayerStats_has_iframes(void);
void PlayerStats_boost_regen(int duration_ms, double multiplier);

/* EMP debuff: spikes feedback to max, halves decay rate */
void PlayerStats_apply_emp(unsigned int duration_ms);
bool PlayerStats_is_emp_debuffed(void);

/* Heatwave debuff: multiplies all feedback accumulation */
void PlayerStats_apply_feedback_multiplier(double multiplier, unsigned int duration_ms);
bool PlayerStats_has_feedback_multiplier(void);
double PlayerStats_get_feedback_multiplier(void);

/* Damage resistance: reduces incoming damage by 50% */
void PlayerStats_set_resist(bool enabled, unsigned int duration_ms);
bool PlayerStats_has_resist(void);

/* Damage reflection: when active, 50% of incoming damage is accumulated for reflect */
void PlayerStats_set_reflect(bool enabled);
bool PlayerStats_has_reflect(void);
/* Returns accumulated reflected damage and clears it. Call once per frame. */
double PlayerStats_flush_reflected_damage(void);

/* Snapshot/restore for zone transitions */
typedef struct {
	double integrity;
	double feedback;
	bool shielded;
} PlayerStatsSnapshot;

PlayerStatsSnapshot PlayerStats_snapshot(void);
void PlayerStats_restore(PlayerStatsSnapshot snap);

#endif
