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
	void (*resolve)(const void *state, const Collision collision);
	void *state;
	Collision collision;
} ResolveCollisionCommand;

Entity Entity_initialize_entity();
Entity* Entity_add_entity(const Entity entity);
void Entity_destroy_all(void);
void Entity_destroy(const unsigned int entityId);
void Entity_add_state(const unsigned int entityId, void *state);
void Entity_add_placeable(const unsigned int entityId, PlaceableComponent *placeable);
void Entity_add_renderable(const unsigned int entityId, RenderableComponent *renderable);
void Entity_add_user_updatable(const unsigned int entityId, UserUpdatableComponent *updatable);
void Entity_add_ai_updatable(const unsigned int entityId, AIUpdatableComponent *updatable);
void Entity_add_collidable(const unsigned int entityId, CollidableComponent *collidable);

void Entity_user_update_system(const Input *input, const unsigned int ticks);
void Entity_ai_update_system(const unsigned int ticks);
void Entity_render_system(void);
void Entity_collision_system(void);
void Entity_create_collision_command(void (*resolve)(const void *state, const Collision collision),
	void *state, Collision collision);

#endif
