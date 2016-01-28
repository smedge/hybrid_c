#ifndef WORLD_H
#define WORLD_H

#include "view.h"

void World_initialize();
void World_collide();
void World_resolve();
void World_render(const View *view);
void World_cleanup();


#endif