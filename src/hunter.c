#include "hunter.h"
#include "sub_pea.h"
#include "sub_mgun.h"
#include "sub_mine.h"
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

#define HUNTER_COUNT 64
#define HUNTER_SPEED 400.0
#define HUNTER_HP 100.0
#define AGGRO_RANGE 1200.0
#define DEAGGRO_RANGE 1800.0
#define FIRE_RANGE AGGRO_RANGE
#define IDLE_DRIFT_RADIUS 400.0
#define IDLE_DRIFT_SPEED 80.0
#define IDLE_WANDER_INTERVAL 2000

#define BURST_COUNT 3
#define BURST_INTERVAL 100
#define BURST_COOLDOWN 1500

#define DEATH_FLASH_MS 200
#define RESPAWN_MS 30000

#define PROJ_COUNT 32
#define PROJ_VELOCITY 2000.0
#define PROJ_TTL 800
#define PROJ_DAMAGE 15.0
#define PROJ_SPARK_DURATION 80
#define PROJ_SPARK_SIZE 12.0

#define BODY_SIZE 12.0
#define WALL_CHECK_DIST 50.0

#define PI 3.14159265358979323846

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

typedef struct {
	bool active;
	Position position;
	Position prevPosition;
	double headingSin;
	double headingCos;
	int ticksLived;
} HunterProjectile;

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
static const ColorFloat colorProj     = {1.0f, 0.35f, 0.05f, 1.0f};


/* State arrays */
static HunterState hunters[HUNTER_COUNT];
static PlaceableComponent placeables[HUNTER_COUNT];
static int highestUsedIndex = 0;

/* Projectile pool */
static HunterProjectile projectiles[PROJ_COUNT];

/* Sparks */
static bool sparkActive = false;
static Position sparkPosition;
static int sparkTicksLeft = 0;

/* Audio */
static Mix_Chunk *sampleShoot = 0;
static Mix_Chunk *sampleDeath = 0;
static Mix_Chunk *sampleRespawn = 0;
static Mix_Chunk *sampleHit = 0;

/* Helpers */
static double get_radians(double degrees)
{
	return degrees * PI / 180.0;
}

static double distance_between(Position a, Position b)
{
	double dx = b.x - a.x;
	double dy = b.y - a.y;
	return sqrt(dx * dx + dy * dy);
}

static void pick_wander_target(HunterState *h)
{
	double angle = (rand() % 360) * PI / 180.0;
	double dist = (rand() % (int)IDLE_DRIFT_RADIUS);
	h->wanderTarget.x = h->spawnPoint.x + cos(angle) * dist;
	h->wanderTarget.y = h->spawnPoint.y + sin(angle) * dist;
	h->wanderTimer = IDLE_WANDER_INTERVAL + (rand() % 1000);
}

static bool has_line_of_sight(Position from, Position to)
{
	double hx, hy;
	return !Map_line_test_hit(from.x, from.y, to.x, to.y, &hx, &hy);
}

static void move_toward(PlaceableComponent *pl, Position target, double speed, double dt)
{
	double dx = target.x - pl->position.x;
	double dy = target.y - pl->position.y;
	double dist = sqrt(dx * dx + dy * dy);
	if (dist < 1.0)
		return;

	double nx = dx / dist;
	double ny = dy / dist;

	/* Wall avoidance: check ahead */
	double checkX = pl->position.x + nx * WALL_CHECK_DIST;
	double checkY = pl->position.y + ny * WALL_CHECK_DIST;
	double hx, hy;
	if (Map_line_test_hit(pl->position.x, pl->position.y, checkX, checkY, &hx, &hy))
		return; /* don't move into wall */

	double move = speed * dt;
	if (move > dist)
		move = dist;

	pl->position.x += nx * move;
	pl->position.y += ny * move;
}

static void fire_projectile(Position origin, Position target)
{
	/* Find inactive slot */
	int slot = -1;
	for (int i = 0; i < PROJ_COUNT; i++) {
		if (!projectiles[i].active) {
			slot = i;
			break;
		}
	}
	if (slot < 0)
		slot = 0;

	HunterProjectile *p = &projectiles[slot];
	p->active = true;
	p->ticksLived = 0;
	p->position = origin;
	p->prevPosition = origin;

	double heading = Position_get_heading(origin, target);
	double rad = get_radians(heading);
	p->headingSin = sin(rad);
	p->headingCos = cos(rad);

	Audio_play_sample(&sampleShoot);
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

	Entity_add_entity(entity);

	highestUsedIndex++;

	Audio_load_sample(&sampleShoot, "resources/sounds/long_beam.wav");
	Audio_load_sample(&sampleDeath, "resources/sounds/bomb_explode.wav");
	Audio_load_sample(&sampleRespawn, "resources/sounds/door.wav");
	Audio_load_sample(&sampleHit, "resources/sounds/samus_hurt.wav");
}

void Hunter_cleanup(void)
{
	highestUsedIndex = 0;

	for (int i = 0; i < PROJ_COUNT; i++)
		projectiles[i].active = false;
	sparkActive = false;

	Audio_unload_sample(&sampleShoot);
	Audio_unload_sample(&sampleDeath);
	Audio_unload_sample(&sampleRespawn);
	Audio_unload_sample(&sampleHit);
}

Collision Hunter_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
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
	}

	return collision;
}

void Hunter_resolve(const void *state, const Collision collision)
{
	/* Hunter doesn't react to collision resolution from ship touching it.
	   Damage is handled via projectile check_hit in update. */
	(void)state;
	(void)collision;
}

void Hunter_update(const void *state, const PlaceableComponent *placeable, unsigned int ticks)
{
	HunterState *h = (HunterState *)state;
	/* Find our index to get the mutable placeable */
	int idx = (int)(h - hunters);
	PlaceableComponent *pl = &placeables[idx];
	double dt = ticks / 1000.0;

	/* --- Check for incoming player projectiles (if alive and not dying) --- */
	if (h->alive && h->aiState != HUNTER_DYING) {
		Rectangle body = {-BODY_SIZE, BODY_SIZE, BODY_SIZE, -BODY_SIZE};
		Rectangle hitBox = Collision_transform_bounding_box(pl->position, body);

		/* Check all player weapon types */
		bool hit = false;
		if (Sub_Pea_check_hit(hitBox)) {
			h->hp -= 50.0;  /* sub_pea = 50 damage */
			hit = true;
		}
		if (Sub_Mgun_check_hit(hitBox)) {
			h->hp -= 20.0;  /* sub_mgun = 20 damage */
			hit = true;
		}
		if (Sub_Mine_check_hit(hitBox)) {
			h->hp -= 100.0; /* sub_mine = instant kill */
			hit = true;
		}

		if (hit) {
			sparkActive = true;
			sparkPosition = pl->position;
			sparkTicksLeft = PROJ_SPARK_DURATION;
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
		}
	}

	/* --- State machine --- */
	switch (h->aiState) {
	case HUNTER_IDLE: {
		/* Wander toward target */
		move_toward(pl, h->wanderTarget, IDLE_DRIFT_SPEED, dt);

		h->wanderTimer -= ticks;
		if (h->wanderTimer <= 0)
			pick_wander_target(h);

		/* Check aggro — requires line of sight and ship alive */
		Position shipPos = Ship_get_position();
		double dist = distance_between(pl->position, shipPos);
		bool nearbyShot = Sub_Pea_check_nearby(pl->position, 200.0)
						|| Sub_Mgun_check_nearby(pl->position, 200.0);
		if (!Ship_is_destroyed() &&
			((dist < AGGRO_RANGE && has_line_of_sight(pl->position, shipPos)) || nearbyShot)) {
			h->aiState = HUNTER_CHASING;
			h->cooldownTimer = 0;
		}

		/* Face wander target */
		h->facing = Position_get_heading(pl->position, h->wanderTarget);
		break;
	}
	case HUNTER_CHASING: {
		Position shipPos = Ship_get_position();
		double dist = distance_between(pl->position, shipPos);
		bool los = has_line_of_sight(pl->position, shipPos);

		/* De-aggro: out of range, lost line of sight, or ship dead */
		if (Ship_is_destroyed() || dist > DEAGGRO_RANGE || !los) {
			h->aiState = HUNTER_IDLE;
			pick_wander_target(h);
			break;
		}

		/* Move toward player */
		move_toward(pl, shipPos, HUNTER_SPEED, dt);
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
			fire_projectile(pl->position, shipPos);
			h->burstShotsFired++;
			h->burstTimer = BURST_INTERVAL;
		}

		/* Burst complete */
		if (h->burstShotsFired >= BURST_COUNT) {
			h->aiState = HUNTER_CHASING;
			h->cooldownTimer = BURST_COOLDOWN;
		}

		/* De-aggro: out of range, lost line of sight, or ship dead */
		double dist = distance_between(pl->position, shipPos);
		if (Ship_is_destroyed() || dist > DEAGGRO_RANGE || !has_line_of_sight(pl->position, shipPos)) {
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
			if (h->killedByPlayer && !Progression_is_unlocked(SUB_ID_MGUN))
				Fragment_spawn(pl->position, FRAG_TYPE_HUNTER);
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

	/* --- Update projectiles (done for ALL hunters from any update call) --- */
	/* Only do this from hunter index 0 to avoid processing projectiles N times */
	if (idx == 0) {
		Position shipPos = Ship_get_position();
		Rectangle shipBB = {-15.0, 15.0, 15.0, -15.0};
		Rectangle shipWorld = Collision_transform_bounding_box(shipPos, shipBB);

		for (int i = 0; i < PROJ_COUNT; i++) {
			HunterProjectile *p = &projectiles[i];
			if (!p->active)
				continue;

			p->ticksLived += ticks;
			if (p->ticksLived > PROJ_TTL) {
				p->active = false;
				continue;
			}

			p->prevPosition = p->position;

			double dx = p->headingSin * PROJ_VELOCITY * dt;
			double dy = p->headingCos * PROJ_VELOCITY * dt;
			p->position.x += dx;
			p->position.y += dy;

			/* Wall collision */
			double hx, hy;
			if (Map_line_test_hit(p->prevPosition.x, p->prevPosition.y,
					p->position.x, p->position.y, &hx, &hy)) {
				sparkActive = true;
				sparkPosition.x = hx;
				sparkPosition.y = hy;
				sparkTicksLeft = PROJ_SPARK_DURATION;
				p->active = false;
				continue;
			}

			/* Hit ship */
			if (Collision_line_aabb_test(p->prevPosition.x, p->prevPosition.y,
					p->position.x, p->position.y, shipWorld, NULL)) {
				p->active = false;
				PlayerStats_damage(PROJ_DAMAGE);
				continue;
			}
		}

		/* Spark decay */
		if (sparkActive) {
			sparkTicksLeft -= ticks;
			if (sparkTicksLeft <= 0)
				sparkActive = false;
		}
	}
}

void Hunter_render(const void *state, const PlaceableComponent *placeable)
{
	HunterState *h = (HunterState *)state;

	if (h->aiState == HUNTER_DEAD)
		return;

	/* Death flash */
	if (h->aiState == HUNTER_DYING) {
		float fade = 1.0f - (float)h->deathTimer / DEATH_FLASH_MS;
		ColorFloat flash = {1.0f, 1.0f, 1.0f, fade};
		Rectangle sparkRect = {-20.0, 20.0, 20.0, -20.0};
		Render_quad(&placeable->position, 0.0, sparkRect, &flash);
		Render_quad(&placeable->position, 45.0, sparkRect, &flash);
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

	/* --- Render all projectiles (from any hunter's render call, guard with idx 0) --- */
	int idx = (int)((HunterState *)state - hunters);
	if (idx == 0) {
		View view = View_get_view();
		const double UNSCALED_POINT_SIZE = 6.0;
		const double MIN_POINT_SIZE = 2.0;

		for (int i = 0; i < PROJ_COUNT; i++) {
			if (!projectiles[i].active)
				continue;

			/* Motion trail */
			Render_thick_line(
				(float)projectiles[i].prevPosition.x,
				(float)projectiles[i].prevPosition.y,
				(float)projectiles[i].position.x,
				(float)projectiles[i].position.y,
				2.5f, colorProj.red, colorProj.green, colorProj.blue, 0.6f);

			double size = UNSCALED_POINT_SIZE * view.scale;
			if (size < MIN_POINT_SIZE)
				size = MIN_POINT_SIZE;
			Render_point(&projectiles[i].position, size, &colorProj);
		}

		/* Wall hit spark */
		if (sparkActive) {
			float fade = (float)sparkTicksLeft / PROJ_SPARK_DURATION;
			ColorFloat sparkColor = {1.0f, 0.5f, 0.1f, fade};
			Rectangle sparkRect = {-PROJ_SPARK_SIZE, PROJ_SPARK_SIZE, PROJ_SPARK_SIZE, -PROJ_SPARK_SIZE};
			Render_quad(&sparkPosition, 0.0, sparkRect, &sparkColor);
			Render_quad(&sparkPosition, 45.0, sparkRect, &sparkColor);
		}
	}
}

void Hunter_render_bloom_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		HunterState *h = &hunters[i];
		PlaceableComponent *pl = &placeables[i];

		if (h->aiState == HUNTER_DEAD)
			continue;

		if (h->aiState == HUNTER_DYING) {
			float fade = 1.0f - (float)h->deathTimer / DEATH_FLASH_MS;
			ColorFloat flash = {1.0f, 1.0f, 1.0f, fade};
			Rectangle sparkRect = {-20.0, 20.0, 20.0, -20.0};
			Render_quad(&pl->position, 0.0, sparkRect, &flash);
			Render_quad(&pl->position, 45.0, sparkRect, &flash);
			continue;
		}

		/* Body glow */
		const ColorFloat *bodyColor = (h->aiState == HUNTER_IDLE) ? &colorBody : &colorAggro;
		Render_point(&pl->position, 6.0, bodyColor);
	}

	/* Projectile bloom */
	View view = View_get_view();
	const double UNSCALED_POINT_SIZE = 6.0;
	const double MIN_POINT_SIZE = 2.0;

	for (int i = 0; i < PROJ_COUNT; i++) {
		if (!projectiles[i].active)
			continue;

		Render_thick_line(
			(float)projectiles[i].prevPosition.x,
			(float)projectiles[i].prevPosition.y,
			(float)projectiles[i].position.x,
			(float)projectiles[i].position.y,
			2.5f, colorProj.red, colorProj.green, colorProj.blue, 0.6f);

		double size = UNSCALED_POINT_SIZE * view.scale;
		if (size < MIN_POINT_SIZE)
			size = MIN_POINT_SIZE;
		Render_point(&projectiles[i].position, size, &colorProj);
	}

	/* Spark bloom */
	if (sparkActive) {
		float fade = (float)sparkTicksLeft / PROJ_SPARK_DURATION;
		ColorFloat sparkColor = {1.0f, 0.5f, 0.1f, fade};
		Rectangle sparkRect = {-PROJ_SPARK_SIZE, PROJ_SPARK_SIZE, PROJ_SPARK_SIZE, -PROJ_SPARK_SIZE};
		Render_quad(&sparkPosition, 0.0, sparkRect, &sparkColor);
		Render_quad(&sparkPosition, 45.0, sparkRect, &sparkColor);
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
	for (int i = 0; i < PROJ_COUNT; i++)
		projectiles[i].active = false;
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

	for (int i = 0; i < PROJ_COUNT; i++)
		projectiles[i].active = false;
	sparkActive = false;
}
