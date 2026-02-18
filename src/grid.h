#ifndef GRID_H
#define GRID_H

#include "graphics.h"
#include "view.h"
#include "entity.h"
#include "render.h"

void Grid_initialize(void);
void Grid_render(const void *state, const PlaceableComponent *placeable);
void Grid_cleanup(void);

#endif
