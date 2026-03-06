#include "sub_blaze.h"

#include "sub_dash_core.h"
#include "skillbar.h"
#include "progression.h"
#include "ship.h"
#include "player_stats.h"
#include "burn.h"
#include "render.h"

#include <math.h>
#include <string.h>

#define DEG_TO_RAD (M_PI / 180.0)

/* --- Config --- */

static SubDashCore core;
static const SubDashConfig cfg = {
	.duration_ms = 200,
	.speed = 4500.0,
	.cooldown_ms = 3000,
	.damage = 0.0,  /* no contact damage — corridor is the damage */
};

static const double FEEDBACK_COST = 20.0;

/* --- Corridor --- */

#define CORRIDOR_MAX       58
#define CORRIDOR_LIFE_MS   4000
#define CORRIDOR_RADIUS    30.0
#define CORRIDOR_BURN_INTERVAL_MS 500
#define SEGMENT_SPAWN_MS   3    /* deposit a segment every ~3ms during 200ms dash */

typedef struct {
	bool active;
	Position position;
	int life_ms;
	int burn_tick_ms;   /* countdown per-segment for interval burn */
	BurnState burn;     /* for visual registration with burn system */
} CorridorSegment;

static CorridorSegment corridor[CORRIDOR_MAX];
static int segmentSpawnTimer;

static bool shiftWasDown;

/* --- Lifecycle --- */

void Sub_Blaze_initialize(void)
{
	SubDash_init(&core);
	SubDash_initialize_audio();
	shiftWasDown = false;
	segmentSpawnTimer = 0;
	Sub_Blaze_deactivate_all();
}

void Sub_Blaze_cleanup(void)
{
	SubDash_cleanup_audio();
	Sub_Blaze_deactivate_all();
}

/* --- Update --- */

static void spawn_corridor_segment(void)
{
	for (int i = 0; i < CORRIDOR_MAX; i++) {
		if (!corridor[i].active) {
			corridor[i].active = true;
			corridor[i].position = Ship_get_position();
			corridor[i].life_ms = CORRIDOR_LIFE_MS;
			corridor[i].burn_tick_ms = 0;
			Burn_reset(&corridor[i].burn);
			Burn_apply(&corridor[i].burn, CORRIDOR_LIFE_MS);
			return;
		}
	}
}

void Sub_Blaze_update(const Input *input, unsigned int ticks)
{
	bool shiftDown = input->keyLShift && Skillbar_is_active(SUB_ID_BLAZE);

	if (!SubDash_is_active(&core) && core.cooldownMs <= 0) {
		if (shiftDown && !shiftWasDown) {
			double dx = 0.0, dy = 0.0;
			if (input->keyW) dy += 1.0;
			if (input->keyS) dy -= 1.0;
			if (input->keyD) dx += 1.0;
			if (input->keyA) dx -= 1.0;

			double len = sqrt(dx * dx + dy * dy);
			if (len > 0.0) {
				dx /= len;
				dy /= len;
			} else {
				double heading = Ship_get_heading();
				dx = sin(heading * DEG_TO_RAD);
				dy = cos(heading * DEG_TO_RAD);
			}

			if (SubDash_try_activate(&core, &cfg, dx, dy)) {
				PlayerStats_add_feedback(FEEDBACK_COST);
				PlayerStats_set_iframes(cfg.duration_ms);
				segmentSpawnTimer = 0;
				spawn_corridor_segment(); /* immediate segment at dash origin */
			}
		}
	}

	/* Deposit corridor segments during dash */
	if (SubDash_is_active(&core)) {
		segmentSpawnTimer += (int)ticks;
		while (segmentSpawnTimer >= SEGMENT_SPAWN_MS) {
			segmentSpawnTimer -= SEGMENT_SPAWN_MS;
			spawn_corridor_segment();
		}
	}

	SubDash_update(&core, &cfg, ticks);
	shiftWasDown = shiftDown;
}

/* --- Corridor update --- */

void Sub_Blaze_update_corridor(unsigned int ticks)
{
	for (int i = 0; i < CORRIDOR_MAX; i++) {
		if (!corridor[i].active)
			continue;

		corridor[i].life_ms -= (int)ticks;
		if (corridor[i].life_ms <= 0) {
			corridor[i].active = false;
			Burn_reset(&corridor[i].burn);
			continue;
		}

		/* Tick the burn visual state to keep stacks alive */
		Burn_update(&corridor[i].burn, ticks);

		/* Re-apply burn stack if it expired (keep 1 stack alive for visuals) */
		if (!Burn_is_active(&corridor[i].burn))
			Burn_apply(&corridor[i].burn, corridor[i].life_ms);

		/* Tick burn interval timer */
		if (corridor[i].burn_tick_ms > 0)
			corridor[i].burn_tick_ms -= (int)ticks;

		/* Register with centralized burn renderer */
		Burn_register(&corridor[i].burn, corridor[i].position);
	}
}

/* --- Corridor burn check (called per enemy per frame) --- */

static bool circle_aabb_overlap(Position center, double radius, Rectangle r)
{
	/* Closest point on AABB to circle center */
	double cx = center.x;
	double cy = center.y;
	double minX = r.aX < r.bX ? r.aX : r.bX;
	double maxX = r.aX > r.bX ? r.aX : r.bX;
	double minY = r.aY < r.bY ? r.aY : r.bY;
	double maxY = r.aY > r.bY ? r.aY : r.bY;

	double nearX = cx < minX ? minX : (cx > maxX ? maxX : cx);
	double nearY = cy < minY ? minY : (cy > maxY ? maxY : cy);

	double dx = cx - nearX;
	double dy = cy - nearY;
	return (dx * dx + dy * dy) <= radius * radius;
}

int Sub_Blaze_check_corridor_burn(Rectangle target)
{
	int hits = 0;
	for (int i = 0; i < CORRIDOR_MAX; i++) {
		if (!corridor[i].active)
			continue;
		if (corridor[i].burn_tick_ms > 0)
			continue;

		if (circle_aabb_overlap(corridor[i].position, CORRIDOR_RADIUS, target)) {
			corridor[i].burn_tick_ms = CORRIDOR_BURN_INTERVAL_MS;
			hits++;
		}
	}
	return hits;
}

/* --- Deactivate --- */

void Sub_Blaze_deactivate_all(void)
{
	for (int i = 0; i < CORRIDOR_MAX; i++) {
		corridor[i].active = false;
		Burn_reset(&corridor[i].burn);
	}
}

/* --- Accessors --- */

bool Sub_Blaze_is_dashing(void)
{
	return SubDash_is_active(&core);
}

double Sub_Blaze_get_dash_vx(void)
{
	return SubDash_is_active(&core) ? core.dirX * cfg.speed : 0.0;
}

double Sub_Blaze_get_dash_vy(void)
{
	return SubDash_is_active(&core) ? core.dirY * cfg.speed : 0.0;
}

float Sub_Blaze_get_cooldown_fraction(void)
{
	return SubDash_get_cooldown_fraction(&core, &cfg);
}

/* --- Rendering --- */

void Sub_Blaze_render_corridor(void)
{
	for (int i = 0; i < CORRIDOR_MAX; i++) {
		if (!corridor[i].active)
			continue;

		float life_frac = (float)corridor[i].life_ms / (float)CORRIDOR_LIFE_MS;
		float alpha = 0.25f * life_frac;
		float cx = (float)corridor[i].position.x;
		float cy = (float)corridor[i].position.y;

		/* Ground glow circle */
		Render_filled_circle(cx, cy, (float)CORRIDOR_RADIUS, 12,
			1.0f, 0.4f, 0.05f, alpha);
		/* Brighter inner core */
		Render_filled_circle(cx, cy, (float)CORRIDOR_RADIUS * 0.5f, 8,
			1.0f, 0.6f, 0.15f, alpha * 1.5f);
	}
}

void Sub_Blaze_render_corridor_bloom_source(void)
{
	for (int i = 0; i < CORRIDOR_MAX; i++) {
		if (!corridor[i].active)
			continue;

		float life_frac = (float)corridor[i].life_ms / (float)CORRIDOR_LIFE_MS;
		float alpha = 0.4f * life_frac;
		float cx = (float)corridor[i].position.x;
		float cy = (float)corridor[i].position.y;

		Render_filled_circle(cx, cy, (float)CORRIDOR_RADIUS * 1.3f, 12,
			1.0f, 0.5f, 0.1f, alpha);
	}
}

void Sub_Blaze_render_corridor_light_source(void)
{
	for (int i = 0; i < CORRIDOR_MAX; i++) {
		if (!corridor[i].active)
			continue;

		float life_frac = (float)corridor[i].life_ms / (float)CORRIDOR_LIFE_MS;
		float alpha = 0.35f * life_frac;
		float cx = (float)corridor[i].position.x;
		float cy = (float)corridor[i].position.y;

		Render_filled_circle(cx, cy, 120.0f, 12,
			1.0f, 0.5f, 0.1f, alpha);
	}
}
