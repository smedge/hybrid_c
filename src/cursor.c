#include "cursor.h"

static int x = 0;
static int y = 0;
static bool visible = false;

void cursor_update(const Input *input) 
{
	visible = input->showMouse;
	x = input->mouseX;
	y = input->mouseY;
}

void cursor_render() 
{
	if (visible) {
		glPushMatrix();
		glPointSize(2.0);
		glColor4f(0.9, 0.0, 0.0, 0.70);
		glTranslatef(x, y, 0.0);
		glBegin(GL_POINTS);
			glVertex2f(0.0, 0.0);
		glEnd();
		glLineWidth(2.0);
		glColor4f(0.9, 0.9, 0.9, 0.50);
		glBegin(GL_LINES);
			glVertex2f(0.0, 7.0);
			glVertex2f(0.0, 3.0);

			glVertex2f(7.0, 0.0);
			glVertex2f(3.0, 0.0);

			glVertex2f(0.0, -3.0);
			glVertex2f(0.0, -7.0);

			glVertex2f(-7.0, 0.0);
			glVertex2f(-3.0, 0.0);
		glEnd();
		glPopMatrix();
	}
}