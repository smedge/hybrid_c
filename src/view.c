#include "view.h"

#include <SDL2/SDL_opengl.h>

static View view = {{100.0, 100.0}, 1.0};

void view_update(const Input *input, const unsigned int ticks)
{
	
}

void view_transform(const Screen *screen)
{
	glTranslatef(screen->width / 2.0 - view.position.x, 
		screen->height / 2.0 - view.position.y, 0.0);
}

const View view_get_view(void) {
	return view;
}