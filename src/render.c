#include "render.h"

#include <SDL2/SDL_opengl.h>

void Render_point(const Position *position, const float size, 
	const ColorFloat *color)
{
	glPushMatrix();
	glPointSize(size);
	glColor4f(color->red, color->green, color->blue, color->alpha);
	glTranslatef(position->x, position->y, 0.0);
	glBegin(GL_POINTS);
		glVertex2f(0.0, 0.0);
	glEnd();
	glPopMatrix();
}

void Render_line()
{
	
}

void Render_triangle(const Position *position, const double heading, 
	const ColorFloat *color) 
{
	glPushMatrix();
	glTranslatef(position->x, position->y, 0.0);
	glRotatef(heading*-1, 0, 0, 1);
	glBegin(GL_TRIANGLES);
	    glColor4f(color->red, color->green, color->blue, color->alpha);
		glVertex2f(0.0, 20.0);
		glVertex2f(10.0, -10.0);
		glVertex2f(-10.0, -10.0);
	glEnd();
	glPopMatrix();
}

void Render_quad(const Position *position, const double rotation, 
	const Rectangle rectangle, const ColorFloat *color)
{
	glPushMatrix();
	glColor4f(color->red, color->green, color->blue, color->alpha);
	glTranslatef(position->x, position->y, 0.0);
	glRotatef(rotation*-1, 0, 0, 1);
	glBegin(GL_QUADS);
		glVertex2f(rectangle.aX, rectangle.aY);
		glVertex2f(rectangle.aX, rectangle.bY);
		glVertex2f(rectangle.bX, rectangle.bY);
		glVertex2f(rectangle.bX, rectangle.aY);
	glEnd();
	glPopMatrix();
}

void Render_convex_poly()
{
	
}

void Render_bounding_box(const Position *position, 
		const Rectangle *boundingBox) 
{
	glPushMatrix();
	glTranslatef(position->x, position->y, 0.0);

	glLineWidth(1.0);
	glColor4f(1.0, 1.0, 0.0, 0.60);
	glBegin(GL_LINE_LOOP);
	glVertex2f(boundingBox->aX, boundingBox->aY);
	glVertex2f(boundingBox->aX, boundingBox->bY);
	glVertex2f(boundingBox->bX, boundingBox->bY);
	glVertex2f(boundingBox->bX, boundingBox->aY);
	glEnd();

	glPopMatrix();
}