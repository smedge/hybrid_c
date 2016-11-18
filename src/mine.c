#include "mine.h"
#include "view.h"
#include "render.h"

#define MINE_COUNT 16
#define MINE_ROTATION 0.0

static RenderableComponent renderable = {Mine_render};
static CollidableComponent collidable = {{-5.0, 5.0, 5.0, -5.0}, true, Mine_collide, Mine_resolve};

static ColorRGB color = {0, 0, 255, 255};

typedef struct {
	PlaceableComponent placeable;
	bool active;
} MineState;

static MineState mines[MINE_COUNT];
static int highestUsedIndex = 0;

void Mine_initialize(Position position)
{
	int id = Entity_create_entity(COMPONENT_PLACEABLE | 
									COMPONENT_RENDERABLE |
									COMPONENT_COLLIDABLE);

	if (highestUsedIndex == MINE_COUNT) {
		// TODO bail, too many
	}

	mines[highestUsedIndex].placeable.position.x = 0.0;
	mines[highestUsedIndex].placeable.position.y = 0.0;
	mines[highestUsedIndex].placeable.heading = 0.0;

	mines[highestUsedIndex].placeable.position = position;
	Entity_add_placeable(id, &mines[highestUsedIndex].placeable);
	Entity_add_renderable(id, &renderable);
	Entity_add_collidable(id, &collidable);
	highestUsedIndex++;
}

void Mine_cleanup()
{
	highestUsedIndex = 0;
}

Collision Mine_collide(const Rectangle boundingBox, const PlaceableComponent *placeable)
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

void Mine_resolve(Collision collision) 
{
	color.red = 255;
	color.green = 0;
	color.blue = 0;
}

void Mine_update(const Input *userInput, const unsigned int ticks,
	PlaceableComponent *placeable) 
{
	
}

void Mine_render(const PlaceableComponent *placeable) 
{
	//View view =  View_get_view();
	ColorFloat colorFloat = Color_rgb_to_float(&color);
	Render_point(&placeable->position, colorFloat.red, colorFloat.green, 
			colorFloat.blue, colorFloat.alpha);
}