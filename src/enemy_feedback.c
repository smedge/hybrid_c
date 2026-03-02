#include "enemy_feedback.h"

#include <stdlib.h>

void EnemyFeedback_init(EnemyFeedback *fb)
{
	fb->feedback = 0.0;
	fb->graceTimer = ENEMY_FEEDBACK_GRACE_MS;
	fb->aggression = (rand() % 10) + 1;
	fb->empDebuffed = false;
	fb->empTimer = 0;
}

void EnemyFeedback_update(EnemyFeedback *fb, unsigned int ticks)
{
	double dt = ticks / 1000.0;

	/* Tick grace timer */
	fb->graceTimer += ticks;
	if (fb->graceTimer > 10000) fb->graceTimer = 10000;

	/* Tick EMP debuff */
	if (fb->empDebuffed) {
		if (ticks >= fb->empTimer) {
			fb->empTimer = 0;
			fb->empDebuffed = false;
		} else {
			fb->empTimer -= ticks;
		}
	}

	/* Decay after grace period */
	if (fb->graceTimer >= ENEMY_FEEDBACK_GRACE_MS) {
		double decay = ENEMY_FEEDBACK_DECAY;
		if (fb->empDebuffed)
			decay *= 0.5;
		fb->feedback -= decay * dt;
		if (fb->feedback < 0.0)
			fb->feedback = 0.0;
	}
}

bool EnemyFeedback_try_spend(EnemyFeedback *fb, double cost, double *hp_ptr)
{
	double newFeedback = fb->feedback + cost;
	double spillover = 0.0;

	if (newFeedback > ENEMY_FEEDBACK_MAX)
		spillover = newFeedback - ENEMY_FEEDBACK_MAX;

	/* Check aggression tolerance for spillover */
	if (spillover > 0.0) {
		double tolerance;
		if (fb->aggression <= 3)
			tolerance = 0.0;
		else if (fb->aggression <= 6)
			tolerance = 25.0;
		else if (fb->aggression <= 9)
			tolerance = 50.0;
		else
			tolerance = 9999.0; /* aggression 10: will self-destruct */

		if (spillover > tolerance)
			return false;
	}

	/* Commit the spend */
	fb->feedback += cost;
	if (fb->feedback > ENEMY_FEEDBACK_MAX) {
		spillover = fb->feedback - ENEMY_FEEDBACK_MAX;
		fb->feedback = ENEMY_FEEDBACK_MAX;
		if (hp_ptr)
			*hp_ptr -= spillover;
	}
	fb->graceTimer = 0;
	return true;
}

void EnemyFeedback_apply_emp(EnemyFeedback *fb, unsigned int duration_ms)
{
	fb->feedback = ENEMY_FEEDBACK_MAX;
	fb->graceTimer = 0;
	fb->empDebuffed = true;
	fb->empTimer = duration_ms;
}

void EnemyFeedback_reset(EnemyFeedback *fb)
{
	fb->feedback = 0.0;
	fb->graceTimer = ENEMY_FEEDBACK_GRACE_MS;
	fb->aggression = (rand() % 10) + 1;
	fb->empDebuffed = false;
	fb->empTimer = 0;
}
