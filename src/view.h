#ifndef VIEW_H
#define VIEW_H

#include "input.h"
#include "position.h"
#include "screen.h"

typedef struct {
	Position position;
	double scale;
} View;

void View_initialize();
void View_update(const Input *input, const unsigned int ticks);
void View_transform(const Screen *screen);
Position View_get_world_position(const Screen *screen, const Position uiPosition);
Position View_get_world_position_gl(const Screen *screen, const Position screenPosition);
const View View_get_view(void);
void View_set_position(const Position position);

#endif
