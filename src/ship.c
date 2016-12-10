#include "ship.h"
#include "view.h"
#include "render.h"
#include "color.h"

static const double NORMAL_VELOCITY = 500.0;
static const double FAST_VELOCITY = 1500.0;
static const double SLOW_VELOCITY = 100.0;

static ColorRGB color = {255, 0, 0, 255};

static PlaceableComponent placeable = {{0.0, 0.0}, 0.0};
static RenderableComponent renderable = {Ship_render};
static UserUpdatableComponent updatable = {Ship_update};
static CollidableComponent collidable = {{-20.0, 20.0, 20.0, -20.0}, true, Ship_collide, Ship_resolve};

static double get_heading(bool n, bool s, bool e, bool w);

void Ship_initialize() 
{
	Entity entity = Entity_initialize_entity();
	entity.placeable = &placeable;
	entity.renderable = &renderable;
	entity.userUpdatable = &updatable;
	entity.collidable = &collidable;

	Entity_add_entity(entity);
}

void Ship_cleanup() 
{
	placeable.position.x = 0.0;
	placeable.position.y = 0.0;
	placeable.heading = 0.0;
}

Collision Ship_collide(const void *entity, const PlaceableComponent *placeable, const Rectangle boundingBox) 
{
	Position position = placeable->position;
	Rectangle thisBoundingBox = collidable.boundingBox;
	Rectangle transformedBoundingBox = {
		thisBoundingBox.aX + position.x,
		thisBoundingBox.aY + position.y,
		thisBoundingBox.bX + position.x,
		thisBoundingBox.bY + position.y,
	};

	Collision collision = {false, true};

	if (Collision_aabb_test(transformedBoundingBox, boundingBox)) {
		collision.collisionDetected = true;
	}

	return collision;
}

void Ship_resolve(const void *entity, const Collision collision)
{
	if (collision.solid)
	{
		placeable.position.x = 0.0;
		placeable.position.y = 0.0;
	}
}

void Ship_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable)
{
	double ticksNormalized = ticks / 1000.0;
	
	double velocity;
	if (userInput->keyLShift)
		velocity = FAST_VELOCITY;
	else if (userInput->keyLControl)
		velocity = SLOW_VELOCITY;
	else
		velocity = NORMAL_VELOCITY;

	if (userInput->keyW)
		placeable->position.y += velocity * ticksNormalized;
	if (userInput->keyS)
		placeable->position.y -= velocity * ticksNormalized;
	if (userInput->keyD)
		placeable->position.x += velocity * ticksNormalized;
	if (userInput->keyA)
		placeable->position.x -= velocity * ticksNormalized;

	if (userInput->keyW || userInput->keyA || 
		userInput->keyS || userInput->keyD)
	{
		placeable->heading = get_heading(userInput->keyW, userInput->keyS, 
										userInput->keyD, userInput->keyA);
	}
}

void Ship_render(const void *entity, const PlaceableComponent *placeable)
{
	View view =  View_get_view();
	ColorFloat colorFloat = Color_rgb_to_float(&color);

	if (view.scale > 0.09)
		Render_triangle(&placeable->position, placeable->heading, 
			colorFloat.red, colorFloat.green, colorFloat.blue, colorFloat.alpha);
	else
		Render_point(&placeable->position, colorFloat.red, colorFloat.green, 
			colorFloat.blue, colorFloat.alpha);

	//Render_bounding_box(&placeable->position, &collidable.boundingBox);
}

Position Ship_get_position()
{
	return placeable.position;
}

static double get_heading(const bool n, const bool s, 
	const bool e, const bool w)
{
	if (n) {
		if (e)
			return 45.0;
		else if (w)
			return 315.0;
		else
			return 0.0;
	}
	if (s) {
		if (e)
			return 135.0;
		else if (w)
			return 225.0;
		else
			return 180.0;
	}
	if (e)
		return 90.0;
	if (w)
		return 270.0;

	return 0.0;
}