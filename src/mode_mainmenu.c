#include "mode_mainmenu.h"

static Rectangle newBounds = {98.0, 190.0, 157.0, 202.0};
//static Rectangle loadBounds = {98.0, 228.0, 157.0, 242.0};
static Rectangle exitBounds = {98.0, 228.0, 157.0, 242.0};

static bool hoverNew = false;
static bool activeNew = false;

// static bool hoverLoad = false;
// static bool activeLoad = false;

static bool hoverExit = false;
static bool activeExit = false;

static void update_new_button(const Input *input, Mode *mode);
static void update_exit_button(const Input *input, bool *quit);
static void render_menu_text();

void mode_mainmenu_update(const Input *input, 
	const unsigned int ticks, bool *quit, Mode *mode)
{
	update_new_button(input, mode);
	update_exit_button(input, quit);
	cursor_update(input);
}

void mode_mainmenu_render()
{
	graphics_clear();
	graphics_set_ui_projection();

	render_menu_text();
	cursor_render();

	graphics_flip();
}

static void update_new_button(const Input *input, Mode *mode)
{
	if (collision_point_test(input->mouseX, input->mouseY, newBounds)) {
		hoverNew = true;

		// begin newing on mouse down
		if (input->mouseLeft) {
			activeNew = true;
		}

		// new on mouse up
		if (activeNew && !input->mouseLeft) {
			activeNew = false;
			*mode = GAMEPLAY;
		}
	}
	else {
		hoverNew = false;
		activeNew = false;
	}
}

static void update_exit_button(const Input *input, bool *quit) 
{
	if (collision_point_test(input->mouseX, input->mouseY, exitBounds)) {
		hoverExit = true;

		// begin quitting on mouse down
		if (input->mouseLeft) {
			activeExit = true;
		}

		// quit on mouse up
		if (activeExit && !input->mouseLeft) {
			activeExit = false;
			*quit = true;
		}
	}
	else {
		hoverExit = false;
		activeExit = false;
	}
}

static void render_menu_text() 
{
	glPushMatrix();

	FTGLfont *font = ftglCreatePixmapFont(SQUARE_FONT_PATH);

	// TODO
	// if(!font)
	//     exit();

	ftglSetFontFaceSize(font, 80, 80);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glRasterPos2f(100.0f, 100.0f);
	ftglRenderFont(font, "HYBRID", FTGL_RENDER_ALL);

	ftglSetFontFaceSize(font, 20, 20);;
	
	if (activeNew)
		glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
	else if (hoverNew)
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	else
		glColor4f(1.0f, 1.0f, 1.0f, 0.50f);
	glRasterPos2f(100.0f, 200.0f);
	ftglRenderFont(font, "NEW", FTGL_RENDER_ALL);

	glColor4f(1.0f, 1.0f, 1.0f, 0.50f);
	glRasterPos2f(100.0f, 220.0f);
	ftglRenderFont(font, "LOAD", FTGL_RENDER_ALL);

	if (activeExit)
		glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
	else if (hoverExit)
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	else
		glColor4f(1.0f, 1.0f, 1.0f, 0.50f);
	glRasterPos2f(100.0f, 240.0f);
	ftglRenderFont(font, "EXIT", FTGL_RENDER_ALL);
	
	ftglDestroyFont(font);

	glPopMatrix();
}