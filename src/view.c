#include "view.h"

#include <SDL2/SDL_opengl.h>

#include <GL/glu.h>

const double MAX_ZOOM = 4.0;
const double MIN_ZOOM = 0.01;
const double ZOOM_IN_RATE = 1.05;
const double ZOOM_OUT_RATE = 0.95;

static View view;

void View_initialize() 
{
	view.position.x = 0.0;
	view.position.y = 0.0;
	view.scale = 0.5;
}

void View_update(const Input *input, const unsigned int ticks)
{
	if (view.scale <= MAX_ZOOM && input->mouseWheelUp)
		view.scale *= ZOOM_IN_RATE;
	if (view.scale >= MIN_ZOOM && input->mouseWheelDown)
		view.scale *= ZOOM_OUT_RATE;

	if (view.scale > MAX_ZOOM)
		view.scale = MAX_ZOOM;
	else if (view.scale < MIN_ZOOM)
		view.scale = MIN_ZOOM;
}

void View_transform(const Screen *screen)
{
	double x = (screen->width / 2.0) - (view.position.x * view.scale);
	double y = (screen->height / 2.0) - (view.position.y * view.scale);
	glTranslatef(x, y, 0.0);
	glScalef(view.scale, view.scale, view.scale);
}

Position View_get_world_position(const Screen *screen, const Position uiPosition) 
{
	// This method is bubbed, use the gl version below

	// correct when ship doesnt move
	double x = (view.position.x + uiPosition.x - (screen->width / 2)) / view.scale;
	double y = (view.position.y - uiPosition.y + (screen->height / 2)) / view.scale;

	// correct when view isn't scaled
	//double x = (view.position.x + uiPosition.x - (screen->width / 2)) / view.scale - view.position.x;
	//double y = (view.position.y - uiPosition.y + (screen->height / 2)) / view.scale - view.position.y;

	Position position = {x, y};
	return position;
}

Position View_get_world_position_gl(const Screen *screen, const Position screenPosition) {

	View_transform(screen);

	GLdouble worldX, worldY, worldZ;

	GLdouble projectionMatrix[16];
	glGetDoublev(GL_PROJECTION_MATRIX, projectionMatrix);
	GLdouble modelViewMatrix[16];
	glGetDoublev(GL_MODELVIEW_MATRIX, modelViewMatrix);
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	gluUnProject(screenPosition.x, screenPosition.y, 0,
		modelViewMatrix,
		projectionMatrix,
		viewport,
		&worldX, &worldY, &worldZ);

	glLoadIdentity();

	Position worldPosition;
	worldPosition.x = worldX;
	worldPosition.y = worldY;

	return worldPosition;
}


const View View_get_view(void) 
{
	return view;
}

void View_set_position(const Position position) 
{
	view.position = position;
}
