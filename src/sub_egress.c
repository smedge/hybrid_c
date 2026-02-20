#include "sub_egress.h"

#include "sub_dash_core.h"
#include "skillbar.h"
#include "progression.h"
#include "ship.h"
#include "player_stats.h"
#include "collision.h"

#include <math.h>

#define DEG_TO_RAD (M_PI / 180.0)
#define DASH_HIT_HALF 50.0

static SubDashCore core;
static const SubDashConfig cfg = {
	.duration_ms = 150,
	.speed = 4000.0,
	.cooldown_ms = 2000,
	.damage = 50.0,
};

static bool shiftWasDown;

const SubDashConfig *Sub_Egress_get_config(void)
{
	return &cfg;
}

void Sub_Egress_initialize(void)
{
	SubDash_init(&core);
	SubDash_initialize_audio();
	shiftWasDown = false;
}

void Sub_Egress_cleanup(void)
{
	SubDash_cleanup_audio();
}

void Sub_Egress_update(const Input *input, unsigned int ticks)
{
	bool shiftDown = input->keyLShift && Skillbar_is_active(SUB_ID_EGRESS);

	if (!SubDash_is_active(&core) && core.cooldownMs <= 0) {
		/* Edge-detect: shift just pressed */
		if (shiftDown && !shiftWasDown) {
			double dx = 0.0, dy = 0.0;
			if (input->keyW) dy += 1.0;
			if (input->keyS) dy -= 1.0;
			if (input->keyD) dx += 1.0;
			if (input->keyA) dx -= 1.0;

			double len = sqrt(dx * dx + dy * dy);
			if (len > 0.0) {
				dx /= len;
				dy /= len;
			} else {
				double heading = Ship_get_heading();
				dx = sin(heading * DEG_TO_RAD);
				dy = cos(heading * DEG_TO_RAD);
			}

			if (SubDash_try_activate(&core, &cfg, dx, dy)) {
				PlayerStats_add_feedback(25.0);
				PlayerStats_set_iframes(cfg.duration_ms);
			}
		}
	}

	SubDash_update(&core, &cfg, ticks);

	shiftWasDown = shiftDown;
}

bool Sub_Egress_is_dashing(void)
{
	return SubDash_is_active(&core);
}

double Sub_Egress_get_dash_vx(void)
{
	return SubDash_is_active(&core) ? core.dirX * cfg.speed : 0.0;
}

double Sub_Egress_get_dash_vy(void)
{
	return SubDash_is_active(&core) ? core.dirY * cfg.speed : 0.0;
}

float Sub_Egress_get_cooldown_fraction(void)
{
	return SubDash_get_cooldown_fraction(&core, &cfg);
}

double Sub_Egress_check_hit(Rectangle target)
{
	if (!SubDash_is_active(&core))
		return 0.0;
	Position shipPos = Ship_get_position();
	Rectangle dashBox = {
		shipPos.x - DASH_HIT_HALF,
		shipPos.y + DASH_HIT_HALF,
		shipPos.x + DASH_HIT_HALF,
		shipPos.y - DASH_HIT_HALF
	};
	return Collision_aabb_test(dashBox, target) ? cfg.damage : 0.0;
}
