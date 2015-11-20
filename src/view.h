#ifndef VIEW_H
#define VIEW_H

#include "input.h"
#include "position.h"

typedef struct {
	Position position;
	double scale;
} View;

void view_update(const Input *input, const unsigned int ticks);
void view_transform(void);
const View view_get_view(void);

#endif