#include "sub_inferno.h"

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

#define BLOB_COUNT 256
#define SPAWN_INTERVAL_MS 8
#define BLOB_SPEED 2500.0
#define BLOB_TTL 500
#define BLOB_BASE_SIZE 18.0f
#define SPREAD_DEGREES 5.0
#define FEEDBACK_PER_SEC 25.0
#define SOUND_INTERVAL_MS 150

typedef struct {
	bool active;
	Position position;
	Position prevPosition;
	double vx, vy;
	int age;
	float rotation;     /* random orientation per blob */
	float sizeScale;    /* random size variation 0.7 - 1.3 */
	bool mirror;        /* flip secondary quad angle */
} Blob;

static Entity *parent;
static Blob blobs[BLOB_COUNT];
static bool channeling;
static int spawnTimer;
static int soundTimer;
static bool wasChanneling;

static Mix_Chunk *sampleFire = 0;

static double get_radians(double degrees)
{
	return degrees * M_PI / 180.0;
}

/* Irregular blob color based on age ratio (0 = young/white, 1 = old/red) */
static void blob_color(float t, float *r, float *g, float *b, float *a)
{
	if (t < 0.25f) {
		/* White hot */
		float f = t / 0.25f;
		*r = 1.0f;
		*g = 1.0f;
		*b = 1.0f - f * 0.7f;
		*a = 1.0f;
	} else if (t < 0.55f) {
		/* Bright yellow */
		float f = (t - 0.25f) / 0.3f;
		*r = 1.0f;
		*g = 1.0f - f * 0.3f;
		*b = 0.3f - f * 0.2f;
		*a = 1.0f;
	} else if (t < 0.8f) {
		/* Orange */
		float f = (t - 0.55f) / 0.25f;
		*r = 1.0f;
		*g = 0.7f - f * 0.3f;
		*b = 0.1f;
		*a = 1.0f;
	} else {
		/* Dark orange/red, fading out */
		float f = (t - 0.8f) / 0.2f;
		*r = 1.0f - f * 0.3f;
		*g = 0.4f - f * 0.3f;
		*b = 0.1f - f * 0.05f;
		*a = 1.0f - f * 0.8f;
	}
}

static void render_blob(const Blob *bl)
{
	float t = (float)bl->age / BLOB_TTL;
	float r, g, b, a;
	blob_color(t, &r, &g, &b, &a);
	if (a <= 0.01f) return;

	ColorFloat c = {r, g, b, a};
	float sz = BLOB_BASE_SIZE * bl->sizeScale;

	/* Primary quad — larger */
	Rectangle rect1 = {-sz, sz, sz, -sz};
	Render_quad(&bl->position, bl->rotation, rect1, &c);

	/* Secondary quad — smaller, rotated asymmetrically */
	float sz2 = sz * 0.6f;
	Rectangle rect2 = {-sz2, sz2, sz2, -sz2};
	float angle2 = bl->mirror ? bl->rotation - 55.0f : bl->rotation + 55.0f;
	Render_quad(&bl->position, angle2, rect2, &c);
}

static void render_blob_bloom(const Blob *bl)
{
	float t = (float)bl->age / BLOB_TTL;
	float r, g, b, a;
	blob_color(t, &r, &g, &b, &a);
	if (a <= 0.01f) return;

	/* Render at 1.5x brightness for extra bloom intensity */
	float boost = 1.5f;
	ColorFloat c = {r * boost, g * boost, b * boost, a};
	float sz = BLOB_BASE_SIZE * bl->sizeScale * 1.2f; /* Slightly larger for bloom spread */

	Rectangle rect1 = {-sz, sz, sz, -sz};
	Render_quad(&bl->position, bl->rotation, rect1, &c);

	float sz2 = sz * 0.6f;
	Rectangle rect2 = {-sz2, sz2, sz2, -sz2};
	float angle2 = bl->mirror ? bl->rotation - 55.0f : bl->rotation + 55.0f;
	Render_quad(&bl->position, angle2, rect2, &c);
}

void Sub_Inferno_initialize(Entity *p)
{
	parent = p;
	channeling = false;
	wasChanneling = false;
	spawnTimer = 0;
	soundTimer = 0;

	for (int i = 0; i < BLOB_COUNT; i++)
		blobs[i].active = false;

	Audio_load_sample(&sampleFire, "resources/sounds/bomb_explode.wav");
}

void Sub_Inferno_cleanup(void)
{
	Audio_unload_sample(&sampleFire);
}

void Sub_Inferno_update(const Input *userInput, unsigned int ticks, PlaceableComponent *placeable)
{
	ShipState *state = (ShipState *)parent->state;
	double dt = ticks / 1000.0;

	channeling = userInput->mouseLeft && !state->destroyed
		&& Skillbar_is_active(SUB_ID_INFERNO);

	if (channeling) {
		/* Break stealth on first frame of channeling */
		if (!wasChanneling)
			Sub_Stealth_break_attack();

		/* Feedback drain */
		PlayerStats_add_feedback(FEEDBACK_PER_SEC * dt);

		/* Looping fire sound */
		soundTimer -= (int)ticks;
		if (soundTimer <= 0) {
			Audio_play_sample(&sampleFire);
			soundTimer = SOUND_INTERVAL_MS;
		}

		/* Compute aim direction */
		Position cursor = {userInput->mouseX, userInput->mouseY};
		Screen screen = Graphics_get_screen();
		Position cursorWorld = View_get_world_position(&screen, cursor);
		double heading = Position_get_heading(placeable->position, cursorWorld);

		/* Spawn blobs */
		spawnTimer -= (int)ticks;
		while (spawnTimer <= 0) {
			spawnTimer += SPAWN_INTERVAL_MS;

			/* Find inactive slot */
			int slot = -1;
			for (int i = 0; i < BLOB_COUNT; i++) {
				if (!blobs[i].active) {
					slot = i;
					break;
				}
			}
			if (slot < 0) {
				/* Recycle oldest */
				int oldest = 0;
				for (int i = 1; i < BLOB_COUNT; i++) {
					if (blobs[i].age > blobs[oldest].age)
						oldest = i;
				}
				slot = oldest;
			}

			Blob *bl = &blobs[slot];
			bl->active = true;
			bl->age = 0;
			bl->position = placeable->position;
			bl->prevPosition = bl->position;

			/* Spread: random offset ±SPREAD_DEGREES */
			double spread = ((rand() % 1000) / 1000.0 - 0.5) * 2.0 * SPREAD_DEGREES;
			double rad = get_radians(heading + spread);
			bl->vx = sin(rad) * BLOB_SPEED;
			bl->vy = cos(rad) * BLOB_SPEED;

			/* Random visual properties */
			bl->rotation = (float)(rand() % 360);
			bl->sizeScale = 0.7f + (float)(rand() % 600) / 1000.0f; /* 0.7 - 1.3 */
			bl->mirror = (rand() % 2) == 0;
		}
	} else {
		spawnTimer = 0;
		soundTimer = 0;
	}

	wasChanneling = channeling;

	/* Update active blobs */
	for (int i = 0; i < BLOB_COUNT; i++) {
		Blob *bl = &blobs[i];
		if (!bl->active)
			continue;

		bl->age += ticks;
		if (bl->age > BLOB_TTL) {
			bl->active = false;
			continue;
		}

		bl->prevPosition = bl->position;
		bl->position.x += bl->vx * dt;
		bl->position.y += bl->vy * dt;

		/* Wall collision — blob dies on impact */
		double hx, hy;
		if (Map_line_test_hit(bl->prevPosition.x, bl->prevPosition.y,
				bl->position.x, bl->position.y, &hx, &hy)) {
			bl->position.x = hx;
			bl->position.y = hy;
			bl->active = false;
		}
	}
}

void Sub_Inferno_render(void)
{
	for (int i = 0; i < BLOB_COUNT; i++) {
		if (blobs[i].active)
			render_blob(&blobs[i]);
	}
}

void Sub_Inferno_render_bloom_source(void)
{
	for (int i = 0; i < BLOB_COUNT; i++) {
		if (blobs[i].active)
			render_blob_bloom(&blobs[i]);
	}
}

bool Sub_Inferno_check_hit(Rectangle target)
{
	for (int i = 0; i < BLOB_COUNT; i++) {
		Blob *bl = &blobs[i];
		if (!bl->active)
			continue;
		/* Line-segment collision — piercing (blob stays active) */
		if (Collision_line_aabb_test(bl->prevPosition.x, bl->prevPosition.y,
				bl->position.x, bl->position.y, target, NULL))
			return true;
	}
	return false;
}

bool Sub_Inferno_check_nearby(Position pos, double radius)
{
	double r2 = radius * radius;
	for (int i = 0; i < BLOB_COUNT; i++) {
		Blob *bl = &blobs[i];
		if (!bl->active)
			continue;
		double dx = bl->position.x - pos.x;
		double dy = bl->position.y - pos.y;
		if (dx * dx + dy * dy < r2)
			return true;
	}
	return false;
}

void Sub_Inferno_deactivate_all(void)
{
	for (int i = 0; i < BLOB_COUNT; i++)
		blobs[i].active = false;
	channeling = false;
	wasChanneling = false;
}

float Sub_Inferno_get_cooldown_fraction(void)
{
	/* Channeled weapon — no cooldown, always ready */
	return 0.0f;
}
