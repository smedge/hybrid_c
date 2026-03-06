#include "burn.h"
#include "player_stats.h"
#include "render.h"
#include "audio.h"
#include "particle_instance.h"
#include "graphics.h"
#include "view.h"
#include "ship.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>

/* --- Player singleton burn state --- */
static BurnState playerBurn = {0};

/* --- Audio --- */
static Mix_Chunk *sndBurnTick = NULL;
static int burnTickTimer = 0;
#define BURN_TICK_INTERVAL_MS 500 /* sizzle sound every 500ms while burning */

/* --- Registration system --- */

#define BURN_MAX_REGISTERED   128
#define BURN_EMBERS_PER_ENTITY 6
#define BURN_MAX_INSTANCES    1024

typedef struct {
	bool active;
	float x, y;
	float vx, vy;
	int age_ms;
	int ttl_ms;
	float size;
	float rotation;
} BurnEmber;

typedef struct {
	const BurnState *key; /* pointer identity for cross-frame tracking */
	bool active;
	bool seenThisFrame;
	int stacks;
	Position pos;
	BurnEmber embers[BURN_EMBERS_PER_ENTITY];
	int emberSpawnTimer;
} BurnRegistration;

static BurnRegistration registrations[BURN_MAX_REGISTERED];
static ParticleInstanceData burnInstances[BURN_MAX_INSTANCES];

/* --- Core burn logic (works on any BurnState) --- */

void Burn_apply(BurnState *state, int duration_ms)
{
	/* Find an empty slot */
	for (int i = 0; i < BURN_MAX_STACKS; i++) {
		if (state->duration_ms[i] <= 0) {
			state->duration_ms[i] = duration_ms;
			state->stacks++;
			return;
		}
	}

	/* At max stacks — refresh the shortest remaining duration */
	int shortest = 0;
	for (int i = 1; i < BURN_MAX_STACKS; i++) {
		if (state->duration_ms[i] < state->duration_ms[shortest])
			shortest = i;
	}
	state->duration_ms[shortest] = duration_ms;
}

double Burn_update(BurnState *state, unsigned int ticks)
{
	if (state->stacks <= 0)
		return 0.0;

	int active = 0;

	for (int i = 0; i < BURN_MAX_STACKS; i++) {
		if (state->duration_ms[i] > 0) {
			state->duration_ms[i] -= (int)ticks;
			if (state->duration_ms[i] <= 0)
				state->duration_ms[i] = 0;
			else
				active++;
		}
	}

	double dt = ticks / 1000.0;
	double damage = active * BURN_DPS * dt;
	state->stacks = active;
	return damage;
}

void Burn_reset(BurnState *state)
{
	memset(state, 0, sizeof(BurnState));
}

bool Burn_is_active(const BurnState *state)
{
	return state->stacks > 0;
}

int Burn_get_stacks(const BurnState *state)
{
	return state->stacks;
}

/* --- Color helpers --- */

/* Inferno-style fire color: white-hot -> yellow -> orange -> red based on age */
static void fire_color(float t, float *r, float *g, float *b, float *a)
{
	if (t < 0.2f) {
		float f = t / 0.2f;
		*r = 1.0f; *g = 1.0f; *b = 1.0f - f * 0.7f; *a = 0.9f;
	} else if (t < 0.5f) {
		float f = (t - 0.2f) / 0.3f;
		*r = 1.0f; *g = 1.0f - f * 0.3f; *b = 0.3f - f * 0.2f; *a = 0.9f;
	} else if (t < 0.8f) {
		float f = (t - 0.5f) / 0.3f;
		*r = 1.0f; *g = 0.7f - f * 0.35f; *b = 0.1f; *a = 0.8f - f * 0.2f;
	} else {
		float f = (t - 0.8f) / 0.2f;
		*r = 1.0f - f * 0.4f; *g = 0.35f - f * 0.25f; *b = 0.1f - f * 0.05f;
		*a = 0.6f - f * 0.5f;
	}
}

/* Ember color: white-yellow spark -> cooling yellow -> orange dying */
static void ember_color(float t, float *r, float *g, float *b, float *a)
{
	if (t < 0.3f) {
		*r = 1.0f; *g = 1.0f; *b = 0.7f;
	} else if (t < 0.6f) {
		float f = (t - 0.3f) / 0.3f;
		*r = 1.0f; *g = 1.0f - f * 0.3f; *b = 0.7f - f * 0.5f;
	} else {
		float f = (t - 0.6f) / 0.4f;
		*r = 1.0f; *g = 0.7f - f * 0.3f; *b = 0.2f - f * 0.15f;
	}
	/* Quadratic alpha falloff */
	*a = 1.0f - t * t;
}

/* --- Registration API --- */

void Burn_clear_registrations(void)
{
	for (int i = 0; i < BURN_MAX_REGISTERED; i++)
		registrations[i].seenThisFrame = false;
}

void Burn_register(const BurnState *state, Position pos)
{
	if (!state || state->stacks <= 0)
		return;

	/* Find existing registration by pointer identity */
	for (int i = 0; i < BURN_MAX_REGISTERED; i++) {
		if (registrations[i].active && registrations[i].key == state) {
			registrations[i].seenThisFrame = true;
			registrations[i].stacks = state->stacks;
			registrations[i].pos = pos;
			return;
		}
	}

	/* Allocate new slot */
	for (int i = 0; i < BURN_MAX_REGISTERED; i++) {
		if (!registrations[i].active) {
			memset(&registrations[i], 0, sizeof(BurnRegistration));
			registrations[i].key = state;
			registrations[i].active = true;
			registrations[i].seenThisFrame = true;
			registrations[i].stacks = state->stacks;
			registrations[i].pos = pos;
			return;
		}
	}
}

/* --- Ember simulation --- */

static void spawn_ember(BurnEmber *e, Position pos)
{
	e->active = true;
	e->x = (float)pos.x + ((float)(rand() % 10) - 5.0f);
	e->y = (float)pos.y + ((float)(rand() % 10) - 5.0f);
	e->vx = ((float)(rand() % 40) - 20.0f);  /* +/-20 px/sec lateral */
	e->vy = 30.0f + (float)(rand() % 30);     /* 30-60 px/sec upward (+Y = up) */
	e->age_ms = 0;
	e->ttl_ms = 300 + (rand() % 300);         /* 300-600ms */
	e->size = 2.0f + (float)(rand() % 20) / 10.0f; /* 2-4 px */
	e->rotation = (float)(rand() % 360);
}

void Burn_update_embers(unsigned int ticks)
{
	float dt = (float)ticks / 1000.0f;

	for (int i = 0; i < BURN_MAX_REGISTERED; i++) {
		BurnRegistration *reg = &registrations[i];

		/* Deactivate entries not seen this frame */
		if (reg->active && !reg->seenThisFrame) {
			reg->active = false;
			continue;
		}
		if (!reg->active)
			continue;

		/* Update existing embers */
		for (int e = 0; e < BURN_EMBERS_PER_ENTITY; e++) {
			BurnEmber *em = &reg->embers[e];
			if (!em->active) continue;

			em->age_ms += (int)ticks;
			if (em->age_ms >= em->ttl_ms) {
				em->active = false;
				continue;
			}

			em->x += em->vx * dt;
			em->y += em->vy * dt;
			float drag = powf(0.95f, dt);
			em->vx *= drag;
			em->rotation += 180.0f * dt;
		}

		/* Spawn new embers based on stack count */
		int spawnInterval;
		switch (reg->stacks) {
		case 1:  spawnInterval = 200; break;
		case 2:  spawnInterval = 120; break;
		default: spawnInterval = 80;  break;
		}

		reg->emberSpawnTimer -= (int)ticks;
		if (reg->emberSpawnTimer <= 0) {
			reg->emberSpawnTimer = spawnInterval;
			/* Find free ember slot */
			for (int e = 0; e < BURN_EMBERS_PER_ENTITY; e++) {
				if (!reg->embers[e].active) {
					spawn_ember(&reg->embers[e], reg->pos);
					break;
				}
			}
		}
	}
}

/* --- Instance buffer fill (separate flame/ember for different shape modes) --- */

static int fill_flame_instances(ParticleInstanceData *out, float colorBoost, float sizeBoost)
{
	int count = 0;
	unsigned int ticks = SDL_GetTicks();

	for (int i = 0; i < BURN_MAX_REGISTERED; i++) {
		BurnRegistration *reg = &registrations[i];
		if (!reg->active) continue;

		int stacks = reg->stacks;
		float px = (float)reg->pos.x;
		float py = (float)reg->pos.y;

		int kernel_count;
		switch (stacks) {
		case 1:  kernel_count = 3; break;
		case 2:  kernel_count = 5; break;
		default: kernel_count = 8; break;
		}

		float jitter_range = 4.0f + stacks * 1.5f;
		float rise_dist = 20.0f + stacks * 6.0f;
		float base_size = 8.0f + stacks * 2.5f;

		for (int k = 0; k < kernel_count && count < BURN_MAX_INSTANCES; k++) {
			unsigned int seed = (unsigned int)((int)reg->pos.x * 7 + (int)reg->pos.y * 13 + k * 47);
			float life_ms = 700.0f + (float)(seed % 400);
			float phase = (float)((ticks + seed * 73) % (int)life_ms) / life_ms;

			float jx = (float)((seed * 31) % (int)(jitter_range * 2)) - jitter_range;
			float jy = (float)((seed * 17) % 6) - 3.0f;
			float rise = phase * rise_dist;

			float r, g, b, a;
			fire_color(phase, &r, &g, &b, &a);

			/* Rotation: 90° points flame teardrop upward, sway +/-12° */
			float sway = 90.0f + sinf((float)ticks * 0.002f + (float)seed * 0.7f) * 12.0f;
			float sz = base_size * (1.0f - phase * 0.45f);
			/* Stretch along flame axis — taller near base, shorter as it fades */
			float stretch = 1.5f + (1.0f - phase) * 0.8f;

			float yoff = sz * stretch * 0.5f;

			out[count++] = (ParticleInstanceData){
				.x = px + jx, .y = py + jy + rise + yoff,
				.size = sz * sizeBoost,
				.rotation = sway, .stretch = stretch,
				.r = r * colorBoost, .g = g * colorBoost,
				.b = b * colorBoost, .a = a
			};
		}
	}

	return count;
}

static int fill_ember_instances(ParticleInstanceData *out, float colorBoost, float sizeBoost)
{
	int count = 0;

	for (int i = 0; i < BURN_MAX_REGISTERED; i++) {
		BurnRegistration *reg = &registrations[i];
		if (!reg->active) continue;

		for (int e = 0; e < BURN_EMBERS_PER_ENTITY && count < BURN_MAX_INSTANCES; e++) {
			BurnEmber *em = &reg->embers[e];
			if (!em->active) continue;

			float t = (float)em->age_ms / (float)em->ttl_ms;
			float r, g, b, a;
			ember_color(t, &r, &g, &b, &a);

			float sz = em->size * (1.0f - t * 0.6f);

			out[count++] = (ParticleInstanceData){
				.x = em->x, .y = em->y,
				.size = sz * sizeBoost,
				.rotation = em->rotation, .stretch = 1.0f,
				.r = r * colorBoost, .g = g * colorBoost,
				.b = b * colorBoost, .a = a
			};
		}
	}

	return count;
}

/* --- Render functions --- */

static void burn_draw(float flameColorBoost, float flameSizeBoost,
	float emberColorBoost, float emberSizeBoost)
{
	int fc = fill_flame_instances(burnInstances, flameColorBoost, flameSizeBoost);
	int ec = fill_ember_instances(burnInstances + fc, emberColorBoost, emberSizeBoost);
	if (fc + ec <= 0) return;

	Mat4 world_proj = Graphics_get_world_projection();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);
	Render_flush(&world_proj, &view);

	if (fc > 0)
		ParticleInstance_draw(burnInstances, fc, &world_proj, &view, PI_SHAPE_FLAME);
	if (ec > 0)
		ParticleInstance_draw(burnInstances + fc, ec, &world_proj, &view, PI_SHAPE_SOFT);
}

void Burn_render_all(void)
{
	burn_draw(1.0f, 1.0f, 1.0f, 1.0f);
}

void Burn_render_bloom_source(void)
{
	burn_draw(1.4f, 1.3f, 1.5f, 1.2f);
}

void Burn_render_light_source(void)
{
	for (int i = 0; i < BURN_MAX_REGISTERED; i++) {
		BurnRegistration *reg = &registrations[i];
		if (!reg->active) continue;

		float radius = 120.0f + reg->stacks * 40.0f;
		float alpha = 0.3f + reg->stacks * 0.15f;
		Render_filled_circle((float)reg->pos.x, (float)reg->pos.y,
			radius, 12, 1.0f, 0.5f, 0.1f, alpha);
	}
}

/* Old per-entity render — now a no-op (rendering is centralized) */
void Burn_render(const BurnState *state, Position pos)
{
	(void)state;
	(void)pos;
}

/* --- Enemy burn helpers --- */

bool Burn_tick_enemy(BurnState *burn, double *hp, unsigned int ticks)
{
	double damage = Burn_update(burn, ticks);
	if (damage > 0.0) {
		*hp -= damage;
		if (*hp <= 0.0)
			return true; /* burn killed */
	}
	return false;
}

void Burn_apply_from_hits(BurnState *burn, int burn_hits)
{
	for (int i = 0; i < burn_hits; i++)
		Burn_apply(burn, BURN_DURATION_MS);
}

/* --- Player burn --- */

void Burn_apply_to_player(int duration_ms)
{
	Burn_apply(&playerBurn, duration_ms);
}

void Burn_update_player(unsigned int ticks)
{
	double damage = Burn_update(&playerBurn, ticks);
	if (damage > 0.0) {
		PlayerStats_damage(damage);

		/* Periodic sizzle sound */
		burnTickTimer -= (int)ticks;
		if (burnTickTimer <= 0 && sndBurnTick) {
			Mix_PlayChannel(-1, sndBurnTick, 0);
			burnTickTimer = BURN_TICK_INTERVAL_MS;
		}
	} else {
		burnTickTimer = 0;
	}

	/* Register player burn for centralized rendering */
	if (Burn_is_active(&playerBurn))
		Burn_register(&playerBurn, Ship_get_position());
}

void Burn_render_player(Position pos)
{
	(void)pos;
	/* No-op — player burn renders through centralized system */
}

void Burn_reset_player(void)
{
	Burn_reset(&playerBurn);
	burnTickTimer = 0;
}

bool Burn_player_is_burning(void)
{
	return Burn_is_active(&playerBurn);
}

/* --- Audio lifecycle --- */

void Burn_initialize_audio(void)
{
	/* Sound file TBD — will load when asset is created */
	/* Audio_load_sample(&sndBurnTick, "resources/sounds/burn_tick.wav"); */
}

void Burn_cleanup_audio(void)
{
	if (sndBurnTick) {
		Mix_FreeChunk(sndBurnTick);
		sndBurnTick = NULL;
	}
}
