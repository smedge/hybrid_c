#include "sub_aegis.h"

#include "skillbar.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"
#include "render.h"
#include "audio.h"

#include <math.h>
#include <SDL2/SDL_mixer.h>

#define SHIELD_DURATION_MS 10000
#define FEEDBACK_COST 30.0
#define COOLDOWN_MS 30000
#define RING_RADIUS 35.0f
#define RING_SEGMENTS 12
#define PULSE_SPEED 6.0f

typedef enum {
	AEGIS_READY,
	AEGIS_ACTIVE,
	AEGIS_COOLDOWN
} AegisState;

static AegisState state;
static int activeMs;
static int cooldownMs;
static float pulseTimer;

static Mix_Chunk *sampleActivate = 0;
static Mix_Chunk *sampleDeactivate = 0;

void Sub_Aegis_initialize(void)
{
	state = AEGIS_READY;
	activeMs = 0;
	cooldownMs = 0;
	pulseTimer = 0.0f;

	Audio_load_sample(&sampleActivate, "resources/sounds/statue_rise.wav");
	Audio_load_sample(&sampleDeactivate, "resources/sounds/door.wav");
}

void Sub_Aegis_cleanup(void)
{
	/* Drop shield if active */
	if (state == AEGIS_ACTIVE)
		PlayerStats_set_shielded(false);

	Audio_unload_sample(&sampleActivate);
	Audio_unload_sample(&sampleDeactivate);
}

void Sub_Aegis_update(const Input *input, unsigned int ticks)
{
	switch (state) {
	case AEGIS_READY:
		if (input->keyF && Skillbar_is_active(SUB_ID_AEGIS)) {
			PlayerStats_set_shielded(true);
			PlayerStats_add_feedback(FEEDBACK_COST);
			state = AEGIS_ACTIVE;
			activeMs = SHIELD_DURATION_MS;
			pulseTimer = 0.0f;
			Audio_play_sample(&sampleActivate);
		}
		break;

	case AEGIS_ACTIVE:
		pulseTimer += ticks / 1000.0f;
		/* Check if shield was broken externally (e.g. by mine) */
		if (!PlayerStats_is_shielded()) {
			state = AEGIS_COOLDOWN;
			cooldownMs = COOLDOWN_MS;
			Audio_play_sample(&sampleDeactivate);
			break;
		}
		activeMs -= (int)ticks;
		if (activeMs <= 0) {
			activeMs = 0;
			PlayerStats_set_shielded(false);
			state = AEGIS_COOLDOWN;
			cooldownMs = COOLDOWN_MS;
			Audio_play_sample(&sampleDeactivate);
		}
		break;

	case AEGIS_COOLDOWN:
		cooldownMs -= (int)ticks;
		if (cooldownMs <= 0) {
			cooldownMs = 0;
			state = AEGIS_READY;
		}
		break;
	}
}

void Sub_Aegis_render(void)
{
	if (state != AEGIS_ACTIVE)
		return;

	Position shipPos = Ship_get_position();
	float pulse = 0.7f + 0.3f * sinf(pulseTimer * PULSE_SPEED);
	float r = RING_RADIUS * (0.95f + 0.05f * sinf(pulseTimer * PULSE_SPEED * 2.0f));

	for (int i = 0; i < RING_SEGMENTS; i++) {
		float a0 = i * (2.0f * 3.14159f / RING_SEGMENTS);
		float a1 = (i + 1) * (2.0f * 3.14159f / RING_SEGMENTS);
		float x0 = (float)shipPos.x + cosf(a0) * r;
		float y0 = (float)shipPos.y + sinf(a0) * r;
		float x1 = (float)shipPos.x + cosf(a1) * r;
		float y1 = (float)shipPos.y + sinf(a1) * r;
		Render_thick_line(x0, y0, x1, y1, 2.0f,
			0.6f, 0.9f, 1.0f, pulse);
	}
}

void Sub_Aegis_render_bloom_source(void)
{
	if (state != AEGIS_ACTIVE)
		return;

	Position shipPos = Ship_get_position();
	float pulse = 0.5f + 0.3f * sinf(pulseTimer * PULSE_SPEED);
	float r = RING_RADIUS;

	for (int i = 0; i < RING_SEGMENTS; i++) {
		float a0 = i * (2.0f * 3.14159f / RING_SEGMENTS);
		float a1 = (i + 1) * (2.0f * 3.14159f / RING_SEGMENTS);
		float x0 = (float)shipPos.x + cosf(a0) * r;
		float y0 = (float)shipPos.y + sinf(a0) * r;
		float x1 = (float)shipPos.x + cosf(a1) * r;
		float y1 = (float)shipPos.y + sinf(a1) * r;
		Render_thick_line(x0, y0, x1, y1, 3.0f,
			0.6f, 0.9f, 1.0f, pulse);
	}
}

float Sub_Aegis_get_cooldown_fraction(void)
{
	if (state == AEGIS_ACTIVE)
		return (float)activeMs / SHIELD_DURATION_MS;
	if (state == AEGIS_COOLDOWN)
		return (float)cooldownMs / COOLDOWN_MS;
	return 0.0f;
}
