#include "sub_stealth.h"

#include "player_stats.h"
#include "ship.h"
#include "audio.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define COOLDOWN_MS 15000
#define PULSE_SPEED 4.0f
#define AMBUSH_DURATION_MS 1000
#define AMBUSH_RANGE 500.0
#define AMBUSH_MULTIPLIER 5.0

typedef enum {
	STEALTH_READY,
	STEALTH_ACTIVE,
	STEALTH_COOLDOWN
} StealthState;

static StealthState state;
static int cooldownMs;
static float pulseTimer;
static int ambushMs;

static Mix_Chunk *sampleActivate = 0;
static Mix_Chunk *sampleBreak = 0;

void Sub_Stealth_initialize(void)
{
	state = STEALTH_READY;
	cooldownMs = 0;
	pulseTimer = 0.0f;
	ambushMs = 0;

	Audio_load_sample(&sampleActivate, "resources/sounds/refill_start.wav");
	Audio_load_sample(&sampleBreak, "resources/sounds/door.wav");
}

void Sub_Stealth_cleanup(void)
{
	if (state == STEALTH_ACTIVE)
		state = STEALTH_READY;

	Audio_unload_sample(&sampleActivate);
	Audio_unload_sample(&sampleBreak);
}

void Sub_Stealth_update(unsigned int ticks)
{
	if (ambushMs > 0) {
		ambushMs -= (int)ticks;
		if (ambushMs < 0) ambushMs = 0;
	}

	if (Ship_is_destroyed()) {
		if (state == STEALTH_ACTIVE || state == STEALTH_COOLDOWN) {
			state = STEALTH_READY;
			cooldownMs = 0;
		}
		ambushMs = 0;
		return;
	}

	switch (state) {
	case STEALTH_READY:
		break;

	case STEALTH_ACTIVE:
		pulseTimer += ticks / 1000.0f;
		break;

	case STEALTH_COOLDOWN:
		cooldownMs -= (int)ticks;
		if (cooldownMs <= 0) {
			cooldownMs = 0;
			state = STEALTH_READY;
		}
		break;
	}
}

void Sub_Stealth_try_activate(void)
{
	if (Ship_is_destroyed())
		return;

	/* Already stealthed — pressing again unstealths */
	if (state == STEALTH_ACTIVE) {
		Sub_Stealth_break();
		return;
	}

	if (state != STEALTH_READY)
		return;
	if (PlayerStats_get_feedback() > 0.0)
		return;

	state = STEALTH_ACTIVE;
	cooldownMs = COOLDOWN_MS;
	pulseTimer = 0.0f;
	Audio_play_sample(&sampleActivate);
}

void Sub_Stealth_break(void)
{
	if (state != STEALTH_ACTIVE)
		return;

	state = STEALTH_COOLDOWN;

	Audio_play_sample(&sampleBreak);
}

void Sub_Stealth_break_attack(void)
{
	if (state != STEALTH_ACTIVE)
		return;

	ambushMs = AMBUSH_DURATION_MS;
	Sub_Stealth_break();
}

double Sub_Stealth_get_damage_multiplier(double distance)
{
	(void)distance;
	if (ambushMs > 0)
		return AMBUSH_MULTIPLIER;
	return 1.0;
}

void Sub_Stealth_notify_kill(void)
{
	if (ambushMs > 0) {
		cooldownMs = 0;
		state = STEALTH_READY;
	}
}

bool Sub_Stealth_is_stealthed(void)
{
	return state == STEALTH_ACTIVE;
}

bool Sub_Stealth_is_ambush_active(void)
{
	return ambushMs > 0;
}

bool Sub_Stealth_is_available(void)
{
	return state == STEALTH_READY && PlayerStats_get_feedback() <= 0.0;
}

float Sub_Stealth_get_cooldown_fraction(void)
{
	if (state == STEALTH_ACTIVE)
		return 1.0f;
	if (state == STEALTH_COOLDOWN)
		return (float)cooldownMs / COOLDOWN_MS;
	/* Show blocked state when feedback > 0 */
	if (PlayerStats_get_feedback() > 0.0)
		return 0.01f;
	return 0.0f;
}

float Sub_Stealth_get_ship_alpha(void)
{
	if (state != STEALTH_ACTIVE)
		return 1.0f;
	return 0.15f + 0.1f * sinf(pulseTimer * PULSE_SPEED);
}
