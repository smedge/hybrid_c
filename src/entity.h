#ifndef ENTITY_H
#define ENTITY_H

#include "input.h"
#include "position.h"
#include "collision.h"
#include "component.h"

#define ENTITY_COUNT 2048
#define COLLISION_COUNT 512

typedef struct {
	bool empty;
	bool disabled;
	void *state;
	PlaceableComponent *placeable;
	RenderableComponent *renderable;
	CollidableComponent *collidable;
	DynamicsComponent *dynamics;
	UserUpdatableComponent *userUpdatable;
	AIUpdatableComponent *aiUpdatable;
} Entity;

typedef struct {
	void (*resolve)(void *state, const Collision collision);
	void *state;
	Collision collision;
} ResolveCollisionCommand;

Entity Entity_initialize_entity();
Entity* Entity_add_entity(const Entity entity);
void Entity_destroy_all(void);
void Entity_destroy(const unsigned int entityId);
void Entity_recalculate_highest_index(void);

void Entity_user_update_system(const Input *input, const unsigned int ticks);
void Entity_ai_update_system(const unsigned int ticks);
void Entity_render_system(void);
void Entity_collision_system(void);
void Entity_create_collision_command(void (*resolve)(void *state, const Collision collision),
	void *state, Collision collision);

#endif
