#include "grid.h"

void grid_render(const Screen *screen, const View *view)
{
	glPushMatrix();
	glTranslatef(0.0, 0.0, 0.0);
	glLineWidth(2.0 * view->scale);
	glColor4f(0.0, 1.0, 0.0, 0.50);
	glBegin(GL_LINES);
		glVertex2f(0.0, screen->height/2.0);
		glVertex2f(screen->width, screen->height/2.0);
		glVertex2f(screen->width/2.0, 0.0);
		glVertex2f(screen->width/2.0, screen->height);
	glEnd();
	glPopMatrix();
}