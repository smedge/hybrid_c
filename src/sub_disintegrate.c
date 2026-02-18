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
#include <stdlib.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --- Beam mechanics --- */
#define BEAM_MAX_RANGE    2600.0
#define BEAM_HALF_WIDTH   12.0
#define CARVE_SPEED_DEG   120.0
#define FEEDBACK_PER_SEC  22.0
#define DAMAGE_PER_FRAME  15.0
#define SOUND_INTERVAL_MS 150
#define BEAM_ORIGIN_OFFSET 10.0

/* --- Blob visual rendering --- */
#define BEAM_BLOB_COUNT     80
#define BEAM_BLOB_BASE_SIZE 14.0f
#define BEAM_SCROLL_SPEED   3500.0f   /* units/sec — blob flow along beam */
#define WIDTH_PULSE_SPEED   40.0f     /* rad/sec — ~6Hz for 100-160ms cycle */
#define WIDTH_PULSE_AMOUNT  0.12f     /* ±12% size variation */
#define LATERAL_JITTER_MAX  3.0f      /* max perpendicular offset */
#define TIP_FADE_LENGTH     120.0f    /* world units for tip dissipation */
#define TIP_FLICKER_SPEED   25.0f

typedef struct {
	float rotation;
	float sizeScale;      /* 0.7 - 1.3 */
	bool mirror;
	float lateralOffset;  /* ±LATERAL_JITTER_MAX perpendicular to beam */
} BlobProp;

static Entity *parent;
static bool channeling;
static bool wasChanneling;
static double beamAngle;      /* degrees, 0 = north */
static Position beamStart;
static Position beamEnd;
static double beamLength;
static int soundTimer;

static BlobProp blobProps[BEAM_BLOB_COUNT];
static float beamScrollOffset;
static float widthPulseTimer;

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
	soundTimer = 0;
	beamScrollOffset = 0.0f;
	widthPulseTimer = 0.0f;

	/* Pre-generate random visual properties for each blob slot */
	for (int i = 0; i < BEAM_BLOB_COUNT; i++) {
		blobProps[i].rotation = (float)(rand() % 360);
		blobProps[i].sizeScale = 0.7f + (float)(rand() % 600) / 1000.0f;
		blobProps[i].mirror = (rand() % 2) == 0;
		blobProps[i].lateralOffset = ((float)(rand() % 1000) / 500.0f - 1.0f)
			* LATERAL_JITTER_MAX;
	}

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

		/* Scroll blobs along beam + width pulse */
		beamScrollOffset += (float)(dt * BEAM_SCROLL_SPEED);
		widthPulseTimer += (float)(dt * WIDTH_PULSE_SPEED);
		if (widthPulseTimer > 2.0f * (float)M_PI)
			widthPulseTimer -= 2.0f * (float)M_PI;
	} else {
		soundTimer = 0;
		beamLength = 0.0;
	}

	wasChanneling = channeling;

}

/* Shared blob rendering logic used by both render and bloom source.
   bloomBoost > 1.0 for bloom pass brightness, 1.0 for normal render. */
static void render_beam_blobs(float bloomBoost, float sizeBoost)
{
	float len = (float)beamLength;
	float sx = (float)beamStart.x;
	float sy = (float)beamStart.y;
	float ex = (float)beamEnd.x;
	float ey = (float)beamEnd.y;

	/* Direction + perpendicular unit vectors */
	float dirX = (ex - sx) / len;
	float dirY = (ey - sy) / len;
	float perpX = -dirY;
	float perpY = dirX;

	/* Width pulse: subtle breathing ~6Hz */
	float pulseMul = 1.0f + WIDTH_PULSE_AMOUNT * sinf(widthPulseTimer);

	float spacing = len / BEAM_BLOB_COUNT;
	if (spacing < 1.0f) spacing = 1.0f;
	/* Scroll wraps per spacing so blob properties cycle smoothly */
	float scrollFrac = fmodf(beamScrollOffset, spacing);
	int scrollIdx = (int)(beamScrollOffset / spacing);

	for (int i = 0; i < BEAM_BLOB_COUNT; i++) {
		float dist = (float)i * spacing + scrollFrac;
		if (dist > len || dist < 0.0f) continue;

		/* Index into pre-generated properties — shifts with scroll for organic motion */
		int propIdx = (unsigned int)(i + scrollIdx) % BEAM_BLOB_COUNT;
		BlobProp *bp = &blobProps[propIdx];

		float size = BEAM_BLOB_BASE_SIZE * bp->sizeScale * pulseMul * sizeBoost;

		/* Tip fade: last TIP_FADE_LENGTH units shrink + fade */
		float tipFade = 1.0f;
		float tipDist = len - dist;
		if (tipDist < TIP_FADE_LENGTH) {
			tipFade = tipDist / TIP_FADE_LENGTH;
			/* Per-blob flicker near tip */
			float flicker = 0.7f + 0.3f * sinf(
				widthPulseTimer * TIP_FLICKER_SPEED + (float)propIdx * 2.3f);
			tipFade *= flicker;
			if (tipFade < 0.0f) tipFade = 0.0f;
			size *= (0.4f + 0.6f * tipFade);
		}

		/* Position along beam + lateral jitter */
		Position blobPos;
		blobPos.x = sx + dirX * dist + perpX * bp->lateralOffset;
		blobPos.y = sy + dirY * dist + perpY * bp->lateralOffset;

		float angle2 = bp->mirror ? bp->rotation - 55.0f : bp->rotation + 55.0f;

		/* Layer 1: purple outer glow */
		float outerSz = size * 1.4f;
		Rectangle outerRect = {-outerSz, outerSz, outerSz, -outerSz};
		float pr = 0.6f * bloomBoost, pg = 0.2f * bloomBoost, pb = 0.9f * bloomBoost;
		ColorFloat outerColor = {pr, pg, pb, 0.25f * tipFade};
		Render_quad(&blobPos, bp->rotation, outerRect, &outerColor);

		/* Layer 2: light purple mid (mirrored quad like inferno) */
		float midSz = size * 0.8f;
		Rectangle midRect = {-midSz, midSz, midSz, -midSz};
		float mr = 0.8f * bloomBoost, mg = 0.5f * bloomBoost, mb = 1.0f * bloomBoost;
		ColorFloat midColor = {mr, mg, mb, 0.7f * tipFade};
		Render_quad(&blobPos, angle2, midRect, &midColor);

		/* Layer 3: white-hot core */
		float coreSz = size * 0.35f;
		Rectangle coreRect = {-coreSz, coreSz, coreSz, -coreSz};
		float cr = 1.0f * bloomBoost, cg = 0.95f * bloomBoost, cb = 1.0f * bloomBoost;
		ColorFloat coreColor = {cr, cg, cb, 0.95f * tipFade};
		Render_quad(&blobPos, bp->rotation + 22.5f, coreRect, &coreColor);
	}
}

void Sub_Disintegrate_render(void)
{
	if (!channeling || beamLength < 1.0)
		return;
	render_beam_blobs(1.0f, 1.0f);
}

void Sub_Disintegrate_render_bloom_source(void)
{
	if (!channeling || beamLength < 1.0)
		return;
	render_beam_blobs(1.5f, 1.2f);
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
