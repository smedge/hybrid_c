#ifndef COMPONENT_H
#define COMPONENT_H

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
	Collision (*collide)(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
	void (*resolve)(const void *state, const Collision collision);
} CollidableComponent;

typedef struct {
	double mass;
} DynamicsComponent;

typedef struct {
	void (*update)(const Input *input, const unsigned int ticks, PlaceableComponent *placeable);
} UserUpdatableComponent;

typedef struct {
	void (*update)(const void *state, const PlaceableComponent *placeable, const unsigned int ticks);
} AIUpdatableComponent;

#endif
