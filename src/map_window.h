#ifndef MAP_WINDOW_H
#define MAP_WINDOW_H

#include <stdbool.h>
#include "input.h"
#include "screen.h"

void MapWindow_initialize(void);
void MapWindow_cleanup(void);
void MapWindow_toggle(void);
bool MapWindow_is_open(void);
void MapWindow_update(const Input *input);
void MapWindow_render(const Screen *screen);

#endif
