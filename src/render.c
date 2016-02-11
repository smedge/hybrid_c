#include "render.h"

#include <SDL2/SDL_opengl.h>

extern void Render_point(const Position *position, const double colorR, 
	const double colorG, const double colorB, const double colorA)
{
	glPushMatrix();
	glPointSize(2.0);
	glColor4f(colorR, colorG, colorB, colorA);
	glTranslatef(position->x, position->y, 0.0);
	glBegin(GL_POINTS);
		glVertex2f(0.0, 0.0);
	glEnd();
	glPopMatrix();
}

extern void Render_line()
{
	
}

void Render_triangle(const Position *position, const double heading, 
	const double colorR, const double colorG, const double colorB, 
	const double colorA) 
{
	glPushMatrix();
	glTranslatef(position->x, position->y, 0.0);
	glRotatef(heading*-1, 0, 0, 1);
	glBegin(GL_TRIANGLES);
	    glColor4f(colorR, colorG, colorB, colorA);
		glVertex2f(0.0, 20.0);
		glVertex2f(10.0, -10.0);
		glVertex2f(-10.0, -10.0);
	glEnd();
	glPopMatrix();
}