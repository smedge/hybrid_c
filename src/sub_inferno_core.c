#include "sub_inferno_core.h"

#include "graphics.h"
#include "audio.h"
#include "map.h"
#include "render.h"
#include "view.h"
#include "particle_instance.h"

#include <math.h>
#include <stdlib.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --- Shared config singleton --- */

static const SubInfernoConfig infernoConfig = {
	.spawn_interval_ms = 8,
	.blob_speed        = 2500.0,
	.blob_ttl_ms       = 500,
	.base_size         = 18.0f,
	.spread_degrees    = 5.0,
	.sound_interval_ms = 250,
};

const SubInfernoConfig *SubInferno_get_config(void)
{
	return &infernoConfig;
}

/* --- Audio (global, not per-state) --- */

static Mix_Chunk *sampleFire = NULL;
static int audioRefCount = 0;

void SubInfernoCore_init_audio(void)
{
	if (audioRefCount == 0)
		Audio_load_sample(&sampleFire, "resources/sounds/bomb_explode.wav");
	audioRefCount++;
}

void SubInfernoCore_cleanup_audio(void)
{
	audioRefCount--;
	if (audioRefCount <= 0) {
		Audio_unload_sample(&sampleFire);
		audioRefCount = 0;
	}
}

/* --- Color helper --- */

static void blob_color(float t, float *r, float *g, float *b, float *a)
{
	if (t < 0.25f) {
		float f = t / 0.25f;
		*r = 1.0f;
		*g = 1.0f;
		*b = 1.0f - f * 0.7f;
		*a = 1.0f;
	} else if (t < 0.55f) {
		float f = (t - 0.25f) / 0.3f;
		*r = 1.0f;
		*g = 1.0f - f * 0.3f;
		*b = 0.3f - f * 0.2f;
		*a = 1.0f;
	} else if (t < 0.8f) {
		float f = (t - 0.55f) / 0.25f;
		*r = 1.0f;
		*g = 0.7f - f * 0.3f;
		*b = 0.1f;
		*a = 1.0f;
	} else {
		float f = (t - 0.8f) / 0.2f;
		*r = 1.0f - f * 0.3f;
		*g = 0.4f - f * 0.3f;
		*b = 0.1f - f * 0.05f;
		*a = 1.0f - f * 0.8f;
	}
}

/* --- Instance buffer (shared across sequential render calls) --- */

#define MAX_INSTANCES 512

static ParticleInstanceData instances[MAX_INSTANCES];

static int fill_inferno_instances(const InfernoBlob *blobs, int capacity,
	float base_size, float colorBoost, float sizeBoost)
{
	int count = 0;
	for (int i = 0; i < capacity; i++) {
		const InfernoBlob *bl = &blobs[i];
		if (!bl->active) continue;

		float t = (float)bl->age / bl->ttl;
		float r, g, b, a;
		blob_color(t, &r, &g, &b, &a);
		if (a <= 0.01f) continue;

		float sz = base_size * bl->sizeScale * sizeBoost;

		/* Primary quad */
		instances[count] = (ParticleInstanceData){
			(float)bl->position.x, (float)bl->position.y,
			sz, bl->rotation, 1.0f,
			r * colorBoost, g * colorBoost, b * colorBoost, a
		};
		count++;

		/* Secondary quad — smaller, rotated asymmetrically */
		instances[count] = (ParticleInstanceData){
			(float)bl->position.x, (float)bl->position.y,
			sz * 0.6f,
			bl->mirror ? bl->rotation - 55.0f : bl->rotation + 55.0f,
			1.0f,
			r * colorBoost, g * colorBoost, b * colorBoost, a
		};
		count++;

		if (count >= MAX_INSTANCES) break;
	}
	return count;
}

/* --- Per-state lifecycle --- */

void SubInfernoCore_init(SubInfernoCoreState *state, InfernoBlob *blob_array, int capacity)
{
	state->blobs = blob_array;
	state->blob_capacity = capacity;
	state->channeling = false;
	state->spawn_timer = 0;
	state->sound_timer = 0;
	state->origin = (Position){0, 0};
	state->aim_angle = 0.0;

	for (int i = 0; i < capacity; i++)
		blob_array[i].active = false;
}

void SubInfernoCore_set_origin(SubInfernoCoreState *state, Position pos)
{
	state->origin = pos;
}

void SubInfernoCore_set_aim_angle(SubInfernoCoreState *state, double radians)
{
	state->aim_angle = radians;
}

void SubInfernoCore_set_channeling(SubInfernoCoreState *state, bool active)
{
	state->channeling = active;
}

/* --- Update --- */

void SubInfernoCore_update(SubInfernoCoreState *state, const SubInfernoConfig *cfg, unsigned int ticks)
{
	if (!cfg) cfg = &infernoConfig;
	double dt = ticks / 1000.0;

	if (state->channeling) {
		/* Fire sound loop */
		state->sound_timer -= (int)ticks;
		if (state->sound_timer <= 0) {
			Audio_play_sample_at(&sampleFire, state->origin);
			state->sound_timer = cfg->sound_interval_ms;
		}

		/* Spawn blobs */
		state->spawn_timer -= (int)ticks;
		while (state->spawn_timer <= 0) {
			state->spawn_timer += cfg->spawn_interval_ms;

			/* Find inactive slot */
			int slot = -1;
			for (int i = 0; i < state->blob_capacity; i++) {
				if (!state->blobs[i].active) {
					slot = i;
					break;
				}
			}
			if (slot < 0) {
				/* Recycle oldest */
				int oldest = 0;
				for (int i = 1; i < state->blob_capacity; i++) {
					if (state->blobs[i].age > state->blobs[oldest].age)
						oldest = i;
				}
				slot = oldest;
			}

			InfernoBlob *bl = &state->blobs[slot];
			bl->active = true;
			bl->age = 0;
			bl->position = state->origin;
			bl->prevPosition = bl->position;

			/* Per-blob TTL: +/-30% */
			bl->ttl = cfg->blob_ttl_ms +
				(rand() % (cfg->blob_ttl_ms * 6 / 10)) - cfg->blob_ttl_ms * 3 / 10;

			/* Spread: random offset +/-spread_degrees */
			double spread = ((rand() % 1000) / 1000.0 - 0.5) * 2.0 * cfg->spread_degrees;
			double rad = state->aim_angle + spread * M_PI / 180.0;
			/* Speed variation: +/-15% */
			double speed = cfg->blob_speed * (0.85 + (rand() % 300) / 1000.0);
			bl->vx = sin(rad) * speed;
			bl->vy = cos(rad) * speed;

			bl->rotation = (float)(rand() % 360);
			bl->sizeScale = 0.7f + (float)(rand() % 600) / 1000.0f;
			bl->mirror = (rand() % 2) == 0;
		}
	} else {
		state->spawn_timer = 0;
		state->sound_timer = 0;
	}

	/* Update active blobs */
	for (int i = 0; i < state->blob_capacity; i++) {
		InfernoBlob *bl = &state->blobs[i];
		if (!bl->active) continue;

		bl->age += ticks;
		if (bl->age > bl->ttl) {
			bl->active = false;
			continue;
		}

		bl->prevPosition = bl->position;
		bl->position.x += bl->vx * dt;
		bl->position.y += bl->vy * dt;

		/* Wall collision — blob dies on impact.
		   Skip for very young blobs so turret beams can clear their own pillar. */
		if (bl->age > 50) {
			double hx, hy;
			if (Map_line_test_hit(bl->prevPosition.x, bl->prevPosition.y,
					bl->position.x, bl->position.y, &hx, &hy)) {
				bl->position.x = hx;
				bl->position.y = hy;
				bl->active = false;
			}
		}
	}
}

/* --- Rendering --- */

void SubInfernoCore_render(const SubInfernoCoreState *state, const SubInfernoConfig *cfg)
{
	if (!cfg) cfg = &infernoConfig;
	int count = fill_inferno_instances(state->blobs, state->blob_capacity,
		cfg->base_size, 1.0f, 1.0f);
	if (count <= 0) return;

	Mat4 world_proj = Graphics_get_world_projection();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);
	Render_flush(&world_proj, &view);

	ParticleInstance_draw(instances, count, &world_proj, &view, PI_SHAPE_SHARP);
}

void SubInfernoCore_render_bloom(const SubInfernoCoreState *state, const SubInfernoConfig *cfg)
{
	if (!cfg) cfg = &infernoConfig;
	int count = fill_inferno_instances(state->blobs, state->blob_capacity,
		cfg->base_size, 1.5f, 1.2f);
	if (count <= 0) return;

	Mat4 world_proj = Graphics_get_world_projection();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);
	Render_flush(&world_proj, &view);

	ParticleInstance_draw(instances, count, &world_proj, &view, PI_SHAPE_SHARP);
}

void SubInfernoCore_render_light(const SubInfernoCoreState *state)
{
	if (!state->channeling) return;

	for (int i = 0; i < state->blob_capacity; i += 8) {
		if (!state->blobs[i].active) continue;
		float age_frac = (float)state->blobs[i].age / (float)state->blobs[i].ttl;
		float alpha = (1.0f - age_frac) * 0.7f;
		Render_filled_circle(
			(float)state->blobs[i].position.x,
			(float)state->blobs[i].position.y,
			210.0f, 12, 1.0f, 0.6f, 0.15f, alpha);
	}
}

/* --- Hit detection --- */

bool SubInfernoCore_check_hit(const SubInfernoCoreState *state, Rectangle target)
{
	for (int i = 0; i < state->blob_capacity; i++) {
		const InfernoBlob *bl = &state->blobs[i];
		if (!bl->active) continue;
		if (Collision_line_aabb_test(bl->prevPosition.x, bl->prevPosition.y,
				bl->position.x, bl->position.y, target, NULL))
			return true;
	}
	return false;
}

bool SubInfernoCore_check_nearby(const SubInfernoCoreState *state, Position pos, double radius)
{
	double r2 = radius * radius;
	for (int i = 0; i < state->blob_capacity; i++) {
		const InfernoBlob *bl = &state->blobs[i];
		if (!bl->active) continue;
		double dx = bl->position.x - pos.x;
		double dy = bl->position.y - pos.y;
		if (dx * dx + dy * dy < r2)
			return true;
	}
	return false;
}

void SubInfernoCore_deactivate_all(SubInfernoCoreState *state)
{
	for (int i = 0; i < state->blob_capacity; i++)
		state->blobs[i].active = false;
	state->channeling = false;
}
