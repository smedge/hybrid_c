#include "render.h"

#include <SDL2/SDL_opengl.h>

extern void Render_point()
{

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
		glVertex3f(0.0, 20.0, 0.0);
		glVertex3f(10.0, -10.0, 0.0);
		glVertex3f(-10.0, -10.0, 0.0);
	glEnd();
	glPopMatrix();
}