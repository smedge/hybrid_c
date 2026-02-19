#ifndef MAP_REFLECT_H
#define MAP_REFLECT_H

#include <stdbool.h>
#include "mat4.h"

void MapReflect_initialize(void);
void MapReflect_cleanup(void);
void MapReflect_render(const Mat4 *world_proj, const Mat4 *view,
	int draw_w, int draw_h);
void MapReflect_set_enabled(bool enabled);
bool MapReflect_get_enabled(void);

#endif
