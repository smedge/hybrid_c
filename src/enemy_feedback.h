#ifndef ENEMY_FEEDBACK_H
#define ENEMY_FEEDBACK_H

#include <stdbool.h>

#define ENEMY_FEEDBACK_MAX 100.0
#define ENEMY_FEEDBACK_DECAY 15.0
#define ENEMY_FEEDBACK_GRACE_MS 500

typedef struct {
	double feedback;
	unsigned int graceTimer;
	int aggression;       /* 1-10, random at spawn */
	bool empDebuffed;
	unsigned int empTimer;
} EnemyFeedback;

/* Initialize: zero feedback, randomize aggression 1-10 */
void EnemyFeedback_init(EnemyFeedback *fb);

/* Decay feedback, tick EMP timer */
void EnemyFeedback_update(EnemyFeedback *fb, unsigned int ticks);

/* Try to spend feedback cost. Returns true if ability fires.
   Spillover damages hp_ptr. Returns false if aggression says no. */
bool EnemyFeedback_try_spend(EnemyFeedback *fb, double cost, double *hp_ptr);

/* Spike feedback to max, apply EMP debuff (halved decay) */
void EnemyFeedback_apply_emp(EnemyFeedback *fb, unsigned int duration_ms);

/* Reset all state, re-randomize aggression */
void EnemyFeedback_reset(EnemyFeedback *fb);

#endif
