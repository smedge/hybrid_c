#include "view.h"

static View view = {{0.0, 0.0}, 1.0};

void view_update(const Input *input, const unsigned int ticks)
{
	
}

void view_transform(const Screen *screen)
{
	
}

const View view_get_view(void) {
	return view;
}