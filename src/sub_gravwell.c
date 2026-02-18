#include "sub_gravwell.h"

#include "player_stats.h"
#include "ship.h"
#include "audio.h"
#include "render.h"
#include "graphics.h"
#include "view.h"
#include "sub_stealth.h"
#include "skillbar.h"
#include "particle_instance.h"

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

/* ================================================================
 * Instanced whirlpool blob system
 * ================================================================ */

#define BLOB_COUNT 640
#define BLOB_BASE_SIZE 14.0f
#define TRAIL_GHOSTS 8
#define TRAIL_DT 0.025f   /* 25ms between ghost positions */
#define MAX_INSTANCES (BLOB_COUNT * (1 + TRAIL_GHOSTS))

/* Per-blob CPU state */
typedef struct {
	float angle;         /* orbital position in radians */
	float radius_frac;   /* 0=center, 1=edge */
	float angular_speed; /* base radians/sec */
	float inward_speed;  /* base frac/sec */
	float size;          /* base size multiplier */
} WhirlpoolBlob;

/* State */
static bool wellActive;
static Position wellPosition;
static int durationMs;
static int cooldownMs;
static float animTimer;

/* Cached mouse world position from last update */
static Position cachedCursorWorld;

static WhirlpoolBlob blobs[BLOB_COUNT];
static ParticleInstanceData instanceData[MAX_INSTANCES];

static Mix_Chunk *samplePlace = 0;
static Mix_Chunk *sampleExpire = 0;

/* ================================================================
 * Blob particle system
 * ================================================================ */

/* Spawn blob at outer edge — used for continuous respawning */
static void respawn_blob(WhirlpoolBlob *b)
{
	b->angle = (float)(rand() % 6283) / 1000.0f;
	b->radius_frac = 0.88f + (float)(rand() % 120) / 1000.0f;
	b->angular_speed = 1.8f * (0.7f + (float)(rand() % 600) / 1000.0f);
	b->inward_speed = 0.08f * (0.7f + (float)(rand() % 600) / 1000.0f);
	b->size = BLOB_BASE_SIZE * (0.7f + (float)(rand() % 600) / 1000.0f);
}

/* Spawn blob anywhere across the radius — fill the vortex instantly on activation */
static void init_blob(WhirlpoolBlob *b)
{
	respawn_blob(b);
	b->radius_frac = 0.06f + (float)(rand() % 940) / 1000.0f;
}

/* Thin bright rim -> swirling blues/indigos -> deep black core */
static void blob_color_by_radius(float radius_frac, float fade,
	float *r, float *g, float *b, float *a)
{
	float t = radius_frac;

	if (t > 0.85f) {
		/* Thin bright rim — white-blue */
		float f = (t - 0.85f) / 0.15f;
		*r = 0.15f + f * 0.45f;
		*g = 0.25f + f * 0.55f;
		*b = 0.5f  + f * 0.5f;
	} else if (t > 0.5f) {
		/* Mid zone — swirling blues */
		float f = (t - 0.5f) / 0.35f;
		*r = 0.04f + f * 0.11f;
		*g = 0.06f + f * 0.19f;
		*b = 0.15f + f * 0.35f;
	} else if (t > 0.25f) {
		/* Deep indigo transition */
		float f = (t - 0.25f) / 0.25f;
		*r = 0.01f + f * 0.03f;
		*g = 0.01f + f * 0.05f;
		*b = 0.03f + f * 0.12f;
	} else {
		/* Black core */
		*r = 0.01f;
		*g = 0.01f;
		*b = 0.03f;
	}

	*a = fade;
}

/* Fill instance data array with ghost trails, returns count */
static int build_instance_data(ParticleInstanceData *out, float brightness_boost,
	float min_radius_frac)
{
	float fade = 1.0f;
	if (durationMs < FADE_MS)
		fade = (float)durationMs / FADE_MS;

	float cx = (float)wellPosition.x;
	float cy = (float)wellPosition.y;
	float radius = (float)RADIUS;

	int count = 0;
	for (int i = 0; i < BLOB_COUNT; i++) {
		WhirlpoolBlob *bl = &blobs[i];

		if (bl->radius_frac < min_radius_frac)
			continue;

		/* Color from radial position */
		float cr, cg, cb, ca;
		blob_color_by_radius(bl->radius_frac, fade, &cr, &cg, &cb, &ca);
		if (ca <= 0.01f) continue;

		/* Compute current angular velocity for trail spacing */
		float r_clamped = bl->radius_frac < 0.06f ? 0.06f : bl->radius_frac;
		float speed_mult = 1.0f / (r_clamped * r_clamped);
		if (speed_mult > 200.0f) speed_mult = 200.0f;
		float ang_vel = bl->angular_speed * speed_mult;

		float pr = radius * bl->radius_frac * fade;

		/* Render blob + ghost trail behind it (counterclockwise motion,
		   so ghosts trail at earlier/smaller angles) */
		for (int g = 0; g <= TRAIL_GHOSTS; g++) {
			float ghost_angle = bl->angle - (float)g * ang_vel * TRAIL_DT;
			float ghost_t = (float)g / (float)(TRAIL_GHOSTS + 1);
			float ghost_alpha = ca * (1.0f - ghost_t);

			if (ghost_alpha <= 0.01f) break;

			float px = cx + cosf(ghost_angle) * pr;
			float py = cy + sinf(ghost_angle) * pr;

			/* Tangent direction (counterclockwise = +PI/2) */
			float tangent_angle = ghost_angle + (float)M_PI * 0.5f;

			ParticleInstanceData *inst = &out[count++];
			inst->x = px;
			inst->y = py;
			inst->size = bl->size;
			inst->rotation = tangent_angle * (180.0f / (float)M_PI);
			inst->stretch = 1.5f;
			inst->r = cr * brightness_boost;
			inst->g = cg * brightness_boost;
			inst->b = cb * brightness_boost;
			inst->a = ghost_alpha;

			if (count >= MAX_INSTANCES) return count;
		}
	}
	return count;
}

/* ================================================================
 * Public API — lifecycle
 * ================================================================ */

void Sub_Gravwell_initialize(void)
{
	wellActive = false;
	durationMs = 0;
	cooldownMs = 0;
	animTimer = 0.0f;
	cachedCursorWorld.x = 0.0;
	cachedCursorWorld.y = 0.0;

	for (int i = 0; i < BLOB_COUNT; i++)
		init_blob(&blobs[i]);

	Audio_load_sample(&samplePlace, "resources/sounds/bomb_set.wav");
	Audio_load_sample(&sampleExpire, "resources/sounds/door.wav");
}

void Sub_Gravwell_cleanup(void)
{
	wellActive = false;
	cooldownMs = 0;
	ParticleInstance_cleanup();
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

	/* Update whirlpool blobs — spiral inward */
	for (int i = 0; i < BLOB_COUNT; i++) {
		WhirlpoolBlob *bl = &blobs[i];

		/* Angular velocity: 1/r^2 — dramatic acceleration into center */
		float r_clamped = bl->radius_frac < 0.06f ? 0.06f : bl->radius_frac;
		float speed_mult = 1.0f / (r_clamped * r_clamped);
		if (speed_mult > 200.0f) speed_mult = 200.0f;
		/* All counterclockwise (positive angle in world space) */
		bl->angle += bl->angular_speed * speed_mult * dt;

		/* Keep angle in [0, 2PI] */
		if (bl->angle > 2.0f * (float)M_PI)
			bl->angle -= 2.0f * (float)M_PI;

		/* Inward speed accelerates toward center — blobs linger at edge,
		   streak through the middle. depth^2 ramp. */
		float depth = 1.0f - bl->radius_frac;
		float inward = bl->inward_speed * (1.0f + depth * depth * 4.0f);
		if (fade < 1.0f)
			inward *= 3.0f;
		bl->radius_frac -= inward * dt;

		/* Respawn at outer edge when reaching center */
		if (bl->radius_frac < 0.05f) {
			if (fade >= 1.0f) {
				respawn_blob(&blobs[i]);
			} else {
				bl->radius_frac = 0.01f; /* clamp, let it collapse */
			}
		}
	}
}

/* ================================================================
 * Public API — activation / mechanics
 * ================================================================ */

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

	/* Reset blobs */
	for (int i = 0; i < BLOB_COUNT; i++)
		init_blob(&blobs[i]);

	Audio_play_sample(&samplePlace);
}

/* ================================================================
 * Public API — rendering
 * ================================================================ */

void Sub_Gravwell_render(void)
{
	if (!wellActive)
		return;

	float fade = 1.0f;
	if (durationMs < FADE_MS)
		fade = (float)durationMs / FADE_MS;

	/* Solid black circle base — covers background so the void is truly black */
	float cx = (float)wellPosition.x;
	float cy = (float)wellPosition.y;
	Render_filled_circle(cx, cy, (float)RADIUS * 0.85f * fade, 48,
		0.0f, 0.0f, 0.0f, fade);

	/* Flush batch (including black circle) before instanced pass */
	Mat4 world_proj = Graphics_get_world_projection();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);
	Render_flush(&world_proj, &view);

	/* Build instance data (all blobs, 1.0x brightness) */
	int count = build_instance_data(instanceData, 1.0f, 0.0f);

	/* Draw instanced */
	ParticleInstance_draw(instanceData, count, &world_proj, &view, true);
}

void Sub_Gravwell_render_bloom_source(void)
{
	if (!wellActive)
		return;

	/* Flush pending batch geometry in the bloom FBO */
	Mat4 world_proj = Graphics_get_world_projection();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);
	Render_flush(&world_proj, &view);

	/* Only outer blobs (radius_frac > 0.5) at 1.5x brightness for bloom */
	int count = build_instance_data(instanceData, 1.5f, 0.5f);

	ParticleInstance_draw(instanceData, count, &world_proj, &view, true);
}

/* ================================================================
 * Public API — query / state
 * ================================================================ */

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
