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
const View View_get_view(void);
void View_set_position(Position position);

#endif