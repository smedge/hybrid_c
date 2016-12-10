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

extern void Render_triangle(const Position *position, const double heading, 
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

extern void Render_quad(const Position *position, const double rotation, 
	const Rectangle rectangle, const ColorRGB *color)
{
	glPushMatrix();
	ColorFloat primaryColor = Color_rgb_to_float(color);
	glColor4f(primaryColor.red, primaryColor.green, primaryColor.blue, primaryColor.alpha);
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

extern void Render_convex_poly()
{
	
}

extern void Render_bounding_box(const Position *position, 
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