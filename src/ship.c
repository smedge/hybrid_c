#include "ship.h"
#include "view.h"
#include "render.h"

static const double NORMAL_VELOCITY = 20.0;
static const double FAST_VELOCITY = 50.0;
static const double SLOW_VELOCITY = 5.0;

static double get_heading(bool n, bool s, bool e, bool w);

void Ship_initialize() 
{
	int id = Entity_create_entity(COMPONENT_PLACEABLE | 
									COMPONENT_RENDERABLE |
									COMPONENT_PLAYER_UPDATABLE);

	PlaceableComponent placeable = {{0.0, 0.0}, 0.0};
	Entity_add_placeable(id, placeable);
	
	RenderableComponent renderable;
	renderable.render = Ship_render;
	Entity_add_renderable(id, renderable);

	UserUpdatableComponent updatable;
	updatable.update = Ship_update;
	Entity_add_user_updatable(id, updatable);
}

void Ship_cleanup() 
{

}

void Ship_collide(void) 
{

}

void Ship_resolve(void)
{

}

void Ship_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable)
{
	double velocity;
	
	if (userInput->keyLShift)
		velocity = FAST_VELOCITY;
	else if (userInput->keyLControl)
		velocity = SLOW_VELOCITY;
	else
		velocity = NORMAL_VELOCITY;

	if (userInput->keyW)
		placeable->position.y += velocity;
	if (userInput->keyS)
		placeable->position.y -= velocity;
	if (userInput->keyD)
		placeable->position.x += velocity;
	if (userInput->keyA)
		placeable->position.x -= velocity;

	if (userInput->keyW || userInput->keyA || 
		userInput->keyS || userInput->keyD)
	{
		placeable->heading = get_heading(userInput->keyW, userInput->keyS, 
										userInput->keyD, userInput->keyA);
										View_set_position(placeable->position);
	}
}

void Ship_render(const PlaceableComponent *placeable)
{
	View view =  View_get_view();

	if (view.scale > 0.05)
		Render_triangle(&placeable->position, placeable->heading, 255.0, 0.0, 0.0, 1.0);
	else
		Render_point(&placeable->position, 255.0, 0.0, 0.0, 1.0);
}

static double get_heading(bool n, bool s, bool e, bool w)
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