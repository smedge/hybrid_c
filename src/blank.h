#ifndef BLANK_H
#define BLANK_H

#include "entity.h"

void Blank_initialize();
void Blank_cleanup();

Collision Mine_collide(const Rectangle boundingBox);
void Blank_resolve(Collision collision);
void Blank_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable);
void Blank_render(const PlaceableComponent *placeable);

#endif