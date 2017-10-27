#include "hud.h"

#include <FTGL/ftgl.h>

static FTGLfont *font = 0;

//static void render_left_status(const Screen *screen);
//static void render_right_status(const Screen *screen);
static void render_skill_bar(const Screen *screen);
static void render_blank_skill_button();
static void render_radar(const Screen *screen);

void Hud_initialize()
{
	font = ftglCreatePixmapFont(TITLE_FONT_PATH);
	if(!font) {
		puts("error: failed to load font in hud");
		exit(-1);
	}
}

void Hud_cleanup(void)
{
	ftglDestroyFont(font);
}

void Hud_update(const Input *input, const unsigned int ticks)
{

}

void Hud_render(const Screen *screen)
{
	glPushMatrix();
	//render_left_status(screen);
	//render_right_status(screen);
	render_radar(screen);
	render_skill_bar(screen);
	glPopMatrix();
}

static void render_left_status(const Screen *screen)
{
	glPushMatrix();
	glLineWidth(1.0);
	glColor4f(0.1, 0.1, 0.1, 0.8);
	glTranslatef(10.0, 10.0, 0.0);

	glBegin(GL_LINE_LOOP);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 100.0);
		glVertex2f(250.0, 100.0);
		glVertex2f(250.0, 0.0);
	glEnd();
	glPopMatrix();
}

static void render_right_status(const Screen *screen)
{
	glPushMatrix();
	glLineWidth(1.0);
	glColor4f(0.1, 0.1, 0.1, 0.8);
	glTranslatef(screen->width - 260.0, 10.0, 0.0);

	glBegin(GL_LINE_LOOP);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 100.0);
		glVertex2f(250.0, 100.0);
		glVertex2f(250.0, 0.0);
	glEnd();
	glPopMatrix();
}

static void render_skill_bar(const Screen *screen)
{

	glColor4f(0.1, 0.1, 0.1, 0.8);
	
	glPushMatrix();

	glTranslatef(10.0, screen->height - 60.0, 0.0);
	render_blank_skill_button();

	glTranslatef(60.0, 0.0, 0.0);
	render_blank_skill_button();

	glTranslatef(60.0, 0.0, 0.0);
	render_blank_skill_button();

	glTranslatef(60.0, 0.0, 0.0);
	render_blank_skill_button();

	glTranslatef(60.0, 0.0, 0.0);
	render_blank_skill_button();

	glTranslatef(60.0, 0.0, 0.0);
	render_blank_skill_button();

	glTranslatef(60.0, 0.0, 0.0);
	render_blank_skill_button();

	glTranslatef(60.0, 0.0, 0.0);
	render_blank_skill_button();

	glTranslatef(60.0, 0.0, 0.0);
	render_blank_skill_button();

	glTranslatef(60.0, 0.0, 0.0);
	render_blank_skill_button();

	glPopMatrix();



	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	ftglSetFontFaceSize(font, 14, 72);
	
	glRasterPos2f(10.0f, screen->height - 10.0);
	ftglRenderFont(font, "1", FTGL_RENDER_ALL);

	glRasterPos2f(70.0f, screen->height - 10.0);
	ftglRenderFont(font, "2", FTGL_RENDER_ALL);

	glRasterPos2f(130.0f, screen->height - 10.0);
	ftglRenderFont(font, "3", FTGL_RENDER_ALL);

	glRasterPos2f(190.0f, screen->height - 10.0);
	ftglRenderFont(font, "4", FTGL_RENDER_ALL);

	glRasterPos2f(250.0f, screen->height - 10.0);
	ftglRenderFont(font, "5", FTGL_RENDER_ALL);

	glRasterPos2f(310.0f, screen->height - 10.0);
	ftglRenderFont(font, "6", FTGL_RENDER_ALL);

	glRasterPos2f(370.0f, screen->height - 10.0);
	ftglRenderFont(font, "7", FTGL_RENDER_ALL);

	glRasterPos2f(430.0f, screen->height - 10.0);
	ftglRenderFont(font, "8", FTGL_RENDER_ALL);

	glRasterPos2f(490.0f, screen->height - 10.0);
	ftglRenderFont(font, "9", FTGL_RENDER_ALL);

	glRasterPos2f(550.0f, screen->height - 10.0);
	ftglRenderFont(font, "10", FTGL_RENDER_ALL);
}

static void render_blank_skill_button()
{
	glBegin(GL_QUADS);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 50.0);
		glVertex2f(50.0, 50.0);
		glVertex2f(50.0, 0.0);
	glEnd();
}

static void render_radar(const Screen *screen)
{
	glPushMatrix();
	glColor4f(0.1, 0.1, 0.1, 0.8);
	glTranslatef(screen->width - 210.0, screen->height - 210.0, 0.0);

	glBegin(GL_QUADS);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 200.0);
		glVertex2f(200.0, 200.0);
		glVertex2f(200.0, 0.0);
	glEnd();
	glPopMatrix();
}