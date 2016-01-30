#include "world.h"
#include "entity.h"
#include <SDL2/SDL_opengl.h>

void World_initialize(void)
{
	int id = Entity_create_entity(COMPONENT_PLACEABLE | 
									COMPONENT_RENDERABLE);

	RenderableComponent renderable;
	renderable.render = World_render;
	Entity_add_renderable(id, renderable);
}

void World_cleanup(void)
{

}

void World_collide(void)
{

}

void World_resolve(void)
{

}

void World_render()
{
	View view = View_get_view();

	glPushMatrix();
	
	glLineWidth(8.0 * view.scale);
	
	glColor4f(0.02, 0.0, 0.02, 1.0);
	glBegin(GL_QUADS);
		glVertex2f(1000.0, 1500.0);
		glVertex2f(1500.0, 1500.0);
		glVertex2f(1500.0, -1500.0);
		glVertex2f(1000.0, -1500.0);
	glEnd();

	glColor4f(0.5, 0.0, 0.5, 1.0);
	glBegin(GL_LINE_LOOP);
		glVertex2f(1000.0, 1500.0);
		glVertex2f(1500.0, 1500.0);
		glVertex2f(1500.0, -1500.0);
		glVertex2f(1000.0, -1500.0);
	glEnd();

	glColor4f(0.02, 0.0, 0.02, 1.0);
	glBegin(GL_QUADS);
		glVertex2f(-1000.0, 1500.0);
		glVertex2f(-1500.0, 1500.0);
		glVertex2f(-1500.0, -1500.0);
		glVertex2f(-1000.0, -1500.0);
	glEnd();

	glColor4f(0.5, 0.0, 0.5, 1.0);
	glBegin(GL_LINE_LOOP);
		glVertex2f(-1000.0, 1500.0);
		glVertex2f(-1500.0, 1500.0);
		glVertex2f(-1500.0, -1500.0);
		glVertex2f(-1000.0, -1500.0);
	glEnd();

	glColor4f(0.02, 0.0, 0.02, 1.0);
	glBegin(GL_QUADS);
		glVertex2f(200.0, 200.0);
		glVertex2f(200.0, 300.0);
		glVertex2f(300.0, 300.0);
		glVertex2f(300.0, 200.0);
	glEnd();

	glColor4f(0.5, 0.0, 0.5, 1.0);
	glBegin(GL_LINE_LOOP);
		glVertex2f(200.0, 200.0);
		glVertex2f(200.0, 300.0);
		glVertex2f(300.0, 300.0);
		glVertex2f(300.0, 200.0);
	glEnd();

	glColor4f(0.02, 0.0, 0.02, 1.0);
	glBegin(GL_QUADS);
		glVertex2f(-200.0, 200.0);
		glVertex2f(-200.0, 300.0);
		glVertex2f(-300.0, 300.0);
		glVertex2f(-300.0, 200.0);
	glEnd();

	glColor4f(0.5, 0.0, 0.5, 1.0);
	glBegin(GL_LINE_LOOP);
		glVertex2f(-200.0, 200.0);
		glVertex2f(-200.0, 300.0);
		glVertex2f(-300.0, 300.0);
		glVertex2f(-300.0, 200.0);
	glEnd();

	glColor4f(0.02, 0.0, 0.02, 1.0);
	glBegin(GL_QUADS);
		glVertex2f(-200.0, -200.0);
		glVertex2f(-200.0, -300.0);
		glVertex2f(-300.0, -300.0);
		glVertex2f(-300.0, -200.0);
	glEnd();

	glColor4f(0.5, 0.0, 0.5, 1.0);
	glBegin(GL_LINE_LOOP);
		glVertex2f(-200.0, -200.0);
		glVertex2f(-200.0, -300.0);
		glVertex2f(-300.0, -300.0);
		glVertex2f(-300.0, -200.0);
	glEnd();

	glColor4f(0.02, 0.0, 0.02, 1.0);
	glBegin(GL_QUADS);
		glVertex2f(200.0, -200.0);
		glVertex2f(200.0, -300.0);
		glVertex2f(300.0, -300.0);
		glVertex2f(300.0, -200.0);
	glEnd();

	glColor4f(0.5, 0.0, 0.5, 1.0);
	glBegin(GL_LINE_LOOP);
		glVertex2f(200.0, -200.0);
		glVertex2f(200.0, -300.0);
		glVertex2f(300.0, -300.0);
		glVertex2f(300.0, -200.0);
	glEnd();

	glPopMatrix();
}