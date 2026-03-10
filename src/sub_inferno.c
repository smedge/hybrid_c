#include "sub_inferno.h"
#include "sub_inferno_core.h"

#include "graphics.h"
#include "position.h"
#include "view.h"
#include "shipstate.h"
#include "skillbar.h"
#include "sub_stealth.h"
#include "enemy_util.h"
#include "player_stats.h"
#include "keybinds.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BLOB_COUNT 256
#define FEEDBACK_PER_SEC 25.0

static Entity *parent;
static InfernoBlob blobs[BLOB_COUNT];
static SubInfernoCoreState coreState;
static bool wasChanneling;

void Sub_Inferno_initialize(Entity *p)
{
	parent = p;
	wasChanneling = false;
	SubInfernoCore_init_audio();
	SubInfernoCore_init(&coreState, blobs, BLOB_COUNT);
}

void Sub_Inferno_cleanup(void)
{
	SubInfernoCore_deactivate_all(&coreState);
	SubInfernoCore_cleanup_audio();
}

void Sub_Inferno_update(const Input *userInput, unsigned int ticks, PlaceableComponent *placeable)
{
	ShipState *state = (ShipState *)parent->state;
	double dt = ticks / 1000.0;

	bool fire = Keybinds_held(BIND_PRIMARY_WEAPON)
		|| (!Keybinds_is_lmb_rebound() && userInput->mouseLeft);
	bool active = fire && !state->destroyed
		&& Skillbar_is_active(SUB_ID_INFERNO);

	/* Break stealth on first frame of channeling */
	if (active && !wasChanneling)
		Enemy_break_cloak_attack();

	/* Feedback drain */
	if (active)
		PlayerStats_add_feedback(FEEDBACK_PER_SEC * dt);

	/* Compute aim angle from mouse cursor */
	Position cursor = {userInput->mouseX, userInput->mouseY};
	Screen screen = Graphics_get_screen();
	Position cursorWorld = View_get_world_position(&screen, cursor);
	double heading = Position_get_heading(placeable->position, cursorWorld);
	double aim_rad = heading * M_PI / 180.0;

	/* Delegate to core */
	SubInfernoCore_set_origin(&coreState, placeable->position);
	SubInfernoCore_set_aim_angle(&coreState, aim_rad);
	SubInfernoCore_set_channeling(&coreState, active);
	SubInfernoCore_update(&coreState, NULL, ticks);

	wasChanneling = active;
}

void Sub_Inferno_render(void)
{
	SubInfernoCore_render(&coreState, NULL);
}

void Sub_Inferno_render_bloom_source(void)
{
	SubInfernoCore_render_bloom(&coreState, NULL);
}

void Sub_Inferno_render_light_source(void)
{
	SubInfernoCore_render_light(&coreState);
}

bool Sub_Inferno_check_hit(Rectangle target)
{
	return SubInfernoCore_check_hit(&coreState, target);
}

bool Sub_Inferno_check_nearby(Position pos, double radius)
{
	return SubInfernoCore_check_nearby(&coreState, pos, radius);
}

void Sub_Inferno_deactivate_all(void)
{
	SubInfernoCore_deactivate_all(&coreState);
	wasChanneling = false;
}

float Sub_Inferno_get_cooldown_fraction(void)
{
	return 0.0f;
}
