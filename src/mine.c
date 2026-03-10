#include "mine.h"
#include "sub_mine_core.h"
#include "sub_cinder_core.h"
#include "enemy_util.h"
#include "enemy_variant.h"
#include "fragment.h"
#include "progression.h"
#include "view.h"
#include "render.h"
#include "color.h"
#include "audio.h"
#include "sub_stealth.h"
#include "spatial_grid.h"
#include "burn.h"
#include "ship.h"
#include "player_stats.h"
#include "global_render.h"
#include "global_update.h"

#include <stdlib.h>
#include <SDL2/SDL_mixer.h>

#define MINE_COUNT 4096
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

/* Fire mine config — based on cinder core's visual config with enemy timing overrides */
static SubMineConfig fireMineCfg;
static bool fireMineCfgInitialized = false;

static void init_fire_mine_cfg(void)
{
	if (fireMineCfgInitialized) return;
	fireMineCfg = *SubCinder_get_fire_mine_config();
	fireMineCfg.armed_duration_ms = 500;
	fireMineCfg.dead_duration_ms = 10000;
	fireMineCfgInitialized = true;
}

static const CarriedSubroutine mineCarried[] = {
	{ SUB_ID_MINE, FRAG_TYPE_MINE },
};

static const EnemyVariant mineVariants[THEME_COUNT] = {
	[THEME_FIRE] = {
		.tint = {1.0f, 0.5f, 0.1f, 1.0f},
		.carried = {{ SUB_ID_CINDER, FRAG_TYPE_CINDER }},
		.carried_count = 1,
		.speed_mult = 1.0f, .aggro_range_mult = 1.0f,
		.hp_mult = 1.0f, .attack_cadence_mult = 1.0f,
	},
};

static void mine_render_bloom(const void *state, const PlaceableComponent *placeable);
static void mine_render_light(const void *state, const PlaceableComponent *placeable);
static RenderableComponent renderable = {.passes = {
	[RENDER_PASS_MAIN] = Mine_render,
	[RENDER_PASS_BLOOM_SOURCE] = mine_render_bloom,
	[RENDER_PASS_LIGHT_SOURCE] = mine_render_light,
}};
static CollidableComponent collidable = {{-250.0, 250.0, 250.0, -250.0},
											true,
											COLLISION_LAYER_ENEMY,
											COLLISION_LAYER_PLAYER,
											Mine_collide, Mine_resolve};
static AIUpdatableComponent updatable = {Mine_update};

typedef struct {
	SubMineCore core;
	bool killedByPlayer;
	ZoneTheme theme;
	CinderFirePool firePool;
} MineState;

static MineState mines[MINE_COUNT];
static PlaceableComponent placeables[MINE_COUNT];
static Entity *entityRefs[MINE_COUNT];
static int highestUsedIndex = 0;

/* Audio — entity sounds only (respawn) */
static Mix_Chunk *sampleRespawn = 0;
static bool pipelineRegistered = false;

void Mine_initialize(Position position, ZoneTheme theme)
{
	if (highestUsedIndex == MINE_COUNT) {
		printf("WARNING: Mine pool full (%d)\n", MINE_COUNT);
		return;
	}

	SubMine_init(&mines[highestUsedIndex].core);
	mines[highestUsedIndex].core.position = position;
	mines[highestUsedIndex].core.blinkTimer = rand() % 1000;
	mines[highestUsedIndex].killedByPlayer = false;
	mines[highestUsedIndex].theme = theme;
	SubCinder_init_pool(&mines[highestUsedIndex].firePool);

	if (theme == THEME_FIRE)
		init_fire_mine_cfg();

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

	SpatialGrid_add((EntityRef){ENTITY_MINE, highestUsedIndex - 1}, position.x, position.y);

	/* Load audio once, not per-entity */
	if (!sampleRespawn)
		Audio_load_sample(&sampleRespawn, "resources/sounds/door.wav");

	/* Register pipeline callbacks (survives Zone_rebuild_enemies) */
	if (!pipelineRegistered) {
		GlobalRender_register(RENDER_PASS_WORLD_OVERLAY, Mine_render_fire_pools);
		GlobalRender_register(RENDER_PASS_BLOOM_SOURCE, Mine_render_fire_pool_bloom);
		GlobalRender_register(RENDER_PASS_LIGHT_SOURCE, Mine_render_fire_pool_light);
		GlobalUpdate_register_post_collision(Mine_update_fire_pools);
		pipelineRegistered = true;
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
	pipelineRegistered = false;
}

Collision Mine_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	MineState *ms = (MineState *)state;
	SubMineCore *core = &ms->core;
	Collision collision = {false, false};

	if (core->phase == MINE_PHASE_DEAD)
		return collision;

	Position position = placeable->position;

	/* Select config based on theme */
	const SubMineConfig *cfg = (ms->theme == THEME_FIRE) ? &fireMineCfg : &enemyMineCfg;

	/* Check direct body hit (the grey diamond + red dot) */
	float bs = cfg->body_half_size;
	Rectangle body = {-bs, bs, bs, -bs};
	Rectangle bodyTransformed = Collision_transform_bounding_box(position, body);
	if (core->phase != MINE_PHASE_EXPLODING && Collision_aabb_test(bodyTransformed, boundingBox)) {
		/* Direct contact — instant explosion, breaks stealth */
		Enemy_break_cloak();
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

	const SubMineConfig *cfg = (ms->theme == THEME_FIRE) ? &fireMineCfg : &enemyMineCfg;
	SubMine_arm(core, cfg, core->position);
}

void Mine_update(void *state, const PlaceableComponent *placeable, const unsigned int ticks)
{
	MineState *ms = (MineState *)state;
	SubMineCore *core = &ms->core;

	const SubMineConfig *cfg = (ms->theme == THEME_FIRE) ? &fireMineCfg : &enemyMineCfg;

	/* Dormancy check — only tick DEAD→DORMANT timer */
	if (!SpatialGrid_is_active(placeable->position.x, placeable->position.y)) {
		if (core->phase == MINE_PHASE_DEAD) {
			core->phaseTicks += ticks;
			if (core->phaseTicks >= cfg->dead_duration_ms) {
				core->phase = MINE_PHASE_DORMANT;
				core->phaseTicks = 0;
				ms->killedByPlayer = false;
			}
		}
		return;
	}

	/* Check for player projectile hits on the mine body */
	if (core->phase != MINE_PHASE_DEAD && core->phase != MINE_PHASE_EXPLODING) {
		float bs = cfg->body_half_size;
		Rectangle body = {-bs, bs, bs, -bs};
		Rectangle mineBody = Collision_transform_bounding_box(placeable->position, body);
		if (Enemy_check_any_hit(mineBody)) {
			SubMine_detonate(core);
			ms->killedByPlayer = true;
		}
	}

	MinePhase prevPhase = core->phase;
	MinePhase phase = SubMine_update(core, cfg, ticks);

	/* Fire mine: spawn fire pool on detonation */
	if (ms->theme == THEME_FIRE && phase == MINE_PHASE_EXPLODING && prevPhase == MINE_PHASE_ARMED) {
		SubCinder_spawn_pool(&ms->firePool, 1, core->position);
	}

	/* Just transitioned out of DEAD back to DORMANT — respawn */
	if (phase == MINE_PHASE_DORMANT && prevPhase == MINE_PHASE_DEAD) {
		Audio_play_sample_at(&sampleRespawn, placeable->position);
		ms->killedByPlayer = false;
	}

	/* Fragment drop on transition from EXPLODING to DEAD */
	if (phase == MINE_PHASE_DEAD && prevPhase == MINE_PHASE_EXPLODING) {
		if (ms->killedByPlayer) {
			int count;
			const CarriedSubroutine *carried = Variant_get_carried(
				mineVariants, ms->theme, mineCarried, 1, &count);
			Enemy_drop_fragments(placeable->position, carried, count);
		}
	}
}

void Mine_render(const void *state, const PlaceableComponent *placeable)
{
	const MineState *ms = (const MineState *)state;
	const SubMineCore *core = &ms->core;

	if (core->phase == MINE_PHASE_DEAD)
		return;

	const SubMineConfig *cfg = (ms->theme == THEME_FIRE) ? &fireMineCfg : &enemyMineCfg;
	SubMineCore renderCore = *core;
	renderCore.position = placeable->position;
	SubMine_render(&renderCore, cfg);
}

static void mine_render_bloom(const void *state, const PlaceableComponent *placeable)
{
	const MineState *ms = (const MineState *)state;
	const SubMineCore *core = &ms->core;
	if (core->phase == MINE_PHASE_DEAD)
		return;
	const SubMineConfig *cfg = (ms->theme == THEME_FIRE) ? &fireMineCfg : &enemyMineCfg;
	SubMineCore renderCore = *core;
	renderCore.position = placeable->position;
	SubMine_render_bloom(&renderCore, cfg);
}

static void mine_render_light(const void *state, const PlaceableComponent *placeable)
{
	const MineState *ms = (const MineState *)state;
	const SubMineCore *core = &ms->core;
	if (core->phase == MINE_PHASE_DEAD)
		return;
	const SubMineConfig *cfg = (ms->theme == THEME_FIRE) ? &fireMineCfg : &enemyMineCfg;
	SubMineCore renderCore = *core;
	renderCore.position = placeable->position;
	SubMine_render_light(&renderCore, cfg);
}

void Mine_reset_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		SubMine_init(&mines[i].core);
		mines[i].core.position = placeables[i].position;
		mines[i].core.blinkTimer = rand() % 1000;
		mines[i].killedByPlayer = false;
		SubCinder_init_pool(&mines[i].firePool);
	}
}

int Mine_get_count(void)
{
	return highestUsedIndex;
}

/* --- Fire mine pool functions --- */

void Mine_update_fire_pools(unsigned int ticks)
{
	const SubCinderConfig *cfg = SubCinder_get_config();

	for (int i = 0; i < highestUsedIndex; i++) {
		if (mines[i].theme != THEME_FIRE)
			continue;

		SubCinder_update_pools(&mines[i].firePool, 1, cfg, ticks);

		/* Check if player is standing in the fire pool */
		if (mines[i].firePool.active) {
			Position shipPos = Ship_get_position();
			float hs = 15.0f;
			Rectangle playerBox = {
				shipPos.x - hs, shipPos.y + hs,
				shipPos.x + hs, shipPos.y - hs
			};
			int hits = SubCinder_check_pool_burn(&mines[i].firePool, 1, cfg, playerBox);
			if (hits > 0)
				Burn_apply_to_player(BURN_DURATION_MS);
		}
	}
}

void Mine_render_fire_pools(void)
{
	const SubCinderConfig *cfg = SubCinder_get_config();
	for (int i = 0; i < highestUsedIndex; i++) {
		if (mines[i].theme != THEME_FIRE)
			continue;
		SubCinder_render_pools(&mines[i].firePool, 1, cfg);
	}
}

void Mine_render_fire_pool_bloom(void)
{
	const SubCinderConfig *cfg = SubCinder_get_config();
	for (int i = 0; i < highestUsedIndex; i++) {
		if (mines[i].theme != THEME_FIRE)
			continue;
		SubCinder_render_pools_bloom(&mines[i].firePool, 1, cfg);
	}
}

void Mine_render_fire_pool_light(void)
{
	const SubCinderConfig *cfg = SubCinder_get_config();
	for (int i = 0; i < highestUsedIndex; i++) {
		if (mines[i].theme != THEME_FIRE)
			continue;
		SubCinder_render_pools_light(&mines[i].firePool, 1, cfg);
	}
}
