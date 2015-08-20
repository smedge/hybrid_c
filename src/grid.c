#include "grid.h"

void grid_render()
{
	Screen screen = graphics_get_screen();

	glPushMatrix();
	glColor4f(0.9, 0.0, 0.0, 0.70);
	glTranslatef(0.0, 0.0, 0.0);
	glLineWidth(1.0);
	glColor4f(0.0, 0.1, 0.0, 1.0);
	glBegin(GL_LINES);
		glVertex2f(0.0, 0.0);
		glVertex2f(100.0, 0.0);
	glEnd();
	glPopMatrix();
}