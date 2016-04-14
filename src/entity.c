#include "entity.h"


#include "stdio.h" // DEV


static int entities[ENTITY_COUNT];
static unsigned int highestIndex = 0;
static PlaceableComponent *placeables[ENTITY_COUNT];
static RenderableComponent *renderables[ENTITY_COUNT];
static CollidableComponent *collidables[ENTITY_COUNT];
static UserUpdatableComponent *user_updatables[ENTITY_COUNT];

int Entity_create_entity(int componentMask)
{
	unsigned int entityId;
	for(entityId = 0; entityId < ENTITY_COUNT; ++entityId)
	{
		if(entities[entityId] == COMPONENT_NONE)
			break;
	}

	if (highestIndex < entityId)
		highestIndex = entityId;

	entities[entityId] = componentMask;
	return entityId;
}

void Entity_destroy_all()
{
	unsigned int entityId;
	for(entityId = 0; entityId < ENTITY_COUNT; ++entityId)
	{
		Entity_destroy(entityId);
	}
}

void Entity_destroy(int entityId) 
{
	entities[entityId] = COMPONENT_NONE;
}

void Entity_add_placeable(int entityId, PlaceableComponent *placeable) 
{
	placeables[entityId] = placeable;
}

void Entity_add_renderable(int entityId, RenderableComponent *renderable)
{
	renderables[entityId] = renderable;
}

void Entity_add_collidable(int entityId, CollidableComponent *collidable) 
{
	collidables[entityId] = collidable;
}

void Entity_add_user_updatable(int entityId, UserUpdatableComponent *updatable) 
{
	user_updatables[entityId] = updatable;
}

void Entity_user_update_system(const Input *input, const unsigned int ticks)
{
	for(int i = 0; i <= highestIndex; i++) 
	{
		if ((entities[i] & USER_UPDATE_SYSTEM_MASK) != USER_UPDATE_SYSTEM_MASK)
			continue;

		user_updatables[i]->update(input, ticks, placeables[i]);
	}
}

void Entity_render_system() 
{
	for(int i = 0; i <= highestIndex; i++) 
	{
		if ((entities[i] & RENDER_SYSTEM_MASK) != RENDER_SYSTEM_MASK)
			continue;
		
		renderables[i]->render(placeables[i]);
	}
}

void Entity_collision_system()
{
	for(int i = 0; i <= highestIndex; i++) 
	{
		if ((entities[i] & COLLISION_SYSTEM_MASK) != COLLISION_SYSTEM_MASK)
			continue;

		if (!collidables[i]->collidesWithOthers)
			continue;

		for (int j = 0; j <= highestIndex; j++)
		{
			if ((entities[j] & COLLISION_SYSTEM_MASK) != COLLISION_SYSTEM_MASK)
				continue;

			if (i == j)
				continue;

			// create a cransformed bounding box for i
			Position position = placeables[i]->position;
			Rectangle boundingBox = collidables[i]->boundingBox;
			Rectangle transformedBoundingBox = {
				boundingBox.aX + position.x,
				boundingBox.aY + position.y,
				boundingBox.bX + position.x,
				boundingBox.bY + position.y,
			};

			// call j's collide with i's transformed bounding box
			bool collisionDetected = collidables[j]->collide(transformedBoundingBox);
			if (collisionDetected)
			{
				printf("resolve\n");
				collidables[i]->resolve();
			}
		}
	}
}