#include "burn.h"
#include "player_stats.h"
#include "render.h"
#include "audio.h"
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

/* Inferno-style fire color: white-hot → yellow → orange → red based on age */
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

void Burn_render(const BurnState *state, Position pos)
{
	if (state->stacks <= 0)
		return;

	unsigned int ticks = SDL_GetTicks();
	int stacks = state->stacks;

	/* Fire kernels on the entity — overlapping rotated quads, inferno style */
	int kernel_count = 1 + stacks;
	for (int i = 0; i < kernel_count; i++) {
		unsigned int seed = (unsigned int)((int)pos.x * 7 + (int)pos.y * 13 + i * 47);
		float life_ms = 400.0f + (float)(seed % 200);
		float phase = (float)((ticks + seed * 73) % (int)life_ms) / life_ms;

		float jx = (float)((seed * 31) % 16) - 8.0f;
		float jy = (float)((seed * 17) % 12) - 6.0f;
		float rise = phase * 12.0f;

		float r, g, b, a;
		fire_color(phase, &r, &g, &b, &a);

		float rot = (float)((ticks + seed * 53) % 3600) * 0.1f;
		float sz = (9.0f + stacks * 2.5f) * (1.0f - phase * 0.4f);
		Position kp = {pos.x + jx, pos.y + jy - rise};
		Rectangle rect = {-sz, sz, sz, -sz};
		ColorFloat c = {r, g, b, a};
		Render_quad(&kp, rot, rect, &c);

		float sz2 = sz * 0.6f;
		ColorFloat c2 = {r, g * 0.9f, b * 0.7f, a * 0.8f};
		Render_quad(&kp, rot + 55.0f, (Rectangle){-sz2, sz2, sz2, -sz2}, &c2);
	}
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
}

void Burn_render_player(Position pos)
{
	Burn_render(&playerBurn, pos);
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
