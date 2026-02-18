#include "sub_gravwell.h"

#include "player_stats.h"
#include "ship.h"
#include "audio.h"
#include "render.h"
#include "graphics.h"
#include "view.h"
#include "sub_stealth.h"
#include "skillbar.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Timing */
#define DURATION_MS      8000
#define FADE_MS          2000
#define COOLDOWN_MS      20000
#define FEEDBACK_COST    25.0

/* Pull physics */
#define RADIUS           600.0
#define PULL_MIN         20.0
#define PULL_MAX         300.0
#define SLOW_EDGE        0.6
#define SLOW_CENTER      0.05

/* Visual: vortex particles */
#define PARTICLE_COUNT   48
#define RING_COUNT       5

/* Color palette */
#define EDGE_R  0.15f
#define EDGE_G  0.2f
#define EDGE_B  0.5f
#define EDGE_A  0.6f

#define MID_R   0.05f
#define MID_G   0.08f
#define MID_B   0.2f
#define MID_A   0.8f

#define CORE_R  0.02f
#define CORE_G  0.02f
#define CORE_B  0.05f
#define CORE_A  0.95f

typedef struct {
	float angle;       /* orbital angle in radians */
	float radius_frac; /* 0=center, 1=edge */
	float speed;       /* radians/sec */
	float inward_speed;/* how fast it spirals inward (frac/sec) */
	float size;
} VortexParticle;

/* State */
static bool wellActive;
static Position wellPosition;
static int durationMs;
static int cooldownMs;
static float animTimer;

/* Cached mouse world position from last update */
static Position cachedCursorWorld;

static VortexParticle particles[PARTICLE_COUNT];

static Mix_Chunk *samplePlace = 0;
static Mix_Chunk *sampleExpire = 0;

static void init_particle(VortexParticle *p)
{
	p->angle = (float)(rand() % 628) / 100.0f;
	p->radius_frac = 0.3f + (float)(rand() % 70) / 100.0f;
	p->speed = 1.5f + (float)(rand() % 200) / 100.0f;
	p->inward_speed = 0.15f + (float)(rand() % 20) / 100.0f;
	p->size = 2.0f + (float)(rand() % 30) / 10.0f;
}

void Sub_Gravwell_initialize(void)
{
	wellActive = false;
	durationMs = 0;
	cooldownMs = 0;
	animTimer = 0.0f;
	cachedCursorWorld.x = 0.0;
	cachedCursorWorld.y = 0.0;

	for (int i = 0; i < PARTICLE_COUNT; i++)
		init_particle(&particles[i]);

	Audio_load_sample(&samplePlace, "resources/sounds/bomb_set.wav");
	Audio_load_sample(&sampleExpire, "resources/sounds/door.wav");
}

void Sub_Gravwell_cleanup(void)
{
	wellActive = false;
	cooldownMs = 0;
	Audio_unload_sample(&samplePlace);
	Audio_unload_sample(&sampleExpire);
}

void Sub_Gravwell_update(const Input *userInput, unsigned int ticks)
{
	/* Cache cursor world position for try_activate */
	Screen screen = Graphics_get_screen();
	Position cursor = {userInput->mouseX, userInput->mouseY};
	cachedCursorWorld = View_get_world_position(&screen, cursor);

	/* Cooldown tick */
	if (cooldownMs > 0) {
		cooldownMs -= (int)ticks;
		if (cooldownMs < 0) cooldownMs = 0;
	}

	if (!wellActive)
		return;

	/* Duration tick */
	durationMs -= (int)ticks;
	if (durationMs <= 0) {
		wellActive = false;
		Audio_play_sample(&sampleExpire);
		return;
	}

	/* Animate */
	float dt = ticks / 1000.0f;
	animTimer += dt;

	/* Fade multiplier for last 2 seconds */
	float fade = 1.0f;
	if (durationMs < FADE_MS)
		fade = (float)durationMs / FADE_MS;

	/* Update vortex particles — spiral inward */
	for (int i = 0; i < PARTICLE_COUNT; i++) {
		VortexParticle *p = &particles[i];
		/* Speed increases toward center */
		float speed_mult = 1.0f + (1.0f - p->radius_frac) * 2.0f;
		p->angle += p->speed * speed_mult * dt;
		if (p->angle > 2.0f * (float)M_PI)
			p->angle -= 2.0f * (float)M_PI;

		/* Spiral inward — accelerate during fade-out */
		float inward = p->inward_speed;
		if (fade < 1.0f)
			inward *= 3.0f;
		p->radius_frac -= inward * dt;

		/* Respawn at edge when reaching center */
		if (p->radius_frac <= 0.02f) {
			p->radius_frac = 0.85f + (float)(rand() % 15) / 100.0f;
			p->angle = (float)(rand() % 628) / 100.0f;
		}
	}
}

void Sub_Gravwell_try_activate(void)
{
	if (Ship_is_destroyed())
		return;
	if (cooldownMs > 0)
		return;

	/* Pay feedback cost */
	PlayerStats_add_feedback(FEEDBACK_COST);

	/* Break stealth */
	Sub_Stealth_break_attack();

	/* Replace existing well */
	wellActive = true;
	wellPosition = cachedCursorWorld;
	durationMs = DURATION_MS;
	cooldownMs = COOLDOWN_MS;
	animTimer = 0.0f;

	/* Reset particles */
	for (int i = 0; i < PARTICLE_COUNT; i++)
		init_particle(&particles[i]);

	Audio_play_sample(&samplePlace);
}

void Sub_Gravwell_render(void)
{
	if (!wellActive)
		return;

	float fade = 1.0f;
	if (durationMs < FADE_MS)
		fade = (float)durationMs / FADE_MS;

	float cx = (float)wellPosition.x;
	float cy = (float)wellPosition.y;
	float r = (float)RADIUS;

	/* Fade-out: rings collapse inward */
	float ring_scale = fade;

	/* --- Core: near-black pulsing center --- */
	float core_pulse = 0.7f + 0.3f * sinf(animTimer * 3.0f);
	float core_r = 40.0f * ring_scale;
	Render_filled_circle(cx, cy, core_r, 16,
		CORE_R, CORE_G, CORE_B, CORE_A * core_pulse * fade);

	/* --- Concentric rings: dark blue/black dashed, rotating inward --- */
	for (int ring = 0; ring < RING_COUNT; ring++) {
		float ring_frac = 0.2f + 0.15f * ring;
		float ring_radius = r * ring_frac * ring_scale;

		/* Each ring rotates at different speed */
		float rot_speed = 0.5f + ring * 0.3f;
		float rot = animTimer * rot_speed;

		/* Pulsing opacity */
		float pulse = 0.4f + 0.3f * sinf(animTimer * 2.0f + ring * 1.2f);

		/* Lerp color from mid (inner rings) to edge (outer rings) */
		float t = (float)ring / (RING_COUNT - 1);
		float cr = MID_R + (EDGE_R - MID_R) * t;
		float cg = MID_G + (EDGE_G - MID_G) * t;
		float cb = MID_B + (EDGE_B - MID_B) * t;
		float ca = pulse * fade;

		/* Draw dashed ring as line segments */
		int segs = 16 + ring * 4;
		float dash_on = 0.6f; /* fraction of segment that's visible */
		for (int s = 0; s < segs; s++) {
			float a0 = rot + (float)s / segs * 2.0f * (float)M_PI;
			float a_mid = rot + ((float)s + dash_on) / segs * 2.0f * (float)M_PI;

			float x0 = cx + cosf(a0) * ring_radius;
			float y0 = cy + sinf(a0) * ring_radius;
			float x1 = cx + cosf(a_mid) * ring_radius;
			float y1 = cy + sinf(a_mid) * ring_radius;

			Render_thick_line(x0, y0, x1, y1, 1.0f, cr, cg, cb, ca);
		}
	}

	/* --- Outer accretion ring: thin bright ring at the edge --- */
	{
		float accretion_r = r * ring_scale;
		float pulse = 0.5f + 0.3f * sinf(animTimer * 1.5f);
		int segs = 48;
		for (int s = 0; s < segs; s++) {
			float a0 = (float)s / segs * 2.0f * (float)M_PI;
			float a1 = (float)(s + 1) / segs * 2.0f * (float)M_PI;

			float x0 = cx + cosf(a0) * accretion_r;
			float y0 = cy + sinf(a0) * accretion_r;
			float x1 = cx + cosf(a1) * accretion_r;
			float y1 = cy + sinf(a1) * accretion_r;

			Render_thick_line(x0, y0, x1, y1, 1.5f,
				EDGE_R * 1.5f, EDGE_G * 1.5f, EDGE_B * 1.5f,
				pulse * fade);
		}
	}

	/* --- Vortex particles: spiral inward, dark blue -> black --- */
	for (int i = 0; i < PARTICLE_COUNT; i++) {
		VortexParticle *p = &particles[i];
		float pr = r * p->radius_frac * ring_scale;

		float px = cx + cosf(p->angle) * pr;
		float py = cy + sinf(p->angle) * pr;

		/* Color lerp: edge=dark blue, center=void black */
		float t = p->radius_frac;
		float pcr = CORE_R + (EDGE_R - CORE_R) * t;
		float pcg = CORE_G + (EDGE_G - CORE_G) * t;
		float pcb = CORE_B + (EDGE_B - CORE_B) * t;
		float pca = (0.3f + 0.5f * t) * fade;

		/* Motion blur: elongated quad stretched along orbital path */
		float tang_x = -sinf(p->angle);
		float tang_y = cosf(p->angle);
		float stretch = p->size * (1.0f + (1.0f - t) * 2.0f);

		float x0 = px - tang_x * stretch;
		float y0 = py - tang_y * stretch;
		float x1 = px + tang_x * stretch * 0.3f;
		float y1 = py + tang_y * stretch * 0.3f;

		Render_thick_line(x0, y0, x1, y1, p->size * 0.5f,
			pcr, pcg, pcb, pca);
	}
}

void Sub_Gravwell_render_bloom_source(void)
{
	if (!wellActive)
		return;

	float fade = 1.0f;
	if (durationMs < FADE_MS)
		fade = (float)durationMs / FADE_MS;

	float cx = (float)wellPosition.x;
	float cy = (float)wellPosition.y;
	float r = (float)RADIUS;
	float ring_scale = fade;

	/* Only bloom the outer accretion ring and outermost particles.
	   Dark center, bright edge — the contrast IS the effect. */

	/* Accretion ring bloom */
	{
		float accretion_r = r * ring_scale;
		float pulse = 0.5f + 0.3f * sinf(animTimer * 1.5f);
		int segs = 48;
		for (int s = 0; s < segs; s++) {
			float a0 = (float)s / segs * 2.0f * (float)M_PI;
			float a1 = (float)(s + 1) / segs * 2.0f * (float)M_PI;

			float x0 = cx + cosf(a0) * accretion_r;
			float y0 = cy + sinf(a0) * accretion_r;
			float x1 = cx + cosf(a1) * accretion_r;
			float y1 = cy + sinf(a1) * accretion_r;

			Render_thick_line(x0, y0, x1, y1, 2.0f,
				EDGE_R * 1.5f, EDGE_G * 1.5f, EDGE_B * 1.5f,
				pulse * fade * 0.8f);
		}
	}

	/* Outer particles only (radius_frac > 0.6) */
	for (int i = 0; i < PARTICLE_COUNT; i++) {
		VortexParticle *p = &particles[i];
		if (p->radius_frac < 0.6f)
			continue;

		float pr = r * p->radius_frac * ring_scale;
		float px = cx + cosf(p->angle) * pr;
		float py = cy + sinf(p->angle) * pr;

		float t = p->radius_frac;
		float pcr = EDGE_R * 1.2f;
		float pcg = EDGE_G * 1.2f;
		float pcb = EDGE_B * 1.2f;
		float pca = 0.4f * t * fade;

		Position pp = {px, py};
		ColorFloat pc = {pcr, pcg, pcb, pca};
		Render_point(&pp, p->size * 2.0f, &pc);
	}
}

void Sub_Gravwell_deactivate_all(void)
{
	wellActive = false;
}

float Sub_Gravwell_get_cooldown_fraction(void)
{
	if (wellActive)
		return 0.0f;
	if (cooldownMs > 0)
		return (float)cooldownMs / COOLDOWN_MS;
	return 0.0f;
}

bool Sub_Gravwell_is_active(void)
{
	return wellActive;
}

bool Sub_Gravwell_get_pull(Position pos, double dt,
	double *out_dx, double *out_dy, double *out_speed_mult)
{
	if (!wellActive) {
		*out_dx = 0.0;
		*out_dy = 0.0;
		*out_speed_mult = 1.0;
		return false;
	}

	double dx = wellPosition.x - pos.x;
	double dy = wellPosition.y - pos.y;
	double dist = sqrt(dx * dx + dy * dy);

	if (dist > RADIUS) {
		*out_dx = 0.0;
		*out_dy = 0.0;
		*out_speed_mult = 1.0;
		return false;
	}

	/* t = 0 at center, 1 at edge */
	double t = dist / RADIUS;
	if (t < 0.001) t = 0.001;

	/* Fade-out: last 2 seconds, pull and slow linearly ramp to zero */
	double fade = 1.0;
	if (durationMs < FADE_MS)
		fade = (double)durationMs / FADE_MS;

	/* Pull formula: pull = MIN + (MAX - MIN) * (1 - t^2) */
	double pull = PULL_MIN + (PULL_MAX - PULL_MIN) * (1.0 - t * t);
	pull *= fade;

	/* Direction toward center */
	double nx = dx / dist;
	double ny = dy / dist;

	*out_dx = nx * pull * dt;
	*out_dy = ny * pull * dt;

	/* Slow formula: slow = EDGE + (CENTER - EDGE) * (1 - t)^2 */
	double inv_t = 1.0 - t;
	double slow = SLOW_EDGE + (SLOW_CENTER - SLOW_EDGE) * inv_t * inv_t;
	/* Fade slow back to 1.0 */
	*out_speed_mult = slow + (1.0 - slow) * (1.0 - fade);

	return true;
}
