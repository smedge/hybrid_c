#include "fragment.h"

#include "graphics.h"
#include "render.h"
#include "view.h"
#include "ship.h"
#include "text.h"
#include "mat4.h"
#include "audio.h"
#include "progression.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FRAGMENTS 128
#define LIFETIME_MS 10000
#define FADE_MS 2000
#define ATTRACT_RADIUS 300.0
#define COLLECT_RADIUS 25.0
#define ATTRACT_DURATION_MS 800.0
#define ATTRACT_MAX_SPEED 2000.0
#define DRIFT_DAMPEN 0.97
#define GLOW_WIDTH 20.0f
#define GLOW_HEIGHT 10.0f

typedef enum {
	FRAG_IDLE,
	FRAG_ATTRACTING
} FragmentState;

typedef struct {
	bool active;
	Position position;
	double vel_x;
	double vel_y;
	FragmentType type;
	FragmentState state;
	ColorFloat color;
	char binaryText[9];
	unsigned int ticksAlive;
	unsigned int ticksAttracting;
	unsigned int marqueeTimer;
	float alpha;
} Fragment;

typedef struct {
	ColorFloat color;
} FragmentTypeInfo;

static const FragmentTypeInfo typeInfo[FRAG_TYPE_COUNT] = {
	[FRAG_TYPE_MINE]   = {{1.0f, 0.0f, 1.0f, 1.0f}},
	[FRAG_TYPE_ELITE]  = {{1.0f, 0.84f, 0.0f, 1.0f}},
	[FRAG_TYPE_HUNTER] = {{1.0f, 0.4f, 0.0f, 1.0f}},
	[FRAG_TYPE_SEEKER] = {{0.0f, 1.0f, 0.2f, 1.0f}},
	[FRAG_TYPE_MEND]   = {{0.3f, 0.7f, 1.0f, 1.0f}},
	[FRAG_TYPE_AEGIS]  = {{0.6f, 0.9f, 1.0f, 1.0f}},
	[FRAG_TYPE_GRAVWELL] = {{0.0f, 0.0f, 0.0f, 0.0f}},
	[FRAG_TYPE_STALKER] = {{0.5f, 0.0f, 0.8f, 1.0f}},
	[FRAG_TYPE_INFERNO] = {{1.0f, 0.3f, 0.0f, 1.0f}},
	[FRAG_TYPE_DISINTEGRATE] = {{0.6f, 0.0f, 1.0f, 1.0f}},
	[FRAG_TYPE_TGUN] = {{0.1f, 0.1f, 1.0f, 1.0f}},
};

static Fragment fragments[MAX_FRAGMENTS];
static int collectionCounts[FRAG_TYPE_COUNT];
static Mix_Chunk *collectSample = 0;
static Mix_Chunk *unlockSample = 0;

static void generate_binary_string(char *out)
{
	for (int i = 0; i < 8; i++)
		out[i] = ((rand() >> 8) & 1) ? '1' : '0';
	out[8] = '\0';
}

void Fragment_initialize(void)
{
	for (int i = 0; i < MAX_FRAGMENTS; i++)
		fragments[i].active = false;

	memset(collectionCounts, 0, sizeof(collectionCounts));

	Audio_load_sample(&collectSample, "resources/sounds/samus_pickup.wav");
	Audio_load_sample(&unlockSample, "resources/sounds/samus_pickup2.wav");
}

void Fragment_cleanup(void)
{
	for (int i = 0; i < MAX_FRAGMENTS; i++)
		fragments[i].active = false;

	Audio_unload_sample(&collectSample);
	Audio_unload_sample(&unlockSample);
}

void Fragment_spawn(Position position, FragmentType type)
{
	if (type < 0 || type >= FRAG_TYPE_COUNT) return;

	/* Find inactive slot */
	int slot = -1;
	for (int i = 0; i < MAX_FRAGMENTS; i++) {
		if (!fragments[i].active) {
			slot = i;
			break;
		}
	}
	if (slot < 0)
		slot = 0; /* reuse oldest if all full */

	Fragment *f = &fragments[slot];
	f->active = true;
	f->position = position;
	f->vel_x = ((rand() % 80) - 40);  /* -40 to +40 */
	f->vel_y = 20.0 + ((rand() % 20) - 10);  /* 10 to 30 */
	f->type = type;
	f->state = FRAG_IDLE;
	f->color = typeInfo[type].color;
	f->ticksAlive = 0;
	f->ticksAttracting = 0;
	f->marqueeTimer = 0;
	f->alpha = 1.0f;

	generate_binary_string(f->binaryText);
}

void Fragment_update(unsigned int ticks)
{
	Position shipPos = Ship_get_position();
	double dt = ticks / 1000.0;

	for (int i = 0; i < MAX_FRAGMENTS; i++) {
		Fragment *f = &fragments[i];
		if (!f->active)
			continue;

		f->ticksAlive += ticks;

		/* Marquee: rotate binary text right every 333ms */
		f->marqueeTimer += ticks;
		while (f->marqueeTimer >= 333) {
			f->marqueeTimer -= 333;
			char last = f->binaryText[7];
			for (int j = 7; j > 0; j--)
				f->binaryText[j] = f->binaryText[j - 1];
			f->binaryText[0] = last;
		}

		/* Expire after lifetime */
		if (f->ticksAlive >= LIFETIME_MS) {
			f->active = false;
			continue;
		}

		/* Fade in last 2 seconds */
		unsigned int remaining = LIFETIME_MS - f->ticksAlive;
		if (remaining < FADE_MS)
			f->alpha = (float)remaining / FADE_MS;
		else
			f->alpha = 1.0f;

		/* Distance to ship */
		double dx = shipPos.x - f->position.x;
		double dy = shipPos.y - f->position.y;
		double dist = sqrt(dx * dx + dy * dy);

		if (f->state == FRAG_IDLE) {
			/* Drift */
			f->vel_x *= DRIFT_DAMPEN;
			f->position.x += f->vel_x * dt;
			f->position.y += f->vel_y * dt;

			/* Check attract range */
			if (dist < ATTRACT_RADIUS) {
				f->state = FRAG_ATTRACTING;
				f->ticksAttracting = 0;
			}
		} else {
			/* ATTRACTING */
			f->ticksAttracting += ticks;
			double t = f->ticksAttracting / ATTRACT_DURATION_MS;
			if (t > 1.0) t = 1.0;

			double speed = ATTRACT_MAX_SPEED * t * t;

			if (dist > 0.001) {
				double nx = dx / dist;
				double ny = dy / dist;
				f->vel_x = nx * speed;
				f->vel_y = ny * speed;
			}

			f->position.x += f->vel_x * dt;
			f->position.y += f->vel_y * dt;

			/* Check collection */
			/* Recompute distance after movement */
			dx = shipPos.x - f->position.x;
			dy = shipPos.y - f->position.y;
			dist = sqrt(dx * dx + dy * dy);

			if (dist < COLLECT_RADIUS) {
				f->active = false;
				int newCount = collectionCounts[f->type] + 1;
				collectionCounts[f->type] = newCount;
				if (Progression_would_complete(f->type, newCount))
					Audio_play_sample(&unlockSample);
				else
					Audio_play_sample(&collectSample);
			}
		}
	}
}

void Fragment_render(void)
{
	for (int i = 0; i < MAX_FRAGMENTS; i++) {
		Fragment *f = &fragments[i];
		if (!f->active)
			continue;

		float a = f->alpha;
		float hw = GLOW_WIDTH * 0.5f;
		float hh = GLOW_HEIGHT * 0.5f;

		/* Bright colored quad */
		Render_quad_absolute(
			(float)f->position.x - hw, (float)f->position.y - hh,
			(float)f->position.x + hw, (float)f->position.y + hh,
			f->color.red, f->color.green, f->color.blue, a * 0.8f);

		/* Center point for extra brightness */
		ColorFloat ptColor = {f->color.red, f->color.green, f->color.blue, a};
		Render_point(&f->position, 4.0, &ptColor);
	}
}

void Fragment_render_text(const Screen *screen)
{
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	View view = View_get_view();
	float vs = (float)view.scale;
	float ts = vs * 2.0f;

	for (int i = 0; i < MAX_FRAGMENTS; i++) {
		Fragment *f = &fragments[i];
		if (!f->active)
			continue;

		/* World to screen conversion (use real view scale) */
		float sx = (float)(f->position.x - view.position.x) * vs
			+ screen->width / 2.0f;
		float sy = screen->height / 2.0f
			- (float)(f->position.y - view.position.y) * vs;

		/* Scale text with zoom: translate origin to screen pos, scale, translate back */
		Mat4 t1 = Mat4_translate(sx, sy, 0.0f);
		Mat4 sc = Mat4_scale(ts, ts, 1.0f);
		Mat4 t2 = Mat4_translate(-sx, -sy, 0.0f);
		Mat4 tmp = Mat4_multiply(&t1, &sc);
		Mat4 textView = Mat4_multiply(&tmp, &t2);

		/* Center text roughly (8 chars * ~7px per char = ~56px, offset by half) */
		float textX = sx - 28.0f;
		float textY = sy - 5.0f;

		Text_render(tr, shaders, &proj, &textView,
			f->binaryText, textX, textY,
			f->color.red, f->color.green, f->color.blue, f->alpha * 0.9f);
	}
}

void Fragment_render_bloom_source(void)
{
	for (int i = 0; i < MAX_FRAGMENTS; i++) {
		Fragment *f = &fragments[i];
		if (!f->active)
			continue;

		float a = f->alpha;
		float hw = GLOW_WIDTH * 0.5f;
		float hh = GLOW_HEIGHT * 0.5f;

		Render_quad_absolute(
			(float)f->position.x - hw, (float)f->position.y - hh,
			(float)f->position.x + hw, (float)f->position.y + hh,
			f->color.red, f->color.green, f->color.blue, a);

		ColorFloat ptColor = {f->color.red, f->color.green, f->color.blue, a};
		Render_point(&f->position, 4.0, &ptColor);
	}
}

void Fragment_deactivate_all(void)
{
	for (int i = 0; i < MAX_FRAGMENTS; i++)
		fragments[i].active = false;
}

int Fragment_get_count(FragmentType type)
{
	if (type >= 0 && type < FRAG_TYPE_COUNT)
		return collectionCounts[type];
	return 0;
}

void Fragment_set_count(FragmentType type, int count)
{
	if (type >= 0 && type < FRAG_TYPE_COUNT)
		collectionCounts[type] = count;
}
