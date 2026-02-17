#include "sub_mend.h"

#include "skillbar.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"
#include "audio.h"

#define HEAL_AMOUNT 50.0
#define FEEDBACK_COST 20.0
#define COOLDOWN_MS 10000
#define REGEN_BOOST_DURATION 5000  /* 5s boosted regen after heal */
#define REGEN_BOOST_MULTIPLIER 3.0 /* 3x regen rate */

typedef enum {
	MEND_READY,
	MEND_COOLDOWN
} MendState;

static MendState state;
static int cooldownMs;
static Mix_Chunk *sampleHeal;

void Sub_Mend_initialize(void)
{
	state = MEND_READY;
	cooldownMs = 0;
	Audio_load_sample(&sampleHeal, "resources/sounds/heal.wav");
}

void Sub_Mend_cleanup(void)
{
	Audio_unload_sample(&sampleHeal);
}

void Sub_Mend_update(const Input *input, unsigned int ticks)
{
	if (Ship_is_destroyed()) return;

	switch (state) {
	case MEND_READY:
		if (input->keyG && Skillbar_is_active(SUB_ID_MEND)) {
			PlayerStats_heal(HEAL_AMOUNT);
			PlayerStats_boost_regen(REGEN_BOOST_DURATION, REGEN_BOOST_MULTIPLIER);
			PlayerStats_add_feedback(FEEDBACK_COST);
			Audio_play_sample(&sampleHeal);
			state = MEND_COOLDOWN;
			cooldownMs = COOLDOWN_MS;
		}
		break;

	case MEND_COOLDOWN:
		cooldownMs -= (int)ticks;
		if (cooldownMs <= 0) {
			cooldownMs = 0;
			state = MEND_READY;
		}
		break;
	}
}

float Sub_Mend_get_cooldown_fraction(void)
{
	if (state == MEND_COOLDOWN)
		return (float)cooldownMs / COOLDOWN_MS;
	return 0.0f;
}
