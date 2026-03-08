#ifndef COMPONENT_H
#define COMPONENT_H

#include "render_pass.h"

#define COLLISION_LAYER_TERRAIN  0x01
#define COLLISION_LAYER_PLAYER   0x02
#define COLLISION_LAYER_ENEMY    0x04

typedef struct {
	Position position;
	double heading;
} PlaceableComponent;

typedef void (*RenderFunc)(const void *state, const PlaceableComponent *placeable);

typedef struct {
	RenderFunc passes[RENDER_PASS_COUNT];
} RenderableComponent;

typedef struct {
	Rectangle boundingBox;
	bool collidesWithOthers;
	unsigned int layer;
	unsigned int mask;
	Collision (*collide)(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
	void (*resolve)(void *state, const Collision collision);
} CollidableComponent;

typedef struct {
	double mass;
} DynamicsComponent;

typedef struct {
	void (*update)(const Input *input, const unsigned int ticks, PlaceableComponent *placeable);
} UserUpdatableComponent;

typedef struct {
	void (*update)(void *state, const PlaceableComponent *placeable, const unsigned int ticks);
} AIUpdatableComponent;

#endif
