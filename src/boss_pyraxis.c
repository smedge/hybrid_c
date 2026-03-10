#include "boss_pyraxis.h"
#include "boss_hud.h"
#include "enemy_util.h"
#include "player_stats.h"
#include "ship.h"
#include "render.h"
#include "graphics.h"
#include "view.h"
#include "audio.h"
#include "map.h"
#include "data_node.h"
#include "fragment.h"
#include "particle_instance.h"
#include "global_render.h"
#include "global_update.h"
#include "spatial_grid.h"
#include "collision.h"
#include "burn.h"
#include "reactor_grid.h"


#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL_mixer.h>

/* ----- Tuning Constants ----- */

#define BOSS_MAX_HP          10000.0
#define VISUAL_RADIUS        800.0f    /* 1600u diameter / 2 */
#define HITBOX_RADIUS        640.0     /* 80% of visual radius */
#define PHASE_2_THRESHOLD    0.65      /* HP fraction */
#define PHASE_3_THRESHOLD    0.25

/* Swirl particles */
#define SWIRL_BODY_COUNT     450
#define SWIRL_EMBER_COUNT    65
#define SWIRL_TOTAL          (SWIRL_BODY_COUNT + SWIRL_EMBER_COUNT)

/* Eye rendering */
#define EYE_SEPARATION       120.0f   /* half-distance between eyes */
#define EYE_FORWARD_DIST     250.0f   /* distance from center toward player */
#define EYE_SIZE             20.0f
#define EYE_BLOOM_SIZE       60.0f

/* Ember volley projectile pool */
#define EMBER_POOL_SIZE      128
#define EMBER_COLLISION_RADIUS 30.0
#define EMBER_DESPAWN_RANGE  4000.0
#define EMBER_VISUAL_SIZE    12.0f

/* Ember patterns */
#define RING_COUNT           16
#define RING_SPEED           1200.0
#define RING_INTERVAL_MS     2000

#define SPIRAL_ARMS          3
#define SPIRAL_PER_ARM       6
#define SPIRAL_SPEED         1000.0
#define SPIRAL_INTERVAL_MS   3000

#define AIMED_COUNT          5
#define AIMED_SPEED          1800.0
#define AIMED_INTERVAL_MS    2500

#define PATTERN_GAP_MS       750

/* Heat pulse */
#define HEAT_PULSE_INTERVAL_MS 10000
#define HEAT_PULSE_SPEED     2400.0
#define HEAT_PULSE_WIDTH     200.0
#define HEAT_PULSE_DAMAGE    20.0
#define HEAT_PULSE_TELEGRAPH_MS 2000
#define HEAT_PULSE_MAX_RANGE 5000.0

/* Volley hit cap — max projectile hits from a single weapon firing event */
#define VOLLEY_HIT_CAP       3

/* Center burn zone */
#define CENTER_BURN_HALF_SIZE 1600.0  /* 32 cells * 100u/cell / 2 */
#define CENTER_BURN_DPS      5.0

/* Death sequence */
#define DEATH_TOTAL_MS       8000
#define DEATH_SCATTER_MS     3000     /* swirl scatters (0-3s) */
#define DEATH_EYES_LINGER_MS 5500    /* eyes hang (3-5.5s) */
#define DEATH_EYES_DIM_MS    7500    /* eyes dim (5.5-7.5s) */
/* 7.5-8s = silence */

/* Reveal sequence */
#define REVEAL_EYES_MS       1500    /* eyes fade in first */
#define REVEAL_SWIRL_MS      4500   /* swirl coalesces (1.5-4.5s total) */
#define CENTER_IGNITE_DELAY_MS 500  /* center burn starts after speech */

/* ----- State ----- */

typedef enum {
	BOSS_INACTIVE,
	BOSS_INTRO_SPEECH,
	BOSS_REVEAL,
	BOSS_ACTIVE,
	BOSS_DYING,
	BOSS_DEFEATED
} BossState;

typedef enum {
	PATTERN_RING,
	PATTERN_SPIRAL,
	PATTERN_AIMED,
	PATTERN_COUNT
} EmberPattern;

/* Single ember projectile */
typedef struct {
	bool active;
	double x, y;
	double vx, vy;
	double age_ms;
} Ember;

/* Heat pulse ring */
typedef struct {
	bool active;
	double radius;
	bool playerHit;    /* only hit player once per pulse */
} HeatPulse;

/* Swirl particle (precomputed orbital params) */
typedef struct {
	float orbit_radius;
	float orbit_speed;    /* radians/sec */
	float orbit_phase;    /* current angle */
	float size;
	float r, g, b, a;
	bool is_ember;
} SwirlParticle;

static struct {
	BossState state;
	Position center;
	double hp;
	int stateTimer;       /* ms elapsed in current state */

	/* Swirl */
	SwirlParticle swirl[SWIRL_TOTAL];
	float swirlSpeedMult; /* speed modifier for phase transitions */

	/* Eyes */
	float eyeAngle;       /* angle toward player (radians) */
	float eyeAlpha;       /* for fade in/out */

	/* Ember volleys */
	Ember embers[EMBER_POOL_SIZE];
	EmberPattern currentPattern;
	int patternTimer;     /* ms until next volley */
	int patternGapTimer;  /* ms remaining in gap between patterns */
	float spiralBaseAngle;

	/* Heat pulse */
	HeatPulse pulse;
	int pulseTimer;       /* ms until next pulse */
	int pulseTelegraphTimer; /* ms remaining in telegraph */
	bool pulseTelegraphing;

	/* Center burn */
	bool centerBurnActive;
	int centerBurnAccum;  /* ms accumulator for DOT ticks */

	/* Entity registration */
	PlaceableComponent placeable;
	Entity *entityRef;
	bool pipelineRegistered;
	bool initialized;
} boss;

/* Shared singleton components */
static void boss_render_bloom(const void *state, const PlaceableComponent *placeable);
static void boss_render_global_bloom(void);
static void boss_render_global_light(void);
static void boss_update_global(unsigned int ticks);

static RenderableComponent renderable = {.passes = {
	[RENDER_PASS_MAIN] = BossPyraxis_render,
	[RENDER_PASS_BLOOM_SOURCE] = boss_render_bloom,
}};

static CollidableComponent collidable = {
	{-HITBOX_RADIUS, HITBOX_RADIUS, HITBOX_RADIUS, -HITBOX_RADIUS},
	true,
	COLLISION_LAYER_ENEMY,
	COLLISION_LAYER_PLAYER,
	BossPyraxis_collide,
	BossPyraxis_resolve
};

static AIUpdatableComponent updatable = {BossPyraxis_update};

/* Audio */
static Mix_Chunk *sampleHit = NULL;
static Mix_Chunk *sampleDeath = NULL;

/* Particle instance buffer for swirl + embers */
static ParticleInstanceData swirlBuffer[SWIRL_TOTAL];
static ParticleInstanceData emberBuffer[EMBER_POOL_SIZE];

/* ----- Helpers ----- */

static double randf(void)
{
	return (double)rand() / (double)RAND_MAX;
}

static void init_swirl_particle(SwirlParticle *p, int index)
{
	bool is_ember = (index >= SWIRL_BODY_COUNT);
	p->is_ember = is_ember;

	if (is_ember) {
		/* Ember spark — brighter, smaller, faster */
		p->orbit_radius = 100.0f + (float)(randf() * 700.0);
		p->orbit_speed = 0.8f + (float)(randf() * 1.2f);
		/* Randomly CW or CCW */
		if (rand() % 2) p->orbit_speed = -p->orbit_speed;
		p->orbit_phase = (float)(randf() * 2.0 * M_PI);
		p->size = 6.0f + (float)(randf() * 10.0);
		p->r = 1.0f;
		p->g = 0.4f + (float)(randf() * 0.4);
		p->b = 0.05f + (float)(randf() * 0.1);
		p->a = 0.5f + (float)(randf() * 0.5);
	} else {
		/* Dark body blob — large, slow, mostly opaque */
		p->orbit_radius = 50.0f + (float)(randf() * 750.0);
		p->orbit_speed = 0.15f + (float)(randf() * 0.4f);
		if (rand() % 2) p->orbit_speed = -p->orbit_speed;
		p->orbit_phase = (float)(randf() * 2.0 * M_PI);
		p->size = 30.0f + (float)(randf() * 60.0);
		/* Dark red to near-black */
		float darkness = (float)(randf() * 0.3);
		p->r = 0.15f + darkness * 0.5f;
		p->g = 0.02f + darkness * 0.1f;
		p->b = 0.0f;
		p->a = 0.4f + (float)(randf() * 0.4);
	}
}

static void init_swirl(void)
{
	for (int i = 0; i < SWIRL_TOTAL; i++)
		init_swirl_particle(&boss.swirl[i], i);
	boss.swirlSpeedMult = 1.0f;
}

static void fire_ring(void)
{
	double cx = boss.center.x;
	double cy = boss.center.y;

	for (int i = 0; i < RING_COUNT; i++) {
		double angle = (2.0 * M_PI * i) / RING_COUNT;
		double vx = cos(angle) * RING_SPEED;
		double vy = sin(angle) * RING_SPEED;

		/* Find free slot */
		for (int j = 0; j < EMBER_POOL_SIZE; j++) {
			if (!boss.embers[j].active) {
				boss.embers[j] = (Ember){true, cx, cy, vx, vy, 0};
				break;
			}
		}
	}
}

static void fire_spiral(void)
{
	double cx = boss.center.x;
	double cy = boss.center.y;

	for (int arm = 0; arm < SPIRAL_ARMS; arm++) {
		double armAngle = boss.spiralBaseAngle + (2.0 * M_PI * arm) / SPIRAL_ARMS;
		for (int shot = 0; shot < SPIRAL_PER_ARM; shot++) {
			double angle = armAngle + shot * 0.15; /* slight spread per shot */
			double vx = cos(angle) * SPIRAL_SPEED;
			double vy = sin(angle) * SPIRAL_SPEED;

			for (int j = 0; j < EMBER_POOL_SIZE; j++) {
				if (!boss.embers[j].active) {
					boss.embers[j] = (Ember){true, cx, cy, vx, vy, 0};
					break;
				}
			}
		}
	}
	boss.spiralBaseAngle += 0.4; /* rotate for next spiral */
}

static void fire_aimed(void)
{
	double cx = boss.center.x;
	double cy = boss.center.y;
	Position shipPos = Ship_get_position();
	double dx = shipPos.x - cx;
	double dy = shipPos.y - cy;
	double baseAngle = atan2(dy, dx);

	/* 5-shot spread */
	double spreadStep = 0.12; /* ~7 degrees between shots */
	double startAngle = baseAngle - spreadStep * (AIMED_COUNT - 1) / 2.0;

	for (int i = 0; i < AIMED_COUNT; i++) {
		double angle = startAngle + i * spreadStep;
		double vx = cos(angle) * AIMED_SPEED;
		double vy = sin(angle) * AIMED_SPEED;

		for (int j = 0; j < EMBER_POOL_SIZE; j++) {
			if (!boss.embers[j].active) {
				boss.embers[j] = (Ember){true, cx, cy, vx, vy, 0};
				break;
			}
		}
	}
}

static void fire_current_pattern(void)
{
	switch (boss.currentPattern) {
	case PATTERN_RING:   fire_ring();   break;
	case PATTERN_SPIRAL: fire_spiral(); break;
	case PATTERN_AIMED:  fire_aimed();  break;
	default: break;
	}
}

static int get_pattern_interval(EmberPattern p)
{
	switch (p) {
	case PATTERN_RING:   return RING_INTERVAL_MS;
	case PATTERN_SPIRAL: return SPIRAL_INTERVAL_MS;
	case PATTERN_AIMED:  return AIMED_INTERVAL_MS;
	default: return 4000;
	}
}

static void advance_pattern(void)
{
	boss.currentPattern = (boss.currentPattern + 1) % PATTERN_COUNT;
	boss.patternTimer = get_pattern_interval(boss.currentPattern);
	boss.patternGapTimer = PATTERN_GAP_MS;
}

static void update_embers(unsigned int ticks)
{
	double dt = ticks / 1000.0;
	double cx = boss.center.x;
	double cy = boss.center.y;

	for (int i = 0; i < EMBER_POOL_SIZE; i++) {
		Ember *e = &boss.embers[i];
		if (!e->active) continue;

		e->x += e->vx * dt;
		e->y += e->vy * dt;
		e->age_ms += ticks;

		/* Despawn check: distance from center */
		double dx = e->x - cx;
		double dy = e->y - cy;
		if (dx * dx + dy * dy > EMBER_DESPAWN_RANGE * EMBER_DESPAWN_RANGE) {
			e->active = false;
			continue;
		}

		/* Wall collision check */
		int gx = (int)((e->x / MAP_CELL_SIZE) + (MAP_SIZE / 2));
		int gy = (int)((e->y / MAP_CELL_SIZE) + (MAP_SIZE / 2));
		if (gx >= 0 && gx < MAP_SIZE && gy >= 0 && gy < MAP_SIZE) {
			const MapCell *cell = Map_get_cell(gx, gy);
			if (cell && !cell->empty) {
				e->active = false;
				continue;
			}
		}

		/* Player hit check */
		if (!Ship_is_destroyed()) {
			Position shipPos = Ship_get_position();
			double pdx = e->x - shipPos.x;
			double pdy = e->y - shipPos.y;
			if (pdx * pdx + pdy * pdy < EMBER_COLLISION_RADIUS * EMBER_COLLISION_RADIUS) {
				PlayerStats_damage(10.0);
				for (int b = 0; b < 3; b++)
					Burn_apply_to_player(BURN_DURATION_MS);
				e->active = false;
			}
		}
	}
}

static void deactivate_all_embers(void)
{
	for (int i = 0; i < EMBER_POOL_SIZE; i++)
		boss.embers[i].active = false;
}

static void update_heat_pulse(unsigned int ticks)
{
	double dt = ticks / 1000.0;

	/* Telegraph countdown */
	if (boss.pulseTelegraphing) {
		boss.pulseTelegraphTimer -= ticks;
		if (boss.pulseTelegraphTimer <= 0) {
			boss.pulseTelegraphing = false;
			boss.pulse.active = true;
			boss.pulse.radius = 0.0;
			boss.pulse.playerHit = false;
		}
		return;
	}

	/* Active pulse expansion */
	if (boss.pulse.active) {
		boss.pulse.radius += HEAT_PULSE_SPEED * dt;

		/* Check player hit */
		if (!boss.pulse.playerHit && !Ship_is_destroyed()) {
			Position shipPos = Ship_get_position();
			double dx = shipPos.x - boss.center.x;
			double dy = shipPos.y - boss.center.y;
			double dist = sqrt(dx * dx + dy * dy);

			/* Player is in the ring if their distance is within the ring band */
			if (dist >= boss.pulse.radius - HEAT_PULSE_WIDTH * 0.5 &&
				dist <= boss.pulse.radius + HEAT_PULSE_WIDTH * 0.5) {
				/* Check if player has i-frames or shield */
				if (!PlayerStats_has_iframes() && !PlayerStats_is_shielded()) {
					PlayerStats_damage(HEAT_PULSE_DAMAGE);
					for (int b = 0; b < 3; b++)
						Burn_apply_to_player(BURN_DURATION_MS);
					boss.pulse.playerHit = true;
				}
			}
		}

		/* Dissipate when past arena */
		if (boss.pulse.radius > HEAT_PULSE_MAX_RANGE)
			boss.pulse.active = false;
		return;
	}

	/* Cooldown timer */
	boss.pulseTimer -= ticks;
	if (boss.pulseTimer <= 0) {
		boss.pulseTelegraphing = true;
		boss.pulseTelegraphTimer = HEAT_PULSE_TELEGRAPH_MS;
		boss.pulseTimer = HEAT_PULSE_INTERVAL_MS;
	}
}

static void update_center_burn(unsigned int ticks)
{
	if (!boss.centerBurnActive) return;
	if (Ship_is_destroyed()) return;

	Position shipPos = Ship_get_position();
	double dx = shipPos.x - boss.center.x;
	double dy = shipPos.y - boss.center.y;

	/* Square zone check */
	if (fabs(dx) < CENTER_BURN_HALF_SIZE && fabs(dy) < CENTER_BURN_HALF_SIZE) {
		if (!PlayerStats_is_shielded() && !PlayerStats_has_iframes()) {
			boss.centerBurnAccum += ticks;
			/* Tick damage at 1-second intervals */
			while (boss.centerBurnAccum >= 200) {
				PlayerStats_damage(CENTER_BURN_DPS * 0.2);
				boss.centerBurnAccum -= 200;
			}
		}
	} else {
		boss.centerBurnAccum = 0;
	}
}

/* ----- Public API ----- */

void BossPyraxis_initialize(Position position)
{
	memset(&boss, 0, sizeof(boss));
	boss.state = BOSS_INACTIVE;
	boss.center = position;
	boss.hp = BOSS_MAX_HP;
	boss.swirlSpeedMult = 1.0f;
	boss.eyeAlpha = 0.0f;
	boss.currentPattern = PATTERN_RING;
	boss.patternTimer = RING_INTERVAL_MS;
	boss.pulseTimer = HEAT_PULSE_INTERVAL_MS;

	init_swirl();

	boss.placeable.position = position;
	boss.placeable.heading = 0.0;

	Entity entity = Entity_initialize_entity();
	entity.state = &boss;
	entity.placeable = &boss.placeable;
	entity.renderable = &renderable;
	entity.collidable = &collidable;
	entity.aiUpdatable = &updatable;

	boss.entityRef = Entity_add_entity(entity);

	SpatialGrid_add((EntityRef){ENTITY_BOSS_PYRAXIS, 0}, position.x, position.y);

	/* Load audio */
	Audio_load_sample(&sampleHit, "resources/sounds/samus_hurt.wav");
	Audio_load_sample(&sampleDeath, "resources/sounds/bomb_explode.wav");

	/* Register pipeline callbacks once */
	if (!boss.pipelineRegistered) {
		GlobalRender_register(RENDER_PASS_BLOOM_SOURCE, boss_render_global_bloom);
		GlobalRender_register(RENDER_PASS_LIGHT_SOURCE, boss_render_global_light);
		GlobalUpdate_register_pre_collision(boss_update_global);
		boss.pipelineRegistered = true;
	}

	boss.initialized = true;

	/* Reactor grid midground */
	ReactorGrid_initialize((float)position.x, (float)position.y);

	/* Start the encounter — trigger intro speech immediately */
	boss.state = BOSS_INTRO_SPEECH;
	boss.stateTimer = 0;
	DataNode_trigger_transfer("CRU-01");

	/* Activate HUD */
	BossHUD_set_active(true);
	BossHUD_set_health(boss.hp, BOSS_MAX_HP);
}

void BossPyraxis_cleanup(void)
{
	if (boss.entityRef) {
		boss.entityRef->empty = true;
		boss.entityRef = NULL;
	}

	Audio_unload_sample(&sampleHit);
	Audio_unload_sample(&sampleDeath);

	BossHUD_set_active(false);
	ReactorGrid_cleanup();

	boss.initialized = false;
	boss.pipelineRegistered = false;
	boss.state = BOSS_INACTIVE;
}

bool BossPyraxis_is_active(void)
{
	return boss.initialized && boss.state == BOSS_ACTIVE;
}

/* ----- Update ----- */

void BossPyraxis_update(void *state, const PlaceableComponent *placeable, unsigned int ticks)
{
	(void)state;
	(void)placeable;

	if (!boss.initialized) return;

	double dt = ticks / 1000.0;
	boss.stateTimer += ticks;

	/* Update eye tracking angle */
	if (!Ship_is_destroyed()) {
		Position shipPos = Ship_get_position();
		double dx = shipPos.x - boss.center.x;
		double dy = shipPos.y - boss.center.y;
		boss.eyeAngle = (float)atan2(dy, dx);
	}

	/* Update swirl particle orbits */
	for (int i = 0; i < SWIRL_TOTAL; i++) {
		SwirlParticle *p = &boss.swirl[i];
		p->orbit_phase += p->orbit_speed * boss.swirlSpeedMult * (float)dt;
	}

	switch (boss.state) {
	case BOSS_INACTIVE:
		break;

	case BOSS_INTRO_SPEECH:
		/* Wait for data node dialog to finish */
		if (!DataNode_is_reading()) {
			boss.state = BOSS_REVEAL;
			boss.stateTimer = 0;
			boss.centerBurnActive = true;
		}
		break;

	case BOSS_REVEAL: {
		/* Phase 1: eyes fade in (0 - REVEAL_EYES_MS) */
		/* Phase 2: swirl coalesces (REVEAL_EYES_MS - REVEAL_SWIRL_MS) */
		float revealProgress = (float)boss.stateTimer / (float)REVEAL_SWIRL_MS;
		if (revealProgress > 1.0f) revealProgress = 1.0f;

		/* Eye alpha: ramp up during first phase */
		float eyeProgress = (float)boss.stateTimer / (float)REVEAL_EYES_MS;
		if (eyeProgress > 1.0f) eyeProgress = 1.0f;
		boss.eyeAlpha = eyeProgress;

		/* Reactor grid grows with overall reveal */
		ReactorGrid_set_scale(revealProgress);

		/* Swirl: particles start far out and converge inward */
		if (boss.stateTimer >= REVEAL_EYES_MS) {
			float swirlFrac = (float)(boss.stateTimer - REVEAL_EYES_MS) /
				(float)(REVEAL_SWIRL_MS - REVEAL_EYES_MS);
			if (swirlFrac > 1.0f) swirlFrac = 1.0f;
			boss.swirlSpeedMult = 0.3f + swirlFrac * 0.7f;
		} else {
			boss.swirlSpeedMult = 0.3f;
		}

		/* Reveal complete — start the fight */
		if (boss.stateTimer >= REVEAL_SWIRL_MS) {
			boss.state = BOSS_ACTIVE;
			boss.stateTimer = 0;
			boss.eyeAlpha = 1.0f;
			boss.swirlSpeedMult = 1.0f;
		}
		break;
	}

	case BOSS_ACTIVE: {
		/* --- Damage detection --- */
		Rectangle hitBox = {
			boss.center.x - HITBOX_RADIUS, boss.center.y + HITBOX_RADIUS,
			boss.center.x + HITBOX_RADIUS, boss.center.y - HITBOX_RADIUS
		};

		PlayerDamageResult dmg = Enemy_check_player_damage_capped(hitBox, boss.center, VOLLEY_HIT_CAP);
		if (dmg.hit) {
			boss.hp -= dmg.damage + dmg.mine_damage;
			Audio_play_sample_at(&sampleHit, boss.center);

			if (boss.hp <= 0.0) {
				boss.hp = 0.0;
				boss.state = BOSS_DYING;
				boss.stateTimer = 0;
				deactivate_all_embers();
				boss.pulse.active = false;
				boss.pulseTelegraphing = false;
				Audio_play_sample_at(&sampleDeath, boss.center);
			}
		}

		BossHUD_set_health(boss.hp, BOSS_MAX_HP);

		/* --- Ember volley pattern cycling --- */
		if (boss.patternGapTimer > 0) {
			boss.patternGapTimer -= ticks;
		} else {
			boss.patternTimer -= ticks;
			if (boss.patternTimer <= 0) {
				fire_current_pattern();
				advance_pattern();
			}
		}

		/* --- Heat pulse --- */
		update_heat_pulse(ticks);

		/* --- Center burn --- */
		update_center_burn(ticks);

		break;
	}

	case BOSS_DYING: {
		/* Reactor grid shrinks over full death sequence */
		float deathFrac = (float)boss.stateTimer / (float)DEATH_TOTAL_MS;
		if (deathFrac > 1.0f) deathFrac = 1.0f;
		ReactorGrid_set_scale(1.0f - deathFrac);

		/* 8-second death sequence */
		/* Swirl scatters outward (0 - DEATH_SCATTER_MS) */
		if (boss.stateTimer < DEATH_SCATTER_MS) {
			/* Reverse rotation, particles fly outward */
			boss.swirlSpeedMult = -2.0f;
			/* Expand orbits */
			float scatter = (float)boss.stateTimer / (float)DEATH_SCATTER_MS;
			for (int i = 0; i < SWIRL_TOTAL; i++) {
				boss.swirl[i].orbit_radius += 500.0f * (float)dt * scatter;
				boss.swirl[i].a *= (1.0f - 0.5f * (float)dt);
			}
		}

		/* Eyes linger (DEATH_SCATTER_MS - DEATH_EYES_LINGER_MS) */
		if (boss.stateTimer >= DEATH_SCATTER_MS && boss.stateTimer < DEATH_EYES_LINGER_MS) {
			boss.eyeAlpha = 1.0f;
			boss.swirlSpeedMult = 0.0f; /* swirl gone */
		}

		/* Eyes dim (DEATH_EYES_LINGER_MS - DEATH_EYES_DIM_MS) */
		if (boss.stateTimer >= DEATH_EYES_LINGER_MS && boss.stateTimer < DEATH_EYES_DIM_MS) {
			float dimFrac = (float)(boss.stateTimer - DEATH_EYES_LINGER_MS) /
				(float)(DEATH_EYES_DIM_MS - DEATH_EYES_LINGER_MS);
			boss.eyeAlpha = 1.0f - dimFrac;
		}

		/* Silence (DEATH_EYES_DIM_MS - DEATH_TOTAL_MS) */
		if (boss.stateTimer >= DEATH_EYES_DIM_MS) {
			boss.eyeAlpha = 0.0f;
		}

		/* Death complete */
		if (boss.stateTimer >= DEATH_TOTAL_MS) {
			boss.state = BOSS_DEFEATED;
			boss.stateTimer = 0;
			boss.centerBurnActive = false;

			/* Trigger death speech */
			DataNode_trigger_transfer("CRU-09");
		}
		break;
	}

	case BOSS_DEFEATED:
		/* Wait for death speech to finish, then spawn fragment */
		if (!DataNode_is_reading() && boss.stateTimer > 500) {
			/* Spawn persistent elite inferno fragment */
			Fragment_spawn(boss.center, FRAG_TYPE_INFERNO, TIER_ELITE);

			/* Deactivate HUD */
			BossHUD_set_active(false);

			/* Mark as done — cleanup will happen on zone exit */
			boss.state = BOSS_INACTIVE;
		}
		break;
	}
}

/* ----- Collision ----- */

Collision BossPyraxis_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	(void)state;
	(void)placeable;
	Collision collision = {false, false};

	if (boss.state != BOSS_ACTIVE)
		return collision;

	Rectangle bossBox = {
		boss.center.x - HITBOX_RADIUS, boss.center.y + HITBOX_RADIUS,
		boss.center.x + HITBOX_RADIUS, boss.center.y - HITBOX_RADIUS
	};

	if (Collision_aabb_test(bossBox, boundingBox)) {
		collision.collisionDetected = true;
		collision.solid = true;
	}

	return collision;
}

void BossPyraxis_resolve(void *state, const Collision collision)
{
	(void)state;
	(void)collision;
}

/* ----- Render ----- */

static void render_swirl(float alpha_mult)
{
	float cx = (float)boss.center.x;
	float cy = (float)boss.center.y;
	int count = 0;

	/* During reveal, scale down orbit radii based on progress */
	float radiusMult = 1.0f;
	if (boss.state == BOSS_REVEAL && boss.stateTimer < REVEAL_SWIRL_MS) {
		if (boss.stateTimer < REVEAL_EYES_MS) {
			radiusMult = 3.0f; /* particles start far out */
		} else {
			float frac = (float)(boss.stateTimer - REVEAL_EYES_MS) /
				(float)(REVEAL_SWIRL_MS - REVEAL_EYES_MS);
			radiusMult = 3.0f - frac * 2.0f; /* converge to 1.0 */
		}
	}

	/* During death scatter, particles are already expanding via orbit_radius */
	float deathAlpha = 1.0f;
	if (boss.state == BOSS_DYING && boss.stateTimer < DEATH_SCATTER_MS) {
		deathAlpha = 1.0f - (float)boss.stateTimer / (float)DEATH_SCATTER_MS;
	} else if (boss.state == BOSS_DYING && boss.stateTimer >= DEATH_SCATTER_MS) {
		deathAlpha = 0.0f; /* swirl gone */
	}

	for (int i = 0; i < SWIRL_TOTAL; i++) {
		SwirlParticle *p = &boss.swirl[i];
		float r = p->orbit_radius * radiusMult;
		float px = cx + cosf(p->orbit_phase) * r;
		float py = cy + sinf(p->orbit_phase) * r;

		float a = p->a * alpha_mult * deathAlpha;
		if (a < 0.001f) continue;

		swirlBuffer[count] = (ParticleInstanceData){
			px, py, p->size,
			p->orbit_phase * (180.0f / (float)M_PI), /* rotation in degrees */
			p->is_ember ? 1.5f : 1.0f,  /* embers slightly elongated */
			p->r, p->g, p->b, a
		};
		count++;
	}

	if (count > 0) {
		Screen screen = Graphics_get_screen();
		Mat4 proj = Graphics_get_world_projection();
		Mat4 view = View_get_transform(&screen);

		/* Body blobs use PI_SHAPE_SOFT, embers use PI_SHAPE_FLAME */
		/* Render body blobs first (they're indices 0..SWIRL_BODY_COUNT-1) */
		int bodyCount = 0;
		int emberStart = 0;
		for (int i = 0; i < count; i++) {
			if (i < SWIRL_BODY_COUNT) bodyCount++;
			else if (emberStart == 0) emberStart = i;
		}

		if (bodyCount > 0)
			ParticleInstance_draw(swirlBuffer, bodyCount, &proj, &view, PI_SHAPE_SOFT);
		if (emberStart > 0 && count > emberStart)
			ParticleInstance_draw(&swirlBuffer[emberStart], count - emberStart, &proj, &view, PI_SHAPE_FLAME);
	}
}

static void render_eyes(void)
{
	if (boss.eyeAlpha < 0.01f) return;

	float cx = (float)boss.center.x;
	float cy = (float)boss.center.y;

	/* Eyes positioned toward player */
	float fwd_x = cosf(boss.eyeAngle) * EYE_FORWARD_DIST;
	float fwd_y = sinf(boss.eyeAngle) * EYE_FORWARD_DIST;

	/* Perpendicular for eye separation */
	float perp_x = -sinf(boss.eyeAngle) * EYE_SEPARATION;
	float perp_y = cosf(boss.eyeAngle) * EYE_SEPARATION;

	float a = boss.eyeAlpha;

	Position p1 = {cx + fwd_x + perp_x, cy + fwd_y + perp_y};
	Position p2 = {cx + fwd_x - perp_x, cy + fwd_y - perp_y};

	double heading = -(double)boss.eyeAngle * (180.0 / M_PI);

	/* Outer orange quad */
	float ow = EYE_SIZE * 1.6f;
	float oh = EYE_SIZE * 1.6f;
	Rectangle outerRect = {-ow, oh, ow, -oh};
	ColorFloat outerColor = {1.0f, 0.6f, 0.1f, a * 0.9f};
	Render_quad(&p1, heading, outerRect, &outerColor);
	Render_quad(&p2, heading, outerRect, &outerColor);

	/* Inner orange-white core */
	float ew = EYE_SIZE;
	float eh = EYE_SIZE;
	Rectangle eyeRect = {-ew, eh, ew, -eh};
	ColorFloat eyeColor = {1.0f, 0.8f, 0.3f, a};
	Render_quad(&p1, heading, eyeRect, &eyeColor);
	Render_quad(&p2, heading, eyeRect, &eyeColor);

	/* Bright white center slit */
	float iw = EYE_SIZE * 0.6f;
	float ih = EYE_SIZE * 0.5f;
	Rectangle innerRect = {-iw, ih, iw, -ih};
	ColorFloat eyeCenter = {1.0f, 1.0f, 0.9f, a * 0.8f};
	Render_quad(&p1, heading, innerRect, &eyeCenter);
	Render_quad(&p2, heading, innerRect, &eyeCenter);


}

static void render_eyes_bloom(void)
{
	if (boss.eyeAlpha < 0.01f) return;

	float cx = (float)boss.center.x;
	float cy = (float)boss.center.y;
	float fwd_x = cosf(boss.eyeAngle) * EYE_FORWARD_DIST;
	float fwd_y = sinf(boss.eyeAngle) * EYE_FORWARD_DIST;
	float perp_x = -sinf(boss.eyeAngle) * EYE_SEPARATION;
	float perp_y = cosf(boss.eyeAngle) * EYE_SEPARATION;

	float a = boss.eyeAlpha;
	Position p1 = {cx + fwd_x + perp_x, cy + fwd_y + perp_y};
	Position p2 = {cx + fwd_x - perp_x, cy + fwd_y - perp_y};
	double heading = -(double)boss.eyeAngle * (180.0 / M_PI);
	float bs = EYE_BLOOM_SIZE;
	Rectangle bloomRect = {-bs, bs, bs, -bs};
	ColorFloat bloomColor = {1.0f, 0.7f, 0.2f, a};
	Render_quad(&p1, heading, bloomRect, &bloomColor);
	Render_quad(&p2, heading, bloomRect, &bloomColor);


}

static void render_embers(void)
{
	int count = 0;
	for (int i = 0; i < EMBER_POOL_SIZE; i++) {
		Ember *e = &boss.embers[i];
		if (!e->active) continue;

		/* Rotation from velocity direction */
		float angle = (float)(atan2(e->vy, e->vx) * 180.0 / M_PI);

		/* Slight fade with age */
		float a = 0.9f;
		if (e->age_ms > 2000) {
			a = 0.9f - (float)(e->age_ms - 2000) / 4000.0f;
			if (a < 0.1f) a = 0.1f;
		}

		emberBuffer[count] = (ParticleInstanceData){
			(float)e->x, (float)e->y,
			EMBER_VISUAL_SIZE, angle, 1.8f,
			1.0f, 0.4f, 0.05f, a
		};
		count++;
	}

	if (count > 0) {
		Screen screen = Graphics_get_screen();
		Mat4 proj = Graphics_get_world_projection();
		Mat4 view = View_get_transform(&screen);
		ParticleInstance_draw(emberBuffer, count, &proj, &view, PI_SHAPE_FLAME);
	}
}

static void render_heat_pulse(void)
{
	if (!boss.pulse.active) return;

	float cx = (float)boss.center.x;
	float cy = (float)boss.center.y;
	float r = (float)boss.pulse.radius;
	int segments = 64;

	/* Outer ring edge */
	float outerR = r + (float)(HEAT_PULSE_WIDTH * 0.5);
	float innerR = r - (float)(HEAT_PULSE_WIDTH * 0.5);
	if (innerR < 0.0f) innerR = 0.0f;

	/* Fade as it expands */
	float fade = 1.0f - (float)(boss.pulse.radius / HEAT_PULSE_MAX_RANGE);
	if (fade < 0.0f) fade = 0.0f;

	/* Draw as a thick ring using line segments */
	for (int i = 0; i < segments; i++) {
		float a0 = (float)(2.0 * M_PI * i) / segments;
		float a1 = (float)(2.0 * M_PI * (i + 1)) / segments;

		/* Outer edge */
		Render_line_segment(
			cx + cosf(a0) * outerR, cy + sinf(a0) * outerR,
			cx + cosf(a1) * outerR, cy + sinf(a1) * outerR,
			1.0f, 0.5f, 0.1f, 0.7f * fade);

		/* Inner edge */
		Render_line_segment(
			cx + cosf(a0) * innerR, cy + sinf(a0) * innerR,
			cx + cosf(a1) * innerR, cy + sinf(a1) * innerR,
			1.0f, 0.3f, 0.05f, 0.5f * fade);
	}
}

static void render_heat_pulse_telegraph(void)
{
	if (!boss.pulseTelegraphing) return;

	float cx = (float)boss.center.x;
	float cy = (float)boss.center.y;
	float progress = 1.0f - (float)boss.pulseTelegraphTimer / (float)HEAT_PULSE_TELEGRAPH_MS;

	/* Pulsing circle at center that contracts inward */
	float r = VISUAL_RADIUS * (1.0f - progress * 0.5f);
	int segments = 48;
	float a = 0.3f + progress * 0.4f;

	for (int i = 0; i < segments; i++) {
		float a0 = (float)(2.0 * M_PI * i) / segments;
		float a1 = (float)(2.0 * M_PI * (i + 1)) / segments;
		Render_line_segment(
			cx + cosf(a0) * r, cy + sinf(a0) * r,
			cx + cosf(a1) * r, cy + sinf(a1) * r,
			1.0f, 0.6f, 0.1f, a);
	}
}

static void render_center_burn_indicator(void)
{
	if (!boss.centerBurnActive) return;

	float cx = (float)boss.center.x;
	float cy = (float)boss.center.y;
	float hs = (float)CENTER_BURN_HALF_SIZE;

	/* Subtle glowing border around center burn zone */
	float a = 0.2f;
	Render_line_segment(cx - hs, cy + hs, cx + hs, cy + hs, 1.0f, 0.3f, 0.0f, a);
	Render_line_segment(cx + hs, cy + hs, cx + hs, cy - hs, 1.0f, 0.3f, 0.0f, a);
	Render_line_segment(cx + hs, cy - hs, cx - hs, cy - hs, 1.0f, 0.3f, 0.0f, a);
	Render_line_segment(cx - hs, cy - hs, cx - hs, cy + hs, 1.0f, 0.3f, 0.0f, a);
}

void BossPyraxis_render(const void *state, const PlaceableComponent *placeable)
{
	(void)state;
	(void)placeable;

	if (!boss.initialized) return;
	if (boss.state == BOSS_INACTIVE) return;

	/* Render order: center burn → swirl → eyes → embers → heat pulse */
	render_center_burn_indicator();

	/* Swirl visible during reveal, active, and dying (scatter) */
	if (boss.state == BOSS_REVEAL || boss.state == BOSS_ACTIVE ||
		(boss.state == BOSS_DYING && boss.stateTimer < DEATH_SCATTER_MS)) {
		render_swirl(1.0f);
	}

	/* Eyes visible during reveal (fading in), active, and dying (until dim) */
	if (boss.state == BOSS_REVEAL || boss.state == BOSS_ACTIVE ||
		(boss.state == BOSS_DYING && boss.stateTimer < DEATH_EYES_DIM_MS)) {
		render_eyes();
	}

	/* Embers only during active combat */
	if (boss.state == BOSS_ACTIVE)
		render_embers();

	/* Heat pulse render */
	render_heat_pulse_telegraph();
	render_heat_pulse();
}

static void boss_render_bloom(const void *state, const PlaceableComponent *placeable)
{
	(void)state;
	(void)placeable;

	if (!boss.initialized) return;
	if (boss.state == BOSS_INACTIVE) return;

	/* Eye bloom */
	render_eyes_bloom();

	/* Swirl ember bloom */
	if (boss.state == BOSS_ACTIVE || boss.state == BOSS_REVEAL) {
		/* Re-render just the ember particles as bloom source */
		float cx = (float)boss.center.x;
		float cy = (float)boss.center.y;

		for (int i = SWIRL_BODY_COUNT; i < SWIRL_TOTAL; i++) {
			SwirlParticle *p = &boss.swirl[i];
			float r = p->orbit_radius;
			float px = cx + cosf(p->orbit_phase) * r;
			float py = cy + sinf(p->orbit_phase) * r;
			Position pos = {px, py};
			ColorFloat c = {p->r, p->g, p->b, p->a * 0.6f};
			Render_point(&pos, p->size * 1.5f, &c);
		}
	}
}

/* Global pipeline callbacks — render embers and pulse as bloom/light */
static void boss_render_global_bloom(void)
{
	if (!boss.initialized || boss.state == BOSS_INACTIVE) return;

	/* Ember projectile bloom */
	for (int i = 0; i < EMBER_POOL_SIZE; i++) {
		Ember *e = &boss.embers[i];
		if (!e->active) continue;
		Position pos = {e->x, e->y};
		ColorFloat c = {1.0f, 0.5f, 0.1f, 0.6f};
		Render_point(&pos, 16.0f, &c);
	}

	/* Heat pulse bloom */
	if (boss.pulse.active) {
		float cx = (float)boss.center.x;
		float cy = (float)boss.center.y;
		float r = (float)boss.pulse.radius;
		float fade = 1.0f - (float)(boss.pulse.radius / HEAT_PULSE_MAX_RANGE);
		if (fade < 0.0f) fade = 0.0f;
		int segs = 32;
		for (int i = 0; i < segs; i++) {
			float a = (float)(2.0 * M_PI * i) / segs;
			Position pos = {cx + cosf(a) * r, cy + sinf(a) * r};
			ColorFloat c = {1.0f, 0.4f, 0.05f, 0.4f * fade};
			Render_point(&pos, 20.0f, &c);
		}
	}
}

static void boss_render_global_light(void)
{
	if (!boss.initialized || boss.state == BOSS_INACTIVE) return;

	/* Node center glow on map cells */
	if (boss.state == BOSS_ACTIVE || boss.state == BOSS_REVEAL) {
		Render_filled_circle(
			(float)boss.center.x, (float)boss.center.y,
			VISUAL_RADIUS * 1.2f, 24,
			1.0f, 0.3f, 0.05f, 0.3f);
	}

	/* Ember projectile light */
	for (int i = 0; i < EMBER_POOL_SIZE; i++) {
		Ember *e = &boss.embers[i];
		if (!e->active) continue;
		Render_filled_circle((float)e->x, (float)e->y, 80.0f, 8,
			1.0f, 0.5f, 0.1f, 0.5f);
	}
}

/* Global update — updates ember projectiles (runs unconditionally like hunter projectiles) */
static void boss_update_global(unsigned int ticks)
{
	if (!boss.initialized) return;
	if (boss.state != BOSS_ACTIVE) return;

	update_embers(ticks);
}
