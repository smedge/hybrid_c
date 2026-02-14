#include "sub_egress.h"

#include "skillbar.h"
#include "progression.h"
#include "ship.h"
#include "player_stats.h"

#include <math.h>

#define DEG_TO_RAD (M_PI / 180.0)

#define DASH_DURATION  150    /* 150ms burst */
#define DASH_SPEED     4000.0 /* 5x normal speed */
#define DASH_COOLDOWN  2000   /* 2 seconds between dashes */

typedef enum {
	EGRESS_READY,
	EGRESS_DASHING,
	EGRESS_COOLDOWN
} EgressState;

static EgressState state;
static int dashTimeLeft;
static int cooldownMs;
static double dashDirX, dashDirY;
static bool shiftWasDown;

void Sub_Egress_initialize(void)
{
	state = EGRESS_READY;
	dashTimeLeft = 0;
	cooldownMs = 0;
	dashDirX = 0.0;
	dashDirY = 0.0;
	shiftWasDown = false;
}

void Sub_Egress_cleanup(void)
{
}

void Sub_Egress_update(const Input *input, unsigned int ticks)
{
	bool shiftDown = input->keyLShift && Skillbar_is_active(SUB_ID_EGRESS);

	switch (state) {
	case EGRESS_READY:
		/* Edge-detect: shift just pressed */
		if (shiftDown && !shiftWasDown) {
			/* Get movement direction from WASD */
			double dx = 0.0, dy = 0.0;
			if (input->keyW) dy += 1.0;
			if (input->keyS) dy -= 1.0;
			if (input->keyD) dx += 1.0;
			if (input->keyA) dx -= 1.0;

			/* Normalize */
			double len = sqrt(dx * dx + dy * dy);
			if (len > 0.0) {
				dashDirX = dx / len;
				dashDirY = dy / len;
			} else {
				/* No direction held — dash in facing direction */
				double heading = Ship_get_heading();
				dashDirX = sin(heading * DEG_TO_RAD);
				dashDirY = cos(heading * DEG_TO_RAD);
			}

			state = EGRESS_DASHING;
			dashTimeLeft = DASH_DURATION;
			PlayerStats_add_feedback(25.0);
		}
		break;

	case EGRESS_DASHING:
		dashTimeLeft -= (int)ticks;
		if (dashTimeLeft <= 0) {
			dashTimeLeft = 0;
			state = EGRESS_COOLDOWN;
			cooldownMs = DASH_COOLDOWN;
		}
		break;

	case EGRESS_COOLDOWN:
		cooldownMs -= (int)ticks;
		if (cooldownMs <= 0) {
			cooldownMs = 0;
			state = EGRESS_READY;
		}
		break;
	}

	shiftWasDown = shiftDown;
}

bool Sub_Egress_is_dashing(void)
{
	return state == EGRESS_DASHING;
}

double Sub_Egress_get_dash_vx(void)
{
	return dashDirX * DASH_SPEED;
}

double Sub_Egress_get_dash_vy(void)
{
	return dashDirY * DASH_SPEED;
}

float Sub_Egress_get_cooldown_fraction(void)
{
	if (state == EGRESS_COOLDOWN)
		return (float)cooldownMs / DASH_COOLDOWN;
	if (state == EGRESS_DASHING)
		return 1.0f;
	return 0.0f;
}
