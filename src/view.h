#ifndef VIEW_H
#define VIEW_H

#include <stdbool.h>
#include "input.h"
#include "position.h"
#include "screen.h"
#include "mat4.h"

typedef struct {
	Position position;
	double scale;
} View;

void View_initialize();
void View_update(const Input *input, const unsigned int ticks);
Mat4 View_get_transform(const Screen *screen);
Position View_get_world_position(const Screen *screen, const Position uiPosition);
const View View_get_view(void);
void View_set_position(const Position position);
void View_set_scale(double scale);
void View_set_min_zoom(double minZoom);
void View_set_pixel_snapping(bool enabled);
bool View_get_pixel_snapping(void);

#endif
