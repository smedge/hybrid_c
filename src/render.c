#include "render.h"

void render_triangle(Position *position, double heading, 
	double colorR, double colorG, double colorB, double colorA) {

	glPushMatrix();
	glTranslatef(position->x, position->y, 0.0);
	glRotatef(heading*-1, 0, 0, 1);
	glScalef(5, 5, 1);
	glBegin(GL_TRIANGLES);
	    glColor4f(colorR, colorG, colorB, colorA);
		glVertex3f(0.0, 4.0, 0.0);
		glVertex3f(2.0, -2.0, 0.0);
		glVertex3f(-2.0, -2.0, 0.0);
	glEnd();
	glPopMatrix();
}