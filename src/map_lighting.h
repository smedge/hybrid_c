#ifndef MAP_LIGHTING_H
#define MAP_LIGHTING_H

#include <stdbool.h>

void MapLighting_initialize(void);
void MapLighting_cleanup(void);
void MapLighting_render(int draw_w, int draw_h);
void MapLighting_set_enabled(bool enabled);
bool MapLighting_get_enabled(void);

#endif
