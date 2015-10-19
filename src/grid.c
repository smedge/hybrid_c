#include "grid.h"

void grid_render(const Screen *screen)
{
	glPushMatrix();
	glTranslatef(0.0, 0.0, 0.0);
	glLineWidth(2.0);
	glColor4f(0.0, 1.0, 0.0, 0.75);
	glBegin(GL_LINES);
		glVertex2f(0.0, 0.0);
		glVertex2f(400.0, 0.0);
	glEnd();
	glPopMatrix();
}