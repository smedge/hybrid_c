#include "mine.h"
#include "sub_mine_core.h"
#include "enemy_util.h"
#include "fragment.h"
#include "progression.h"
#include "view.h"
#include "render.h"
#include "color.h"
#include "audio.h"
#include "sub_stealth.h"

#include <stdlib.h>
#include <SDL2/SDL_mixer.h>

#define MINE_COUNT 512
#define MINE_ROTATION 0.0

static const SubMineConfig enemyMineCfg = {
	.armed_duration_ms = 500,
	.exploding_duration_ms = 100,
	.dead_duration_ms = 10000,
	.explosion_half_size = 250.0f,
	.body_half_size = 10.0f,
	.body_r = 0.196f, .body_g = 0.196f, .body_b = 0.196f, .body_a = 1.0f,
	.blink_r = 1.0f, .blink_g = 0.0f, .blink_b = 0.0f, .blink_a = 1.0f,
	.explosion_r = 1.0f, .explosion_g = 1.0f, .explosion_b = 1.0f, .explosion_a = 1.0f,
	.light_armed_radius = 150.0f,
	.light_armed_r = 1.0f, .light_armed_g = 0.2f, .light_armed_b = 0.1f, .light_armed_a = 0.3f,
	.light_explode_radius = 750.0f,
	.light_explode_r = 1.0f, .light_explode_g = 0.9f, .light_explode_b = 0.7f, .light_explode_a = 1.0f,
};

static const CarriedSubroutine mineCarried[] = {
	{ SUB_ID_MINE, FRAG_TYPE_MINE },
};

static RenderableComponent renderable = {Mine_render};
static CollidableComponent collidable = {{-250.0, 250.0, 250.0, -250.0},
											true,
											COLLISION_LAYER_ENEMY,
											COLLISION_LAYER_PLAYER,
											Mine_collide, Mine_resolve};
static AIUpdatableComponent updatable = {Mine_update};

typedef struct {
	SubMineCore core;
	bool killedByPlayer;
} MineState;

static MineState mines[MINE_COUNT];
static PlaceableComponent placeables[MINE_COUNT];
static Entity *entityRefs[MINE_COUNT];
static int highestUsedIndex = 0;

/* Audio — entity sounds only (respawn) */
static Mix_Chunk *sampleRespawn = 0;

void Mine_initialize(Position position)
{
	if (highestUsedIndex == MINE_COUNT) {
		printf("WARNING: Mine pool full (%d)\n", MINE_COUNT);
		return;
	}

	SubMine_init(&mines[highestUsedIndex].core);
	mines[highestUsedIndex].core.position = position;
	mines[highestUsedIndex].core.blinkTimer = rand() % 1000;
	mines[highestUsedIndex].killedByPlayer = false;

	placeables[highestUsedIndex].position = position;
	placeables[highestUsedIndex].heading = MINE_ROTATION;

	Entity entity = Entity_initialize_entity();
	entity.state = &mines[highestUsedIndex];
	entity.placeable = &placeables[highestUsedIndex];
	entity.renderable = &renderable;
	entity.collidable = &collidable;
	entity.aiUpdatable = &updatable;

	entityRefs[highestUsedIndex] = Entity_add_entity(entity);

	highestUsedIndex++;

	/* Load audio once, not per-entity */
	if (!sampleRespawn) {
		Audio_load_sample(&sampleRespawn, "resources/sounds/door.wav");
	}
}

void Mine_cleanup()
{
	for (int i = 0; i < highestUsedIndex; i++) {
		if (entityRefs[i]) {
			entityRefs[i]->empty = true;
			entityRefs[i] = NULL;
		}
	}
	highestUsedIndex = 0;

	Audio_unload_sample(&sampleRespawn);
}

Collision Mine_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	MineState *ms = (MineState *)state;
	SubMineCore *core = &ms->core;
	Collision collision = {false, false};

	if (core->phase == MINE_PHASE_DEAD)
		return collision;

	Position position = placeable->position;

	/* Check direct body hit (the grey diamond + red dot) */
	float bs = enemyMineCfg.body_half_size;
	Rectangle body = {-bs, bs, bs, -bs};
	Rectangle bodyTransformed = Collision_transform_bounding_box(position, body);
	if (core->phase != MINE_PHASE_EXPLODING && Collision_aabb_test(bodyTransformed, boundingBox)) {
		/* Direct contact — instant explosion, breaks stealth */
		Sub_Stealth_break();
		SubMine_detonate(core);
		ms->killedByPlayer = true;
		collision.collisionDetected = true;
		collision.solid = true;
		return collision;
	}

	/* Detection radius — only triggers when unstealthed */
	Rectangle thisBoundingBox = collidable.boundingBox;
	Rectangle transformedBoundingBox = Collision_transform_bounding_box(position, thisBoundingBox);

	if (Collision_aabb_test(transformedBoundingBox, boundingBox)) {
		collision.collisionDetected = true;
		if (core->phase == MINE_PHASE_EXPLODING)
			collision.solid = true;
	}

	return collision;
}

void Mine_resolve(void *state, const Collision collision)
{
	MineState *ms = (MineState *)state;
	SubMineCore *core = &ms->core;

	if (core->phase != MINE_PHASE_DORMANT)
		return;

	/* Detection radius — stealth bypasses it */
	if (Sub_Stealth_is_stealthed())
		return;

	SubMine_arm(core, &enemyMineCfg, core->position);
}

void Mine_update(void *state, const PlaceableComponent *placeable, const unsigned int ticks)
{
	MineState *ms = (MineState *)state;
	SubMineCore *core = &ms->core;

	/* Check for player projectile hits on the mine body */
	if (core->phase != MINE_PHASE_DEAD && core->phase != MINE_PHASE_EXPLODING) {
		float bs = enemyMineCfg.body_half_size;
		Rectangle body = {-bs, bs, bs, -bs};
		Rectangle mineBody = Collision_transform_bounding_box(placeable->position, body);
		if (Enemy_check_any_hit(mineBody)) {
			SubMine_detonate(core);
			ms->killedByPlayer = true;
		}
	}

	MinePhase prevPhase = core->phase;
	MinePhase phase = SubMine_update(core, &enemyMineCfg, ticks);

	/* Just transitioned out of DEAD back to DORMANT — respawn */
	if (phase == MINE_PHASE_DORMANT && prevPhase == MINE_PHASE_DEAD) {
		Audio_play_sample(&sampleRespawn);
		ms->killedByPlayer = false;
	}

	/* Fragment drop on transition from EXPLODING to DEAD */
	if (phase == MINE_PHASE_DEAD && prevPhase == MINE_PHASE_EXPLODING) {
		if (ms->killedByPlayer)
			Enemy_drop_fragments(placeable->position, mineCarried, 1);
	}
}

void Mine_render(const void *state, const PlaceableComponent *placeable)
{
	MineState *ms = (MineState *)state;
	SubMineCore *core = &ms->core;

	if (core->phase == MINE_PHASE_DEAD)
		return;

	/* Use placeable position (matches entity system) but core stores the same pos */
	SubMineCore renderCore = *core;
	renderCore.position = placeable->position;
	SubMine_render(&renderCore, &enemyMineCfg);
}

void Mine_render_bloom_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		SubMineCore *core = &mines[i].core;
		if (core->phase == MINE_PHASE_DEAD)
			continue;
		SubMineCore renderCore = *core;
		renderCore.position = placeables[i].position;
		SubMine_render_bloom(&renderCore, &enemyMineCfg);
	}
}

void Mine_render_light_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		SubMineCore *core = &mines[i].core;
		if (core->phase == MINE_PHASE_DEAD)
			continue;
		SubMineCore renderCore = *core;
		renderCore.position = placeables[i].position;
		SubMine_render_light(&renderCore, &enemyMineCfg);
	}
}

void Mine_reset_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		SubMine_init(&mines[i].core);
		mines[i].core.position = placeables[i].position;
		mines[i].core.blinkTimer = rand() % 1000;
		mines[i].killedByPlayer = false;
	}
}
