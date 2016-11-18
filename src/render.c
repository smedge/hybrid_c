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

extern void Render_quad()
{
	// ColorFloat primaryColor = Color_rgb_to_float(&mapCell.primaryColor);
	// glColor4f(primaryColor.red, primaryColor.green, primaryColor.blue, primaryColor.alpha);
	// glBegin(GL_QUADS);
	// 	glVertex2f(ax, ay);
	// 	glVertex2f(ax, by);
	// 	glVertex2f(bx, by);
	// 	glVertex2f(bx, ay);
	// glEnd();
}

extern void Render_convex_poly()
{
	
}

extern void Render_bounding_box(/*const PlaceableComponent *placeable*/) {
	// glPushMatrix();
	// glTranslatef(placeable->position.x, placeable->position.y, 0.0);

	// glLineWidth(1.0);
	// glColor4f(1.0, 1.0, 0.0, 1.0);
	// glBegin(GL_LINE_LOOP);
	// glVertex2f(collidable.boundingBox.aX, collidable.boundingBox.aY);
	// glVertex2f(collidable.boundingBox.aX, collidable.boundingBox.bY);
	// glVertex2f(collidable.boundingBox.bX, collidable.boundingBox.bY);
	// glVertex2f(collidable.boundingBox.bX, collidable.boundingBox.aY);
	// glEnd();

	// glPopMatrix();
}