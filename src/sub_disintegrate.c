#include "sub_disintegrate.h"

#include "graphics.h"
#include "audio.h"
#include "map.h"
#include "position.h"
#include "render.h"
#include "view.h"
#include "shipstate.h"
#include "skillbar.h"
#include "sub_stealth.h"
#include "player_stats.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BEAM_MAX_RANGE    2600.0
#define BEAM_HALF_WIDTH   12.0
#define CARVE_SPEED_DEG   120.0
#define FEEDBACK_PER_SEC  22.0
#define DAMAGE_PER_FRAME  15.0
#define SOUND_INTERVAL_MS 150
#define PULSE_SPEED       10.0f
#define BEAM_ORIGIN_OFFSET 10.0
#define TIP_FADE_LENGTH   120.0f  /* world units over which the tip dissipates */
#define TIP_FLICKER_SPEED 25.0f

static Entity *parent;
static bool channeling;
static bool wasChanneling;
static double beamAngle;      /* degrees, 0 = north */
static Position beamStart;
static Position beamEnd;
static double beamLength;
static float pulseTimer;
static int soundTimer;

static Mix_Chunk *sampleFire = 0;

static double normalize_angle(double a)
{
	while (a > 180.0)  a -= 360.0;
	while (a < -180.0) a += 360.0;
	return a;
}

static double deg_to_rad(double d)
{
	return d * M_PI / 180.0;
}

void Sub_Disintegrate_initialize(Entity *p)
{
	parent = p;
	channeling = false;
	wasChanneling = false;
	beamAngle = 0.0;
	beamStart = (Position){0, 0};
	beamEnd = (Position){0, 0};
	beamLength = 0.0;
	pulseTimer = 0.0f;
	soundTimer = 0;

	Audio_load_sample(&sampleFire, "resources/sounds/bomb_explode.wav");
}

void Sub_Disintegrate_cleanup(void)
{
	Audio_unload_sample(&sampleFire);
}

void Sub_Disintegrate_update(const Input *userInput, unsigned int ticks, PlaceableComponent *placeable)
{
	ShipState *state = (ShipState *)parent->state;
	double dt = ticks / 1000.0;

	channeling = userInput->mouseLeft && !state->destroyed
		&& Skillbar_is_active(SUB_ID_DISINTEGRATE);

	if (channeling) {
		/* Break stealth on first frame */
		if (!wasChanneling)
			Sub_Stealth_break_attack();

		/* Compute cursor angle */
		Position cursor = {userInput->mouseX, userInput->mouseY};
		Screen screen = Graphics_get_screen();
		Position cursorWorld = View_get_world_position(&screen, cursor);
		double targetAngle = Position_get_heading(placeable->position, cursorWorld);

		if (!wasChanneling) {
			/* Snap on first frame */
			beamAngle = targetAngle;
		} else {
			/* Carve: rotate toward cursor at max speed */
			double diff = normalize_angle(targetAngle - beamAngle);
			double maxRot = CARVE_SPEED_DEG * dt;
			if (diff > maxRot)
				diff = maxRot;
			else if (diff < -maxRot)
				diff = -maxRot;
			beamAngle = normalize_angle(beamAngle + diff);
		}

		/* Feedback drain */
		PlayerStats_add_feedback(FEEDBACK_PER_SEC * dt);

		/* Looping sound */
		soundTimer -= (int)ticks;
		if (soundTimer <= 0) {
			Audio_play_sample(&sampleFire);
			soundTimer = SOUND_INTERVAL_MS;
		}

		/* Raycast beam from ship along beamAngle */
		double rad = deg_to_rad(beamAngle);
		double dx = sin(rad);
		double dy = cos(rad);

		beamStart.x = placeable->position.x + dx * BEAM_ORIGIN_OFFSET;
		beamStart.y = placeable->position.y + dy * BEAM_ORIGIN_OFFSET;
		double endX = placeable->position.x + dx * BEAM_MAX_RANGE;
		double endY = placeable->position.y + dy * BEAM_MAX_RANGE;

		double hx, hy;
		if (Map_line_test_hit(beamStart.x, beamStart.y, endX, endY, &hx, &hy)) {
			beamEnd.x = hx;
			beamEnd.y = hy;
		} else {
			beamEnd.x = endX;
			beamEnd.y = endY;
		}

		double bx = beamEnd.x - beamStart.x;
		double by = beamEnd.y - beamStart.y;
		beamLength = sqrt(bx * bx + by * by);

		/* Pulse timer */
		pulseTimer += (float)(dt * PULSE_SPEED);
		if (pulseTimer > 2.0f * (float)M_PI)
			pulseTimer -= 2.0f * (float)M_PI;
	} else {
		soundTimer = 0;
		beamLength = 0.0;
	}

	wasChanneling = channeling;

}

void Sub_Disintegrate_render(void)
{
	if (!channeling || beamLength < 1.0)
		return;

	float pulse = 1.0f + 0.15f * sinf(pulseTimer);
	float len = (float)beamLength;

	float sx = (float)beamStart.x;
	float sy = (float)beamStart.y;
	float ex = (float)beamEnd.x;
	float ey = (float)beamEnd.y;

	/* Direction unit vector */
	float dirX = (ex - sx) / len;
	float dirY = (ey - sy) / len;

	/* Split into main body and fading tip */
	float tipLen = TIP_FADE_LENGTH;
	if (tipLen > len) tipLen = len;
	float bodyLen = len - tipLen;

	float mx = sx + dirX * bodyLen;  /* main/tip boundary */
	float my = sy + dirY * bodyLen;

	/* Main body — full intensity */
	if (bodyLen > 1.0f) {
		Render_thick_line(sx, sy, mx, my,
			18.0f * pulse, 0.6f, 0.2f, 0.9f, 0.25f);
		Render_thick_line(sx, sy, mx, my,
			10.0f * pulse, 0.8f, 0.5f, 1.0f, 0.7f);
		Render_thick_line(sx, sy, mx, my,
			4.0f * pulse, 1.0f, 0.95f, 1.0f, 0.95f);
	}

	/* Tip — dissipates in segments with flicker */
	int tipSegs = 5;
	float segLen = tipLen / tipSegs;
	for (int i = 0; i < tipSegs; i++) {
		float t0 = (float)i / tipSegs;
		float t1 = (float)(i + 1) / tipSegs;
		float fade = 1.0f - (t0 + t1) * 0.5f;  /* 1.0 at base, 0.0 at tip */

		/* Per-segment flicker — hash from segment index + timer */
		float flicker = 0.7f + 0.3f * sinf(pulseTimer * TIP_FLICKER_SPEED + (float)i * 2.3f);
		fade *= flicker;
		if (fade < 0.0f) fade = 0.0f;

		/* Width narrows toward tip */
		float widthMul = 1.0f - t0 * 0.6f;

		float x0 = mx + dirX * segLen * (float)i;
		float y0 = my + dirY * segLen * (float)i;
		float x1 = mx + dirX * segLen * (float)(i + 1);
		float y1 = my + dirY * segLen * (float)(i + 1);

		Render_thick_line(x0, y0, x1, y1,
			18.0f * pulse * widthMul, 0.6f, 0.2f, 0.9f, 0.25f * fade);
		Render_thick_line(x0, y0, x1, y1,
			10.0f * pulse * widthMul, 0.8f, 0.5f, 1.0f, 0.7f * fade);
		Render_thick_line(x0, y0, x1, y1,
			4.0f * pulse * widthMul, 1.0f, 0.95f, 1.0f, 0.95f * fade);
	}
}

void Sub_Disintegrate_render_bloom_source(void)
{
	if (!channeling || beamLength < 1.0)
		return;

	float pulse = 1.0f + 0.15f * sinf(pulseTimer);
	float len = (float)beamLength;

	float sx = (float)beamStart.x;
	float sy = (float)beamStart.y;
	float ex = (float)beamEnd.x;
	float ey = (float)beamEnd.y;

	float dirX = (ex - sx) / len;
	float dirY = (ey - sy) / len;

	float tipLen = TIP_FADE_LENGTH;
	if (tipLen > len) tipLen = len;
	float bodyLen = len - tipLen;

	float mx = sx + dirX * bodyLen;
	float my = sy + dirY * bodyLen;

	/* Main body bloom */
	if (bodyLen > 1.0f) {
		Render_thick_line(sx, sy, mx, my,
			24.0f * pulse, 0.7f, 0.3f, 1.0f, 0.6f);
		Render_thick_line(sx, sy, mx, my,
			14.0f * pulse, 0.9f, 0.7f, 1.0f, 0.8f);
		Render_thick_line(sx, sy, mx, my,
			6.0f * pulse, 1.0f, 1.0f, 1.0f, 1.0f);
	}

	/* Tip bloom — fades + flickers to match render */
	int tipSegs = 5;
	float segLen = tipLen / tipSegs;
	for (int i = 0; i < tipSegs; i++) {
		float t0 = (float)i / tipSegs;
		float t1 = (float)(i + 1) / tipSegs;
		float fade = 1.0f - (t0 + t1) * 0.5f;
		float flicker = 0.7f + 0.3f * sinf(pulseTimer * TIP_FLICKER_SPEED + (float)i * 2.3f);
		fade *= flicker;
		if (fade < 0.0f) fade = 0.0f;
		float widthMul = 1.0f - t0 * 0.6f;

		float x0 = mx + dirX * segLen * (float)i;
		float y0 = my + dirY * segLen * (float)i;
		float x1 = mx + dirX * segLen * (float)(i + 1);
		float y1 = my + dirY * segLen * (float)(i + 1);

		Render_thick_line(x0, y0, x1, y1,
			24.0f * pulse * widthMul, 0.7f, 0.3f, 1.0f, 0.6f * fade);
		Render_thick_line(x0, y0, x1, y1,
			14.0f * pulse * widthMul, 0.9f, 0.7f, 1.0f, 0.8f * fade);
		Render_thick_line(x0, y0, x1, y1,
			6.0f * pulse * widthMul, 1.0f, 1.0f, 1.0f, 1.0f * fade);
	}
}

bool Sub_Disintegrate_check_hit(Rectangle target)
{
	if (!channeling || beamLength < 1.0)
		return false;

	/* Point-to-line-segment distance test.
	   Target center vs beam centerline. Hit if distance < beam_half_width + target_half_size */
	double cx = (target.aX + target.bX) * 0.5;
	double cy = (target.aY + target.bY) * 0.5;
	double halfW = fabs(target.bX - target.aX) * 0.5;
	double halfH = fabs(target.aY - target.bY) * 0.5;
	double targetHalfSize = (halfW > halfH) ? halfW : halfH;

	/* Beam segment: beamStart -> beamEnd */
	double ax = beamStart.x, ay = beamStart.y;
	double bx = beamEnd.x,   by = beamEnd.y;
	double abx = bx - ax, aby = by - ay;
	double acx = cx - ax, acy = cy - ay;

	double ab2 = abx * abx + aby * aby;
	if (ab2 < 0.001) return false;

	double t = (acx * abx + acy * aby) / ab2;
	if (t < 0.0) t = 0.0;
	if (t > 1.0) t = 1.0;

	double closestX = ax + abx * t;
	double closestY = ay + aby * t;

	double dx = cx - closestX;
	double dy = cy - closestY;
	double dist = sqrt(dx * dx + dy * dy);

	return dist < (BEAM_HALF_WIDTH + targetHalfSize);
}

void Sub_Disintegrate_deactivate_all(void)
{
	channeling = false;
	wasChanneling = false;
	beamLength = 0.0;
}

float Sub_Disintegrate_get_cooldown_fraction(void)
{
	/* Channeled weapon — no cooldown */
	return 0.0f;
}
