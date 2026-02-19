#include "stalker.h"
#include "sub_dash_core.h"
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

#define STALKER_COUNT 128

#define STALKER_HP 40.0
#define STALK_SPEED 300.0
#define RETREAT_SPEED 500.0
#define AGGRO_RANGE 1200.0
#define DEAGGRO_RANGE 3200.0
#define STRIKE_RANGE 600.0
#define IDLE_DRIFT_RADIUS 500.0
#define IDLE_DRIFT_SPEED 80.0
#define IDLE_WANDER_INTERVAL 2000

#define WINDUP_MS 300
static const SubDashConfig stalkerDashCfg = {
	.duration_ms = 150,
	.speed = 4000.0,
	.cooldown_ms = 0,
	.damage = 60.0,
};

#define BURST_COUNT 3
#define BURST_INTERVAL 100

#define RETREAT_RESTEALTH_MS 3000

#define DEATH_FLASH_MS 200
#define RESPAWN_MS 30000

#define BODY_RADIUS 20.0
#define BODY_WIDTH 10.0
#define WALL_CHECK_DIST 50.0

/* Stealth alpha constants */
#define STEALTH_ALPHA_MIN 0.08f
#define STEALTH_ALPHA_PULSE 0.05f

static const CarriedSubroutine stalkerCarried[] = {
	{ SUB_ID_STEALTH, FRAG_TYPE_STALKER },
	{ SUB_ID_EGRESS,  FRAG_TYPE_SEEKER },
};

typedef enum {
	STALKER_IDLE,
	STALKER_STALKING,
	STALKER_WINDING_UP,
	STALKER_DASHING,
	STALKER_SHOOTING,
	STALKER_RETREATING,
	STALKER_DYING,
	STALKER_DEAD
} StalkerAIState;

typedef struct {
	bool alive;
	double hp;
	StalkerAIState aiState;
	Position spawnPoint;
	double facing;
	bool killedByPlayer;

	/* Idle wander */
	Position wanderTarget;
	int wanderTimer;

	/* Windup */
	int windupTimer;
	double pendingDirX;
	double pendingDirY;

	/* Dash */
	SubDashCore dashCore;

	/* Shooting burst */
	int burstShotsFired;
	int burstTimer;

	/* Retreat */
	int retreatTimer;

	/* Death/respawn */
	int deathTimer;
	int respawnTimer;

	/* Stealth alpha (computed each frame) */
	float stealthAlpha;
} StalkerState;

/* Shared singleton components */
static RenderableComponent renderable = {Stalker_render};
static CollidableComponent collidable = {{-BODY_WIDTH, BODY_RADIUS, BODY_WIDTH, -BODY_RADIUS},
										  true,
										  COLLISION_LAYER_ENEMY,
										  COLLISION_LAYER_PLAYER,
										  Stalker_collide, Stalker_resolve};
static AIUpdatableComponent updatable = {Stalker_update};

/* Colors */
static const ColorFloat colorStealth  = {0.4f, 0.0f, 0.6f, 1.0f};
static const ColorFloat colorVisible  = {0.7f, 0.2f, 0.9f, 1.0f};

/* State arrays */
static StalkerState stalkers[STALKER_COUNT];
static PlaceableComponent placeables[STALKER_COUNT];
static Entity *entityRefs[STALKER_COUNT];
static int highestUsedIndex = 0;

/* Projectile pool (shared across all stalkers) */
static SubProjectilePool stalkerProjPool;

static const SubProjectileConfig stalkerProjCfg = {
	.fire_cooldown_ms = 0,
	.velocity = 2000.0,
	.ttl_ms = 800,
	.pool_size = 32,
	.damage = 15.0,
	.color_r = 0.6f, .color_g = 0.1f, .color_b = 0.8f,
	.trail_thickness = 3.0f,
	.trail_alpha = 0.6f,
	.point_size = 8.0,
	.min_point_size = 2.0,
	.spark_duration_ms = 80,
	.spark_size = 15.0f,
	.light_proj_radius = 60.0f,
	.light_proj_r = 0.6f, .light_proj_g = 0.1f, .light_proj_b = 0.8f, .light_proj_a = 0.7f,
	.light_spark_radius = 90.0f,
	.light_spark_r = 0.6f, .light_spark_g = 0.1f, .light_spark_b = 0.8f, .light_spark_a = 0.6f,
};

/* Body-hit sparks */
#define SPARK_POOL_SIZE 8
#define BODY_SPARK_DURATION 80
#define BODY_SPARK_SIZE 12.0f
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
	sparks[slot].ticksLeft = BODY_SPARK_DURATION;
}

/* Audio -- entity sounds only */
static Mix_Chunk *sampleDeath = 0;
static Mix_Chunk *sampleRespawn = 0;
static Mix_Chunk *sampleHit = 0;

/* Helpers */
static void pick_wander_target(StalkerState *s)
{
	Enemy_pick_wander_target(s->spawnPoint, IDLE_DRIFT_RADIUS, IDLE_WANDER_INTERVAL,
		&s->wanderTarget, &s->wanderTimer);
}

static float compute_stealth_alpha(StalkerState *s, unsigned int globalTicks)
{
	switch (s->aiState) {
	case STALKER_IDLE:
	case STALKER_STALKING:
		return STEALTH_ALPHA_MIN + STEALTH_ALPHA_PULSE * (float)sin(globalTicks / 500.0);
	case STALKER_WINDING_UP: {
		/* Ramp from min to 1.0 over WINDUP_MS */
		float t = (float)s->windupTimer / WINDUP_MS;
		if (t > 1.0f) t = 1.0f;
		return STEALTH_ALPHA_MIN + (1.0f - STEALTH_ALPHA_MIN) * t;
	}
	case STALKER_DASHING:
	case STALKER_SHOOTING:
		return 1.0f;
	case STALKER_RETREATING: {
		/* Fade from 1.0 back to min over RETREAT_RESTEALTH_MS */
		float t = (float)s->retreatTimer / RETREAT_RESTEALTH_MS;
		if (t > 1.0f) t = 1.0f;
		return 1.0f - (1.0f - STEALTH_ALPHA_MIN) * t;
	}
	case STALKER_DYING:
		return 1.0f;
	case STALKER_DEAD:
		return 0.0f;
	}
	return 1.0f;
}

/* ---- Render helpers ---- */

/* Crescent: an arc of thick line segments pointing in facing direction */
static void render_crescent(Position pos, double facing, const ColorFloat *c, float alpha)
{
	float rad = (float)(facing * PI / 180.0);
	/* Draw a crescent arc: 5 segments spanning ~180 degrees */
	int segments = 5;
	float arc_span = (float)(PI * 0.8);  /* ~144 degrees */
	float start_angle = rad - arc_span * 0.5f;

	float prevX = 0, prevY = 0;
	for (int i = 0; i <= segments; i++) {
		float t = (float)i / segments;
		float a = start_angle + arc_span * t;
		/* Crescent: offset the arc center behind the facing direction */
		float cx = (float)pos.x - sinf(rad) * (float)BODY_RADIUS * 0.3f;
		float cy = (float)pos.y - cosf(rad) * (float)BODY_RADIUS * 0.3f;
		float px = cx + sinf(a) * (float)BODY_RADIUS;
		float py = cy + cosf(a) * (float)BODY_RADIUS;

		if (i > 0) {
			Render_thick_line(prevX, prevY, px, py, 2.5f,
				c->red, c->green, c->blue, alpha);
		}
		prevX = px;
		prevY = py;
	}
}

/* ---- Public API ---- */

void Stalker_initialize(Position position)
{
	if (highestUsedIndex >= STALKER_COUNT) {
		printf("FATAL ERROR: Too many stalker entities.\n");
		return;
	}

	int idx = highestUsedIndex;
	StalkerState *s = &stalkers[idx];

	s->alive = true;
	s->hp = STALKER_HP;
	s->aiState = STALKER_IDLE;
	s->spawnPoint = position;
	s->facing = 0.0;
	s->killedByPlayer = false;
	s->windupTimer = 0;
	s->pendingDirX = 0.0;
	s->pendingDirY = 0.0;
	SubDash_init(&s->dashCore);
	s->burstShotsFired = 0;
	s->burstTimer = 0;
	s->retreatTimer = 0;
	s->deathTimer = 0;
	s->respawnTimer = 0;
	s->stealthAlpha = STEALTH_ALPHA_MIN;
	pick_wander_target(s);

	placeables[idx].position = position;
	placeables[idx].heading = 0.0;

	Entity entity = Entity_initialize_entity();
	entity.state = &stalkers[idx];
	entity.placeable = &placeables[idx];
	entity.renderable = &renderable;
	entity.collidable = &collidable;
	entity.aiUpdatable = &updatable;

	entityRefs[idx] = Entity_add_entity(entity);

	highestUsedIndex++;

	/* Load audio and register with enemy registry once */
	if (!sampleDeath) {
		SubProjectile_pool_init(&stalkerProjPool, stalkerProjCfg.pool_size);
		SubProjectile_initialize_audio();
		SubDash_initialize_audio();
		Audio_load_sample(&sampleDeath, "resources/sounds/bomb_explode.wav");
		Audio_load_sample(&sampleRespawn, "resources/sounds/door.wav");
		Audio_load_sample(&sampleHit, "resources/sounds/samus_hurt.wav");

		EnemyTypeCallbacks cb = {Stalker_find_wounded, Stalker_find_aggro, Stalker_heal};
		EnemyRegistry_register(cb);
	}
}

void Stalker_cleanup(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		if (entityRefs[i]) {
			entityRefs[i]->empty = true;
			entityRefs[i] = NULL;
		}
	}
	highestUsedIndex = 0;

	SubProjectile_deactivate_all(&stalkerProjPool);
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;

	SubProjectile_cleanup_audio();
	SubDash_cleanup_audio();
	Audio_unload_sample(&sampleDeath);
	Audio_unload_sample(&sampleRespawn);
	Audio_unload_sample(&sampleHit);
}

Collision Stalker_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	StalkerState *s = (StalkerState *)state;
	Collision collision = {false, false};

	if (!s->alive)
		return collision;

	Rectangle thisBB = collidable.boundingBox;
	Rectangle transformed = Collision_transform_bounding_box(placeable->position, thisBB);

	if (Collision_aabb_test(transformed, boundingBox)) {
		collision.collisionDetected = true;
		/* Dash damage handled in update via explicit AABB check + hitThisDash.
		   Do NOT set solid here — Ship_resolve treats solid as wall → force_kill. */
		Sub_Stealth_break();
	}

	return collision;
}

void Stalker_resolve(void *state, const Collision collision)
{
	(void)state;
	(void)collision;
}

void Stalker_update(void *state, const PlaceableComponent *placeable, unsigned int ticks)
{
	StalkerState *s = (StalkerState *)state;
	int idx = (int)(s - stalkers);
	PlaceableComponent *pl = &placeables[idx];
	double dt = ticks / 1000.0;

	if (s->alive)
		Enemy_check_stealth_proximity(pl->position, s->facing);

	/* Compute stealth alpha */
	static unsigned int globalTicks = 0;
	if (idx == 0) globalTicks += ticks;
	s->stealthAlpha = compute_stealth_alpha(s, globalTicks);

	/* --- Check for incoming player damage --- */
	if (s->alive && s->aiState != STALKER_DYING) {
		Rectangle body = {-BODY_WIDTH, BODY_RADIUS, BODY_WIDTH, -BODY_RADIUS};
		Rectangle hitBox = Collision_transform_bounding_box(pl->position, body);

		PlayerDamageResult dmg = Enemy_check_player_damage(hitBox, pl->position);
		bool shielded = Defender_is_protecting(pl->position) && !dmg.ambush;
		bool hit = dmg.hit;
		if (hit && !shielded)
			s->hp -= dmg.damage + dmg.mine_damage;

		if (hit) {
			activate_spark(pl->position, shielded);
			if (shielded)
				Defender_notify_shield_hit(pl->position);
			else
				Audio_play_sample(&sampleHit);

			/* Getting shot immediately aggroes */
			if (s->aiState == STALKER_IDLE) {
				s->aiState = STALKER_STALKING;
			}
		}

		if (hit && s->hp <= 0.0) {
			s->alive = false;
			s->aiState = STALKER_DYING;
			s->deathTimer = 0;
			s->killedByPlayer = true;
			Audio_play_sample(&sampleDeath);
			Enemy_on_player_kill(&dmg);
		}
	}

	/* --- State machine --- */
	switch (s->aiState) {
	case STALKER_IDLE: {
		Enemy_move_toward(pl, s->wanderTarget, IDLE_DRIFT_SPEED, dt, WALL_CHECK_DIST);

		s->wanderTimer -= ticks;
		if (s->wanderTimer <= 0)
			pick_wander_target(s);

		/* Check aggro */
		Position shipPos = Ship_get_position();
		double dist = Enemy_distance_between(pl->position, shipPos);
		bool nearbyShot = Enemy_check_any_nearby(pl->position, 200.0);
		if (!Ship_is_destroyed() && !Sub_Stealth_is_stealthed() &&
			((dist < AGGRO_RANGE && Enemy_has_line_of_sight(pl->position, shipPos)) || nearbyShot)) {
			s->aiState = STALKER_STALKING;
		}

		s->facing = Position_get_heading(pl->position, s->wanderTarget);
		break;
	}
	case STALKER_STALKING: {
		Position shipPos = Ship_get_position();
		double dist = Enemy_distance_between(pl->position, shipPos);
		bool los = Enemy_has_line_of_sight(pl->position, shipPos);

		/* De-aggro */
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed() || dist > DEAGGRO_RANGE || !los) {
			s->aiState = STALKER_IDLE;
			pick_wander_target(s);
			break;
		}

		/* Move toward player */
		Enemy_move_toward(pl, shipPos, STALK_SPEED, dt, WALL_CHECK_DIST);
		s->facing = Position_get_heading(pl->position, shipPos);

		/* Within strike range → begin windup */
		if (dist < STRIKE_RANGE) {
			s->aiState = STALKER_WINDING_UP;
			s->windupTimer = 0;

			/* Lock dash direction toward player */
			double dx = shipPos.x - pl->position.x;
			double dy = shipPos.y - pl->position.y;
			double len = sqrt(dx * dx + dy * dy);
			if (len > 0.001) {
				s->pendingDirX = dx / len;
				s->pendingDirY = dy / len;
			} else {
				s->pendingDirX = 0.0;
				s->pendingDirY = 1.0;
			}
		}
		break;
	}
	case STALKER_WINDING_UP: {
		/* Stationary, flashing telegraph */
		s->windupTimer += ticks;

		/* Face dash direction */
		s->facing = atan2(s->pendingDirX, s->pendingDirY) * 180.0 / PI;

		if (s->windupTimer >= WINDUP_MS) {
			s->aiState = STALKER_DASHING;
			SubDash_try_activate(&s->dashCore, &stalkerDashCfg, s->pendingDirX, s->pendingDirY);
		}
		break;
	}
	case STALKER_DASHING: {
		double moveX = s->dashCore.dirX * stalkerDashCfg.speed * dt;
		double moveY = s->dashCore.dirY * stalkerDashCfg.speed * dt;

		/* Wall collision */
		double hx, hy;
		double newX = pl->position.x + moveX;
		double newY = pl->position.y + moveY;
		if (Map_line_test_hit(pl->position.x, pl->position.y, newX, newY, &hx, &hy)) {
			pl->position.x = hx - s->dashCore.dirX * 5.0;
			pl->position.y = hy - s->dashCore.dirY * 5.0;
			SubDash_end_early(&s->dashCore, &stalkerDashCfg);
			s->aiState = STALKER_SHOOTING;
			s->burstShotsFired = 0;
			s->burstTimer = 0;
			break;
		}

		pl->position.x = newX;
		pl->position.y = newY;
		s->facing = atan2(s->dashCore.dirX, s->dashCore.dirY) * 180.0 / PI;

		/* Check hit on player during dash */
		if (!Ship_is_destroyed() && !s->dashCore.hitThisDash) {
			Position shipPos = Ship_get_position();
			Rectangle shipBB = {-SHIP_BB_HALF_SIZE, SHIP_BB_HALF_SIZE, SHIP_BB_HALF_SIZE, -SHIP_BB_HALF_SIZE};
			Rectangle shipWorld = Collision_transform_bounding_box(shipPos, shipBB);
			Rectangle stalkerBB = {-BODY_WIDTH, BODY_RADIUS, BODY_WIDTH, -BODY_RADIUS};
			Rectangle stalkerWorld = Collision_transform_bounding_box(pl->position, stalkerBB);

			if (Collision_aabb_test(stalkerWorld, shipWorld)) {
				PlayerStats_damage(stalkerDashCfg.damage);
				s->dashCore.hitThisDash = true;
			}
		}

		if (SubDash_update(&s->dashCore, &stalkerDashCfg, ticks)) {
			/* Dash ended naturally → start shooting */
			s->aiState = STALKER_SHOOTING;
			s->burstShotsFired = 0;
			s->burstTimer = 0;
		}
		break;
	}
	case STALKER_SHOOTING: {
		/* Fire burst of 3 shots at player */
		Position shipPos = Ship_get_position();
		s->facing = Position_get_heading(pl->position, shipPos);

		s->burstTimer -= ticks;
		if (s->burstTimer <= 0 && s->burstShotsFired < BURST_COUNT) {
			SubProjectile_try_fire(&stalkerProjPool, &stalkerProjCfg, pl->position, shipPos);
			s->burstShotsFired++;
			s->burstTimer = BURST_INTERVAL;
		}

		/* Burst complete → retreat */
		if (s->burstShotsFired >= BURST_COUNT && s->burstTimer <= 0) {
			s->aiState = STALKER_RETREATING;
			s->retreatTimer = 0;
		}
		break;
	}
	case STALKER_RETREATING: {
		s->retreatTimer += ticks;

		/* Move away from player */
		Position shipPos = Ship_get_position();
		Enemy_move_away_from(pl, shipPos, RETREAT_SPEED, dt, WALL_CHECK_DIST);
		s->facing = Position_get_heading(pl->position, shipPos);

		/* Re-stealth after 3s */
		if (s->retreatTimer >= RETREAT_RESTEALTH_MS) {
			/* Back to idle if player out of range, else re-stalk */
			if (!Ship_is_destroyed() && !Sub_Stealth_is_stealthed()) {
				double dist = Enemy_distance_between(pl->position, shipPos);
				if (dist < DEAGGRO_RANGE && Enemy_has_line_of_sight(pl->position, shipPos)) {
					s->aiState = STALKER_STALKING;
					break;
				}
			}
			s->aiState = STALKER_IDLE;
			pick_wander_target(s);
		}
		break;
	}
	case STALKER_DYING:
		s->deathTimer += ticks;
		if (s->deathTimer >= DEATH_FLASH_MS) {
			s->aiState = STALKER_DEAD;
			s->respawnTimer = 0;

			if (s->killedByPlayer)
				Enemy_drop_fragments(pl->position, stalkerCarried, 2);
		}
		break;

	case STALKER_DEAD:
		s->respawnTimer += ticks;
		if (s->respawnTimer >= RESPAWN_MS) {
			s->alive = true;
			s->hp = STALKER_HP;
			s->aiState = STALKER_IDLE;
			s->killedByPlayer = false;
			pl->position = s->spawnPoint;
			pick_wander_target(s);
			Audio_play_sample(&sampleRespawn);
		}
		break;
	}

	/* Gravity well pull (alive, not dying/dead, not mid-dash) */
	if (s->alive && s->aiState != STALKER_DYING && s->aiState != STALKER_DEAD
			&& s->aiState != STALKER_DASHING)
		Enemy_apply_gravity(pl, dt);

	/* Update projectiles and sparks from index 0 only */
	if (idx == 0) {
		SubProjectile_update(&stalkerProjPool, &stalkerProjCfg, ticks);

		/* Check player hit */
		if (!Ship_is_destroyed()) {
			Position shipPos = Ship_get_position();
			Rectangle shipBB = {-SHIP_BB_HALF_SIZE, SHIP_BB_HALF_SIZE, SHIP_BB_HALF_SIZE, -SHIP_BB_HALF_SIZE};
			Rectangle shipWorld = Collision_transform_bounding_box(shipPos, shipBB);
			double dmg = SubProjectile_check_hit(&stalkerProjPool, &stalkerProjCfg, shipWorld);
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

void Stalker_render(const void *state, const PlaceableComponent *placeable)
{
	StalkerState *s = (StalkerState *)state;
	int idx = (int)((StalkerState *)state - stalkers);

	/* Shared pool render from index 0 */
	if (idx == 0) {
		SubProjectile_render(&stalkerProjPool, &stalkerProjCfg);

		for (int si = 0; si < SPARK_POOL_SIZE; si++) {
			if (sparks[si].active) {
				Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
					BODY_SPARK_DURATION, BODY_SPARK_SIZE, sparks[si].shielded,
					0.6f, 0.1f, 0.8f);
			}
		}
	}

	if (s->aiState == STALKER_DEAD)
		return;

	/* Death flash */
	if (s->aiState == STALKER_DYING) {
		Enemy_render_death_flash(placeable, (float)s->deathTimer, (float)DEATH_FLASH_MS);
		return;
	}

	float alpha = s->stealthAlpha;

	/* Choose color and rendering based on state */
	const ColorFloat *baseColor;
	if (alpha < 0.5f)
		baseColor = &colorStealth;
	else
		baseColor = &colorVisible;

	/* Windup flash */
	if (s->aiState == STALKER_WINDING_UP) {
		bool flashOn = (s->windupTimer / 50) % 2 == 0;
		if (flashOn) {
			static const ColorFloat white = {1.0f, 1.0f, 1.0f, 1.0f};
			render_crescent(placeable->position, s->facing, &white, alpha);
			Render_point(&placeable->position, 3.0, &(ColorFloat){1.0f, 1.0f, 1.0f, alpha});
			return;
		}
	}

	/* Dash trail */
	if (s->aiState == STALKER_DASHING) {
		for (int g = 5; g >= 1; g--) {
			float t = (float)g / 6.0f;
			Position ghost;
			ghost.x = placeable->position.x - s->dashCore.dirX * BODY_RADIUS * 3.0 * t;
			ghost.y = placeable->position.y - s->dashCore.dirY * BODY_RADIUS * 3.0 * t;
			float ghostAlpha = (1.0f - t) * 0.5f;
			render_crescent(ghost, s->facing, &colorVisible, ghostAlpha);
		}
		/* White flash body during dash */
		static const ColorFloat dashColor = {1.0f, 1.0f, 1.0f, 1.0f};
		render_crescent(placeable->position, s->facing, &dashColor, 1.0f);
		Render_point(&placeable->position, 3.0, &dashColor);
		return;
	}

	render_crescent(placeable->position, s->facing, baseColor, alpha);

	/* Center dot */
	ColorFloat dotColor = {baseColor->red, baseColor->green, baseColor->blue, alpha};
	Render_point(&placeable->position, 3.0, &dotColor);
}

void Stalker_render_bloom_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		StalkerState *s = &stalkers[i];
		PlaceableComponent *pl = &placeables[i];

		if (s->aiState == STALKER_DEAD)
			continue;

		if (s->aiState == STALKER_DYING) {
			Enemy_render_death_flash(pl, (float)s->deathTimer, (float)DEATH_FLASH_MS);
			continue;
		}

		/* Only bloom when sufficiently visible */
		if (s->stealthAlpha < 0.3f)
			continue;

		/* Body glow */
		const ColorFloat *bodyColor;
		if (s->aiState == STALKER_WINDING_UP) {
			bool flashOn = (s->windupTimer / 50) % 2 == 0;
			static const ColorFloat white = {1.0f, 1.0f, 1.0f, 1.0f};
			bodyColor = flashOn ? &white : &colorVisible;
		} else if (s->aiState == STALKER_DASHING) {
			static const ColorFloat dashColor = {1.0f, 1.0f, 1.0f, 1.0f};
			bodyColor = &dashColor;
		} else {
			bodyColor = &colorVisible;
		}

		ColorFloat bloomColor = {bodyColor->red, bodyColor->green, bodyColor->blue, s->stealthAlpha};
		Render_point(&pl->position, 6.0, &bloomColor);

		/* Dash trail bloom */
		if (s->aiState == STALKER_DASHING) {
			for (int g = 3; g >= 1; g--) {
				float t = (float)g / 4.0f;
				Position ghost;
				ghost.x = pl->position.x - s->dashCore.dirX * BODY_RADIUS * 2.0 * t;
				ghost.y = pl->position.y - s->dashCore.dirY * BODY_RADIUS * 2.0 * t;
				float alpha = (1.0f - t) * 0.6f;
				ColorFloat gc = {0.7f, 0.2f, 0.9f, alpha};
				Render_point(&ghost, 4.0, &gc);
			}
		}
	}

	/* Projectile bloom */
	SubProjectile_render_bloom(&stalkerProjPool, &stalkerProjCfg);

	/* Body-hit spark bloom */
	for (int si = 0; si < SPARK_POOL_SIZE; si++) {
		if (sparks[si].active) {
			Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
				BODY_SPARK_DURATION, BODY_SPARK_SIZE, sparks[si].shielded,
				0.6f, 0.1f, 0.8f);
		}
	}
}

void Stalker_render_light_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		StalkerState *s = &stalkers[i];
		PlaceableComponent *pl = &placeables[i];

		if (s->aiState == STALKER_DEAD || s->aiState == STALKER_DYING)
			continue;

		if (s->aiState == STALKER_WINDING_UP) {
			bool flashOn = (s->windupTimer / 50) % 2 == 0;
			if (flashOn) {
				Render_filled_circle(
					(float)pl->position.x, (float)pl->position.y,
					180.0f, 12, 0.5f, 0.1f, 0.8f, 0.6f);
			}
		} else if (s->aiState == STALKER_DASHING) {
			Render_filled_circle(
				(float)pl->position.x, (float)pl->position.y,
				240.0f, 12, 0.5f, 0.1f, 0.8f, 0.8f);
		}
	}

	/* Projectile light */
	SubProjectile_render_light(&stalkerProjPool, &stalkerProjCfg);

	for (int si = 0; si < SPARK_POOL_SIZE; si++) {
		if (sparks[si].active) {
			Render_filled_circle(
				(float)sparks[si].position.x,
				(float)sparks[si].position.y,
				300.0f, 12, 0.6f, 0.1f, 0.8f, 0.6f);
		}
	}
}

void Stalker_deaggro_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		StalkerState *s = &stalkers[i];
		if (s->aiState == STALKER_STALKING || s->aiState == STALKER_WINDING_UP ||
			s->aiState == STALKER_DASHING || s->aiState == STALKER_SHOOTING ||
			s->aiState == STALKER_RETREATING) {
			s->aiState = STALKER_IDLE;
			pick_wander_target(s);
		}
	}

	/* Kill all in-flight stalker projectiles */
	SubProjectile_deactivate_all(&stalkerProjPool);
}

void Stalker_reset_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		StalkerState *s = &stalkers[i];
		s->alive = true;
		s->hp = STALKER_HP;
		s->aiState = STALKER_IDLE;
		s->killedByPlayer = false;
		s->windupTimer = 0;
		s->pendingDirX = 0.0;
		s->pendingDirY = 0.0;
		SubDash_init(&s->dashCore);
		s->burstShotsFired = 0;
		s->burstTimer = 0;
		s->retreatTimer = 0;
		s->deathTimer = 0;
		s->respawnTimer = 0;
		placeables[i].position = s->spawnPoint;
		pick_wander_target(s);
	}

	SubProjectile_deactivate_all(&stalkerProjPool);
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;
}

bool Stalker_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index)
{
	double bestDamage = 0.0;
	int bestIdx = -1;

	for (int i = 0; i < highestUsedIndex; i++) {
		StalkerState *s = &stalkers[i];
		if (!s->alive || s->aiState == STALKER_DYING || s->aiState == STALKER_DEAD)
			continue;
		if (s->hp >= hp_threshold)
			continue;
		double missing = STALKER_HP - s->hp;
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

bool Stalker_find_aggro(Position from, double range, Position *out_pos)
{
	double bestDist = range + 1.0;
	int bestIdx = -1;

	for (int i = 0; i < highestUsedIndex; i++) {
		StalkerState *s = &stalkers[i];
		if (!s->alive)
			continue;
		if (s->aiState == STALKER_IDLE || s->aiState == STALKER_DYING || s->aiState == STALKER_DEAD)
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

void Stalker_heal(int index, double amount)
{
	if (index < 0 || index >= highestUsedIndex)
		return;
	StalkerState *s = &stalkers[index];
	if (!s->alive)
		return;
	s->hp += amount;
	if (s->hp > STALKER_HP)
		s->hp = STALKER_HP;
}
