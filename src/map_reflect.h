#ifndef MAP_REFLECT_H
#define MAP_REFLECT_H

#include "mat4.h"

void MapReflect_initialize(void);
void MapReflect_cleanup(void);
void MapReflect_render(const Mat4 *world_proj, const Mat4 *view,
	int draw_w, int draw_h);

#endif
