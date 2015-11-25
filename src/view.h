#ifndef VIEW_H
#define VIEW_H

#include "input.h"
#include "position.h"
#include "screen.h"

typedef struct {
	Position position;
	double scale;
} View;

void view_initialize();
void view_update(const Input *input, const unsigned int ticks);
void view_transform(const Screen *screen);
const View view_get_view(void);
void view_set_position(Position position);

#endif