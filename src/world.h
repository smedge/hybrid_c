#ifndef WORLD_H
#define WORLD_H

#include "view.h"

void World_collide();
void World_resolve();
void World_render(const View *view);

#endif