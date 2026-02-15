#ifndef COMPONENT_H
#define COMPONENT_H

#define COLLISION_LAYER_TERRAIN  0x01
#define COLLISION_LAYER_PLAYER   0x02
#define COLLISION_LAYER_ENEMY    0x04

typedef struct {
	Position position;
	double heading;
} PlaceableComponent;

typedef struct {
	void (*render)(const void *state, const PlaceableComponent *placeable);
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
