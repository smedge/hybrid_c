#include "mine.h"
#include "view.h"
#include "render.h"

#define MINE_COUNT 16
#define MINE_ROTATION 0.0
#define TICKS_ACTIVE 5000

static RenderableComponent renderable = {Mine_render};
static CollidableComponent collidable = {{-50.0, 50.0, 50.0, -50.0}, true, Mine_collide, Mine_resolve};
static AIUpdatableComponent updatable = {Mine_update};

static ColorRGB color = {120, 120, 120, 255};
static ColorRGB colorActive = {255, 0, 0, 255};

typedef struct {
	bool active;
	unsigned int ticksActive;
} MineState;

static MineState mines[MINE_COUNT];
static PlaceableComponent placeables[MINE_COUNT];
static int highestUsedIndex = 0;

void Mine_initialize(Position position)
{
	int id = Entity_create_entity(COMPONENT_PLACEABLE | 
									COMPONENT_RENDERABLE |
									COMPONENT_COLLIDABLE |
									COMPONENT_AI_UPDATABLE);

	if (highestUsedIndex == MINE_COUNT) {
		// TODO bail, too many
	}

	mines[highestUsedIndex].active = false;
	mines[highestUsedIndex].ticksActive = 0;

	placeables[highestUsedIndex].position = position;
	placeables[highestUsedIndex].heading = MINE_ROTATION;
	
	Entity_add_state(id, &mines[highestUsedIndex]);
	Entity_add_placeable(id, &placeables[highestUsedIndex]);
	Entity_add_renderable(id, &renderable);
	Entity_add_collidable(id, &collidable);
	Entity_add_ai_updatable(id, &updatable);
	
	highestUsedIndex++;
}

void Mine_cleanup()
{
	highestUsedIndex = 0;
}

Collision Mine_collide(const void *entity, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	Position position = placeable->position;
	Rectangle thisBoundingBox = collidable.boundingBox;
	Rectangle transformedBoundingBox = {
		thisBoundingBox.aX + position.x,
		thisBoundingBox.aY + position.y,
		thisBoundingBox.bX + position.x,
		thisBoundingBox.bY + position.y,
	};

	Collision collision = {false, false};

	if (Collision_aabb_test(transformedBoundingBox, boundingBox)) {
		collision.collisionDetected = true;
	}

	return collision;
}

void Mine_resolve(const void *entity, const Collision collision) 
{
	MineState* state = (MineState*)entity;
	state->active = true;
	state->ticksActive = 0;
}

void Mine_update(const void *entity, const PlaceableComponent *placeable, const unsigned int ticks)
{
	MineState* state = (MineState*)entity;
	if (state->active)
	{
		state->ticksActive += ticks;
		if (state->ticksActive > TICKS_ACTIVE)
			state->active = false;
	}
}

void Mine_render(const void *entity, const PlaceableComponent *placeable) 
{
	MineState* state = (MineState*)entity;

	ColorFloat colorFloat;
	if (state->active)
		colorFloat= Color_rgb_to_float(&colorActive);
	else
		colorFloat= Color_rgb_to_float(&color);

	Render_point(&placeable->position, colorFloat.red, colorFloat.green, 
			colorFloat.blue, colorFloat.alpha);
}