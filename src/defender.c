#include "defender.h"
#include "enemy_util.h"
#include "sub_pea.h"
#include "sub_mgun.h"
#include "sub_mine.h"
#include "hunter.h"
#include "seeker.h"
#include "fragment.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"
#include "view.h"
#include "render.h"
#include "color.h"
#include "audio.h"
#include "map.h"

#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL_mixer.h>

#define DEFENDER_COUNT 64
#define DEFENDER_HP 80.0
#define NORMAL_SPEED 250.0
#define FLEE_SPEED 400.0
#define BOOST_MULTIPLIER 2.0
#define AGGRO_RANGE 1200.0
#define DEAGGRO_RANGE 3200.0
#define HEAL_RANGE 800.0
#define HEAL_AMOUNT 50.0
#define HEAL_COOLDOWN_MS 4000
#define FLEE_TRIGGER_RANGE 400.0
#define FLEE_SAFE_RANGE 600.0
#define IDLE_DRIFT_RADIUS 400.0
#define IDLE_DRIFT_SPEED 80.0
#define IDLE_WANDER_INTERVAL 2000
#define WALL_CHECK_DIST 50.0

#define AEGIS_DURATION_MS 10000
#define AEGIS_COOLDOWN_MS 30000

#define DEATH_FLASH_MS 200
#define RESPAWN_MS 30000

#define BODY_SIZE 10.0
#define PROTECT_RADIUS 100.0
#define SHIELD_CHASE_SPEED 800.0
#define HEAL_BEAM_DURATION_MS 200
#define BOOST_TRAIL_GHOSTS 12
#define BOOST_TRAIL_LENGTH 3.0

typedef enum {
	DEFENDER_IDLE,
	DEFENDER_SUPPORTING,
	DEFENDER_FLEEING,
	DEFENDER_DYING,
	DEFENDER_DEAD
} DefenderAIState;

typedef struct {
	bool alive;
	double hp;
	DefenderAIState aiState;
	Position spawnPoint;
	double facing;
	bool killedByPlayer;

	/* Idle wander */
	Position wanderTarget;
	int wanderTimer;

	/* Healing */
	int healCooldown;
	bool healBeamActive;
	int healBeamTimer;
	Position healBeamTarget;

	/* Aegis (enemy self-shield) */
	bool aegisActive;
	int aegisDuration;
	int aegisCooldown;

	/* Boost */
	bool boosting;
	Position prevPosition;

	/* Frame-level snapshot so protection survives same-frame mine break */
	bool aegisWasActive;

	/* Brief invulnerability after mine pops aegis (outlasts explosion) */
	int shieldBreakGrace;

	/* Death/respawn */
	int deathTimer;
	int respawnTimer;
} DefenderState;

/* Shared singleton components */
static RenderableComponent renderable = {Defender_render};
static CollidableComponent collidable = {{-BODY_SIZE, BODY_SIZE, BODY_SIZE, -BODY_SIZE},
										  true,
										  COLLISION_LAYER_ENEMY,
										  COLLISION_LAYER_PLAYER,
										  Defender_collide, Defender_resolve};
static AIUpdatableComponent updatable = {Defender_update};

/* Colors */
static const ColorFloat colorBody   = {0.3f, 0.7f, 1.0f, 1.0f};
static const ColorFloat colorAggro  = {0.5f, 0.8f, 1.0f, 1.0f};
static const ColorFloat colorAegis  = {0.6f, 0.9f, 1.0f, 0.8f};

/* State arrays */
static DefenderState defenders[DEFENDER_COUNT];
static PlaceableComponent placeables[DEFENDER_COUNT];
static Entity *entityRefs[DEFENDER_COUNT];
static int highestUsedIndex = 0;

/* Sparks */
#define SPARK_DURATION 80
#define SPARK_SIZE 12.0
#define SPARK_POOL_SIZE 8
static struct {
	bool active;
	bool shielded;
	Position position;
	int ticksLeft;
} sparks[SPARK_POOL_SIZE];

static void activate_spark(Position pos, bool shielded) {
	int slot = 0;
	for (int i = 0; i < SPARK_POOL_SIZE; i++) {
		if (!sparks[i].active) { slot = i; break; }
		if (sparks[i].ticksLeft < sparks[slot].ticksLeft) slot = i;
	}
	sparks[slot].active = true;
	sparks[slot].shielded = shielded;
	sparks[slot].position = pos;
	sparks[slot].ticksLeft = SPARK_DURATION;
}

/* Audio */
static Mix_Chunk *sampleHeal = 0;
static Mix_Chunk *sampleAegis = 0;
static Mix_Chunk *sampleDeath = 0;
static Mix_Chunk *sampleRespawn = 0;
static Mix_Chunk *sampleHit = 0;
static Mix_Chunk *sampleShieldHit = 0;

/* Helpers */
static void pick_wander_target(DefenderState *d)
{
	Enemy_pick_wander_target(d->spawnPoint, IDLE_DRIFT_RADIUS, IDLE_WANDER_INTERVAL,
		&d->wanderTarget, &d->wanderTimer);
}

static bool try_heal_ally(DefenderState *d, PlaceableComponent *pl)
{
	if (d->healCooldown > 0)
		return false;

	Position healPos;
	int healIdx;
	int bestType = -1; /* 0 = hunter, 1 = seeker */
	Position bestPos = {0, 0};
	int bestIdx = -1;

	/* Check hunters */
	if (Hunter_find_wounded(pl->position, HEAL_RANGE, 50.0, &healPos, &healIdx)) {
		bestType = 0;
		bestPos = healPos;
		bestIdx = healIdx;
	}

	/* Check seekers — pick whichever is closer */
	if (Seeker_find_wounded(pl->position, HEAL_RANGE, 50.0, &healPos, &healIdx)) {
		double distH = bestType >= 0 ? Enemy_distance_between(pl->position, bestPos) : 99999.0;
		double distS = Enemy_distance_between(pl->position, healPos);
		if (bestType < 0 || distS < distH) {
			bestType = 1;
			bestPos = healPos;
			bestIdx = healIdx;
		}
	}

	if (bestType < 0)
		return false;

	/* Heal the ally */
	if (bestType == 0)
		Hunter_heal(bestIdx, HEAL_AMOUNT);
	else
		Seeker_heal(bestIdx, HEAL_AMOUNT);

	d->healCooldown = HEAL_COOLDOWN_MS;
	d->healBeamActive = true;
	d->healBeamTimer = HEAL_BEAM_DURATION_MS;
	d->healBeamTarget = bestPos;
	Audio_play_sample(&sampleHeal);
	return true;
}

static void activate_aegis(DefenderState *d)
{
	if (d->aegisActive || d->aegisCooldown > 0)
		return;
	d->aegisActive = true;
	d->aegisDuration = AEGIS_DURATION_MS;
	Audio_play_sample(&sampleAegis);
}

/* ---- Public API ---- */

void Defender_initialize(Position position)
{
	if (highestUsedIndex >= DEFENDER_COUNT) {
		printf("FATAL ERROR: Too many defender entities.\n");
		return;
	}

	int idx = highestUsedIndex;
	DefenderState *d = &defenders[idx];

	d->alive = true;
	d->hp = DEFENDER_HP;
	d->aiState = DEFENDER_IDLE;
	d->spawnPoint = position;
	d->facing = 0.0;
	d->killedByPlayer = false;
	d->healCooldown = 0;
	d->healBeamActive = false;
	d->healBeamTimer = 0;
	d->aegisActive = false;
	d->aegisDuration = 0;
	d->aegisCooldown = 0;
	d->shieldBreakGrace = 0;
	d->boosting = false;
	d->prevPosition = position;
	d->deathTimer = 0;
	d->respawnTimer = 0;
	pick_wander_target(d);

	placeables[idx].position = position;
	placeables[idx].heading = 0.0;

	Entity entity = Entity_initialize_entity();
	entity.state = &defenders[idx];
	entity.placeable = &placeables[idx];
	entity.renderable = &renderable;
	entity.collidable = &collidable;
	entity.aiUpdatable = &updatable;

	entityRefs[idx] = Entity_add_entity(entity);

	highestUsedIndex++;

	/* Load audio once, not per-entity */
	if (!sampleHeal) {
		Audio_load_sample(&sampleHeal, "resources/sounds/refill_start.wav");
		Audio_load_sample(&sampleAegis, "resources/sounds/statue_rise.wav");
		Audio_load_sample(&sampleDeath, "resources/sounds/bomb_explode.wav");
		Audio_load_sample(&sampleRespawn, "resources/sounds/door.wav");
		Audio_load_sample(&sampleHit, "resources/sounds/samus_hurt.wav");
		Audio_load_sample(&sampleShieldHit, "resources/sounds/ricochet.wav");
	}
}

void Defender_cleanup(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		if (entityRefs[i]) {
			entityRefs[i]->empty = true;
			entityRefs[i] = NULL;
		}
	}
	highestUsedIndex = 0;
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;

	Audio_unload_sample(&sampleHeal);
	Audio_unload_sample(&sampleAegis);
	Audio_unload_sample(&sampleDeath);
	Audio_unload_sample(&sampleRespawn);
	Audio_unload_sample(&sampleHit);
	Audio_unload_sample(&sampleShieldHit);
}

Collision Defender_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	DefenderState *d = (DefenderState *)state;
	Collision collision = {false, false};

	if (!d->alive)
		return collision;

	Rectangle thisBB = collidable.boundingBox;
	Rectangle transformed = Collision_transform_bounding_box(placeable->position, thisBB);

	if (Collision_aabb_test(transformed, boundingBox)) {
		collision.collisionDetected = true;
		collision.solid = true;
	}

	return collision;
}

void Defender_resolve(void *state, const Collision collision)
{
	(void)state;
	(void)collision;
}

void Defender_update(void *state, const PlaceableComponent *placeable, unsigned int ticks)
{
	DefenderState *d = (DefenderState *)state;
	int idx = (int)(d - defenders);
	PlaceableComponent *pl = &placeables[idx];
	double dt = ticks / 1000.0;

	d->prevPosition = pl->position;
	d->boosting = false;
	d->aegisWasActive = d->aegisActive;

	/* --- Tick timers --- */
	if (d->healCooldown > 0)
		d->healCooldown -= ticks;
	if (d->healBeamActive) {
		d->healBeamTimer -= ticks;
		if (d->healBeamTimer <= 0)
			d->healBeamActive = false;
	}

	/* Aegis timer */
	if (d->aegisActive) {
		d->aegisDuration -= ticks;
		if (d->aegisDuration <= 0) {
			d->aegisActive = false;
			d->aegisCooldown = AEGIS_COOLDOWN_MS;
		}
	} else if (d->aegisCooldown > 0) {
		d->aegisCooldown -= ticks;
	}

	/* Shield break grace — brief invulnerability after mine pops aegis */
	if (d->shieldBreakGrace > 0)
		d->shieldBreakGrace -= ticks;

	/* --- Check for incoming player projectiles --- */
	if (d->alive && d->aiState != DEFENDER_DYING && d->shieldBreakGrace <= 0) {
		Rectangle body = {-BODY_SIZE, BODY_SIZE, BODY_SIZE, -BODY_SIZE};
		Rectangle hitBox = Collision_transform_bounding_box(pl->position, body);

		bool hit = false;
		bool mineHit = false;
		double damage = 0.0;
		if (Sub_Pea_check_hit(hitBox)) {
			damage += 50.0;
			hit = true;
		}
		if (Sub_Mgun_check_hit(hitBox)) {
			damage += 20.0;
			hit = true;
		}
		if (Sub_Mine_check_hit(hitBox)) {
			hit = true;
			mineHit = true;
			if (!d->aegisActive)
				damage += 100.0;
		}

		if (hit) {
			activate_spark(pl->position, d->aegisActive);

			if (d->aegisActive) {
				if (mineHit) {
					/* Mine breaks aegis shield — no damage, grace outlasts explosion */
					d->aegisActive = false;
					d->aegisCooldown = AEGIS_COOLDOWN_MS;
					d->shieldBreakGrace = 200; /* > EXPLODING_TIME (100ms) */
				}
				Audio_play_sample(&sampleShieldHit);
			} else {
				d->hp -= damage;
				Audio_play_sample(&sampleHit);

				/* Getting hit while idle triggers flee */
				if (d->aiState == DEFENDER_IDLE) {
					d->aiState = DEFENDER_FLEEING;
				}
			}
		}

		if (!d->aegisActive && hit && d->hp <= 0.0) {
			d->alive = false;
			d->aiState = DEFENDER_DYING;
			d->deathTimer = 0;
			d->killedByPlayer = true;
			Audio_play_sample(&sampleDeath);
		}
	}

	/* --- State machine --- */
	switch (d->aiState) {
	case DEFENDER_IDLE: {
		/* Wander */
		Enemy_move_toward(pl, d->wanderTarget, IDLE_DRIFT_SPEED, dt, WALL_CHECK_DIST);
		d->wanderTimer -= ticks;
		if (d->wanderTimer <= 0)
			pick_wander_target(d);

		/* Check for nearby wounded allies — transition to supporting */
		Position allyPos;
		int allyIdx;
		if (Hunter_find_wounded(pl->position, HEAL_RANGE, 50.0, &allyPos, &allyIdx) ||
			Seeker_find_wounded(pl->position, HEAL_RANGE, 50.0, &allyPos, &allyIdx)) {
			d->aiState = DEFENDER_SUPPORTING;
		}

		/* Nearby ally went aggro — rush to support */
		if (d->aiState == DEFENDER_IDLE) {
			Position aggroPos;
			if (Hunter_find_aggro(pl->position, 1600.0, &aggroPos) ||
				Seeker_find_aggro(pl->position, 1600.0, &aggroPos)) {
				d->aiState = DEFENDER_SUPPORTING;
			}
		}

		/* Check aggro — player proximity triggers awareness */
		Position shipPos = Ship_get_position();
		double dist = Enemy_distance_between(pl->position, shipPos);
		bool nearbyShot = Sub_Pea_check_nearby(pl->position, 200.0)
						|| Sub_Mgun_check_nearby(pl->position, 200.0);
		if (!Ship_is_destroyed() &&
			((dist < AGGRO_RANGE && Enemy_has_line_of_sight(pl->position, shipPos)) || nearbyShot)) {
			d->aiState = DEFENDER_SUPPORTING;
		}

		d->facing = Position_get_heading(pl->position, d->wanderTarget);
		break;
	}
	case DEFENDER_SUPPORTING: {
		Position shipPos = Ship_get_position();
		double shipDist = Enemy_distance_between(pl->position, shipPos);

		/* De-aggro check */
		if (Ship_is_destroyed() || shipDist > DEAGGRO_RANGE) {
			d->aiState = DEFENDER_IDLE;
			pick_wander_target(d);
			break;
		}

		/* Try to heal an ally */
		try_heal_ally(d, pl);

		/* Flee if player gets close — but hold ground while actively shielding */
		if (shipDist < FLEE_TRIGGER_RANGE && !d->aegisActive) {
			d->aiState = DEFENDER_FLEEING;
			break;
		}

		/* Boost toward wounded allies first, then aggro'd allies */
		Position allyPos;
		int allyIdx;
		bool hasTarget = false;
		if (Hunter_find_wounded(pl->position, HEAL_RANGE * 1.5, 50.0, &allyPos, &allyIdx) ||
			Seeker_find_wounded(pl->position, HEAL_RANGE * 1.5, 50.0, &allyPos, &allyIdx)) {
			hasTarget = true;
		} else if (Hunter_find_aggro(pl->position, 1600.0, &allyPos) ||
				   Seeker_find_aggro(pl->position, 1600.0, &allyPos)) {
			hasTarget = true;
		}

		if (hasTarget) {
			/* When aegis is active, chase faster to stay glued to ally */
			double chaseSpeed = d->aegisActive ? SHIELD_CHASE_SPEED : NORMAL_SPEED * BOOST_MULTIPLIER;
			d->boosting = true;
			Enemy_move_toward(pl, allyPos, chaseSpeed, dt, WALL_CHECK_DIST);
			d->facing = Position_get_heading(pl->position, allyPos);

			/* Close enough to protect — pop aegis */
			if (Enemy_distance_between(pl->position, allyPos) < PROTECT_RADIUS)
				activate_aegis(d);
		} else {
			/* No allies in need — drift around spawn but stay aware */
			Enemy_move_toward(pl, d->wanderTarget, IDLE_DRIFT_SPEED, dt, WALL_CHECK_DIST);
			d->wanderTimer -= ticks;
			if (d->wanderTimer <= 0)
				pick_wander_target(d);
			d->facing = Position_get_heading(pl->position, shipPos);
		}
		break;
	}
	case DEFENDER_FLEEING: {
		Position shipPos = Ship_get_position();
		double shipDist = Enemy_distance_between(pl->position, shipPos);

		/* De-aggro */
		if (Ship_is_destroyed() || shipDist > DEAGGRO_RANGE) {
			d->aiState = DEFENDER_IDLE;
			pick_wander_target(d);
			break;
		}

		/* Run away from player */
		Enemy_move_away_from(pl, shipPos, FLEE_SPEED, dt, WALL_CHECK_DIST);
		d->facing = Position_get_heading(pl->position, shipPos);

		/* Still try to heal allies while fleeing */
		try_heal_ally(d, pl);

		/* Safe distance — go back to supporting */
		if (shipDist > FLEE_SAFE_RANGE)
			d->aiState = DEFENDER_SUPPORTING;
		break;
	}
	case DEFENDER_DYING:
		d->deathTimer += ticks;
		if (d->deathTimer >= DEATH_FLASH_MS) {
			d->aiState = DEFENDER_DEAD;
			d->respawnTimer = 0;

			/* Drop fragment — prioritize locked subroutines */
			if (d->killedByPlayer) {
				bool mend_locked = !Progression_is_unlocked(SUB_ID_MEND);
				bool aegis_locked = !Progression_is_unlocked(SUB_ID_AEGIS);
				if (mend_locked || aegis_locked) {
					FragmentType ftype;
					if (mend_locked && aegis_locked)
						ftype = (rand() % 2 == 0) ? FRAG_TYPE_MEND : FRAG_TYPE_AEGIS;
					else if (mend_locked)
						ftype = FRAG_TYPE_MEND;
					else
						ftype = FRAG_TYPE_AEGIS;
					Fragment_spawn(pl->position, ftype);
				}
			}
		}
		break;

	case DEFENDER_DEAD:
		d->respawnTimer += ticks;
		if (d->respawnTimer >= RESPAWN_MS) {
			d->alive = true;
			d->hp = DEFENDER_HP;
			d->aiState = DEFENDER_IDLE;
			d->killedByPlayer = false;
			d->healCooldown = 0;
			d->aegisActive = false;
			d->aegisDuration = 0;
			d->aegisCooldown = 0;
			d->shieldBreakGrace = 0;
			pl->position = d->spawnPoint;
			pick_wander_target(d);
			Audio_play_sample(&sampleRespawn);
		}
		break;
	}

	/* --- Spark decay (from idx 0 only) --- */
	if (idx == 0) {
		for (int si = 0; si < SPARK_POOL_SIZE; si++) {
			if (sparks[si].active) {
				sparks[si].ticksLeft -= ticks;
				if (sparks[si].ticksLeft <= 0)
					sparks[si].active = false;
			}
		}
	}
}

static void render_hexagon(Position pos, float radius, float thickness,
	float r, float g, float b, float a)
{
	for (int i = 0; i < 6; i++) {
		float a0 = i * 60.0f * PI / 180.0f;
		float a1 = (i + 1) * 60.0f * PI / 180.0f;
		float x0 = (float)pos.x + cosf(a0) * radius;
		float y0 = (float)pos.y + sinf(a0) * radius;
		float x1 = (float)pos.x + cosf(a1) * radius;
		float y1 = (float)pos.y + sinf(a1) * radius;
		Render_thick_line(x0, y0, x1, y1, thickness, r, g, b, a);
	}
}

void Defender_render(const void *state, const PlaceableComponent *placeable)
{
	DefenderState *d = (DefenderState *)state;
	int idx = (int)(d - defenders);

	if (d->aiState == DEFENDER_DEAD)
		return;

	/* Death flash */
	if (d->aiState == DEFENDER_DYING) {
		Enemy_render_death_flash(placeable, (float)d->deathTimer, (float)DEATH_FLASH_MS);
		return;
	}

	/* Body hexagon */
	const ColorFloat *bodyColor = (d->aiState == DEFENDER_IDLE) ? &colorBody : &colorAggro;

	/* Boost motion trail */
	if (d->boosting) {
		double dx = placeable->position.x - d->prevPosition.x;
		double dy = placeable->position.y - d->prevPosition.y;
		for (int g = BOOST_TRAIL_GHOSTS; g >= 1; g--) {
			float t = (float)g / (float)(BOOST_TRAIL_GHOSTS + 1);
			Position ghost;
			ghost.x = placeable->position.x - dx * BOOST_TRAIL_LENGTH * t;
			ghost.y = placeable->position.y - dy * BOOST_TRAIL_LENGTH * t;
			float alpha = (1.0f - t) * 0.35f;
			render_hexagon(ghost, BODY_SIZE, 1.5f,
				bodyColor->red, bodyColor->green, bodyColor->blue, alpha);
		}
	}

	render_hexagon(placeable->position, BODY_SIZE, 2.0f,
		bodyColor->red, bodyColor->green, bodyColor->blue, bodyColor->alpha);

	/* Center dot */
	Render_point(&placeable->position, 4.0, bodyColor);

	/* Aegis shield ring */
	if (d->aegisActive) {
		float pulse = 0.6f + 0.4f * sinf(d->aegisDuration / 200.0f);
		render_hexagon(placeable->position, BODY_SIZE + 8.0f, 1.5f,
			colorAegis.red, colorAegis.green, colorAegis.blue, pulse);
	}

	/* Heal beam */
	if (d->healBeamActive) {
		float fade = (float)d->healBeamTimer / HEAL_BEAM_DURATION_MS;
		Render_thick_line(
			(float)placeable->position.x, (float)placeable->position.y,
			(float)d->healBeamTarget.x, (float)d->healBeamTarget.y,
			2.5f, 0.3f, 0.7f, 1.0f, fade);
	}

	/* Sparks (from idx 0 only) */
	if (idx == 0) {
		for (int si = 0; si < SPARK_POOL_SIZE; si++) {
			if (sparks[si].active) {
				Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
					SPARK_DURATION, SPARK_SIZE, sparks[si].shielded,
					0.5f, 0.8f, 1.0f);
			}
		}
	}
}

void Defender_render_bloom_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		DefenderState *d = &defenders[i];
		PlaceableComponent *pl = &placeables[i];

		if (d->aiState == DEFENDER_DEAD)
			continue;

		if (d->aiState == DEFENDER_DYING) {
			Enemy_render_death_flash(pl, (float)d->deathTimer, (float)DEATH_FLASH_MS);
			continue;
		}

		/* Boost trail bloom */
		const ColorFloat *bodyColor = (d->aiState == DEFENDER_IDLE) ? &colorBody : &colorAggro;
		if (d->boosting) {
			double dx = pl->position.x - d->prevPosition.x;
			double dy = pl->position.y - d->prevPosition.y;
			for (int g = BOOST_TRAIL_GHOSTS; g >= 1; g--) {
				float t = (float)g / (float)(BOOST_TRAIL_GHOSTS + 1);
				Position ghost;
				ghost.x = pl->position.x - dx * BOOST_TRAIL_LENGTH * t;
				ghost.y = pl->position.y - dy * BOOST_TRAIL_LENGTH * t;
				float alpha = (1.0f - t) * 0.25f;
				Render_point(&ghost, 6.0f, &(ColorFloat){bodyColor->red, bodyColor->green, bodyColor->blue, alpha});
			}
		}

		/* Body glow */
		Render_point(&pl->position, 6.0, bodyColor);

		/* Aegis ring bloom */
		if (d->aegisActive) {
			render_hexagon(pl->position, BODY_SIZE + 8.0f, 2.0f,
				colorAegis.red, colorAegis.green, colorAegis.blue, 0.5f);
		}

		/* Heal beam bloom */
		if (d->healBeamActive) {
			float fade = (float)d->healBeamTimer / HEAL_BEAM_DURATION_MS;
			Render_thick_line(
				(float)pl->position.x, (float)pl->position.y,
				(float)d->healBeamTarget.x, (float)d->healBeamTarget.y,
				3.0f, 0.3f, 0.7f, 1.0f, fade * 0.6f);
		}
	}

	/* Spark bloom */
	for (int si = 0; si < SPARK_POOL_SIZE; si++) {
		if (sparks[si].active) {
			Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
				SPARK_DURATION, SPARK_SIZE, sparks[si].shielded,
				0.5f, 0.8f, 1.0f);
		}
	}
}

void Defender_deaggro_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		DefenderState *d = &defenders[i];
		if (d->aiState == DEFENDER_SUPPORTING || d->aiState == DEFENDER_FLEEING) {
			d->aiState = DEFENDER_IDLE;
			pick_wander_target(d);
		}
	}
}

void Defender_reset_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		DefenderState *d = &defenders[i];
		d->alive = true;
		d->hp = DEFENDER_HP;
		d->aiState = DEFENDER_IDLE;
		d->killedByPlayer = false;
		d->healCooldown = 0;
		d->healBeamActive = false;
		d->aegisActive = false;
		d->aegisDuration = 0;
		d->aegisCooldown = 0;
		d->shieldBreakGrace = 0;
		d->boosting = false;
		d->deathTimer = 0;
		d->respawnTimer = 0;
		placeables[i].position = d->spawnPoint;
		d->prevPosition = d->spawnPoint;
		pick_wander_target(d);
	}
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;
}

bool Defender_is_protecting(Position pos)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		DefenderState *d = &defenders[i];
		if (!d->alive)
			continue;
		/* Protected if aegis was active this frame OR in post-break grace */
		if (!d->aegisWasActive && d->shieldBreakGrace <= 0)
			continue;
		double dist = Enemy_distance_between(placeables[i].position, pos);
		if (dist < PROTECT_RADIUS)
			return true;
	}
	return false;
}
