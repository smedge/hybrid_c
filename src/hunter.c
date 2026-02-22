#include "hunter.h"
#include "sub_projectile_core.h"
#include "enemy_util.h"
#include "defender.h"
#include "fragment.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"
#include "sub_stealth.h"
#include "view.h"
#include "render.h"
#include "color.h"
#include "audio.h"
#include "map.h"
#include "enemy_registry.h"

#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL_mixer.h>

#define HUNTER_COUNT 512
#define HUNTER_SPEED 400.0
#define HUNTER_HP 100.0
#define AGGRO_RANGE 1600.0
#define DEAGGRO_RANGE 3200.0
#define FIRE_RANGE 1600.0  /* matches projectile max range: velocity * ttl/1000 */
#define IDLE_DRIFT_RADIUS 400.0
#define IDLE_DRIFT_SPEED 80.0
#define IDLE_WANDER_INTERVAL 2000

#define BURST_COUNT 3
#define BURST_INTERVAL 100
#define BURST_COOLDOWN 1500

#define DEATH_FLASH_MS 200
#define RESPAWN_MS 30000

#define BODY_SIZE 12.0
#define WALL_CHECK_DIST 50.0

static const CarriedSubroutine hunterCarried[] = {
	{ SUB_ID_MGUN, FRAG_TYPE_HUNTER },
};

typedef enum {
	HUNTER_IDLE,
	HUNTER_CHASING,
	HUNTER_SHOOTING,
	HUNTER_DYING,
	HUNTER_DEAD
} HunterAIState;

typedef struct {
	bool alive;
	double hp;
	HunterAIState aiState;
	Position spawnPoint;
	double facing;          /* degrees, for rendering */
	bool killedByPlayer;

	/* Idle wander */
	Position wanderTarget;
	int wanderTimer;

	/* Shooting */
	int burstShotsFired;
	int burstTimer;
	int cooldownTimer;

	/* Death/respawn */
	int deathTimer;
	int respawnTimer;
} HunterState;

/* Shared singleton components */
static RenderableComponent renderable = {Hunter_render};
static CollidableComponent collidable = {{-BODY_SIZE, BODY_SIZE, BODY_SIZE, -BODY_SIZE},
										  true,
										  COLLISION_LAYER_ENEMY,
										  COLLISION_LAYER_PLAYER,
										  Hunter_collide, Hunter_resolve};
static AIUpdatableComponent updatable = {Hunter_update};

/* Colors */
static const ColorFloat colorBody     = {1.0f, 0.3f, 0.0f, 1.0f};
static const ColorFloat colorAggro    = {1.0f, 0.5f, 0.1f, 1.0f};


/* State arrays */
static HunterState hunters[HUNTER_COUNT];
static PlaceableComponent placeables[HUNTER_COUNT];
static Entity *entityRefs[HUNTER_COUNT];
static int highestUsedIndex = 0;

/* Projectile pool (shared across all hunters) */
static SubProjectilePool hunterProjPool;

static const SubProjectileConfig hunterProjCfg = {
	.fire_cooldown_ms = 0,
	.velocity = 2000.0,
	.ttl_ms = 800,
	.pool_size = 64,
	.damage = 15.0,
	.color_r = 1.0f, .color_g = 0.35f, .color_b = 0.05f,
	.trail_thickness = 3.0f,
	.trail_alpha = 0.6f,
	.point_size = 8.0,
	.min_point_size = 2.0,
	.spark_duration_ms = 80,
	.spark_size = 15.0f,
	.light_proj_radius = 60.0f,
	.light_proj_r = 1.0f, .light_proj_g = 0.5f, .light_proj_b = 0.1f, .light_proj_a = 0.7f,
	.light_spark_radius = 90.0f,
	.light_spark_r = 1.0f, .light_spark_g = 0.5f, .light_spark_b = 0.1f, .light_spark_a = 0.6f,
};

/* Body-hit sparks (separate from projectile wall sparks) */
#define SPARK_POOL_SIZE 8
static struct {
	bool active;
	bool shielded;
	Position position;
	int ticksLeft;
} sparks[SPARK_POOL_SIZE];

#define BODY_SPARK_DURATION 80
#define BODY_SPARK_SIZE 12.0f

static void activate_spark(Position pos, bool shielded) {
	int slot = 0;
	for (int i = 0; i < SPARK_POOL_SIZE; i++) {
		if (!sparks[i].active) { slot = i; break; }
		if (sparks[i].ticksLeft < sparks[slot].ticksLeft) slot = i;
	}
	sparks[slot].active = true;
	sparks[slot].shielded = shielded;
	sparks[slot].position = pos;
	sparks[slot].ticksLeft = BODY_SPARK_DURATION;
}

/* Audio — entity sounds only (damage/death/respawn) */
static Mix_Chunk *sampleDeath = 0;
static Mix_Chunk *sampleRespawn = 0;
static Mix_Chunk *sampleHit = 0;

/* Helpers */
static double get_radians(double degrees)
{
	return degrees * PI / 180.0;
}

static void pick_wander_target(HunterState *h)
{
	Enemy_pick_wander_target(h->spawnPoint, IDLE_DRIFT_RADIUS, IDLE_WANDER_INTERVAL,
		&h->wanderTarget, &h->wanderTimer);
}

/* ---- Public API ---- */

void Hunter_initialize(Position position)
{
	if (highestUsedIndex >= HUNTER_COUNT) {
		printf("FATAL ERROR: Too many hunter entities.\n");
		return;
	}

	int idx = highestUsedIndex;
	HunterState *h = &hunters[idx];

	h->alive = true;
	h->hp = HUNTER_HP;
	h->aiState = HUNTER_IDLE;
	h->spawnPoint = position;
	h->facing = 0.0;
	h->killedByPlayer = false;
	h->burstShotsFired = 0;
	h->burstTimer = 0;
	h->cooldownTimer = 0;
	h->deathTimer = 0;
	h->respawnTimer = 0;
	pick_wander_target(h);

	placeables[idx].position = position;
	placeables[idx].heading = 0.0;

	Entity entity = Entity_initialize_entity();
	entity.state = &hunters[idx];
	entity.placeable = &placeables[idx];
	entity.renderable = &renderable;
	entity.collidable = &collidable;
	entity.aiUpdatable = &updatable;

	entityRefs[idx] = Entity_add_entity(entity);

	highestUsedIndex++;

	/* Load audio and register with enemy registry once, not per-entity */
	if (!sampleDeath) {
		SubProjectile_pool_init(&hunterProjPool, hunterProjCfg.pool_size);
		Audio_load_sample(&sampleDeath, "resources/sounds/bomb_explode.wav");
		Audio_load_sample(&sampleRespawn, "resources/sounds/door.wav");
		Audio_load_sample(&sampleHit, "resources/sounds/samus_hurt.wav");

		EnemyTypeCallbacks cb = {Hunter_find_wounded, Hunter_find_aggro, Hunter_heal};
		EnemyRegistry_register(cb);
	}
}

void Hunter_cleanup(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		if (entityRefs[i]) {
			entityRefs[i]->empty = true;
			entityRefs[i] = NULL;
		}
	}
	highestUsedIndex = 0;

	SubProjectile_deactivate_all(&hunterProjPool);
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;

	Audio_unload_sample(&sampleDeath);
	Audio_unload_sample(&sampleRespawn);
	Audio_unload_sample(&sampleHit);
}

Collision Hunter_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	HunterState *h = (HunterState *)state;
	Collision collision = {false, false};

	if (!h->alive)
		return collision;

	Rectangle thisBB = collidable.boundingBox;
	Rectangle transformed = Collision_transform_bounding_box(placeable->position, thisBB);

	if (Collision_aabb_test(transformed, boundingBox)) {
		collision.collisionDetected = true;
		collision.solid = true;
		Sub_Stealth_break();
	}

	return collision;
}

void Hunter_resolve(void *state, const Collision collision)
{
	/* Hunter doesn't react to collision resolution from ship touching it.
	   Damage is handled via projectile check_hit in update. */
	(void)state;
	(void)collision;
}

void Hunter_update(void *state, const PlaceableComponent *placeable, unsigned int ticks)
{
	HunterState *h = (HunterState *)state;
	/* Find our index to get the mutable placeable */
	int idx = (int)(h - hunters);
	PlaceableComponent *pl = &placeables[idx];
	double dt = ticks / 1000.0;

	if (h->alive)
		Enemy_check_stealth_proximity(pl->position, h->facing);

	/* --- Check for incoming player projectiles (if alive and not dying) --- */
	if (h->alive && h->aiState != HUNTER_DYING) {
		Rectangle body = {-BODY_SIZE, BODY_SIZE, BODY_SIZE, -BODY_SIZE};
		Rectangle hitBox = Collision_transform_bounding_box(pl->position, body);

		/* Check all player weapon types */
		PlayerDamageResult dmg = Enemy_check_player_damage(hitBox, pl->position);
		bool shielded = Defender_is_protecting(pl->position, dmg.ambush);
		bool hit = dmg.hit;
		if (hit && !shielded)
			h->hp -= dmg.damage + dmg.mine_damage;

		if (hit) {
			activate_spark(pl->position, shielded);
			if (shielded)
				Defender_notify_shield_hit(pl->position);
			else
				Audio_play_sample(&sampleHit);

			/* Getting shot immediately aggroes */
			if (h->aiState == HUNTER_IDLE) {
				h->aiState = HUNTER_CHASING;
				h->cooldownTimer = 0;
			}
		}

		if (hit && h->hp <= 0.0) {
			h->alive = false;
			h->aiState = HUNTER_DYING;
			h->deathTimer = 0;
			h->killedByPlayer = true;
			Audio_play_sample(&sampleDeath);
			Enemy_on_player_kill(&dmg);
		}
	}

	/* --- State machine --- */
	switch (h->aiState) {
	case HUNTER_IDLE: {
		/* Wander toward target */
		Enemy_move_toward(pl, h->wanderTarget, IDLE_DRIFT_SPEED, dt, WALL_CHECK_DIST);

		h->wanderTimer -= ticks;
		if (h->wanderTimer <= 0)
			pick_wander_target(h);

		/* Check aggro — requires line of sight and ship alive */
		Position shipPos = Ship_get_position();
		double dist = Enemy_distance_between(pl->position, shipPos);
		bool nearbyShot = Enemy_check_any_nearby(pl->position, 200.0);
		if (!Ship_is_destroyed() && !Sub_Stealth_is_stealthed() &&
			((dist < AGGRO_RANGE && Enemy_has_line_of_sight(pl->position, shipPos)) || nearbyShot)) {
			h->aiState = HUNTER_CHASING;
			h->cooldownTimer = 0;
		}

		/* Face wander target */
		h->facing = Position_get_heading(pl->position, h->wanderTarget);
		break;
	}
	case HUNTER_CHASING: {
		Position shipPos = Ship_get_position();
		double dist = Enemy_distance_between(pl->position, shipPos);
		bool los = Enemy_has_line_of_sight(pl->position, shipPos);

		/* De-aggro: out of range, lost line of sight, ship dead, or stealthed */
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed() || dist > DEAGGRO_RANGE || !los) {
			h->aiState = HUNTER_IDLE;
			pick_wander_target(h);
			break;
		}

		/* Move toward player */
		Enemy_move_toward(pl, shipPos, HUNTER_SPEED, dt, WALL_CHECK_DIST);
		h->facing = Position_get_heading(pl->position, shipPos);

		/* Cooldown tick */
		if (h->cooldownTimer > 0)
			h->cooldownTimer -= ticks;

		/* In fire range and off cooldown? Start burst */
		if (dist < FIRE_RANGE && h->cooldownTimer <= 0) {
			h->aiState = HUNTER_SHOOTING;
			h->burstShotsFired = 0;
			h->burstTimer = 0;
		}
		break;
	}
	case HUNTER_SHOOTING: {
		Position shipPos = Ship_get_position();
		h->facing = Position_get_heading(pl->position, shipPos);

		h->burstTimer -= ticks;
		if (h->burstTimer <= 0 && h->burstShotsFired < BURST_COUNT) {
			SubProjectile_try_fire(&hunterProjPool, &hunterProjCfg, pl->position, shipPos);
			h->burstShotsFired++;
			h->burstTimer = BURST_INTERVAL;
		}

		/* Burst complete */
		if (h->burstShotsFired >= BURST_COUNT) {
			h->aiState = HUNTER_CHASING;
			h->cooldownTimer = BURST_COOLDOWN;
		}

		/* De-aggro: out of range, lost line of sight, ship dead, or stealthed */
		double dist = Enemy_distance_between(pl->position, shipPos);
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed() || dist > DEAGGRO_RANGE || !Enemy_has_line_of_sight(pl->position, shipPos)) {
			h->aiState = HUNTER_IDLE;
			pick_wander_target(h);
		}
		break;
	}
	case HUNTER_DYING:
		h->deathTimer += ticks;
		if (h->deathTimer >= DEATH_FLASH_MS) {
			h->aiState = HUNTER_DEAD;
			h->respawnTimer = 0;

			/* Drop fragment */
			if (h->killedByPlayer)
				Enemy_drop_fragments(pl->position, hunterCarried, 1);
		}
		break;

	case HUNTER_DEAD:
		h->respawnTimer += ticks;
		if (h->respawnTimer >= RESPAWN_MS) {
			/* Respawn at original position */
			h->alive = true;
			h->hp = HUNTER_HP;
			h->aiState = HUNTER_IDLE;
			h->killedByPlayer = false;
			h->cooldownTimer = 0;
			h->burstShotsFired = 0;
			pl->position = h->spawnPoint;
			pick_wander_target(h);
			Audio_play_sample(&sampleRespawn);
		}
		break;
	}

	/* Gravity well pull (alive and not dying/dead) */
	if (h->alive && h->aiState != HUNTER_DYING && h->aiState != HUNTER_DEAD)
		Enemy_apply_gravity(pl, dt);

	/* --- Update projectiles (done for ALL hunters from any update call) --- */
	/* Only do this from hunter index 0 to avoid processing projectiles N times */
	if (idx == 0) {
		SubProjectile_update(&hunterProjPool, &hunterProjCfg, ticks);

		/* Check player hit */
		if (!Ship_is_destroyed()) {
			Position shipPos = Ship_get_position();
			Rectangle shipBB = {-SHIP_BB_HALF_SIZE, SHIP_BB_HALF_SIZE, SHIP_BB_HALF_SIZE, -SHIP_BB_HALF_SIZE};
			Rectangle shipWorld = Collision_transform_bounding_box(shipPos, shipBB);
			double dmg = SubProjectile_check_hit(&hunterProjPool, &hunterProjCfg, shipWorld);
			if (dmg > 0)
				PlayerStats_damage(dmg);
		}

		/* Body-hit spark decay */
		for (int si = 0; si < SPARK_POOL_SIZE; si++) {
			if (sparks[si].active) {
				sparks[si].ticksLeft -= ticks;
				if (sparks[si].ticksLeft <= 0)
					sparks[si].active = false;
			}
		}
	}
}

void Hunter_render(const void *state, const PlaceableComponent *placeable)
{
	HunterState *h = (HunterState *)state;

	/* Shared pool render must happen regardless of this hunter's alive state */
	int idx = (int)((HunterState *)state - hunters);
	if (idx == 0) {
		SubProjectile_render(&hunterProjPool, &hunterProjCfg);

		for (int si = 0; si < SPARK_POOL_SIZE; si++) {
			if (sparks[si].active) {
				Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
					BODY_SPARK_DURATION, BODY_SPARK_SIZE, sparks[si].shielded,
					1.0f, 0.5f, 0.1f);
			}
		}
	}

	if (h->aiState == HUNTER_DEAD)
		return;

	/* Death flash */
	if (h->aiState == HUNTER_DYING) {
		Enemy_render_death_flash(placeable, (float)h->deathTimer, (float)DEATH_FLASH_MS);
		return;
	}

	/* Choose color based on aggro state */
	const ColorFloat *bodyColor = (h->aiState == HUNTER_IDLE) ? &colorBody : &colorAggro;

	/* Render as a triangle pointing in facing direction */
	float rad = (float)get_radians(h->facing);
	float s = BODY_SIZE;

	/* Triangle vertices: tip forward, two back corners */
	float tipX = (float)placeable->position.x + sinf(rad) * s * 1.5f;
	float tipY = (float)placeable->position.y + cosf(rad) * s * 1.5f;

	float backRad1 = rad + (float)(PI * 0.75);
	float backRad2 = rad - (float)(PI * 0.75);
	float b1x = (float)placeable->position.x + sinf(backRad1) * s;
	float b1y = (float)placeable->position.y + cosf(backRad1) * s;
	float b2x = (float)placeable->position.x + sinf(backRad2) * s;
	float b2y = (float)placeable->position.y + cosf(backRad2) * s;

	/* Render as thick lines forming a triangle (triangles flush order) */
	Render_thick_line(tipX, tipY, b1x, b1y, 2.0f,
		bodyColor->red, bodyColor->green, bodyColor->blue, bodyColor->alpha);
	Render_thick_line(b1x, b1y, b2x, b2y, 2.0f,
		bodyColor->red, bodyColor->green, bodyColor->blue, bodyColor->alpha);
	Render_thick_line(b2x, b2y, tipX, tipY, 2.0f,
		bodyColor->red, bodyColor->green, bodyColor->blue, bodyColor->alpha);

	/* Center dot */
	Render_point(&placeable->position, 4.0, bodyColor);
}

void Hunter_render_bloom_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		HunterState *h = &hunters[i];
		PlaceableComponent *pl = &placeables[i];

		if (h->aiState == HUNTER_DEAD)
			continue;

		if (h->aiState == HUNTER_DYING) {
			Enemy_render_death_flash(pl, (float)h->deathTimer, (float)DEATH_FLASH_MS);
			continue;
		}

		/* Body glow */
		const ColorFloat *bodyColor = (h->aiState == HUNTER_IDLE) ? &colorBody : &colorAggro;
		Render_point(&pl->position, 6.0, bodyColor);
	}

	/* Projectile bloom */
	SubProjectile_render_bloom(&hunterProjPool, &hunterProjCfg);

	/* Body-hit spark bloom */
	for (int si = 0; si < SPARK_POOL_SIZE; si++) {
		if (sparks[si].active) {
			Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
				BODY_SPARK_DURATION, BODY_SPARK_SIZE, sparks[si].shielded,
				1.0f, 0.5f, 0.1f);
		}
	}
}

void Hunter_render_light_source(void)
{
	SubProjectile_render_light(&hunterProjPool, &hunterProjCfg);

	for (int si = 0; si < SPARK_POOL_SIZE; si++) {
		if (sparks[si].active) {
			Render_filled_circle(
				(float)sparks[si].position.x,
				(float)sparks[si].position.y,
				300.0f, 12, 1.0f, 0.5f, 0.1f, 0.6f);
		}
	}
}

void Hunter_deaggro_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		HunterState *h = &hunters[i];
		if (h->aiState == HUNTER_CHASING || h->aiState == HUNTER_SHOOTING) {
			h->aiState = HUNTER_IDLE;
			h->cooldownTimer = 0;
			h->burstShotsFired = 0;
			pick_wander_target(h);
		}
	}

	/* Kill all in-flight hunter projectiles */
	SubProjectile_deactivate_all(&hunterProjPool);
}

void Hunter_reset_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		HunterState *h = &hunters[i];
		h->alive = true;
		h->hp = HUNTER_HP;
		h->aiState = HUNTER_IDLE;
		h->killedByPlayer = false;
		h->cooldownTimer = 0;
		h->burstShotsFired = 0;
		h->burstTimer = 0;
		h->deathTimer = 0;
		h->respawnTimer = 0;
		placeables[i].position = h->spawnPoint;
		pick_wander_target(h);
	}

	SubProjectile_deactivate_all(&hunterProjPool);
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;
}

bool Hunter_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index)
{
	double bestDamage = 0.0;
	int bestIdx = -1;

	for (int i = 0; i < highestUsedIndex; i++) {
		HunterState *h = &hunters[i];
		if (!h->alive || h->aiState == HUNTER_DYING || h->aiState == HUNTER_DEAD)
			continue;
		if (h->hp >= hp_threshold)
			continue;
		double missing = HUNTER_HP - h->hp;
		if (missing <= 0.0)
			continue;
		double dist = Enemy_distance_between(from, placeables[i].position);
		if (dist > range)
			continue;
		if (missing > bestDamage) {
			bestDamage = missing;
			bestIdx = i;
		}
	}

	if (bestIdx >= 0) {
		*out_pos = placeables[bestIdx].position;
		*out_index = bestIdx;
		return true;
	}
	return false;
}

bool Hunter_find_aggro(Position from, double range, Position *out_pos)
{
	double bestDist = range + 1.0;
	int bestIdx = -1;

	for (int i = 0; i < highestUsedIndex; i++) {
		HunterState *h = &hunters[i];
		if (!h->alive)
			continue;
		if (h->aiState != HUNTER_CHASING && h->aiState != HUNTER_SHOOTING)
			continue;
		double dist = Enemy_distance_between(from, placeables[i].position);
		if (dist > range)
			continue;
		if (dist < bestDist) {
			bestDist = dist;
			bestIdx = i;
		}
	}

	if (bestIdx >= 0) {
		*out_pos = placeables[bestIdx].position;
		return true;
	}
	return false;
}

void Hunter_heal(int index, double amount)
{
	if (index < 0 || index >= highestUsedIndex)
		return;
	HunterState *h = &hunters[index];
	if (!h->alive)
		return;
	h->hp += amount;
	if (h->hp > HUNTER_HP)
		h->hp = HUNTER_HP;
}
