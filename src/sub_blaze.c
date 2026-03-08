#include "sub_blaze.h"

#include "sub_blaze_core.h"
#include "sub_dash_core.h"
#include "skillbar.h"
#include "progression.h"
#include "ship.h"
#include "player_stats.h"
#include "burn.h"
#include "render.h"
#include "keybinds.h"

#include <math.h>
#include <string.h>

#define DEG_TO_RAD (M_PI / 180.0)

/* --- Dash config (player-specific) --- */

static SubDashCore dashCore;
static const SubDashConfig dashCfg = {
	.duration_ms = 200,
	.speed = 4500.0,
	.cooldown_ms = 3000,
	.damage = 0.0,  /* no contact damage — corridor is the damage */
};

const SubDashConfig *Sub_Blaze_get_dash_config(void)
{
	return &dashCfg;
}

static const double FEEDBACK_COST = 20.0;

/* --- Corridor (player) --- */

#define PLAYER_CORRIDOR_MAX 200
static BlazeCorridorSegment playerCorridor[PLAYER_CORRIDOR_MAX];
static SubBlazeCore playerBlazeCore;

/* --- Lifecycle --- */

void Sub_Blaze_initialize(void)
{
	SubDash_init(&dashCore);
	SubDash_initialize_audio();
	SubBlaze_init(&playerBlazeCore, playerCorridor, PLAYER_CORRIDOR_MAX);
	Sub_Blaze_deactivate_all();
}

void Sub_Blaze_cleanup(void)
{
	SubDash_cleanup_audio();
	Sub_Blaze_deactivate_all();
}

/* --- Activate --- */

void Sub_Blaze_try_activate(void)
{
	if (SubDash_is_active(&dashCore) || dashCore.cooldownMs > 0) return;

	double dx = 0.0, dy = 0.0;
	if (Keybinds_held(BIND_MOVE_UP))    dy += 1.0;
	if (Keybinds_held(BIND_MOVE_DOWN))  dy -= 1.0;
	if (Keybinds_held(BIND_MOVE_RIGHT)) dx += 1.0;
	if (Keybinds_held(BIND_MOVE_LEFT))  dx -= 1.0;

	double len = sqrt(dx * dx + dy * dy);
	if (len > 0.0) {
		dx /= len;
		dy /= len;
	} else {
		double heading = Ship_get_heading();
		dx = sin(heading * DEG_TO_RAD);
		dy = cos(heading * DEG_TO_RAD);
	}

	if (SubDash_try_activate(&dashCore, &dashCfg, dx, dy)) {
		PlayerStats_add_feedback(FEEDBACK_COST);
		PlayerStats_set_iframes(dashCfg.duration_ms);
		playerBlazeCore.spawn_timer = 0;
		SubBlaze_spawn_segment(&playerBlazeCore, Ship_get_position());
	}
}

/* --- Update --- */

void Sub_Blaze_update(const Input *input, unsigned int ticks)
{
	(void)input;

	/* Deposit corridor segments during dash */
	if (SubDash_is_active(&dashCore)) {
		const SubBlazeConfig *cfg = SubBlaze_get_config();
		playerBlazeCore.spawn_timer += (int)ticks;
		while (playerBlazeCore.spawn_timer >= cfg->segment_spawn_ms) {
			playerBlazeCore.spawn_timer -= cfg->segment_spawn_ms;
			SubBlaze_spawn_segment(&playerBlazeCore, Ship_get_position());
		}
	}

	SubDash_update(&dashCore, &dashCfg, ticks);
}

/* --- Corridor update --- */

void Sub_Blaze_update_corridor(unsigned int ticks)
{
	SubBlaze_update_corridor(&playerBlazeCore, SubBlaze_get_config(), ticks);
}

/* --- Corridor burn check --- */

int Sub_Blaze_check_corridor_burn(Rectangle target)
{
	return SubBlaze_check_corridor_burn(&playerBlazeCore, SubBlaze_get_config(), target);
}

/* --- Deactivate --- */

void Sub_Blaze_deactivate_all(void)
{
	SubBlaze_deactivate_all(&playerBlazeCore);
}

/* --- Accessors --- */

bool Sub_Blaze_is_dashing(void)
{
	return SubDash_is_active(&dashCore);
}

double Sub_Blaze_get_dash_vx(void)
{
	return SubDash_is_active(&dashCore) ? dashCore.dirX * dashCfg.speed : 0.0;
}

double Sub_Blaze_get_dash_vy(void)
{
	return SubDash_is_active(&dashCore) ? dashCore.dirY * dashCfg.speed : 0.0;
}

float Sub_Blaze_get_cooldown_fraction(void)
{
	return SubDash_get_cooldown_fraction(&dashCore, &dashCfg);
}

/* --- Rendering --- */

void Sub_Blaze_render_corridor(void)
{
	SubBlaze_render_corridor(&playerBlazeCore, SubBlaze_get_config());
}

void Sub_Blaze_render_corridor_bloom_source(void)
{
	SubBlaze_render_corridor_bloom_source(&playerBlazeCore, SubBlaze_get_config());
}

void Sub_Blaze_render_corridor_light_source(void)
{
	SubBlaze_render_corridor_light_source(&playerBlazeCore, SubBlaze_get_config());
}
