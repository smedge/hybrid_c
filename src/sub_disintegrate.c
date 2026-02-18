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
#include "particle_instance.h"

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
#define WIDTH_PULSE_AMOUNT  0.12f     /* +/-12% size variation */
#define LATERAL_JITTER_MAX  3.0f      /* max perpendicular offset */
#define TIP_FADE_LENGTH     120.0f    /* world units for tip dissipation */
#define TIP_FLICKER_SPEED   25.0f

/* --- Wall splash particles --- */
#define SPLASH_COUNT        64
#define SPLASH_SPAWN_MS     8       /* spawn interval (~125/sec while hitting wall) */
#define SPLASH_TTL_BASE     200     /* base lifetime ms */
#define SPLASH_TTL_VARY     100     /* +/-random variation */
#define SPLASH_SPEED        900.0   /* units/sec outward */
#define SPLASH_BASE_SIZE    13.0f
#define SPLASH_SPREAD_DEG   200.0   /* wide cone — wraps around impact point */

#define MAX_INSTANCES ((BEAM_BLOB_COUNT + SPLASH_COUNT) * 3)

typedef struct {
	float rotation;
	float sizeScale;      /* 0.7 - 1.3 */
	bool mirror;
	float lateralOffset;  /* +/-LATERAL_JITTER_MAX perpendicular to beam */
} BlobProp;

typedef struct {
	bool active;
	Position position;
	double vx, vy;
	int age;
	int ttl;
	float rotation;
	float sizeScale;
	bool mirror;
} SplashParticle;

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

static SplashParticle splash[SPLASH_COUNT]; /* 64 */
static bool beamHitWall;
static int splashSpawnTimer;

static ParticleInstanceData instances[MAX_INSTANCES];

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
	beamHitWall = false;
	splashSpawnTimer = 0;
	for (int i = 0; i < SPLASH_COUNT; i++)
		splash[i].active = false;

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
	ParticleInstance_cleanup();
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
		beamHitWall = Map_line_test_hit(beamStart.x, beamStart.y, endX, endY, &hx, &hy);
		if (beamHitWall) {
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

		/* Spawn splash particles when beam hits a wall */
		if (beamHitWall) {
			splashSpawnTimer -= (int)ticks;
			while (splashSpawnTimer <= 0) {
				splashSpawnTimer += SPLASH_SPAWN_MS;

				/* Find inactive slot or recycle oldest */
				int slot = -1;
				for (int si = 0; si < SPLASH_COUNT; si++) {
					if (!splash[si].active) { slot = si; break; }
				}
				if (slot < 0) {
					int oldest = 0;
					for (int si = 1; si < SPLASH_COUNT; si++) {
						if (splash[si].age > splash[oldest].age)
							oldest = si;
					}
					slot = oldest;
				}

				SplashParticle *sp = &splash[slot];
				sp->active = true;
				sp->age = 0;
				sp->ttl = SPLASH_TTL_BASE
					+ (rand() % (SPLASH_TTL_VARY * 2 + 1)) - SPLASH_TTL_VARY;
				if (sp->ttl < 50) sp->ttl = 50;
				sp->position = beamEnd;

				/* Spray in a cone away from beam direction */
				double reverseAngle = beamAngle + 180.0;
				double spread = ((rand() % 1000) / 1000.0 - 0.5)
					* SPLASH_SPREAD_DEG;
				double sprayRad = deg_to_rad(reverseAngle + spread);
				double spd = SPLASH_SPEED * (0.5 + (rand() % 1000) / 1000.0);
				sp->vx = sin(sprayRad) * spd;
				sp->vy = cos(sprayRad) * spd;

				sp->rotation = (float)(rand() % 360);
				sp->sizeScale = 0.6f + (float)(rand() % 800) / 1000.0f;
				sp->mirror = (rand() % 2) == 0;
			}
		} else {
			splashSpawnTimer = 0;
		}
	} else {
		soundTimer = 0;
		beamLength = 0.0;
		beamHitWall = false;
	}

	/* Update splash particles (even when not channeling — let existing ones fade) */
	for (int i = 0; i < SPLASH_COUNT; i++) {
		SplashParticle *sp = &splash[i];
		if (!sp->active) continue;
		sp->age += ticks;
		if (sp->age > sp->ttl) {
			sp->active = false;
			continue;
		}
		sp->position.x += sp->vx * dt;
		sp->position.y += sp->vy * dt;
	}

	wasChanneling = channeling;

}

/* Fill beam blob instances, returns count added */
static int fill_beam_instances(ParticleInstanceData *out, int start,
	float bloomBoost, float sizeBoost)
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

	int count = start;
	for (int i = 0; i < BEAM_BLOB_COUNT; i++) {
		float dist = (float)i * spacing + scrollFrac;
		if (dist > len || dist < 0.0f) continue;

		/* Index into pre-generated properties — shifts with scroll for organic motion */
		int propIdx = (unsigned int)(i + scrollIdx) % BEAM_BLOB_COUNT;
		BlobProp *bp = &blobProps[propIdx];

		float size = BEAM_BLOB_BASE_SIZE * bp->sizeScale * pulseMul * sizeBoost;

		/* Tip fade: last TIP_FADE_LENGTH units shrink + fade (only at max range, not wall hits) */
		float tipFade = 1.0f;
		float tipDist = len - dist;
		if (!beamHitWall && tipDist < TIP_FADE_LENGTH) {
			tipFade = tipDist / TIP_FADE_LENGTH;
			/* Per-blob flicker near tip */
			float flicker = 0.7f + 0.3f * sinf(
				widthPulseTimer * TIP_FLICKER_SPEED + (float)propIdx * 2.3f);
			tipFade *= flicker;
			if (tipFade < 0.0f) tipFade = 0.0f;
			size *= (0.4f + 0.6f * tipFade);
		}

		/* Position along beam + lateral jitter */
		float px = sx + dirX * dist + perpX * bp->lateralOffset;
		float py = sy + dirY * dist + perpY * bp->lateralOffset;

		float angle2 = bp->mirror ? bp->rotation - 55.0f : bp->rotation + 55.0f;

		/* Layer 1: purple outer glow */
		ParticleInstanceData *inst = &out[count++];
		inst->x = px;
		inst->y = py;
		inst->size = size * 1.4f;
		inst->rotation = bp->rotation;
		inst->stretch = 1.0f;
		inst->r = 0.6f * bloomBoost;
		inst->g = 0.2f * bloomBoost;
		inst->b = 0.9f * bloomBoost;
		inst->a = 0.25f * tipFade;

		/* Layer 2: light purple mid (mirrored quad) */
		inst = &out[count++];
		inst->x = px;
		inst->y = py;
		inst->size = size * 0.8f;
		inst->rotation = angle2;
		inst->stretch = 1.0f;
		inst->r = 0.8f * bloomBoost;
		inst->g = 0.5f * bloomBoost;
		inst->b = 1.0f * bloomBoost;
		inst->a = 0.7f * tipFade;

		/* Layer 3: white-hot core */
		inst = &out[count++];
		inst->x = px;
		inst->y = py;
		inst->size = size * 0.35f;
		inst->rotation = bp->rotation + 22.5f;
		inst->stretch = 1.0f;
		inst->r = 1.0f * bloomBoost;
		inst->g = 0.95f * bloomBoost;
		inst->b = 1.0f * bloomBoost;
		inst->a = 0.95f * tipFade;

		if (count >= MAX_INSTANCES) return count;
	}
	return count;
}

/* Fill splash particle instances, returns count (total from start) */
static int fill_splash_instances(ParticleInstanceData *out, int start,
	float bloomBoost)
{
	int count = start;
	for (int i = 0; i < SPLASH_COUNT; i++) {
		SplashParticle *sp = &splash[i];
		if (!sp->active) continue;

		float t = (float)sp->age / (float)sp->ttl; /* 0 = young, 1 = dead */
		float fade = 1.0f - t;
		if (fade <= 0.01f) continue;

		float size = SPLASH_BASE_SIZE * sp->sizeScale * (1.0f - t * 0.3f);
		float angle2 = sp->mirror ? sp->rotation - 55.0f : sp->rotation + 55.0f;
		float px = (float)sp->position.x;
		float py = (float)sp->position.y;

		/* Purple outer — big and hot */
		ParticleInstanceData *inst = &out[count++];
		inst->x = px;
		inst->y = py;
		inst->size = size * 1.6f;
		inst->rotation = sp->rotation;
		inst->stretch = 1.0f;
		inst->r = 0.6f * bloomBoost;
		inst->g = 0.2f * bloomBoost;
		inst->b = 0.9f * bloomBoost;
		inst->a = 0.4f * fade;

		/* Light purple mid */
		inst = &out[count++];
		inst->x = px;
		inst->y = py;
		inst->size = size * 0.85f;
		inst->rotation = angle2;
		inst->stretch = 1.0f;
		inst->r = 0.8f * bloomBoost;
		inst->g = 0.5f * bloomBoost;
		inst->b = 1.0f * bloomBoost;
		inst->a = 0.8f * fade;

		/* White-hot core */
		inst = &out[count++];
		inst->x = px;
		inst->y = py;
		inst->size = size * 0.4f;
		inst->rotation = sp->rotation + 22.5f;
		inst->stretch = 1.0f;
		inst->r = 1.0f * bloomBoost;
		inst->g = 0.95f * bloomBoost;
		inst->b = 1.0f * bloomBoost;
		inst->a = 0.95f * fade;

		if (count >= MAX_INSTANCES) return count;
	}
	return count;
}

void Sub_Disintegrate_render(void)
{
	int count = 0;
	if (channeling && beamLength >= 1.0)
		count = fill_beam_instances(instances, count, 1.0f, 1.0f);
	count = fill_splash_instances(instances, count, 1.0f);

	if (count <= 0) return;

	Mat4 world_proj = Graphics_get_world_projection();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);
	Render_flush(&world_proj, &view);

	ParticleInstance_draw(instances, count, &world_proj, &view, false);
}

void Sub_Disintegrate_render_bloom_source(void)
{
	int count = 0;
	if (channeling && beamLength >= 1.0)
		count = fill_beam_instances(instances, count, 1.5f, 1.2f);
	count = fill_splash_instances(instances, count, 1.5f);

	if (count <= 0) return;

	Mat4 world_proj = Graphics_get_world_projection();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);
	Render_flush(&world_proj, &view);

	ParticleInstance_draw(instances, count, &world_proj, &view, false);
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

bool Sub_Disintegrate_check_nearby(Position pos, double radius)
{
	if (!channeling || beamLength < 1.0)
		return false;

	/* Point-to-line-segment distance: pos vs beam centerline */
	double ax = beamStart.x, ay = beamStart.y;
	double bx = beamEnd.x,   by = beamEnd.y;
	double abx = bx - ax, aby = by - ay;
	double acx = pos.x - ax, acy = pos.y - ay;

	double ab2 = abx * abx + aby * aby;
	if (ab2 < 0.001) return false;

	double t = (acx * abx + acy * aby) / ab2;
	if (t < 0.0) t = 0.0;
	if (t > 1.0) t = 1.0;

	double closestX = ax + abx * t;
	double closestY = ay + aby * t;
	double dx = pos.x - closestX;
	double dy = pos.y - closestY;

	return (dx * dx + dy * dy) < (radius * radius);
}

void Sub_Disintegrate_deactivate_all(void)
{
	channeling = false;
	wasChanneling = false;
	beamLength = 0.0;
	beamHitWall = false;
	for (int i = 0; i < SPLASH_COUNT; i++)
		splash[i].active = false;
}

float Sub_Disintegrate_get_cooldown_fraction(void)
{
	/* Channeled weapon — no cooldown */
	return 0.0f;
}
