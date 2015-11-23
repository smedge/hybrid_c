#include "view.h"

#include <SDL2/SDL_opengl.h>

const double MAX_ZOOM = 4.0;
const double MIN_ZOOM = 0.2;

static View view = {{0.0, 0.0}, 1.0};

void view_update(const Input *input, const unsigned int ticks)
{
	if (view.scale <= MAX_ZOOM && input->mouseWheelUp)
		view.scale *= 1.05;
	if (view.scale >= MIN_ZOOM && input->mouseWheelDown)
		view.scale *= 0.95;
}

void view_transform(const Screen *screen)
{
	double newX = (screen->width / 2.0) - (view.position.x * view.scale);
	double newY = (screen->height / 2.0) - (view.position.y * view.scale);
	glTranslatef(newX, newY, 0.0);
	glScalef(view.scale, view.scale, view.scale);
}

const View view_get_view(void) {
	return view;
}