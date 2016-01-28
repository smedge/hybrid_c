#include "world.h"
#include <SDL2/SDL_opengl.h>

void World_initialize(void)
{

}

void World_cleanup(void)
{

}

void World_collide(void)
{

}

void World_resolve(void)
{

}

void World_render(const View *view)
{
	glPushMatrix();
	
	glLineWidth(8.0 * view->scale);
	
	glColor4f(0.01, 0.0, 0.01, 1.0);
	glBegin(GL_QUADS);
		glVertex2f(1000.0, 1500.0);
		glVertex2f(1500.0, 1500.0);
		glVertex2f(1500.0, -1500.0);
		glVertex2f(1000.0, -1500.0);
	glEnd();

	glColor4f(0.5, 0.0, 0.5, 1.0);
	glBegin(GL_LINE_LOOP);
		glVertex2f(1000.0, 1500.0);
		glVertex2f(1500.0, 1500.0);
		glVertex2f(1500.0, -1500.0);
		glVertex2f(1000.0, -1500.0);
	glEnd();

	glColor4f(0.01, 0.0, 0.01, 1.0);
	glBegin(GL_QUADS);
		glVertex2f(-1000.0, 1500.0);
		glVertex2f(-1500.0, 1500.0);
		glVertex2f(-1500.0, -1500.0);
		glVertex2f(-1000.0, -1500.0);
	glEnd();

	glColor4f(0.5, 0.0, 0.5, 1.0);
	glBegin(GL_LINE_LOOP);
		glVertex2f(-1000.0, 1500.0);
		glVertex2f(-1500.0, 1500.0);
		glVertex2f(-1500.0, -1500.0);
		glVertex2f(-1000.0, -1500.0);
	glEnd();

	glPopMatrix();
}