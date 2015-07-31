#include "mode_mainmenu.h"

static ButtonState newButton = {{98.0, 190.0, 157.0, 202.0}, false, false};
static ButtonState exitButton = {{98.0, 228.0, 157.0, 242.0}, false, false};

static FTGLfont *font = 0;

static void render_menu_text();

void mode_mainmenu_initialize()
{
	font = ftglCreatePixmapFont(SQUARE_FONT_PATH);
}

void mode_mainmenu_cleanup()
{
	ftglDestroyFont(font);
}

void mode_mainmenu_update(const Input *input, 
	const unsigned int ticks, void (*quit)(), void (*mode)())
{
	update_button(input, &newButton, mode);
	update_button(input, &exitButton, quit);
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

static void render_menu_text() 
{
	glPushMatrix();

	// TODO
	// if(!font)
	//     error;

	// TODO callback work
	mode_mainmenu_initialize();

	ftglSetFontFaceSize(font, 80, 80);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glRasterPos2f(100.0f, 100.0f);
	ftglRenderFont(font, "HYBRID", FTGL_RENDER_ALL);

	ftglSetFontFaceSize(font, 20, 20);;
	
	if (newButton.active)
		glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
	else if (newButton.hover)
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	else
		glColor4f(1.0f, 1.0f, 1.0f, 0.50f);
	glRasterPos2f(100.0f, 200.0f);
	ftglRenderFont(font, "NEW", FTGL_RENDER_ALL);

	glColor4f(1.0f, 1.0f, 1.0f, 0.50f);
	glRasterPos2f(100.0f, 220.0f);
	ftglRenderFont(font, "LOAD", FTGL_RENDER_ALL);

	if (exitButton.active)
		glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
	else if (exitButton.hover)
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	else
		glColor4f(1.0f, 1.0f, 1.0f, 0.50f);
	glRasterPos2f(100.0f, 240.0f);
	ftglRenderFont(font, "EXIT", FTGL_RENDER_ALL);

	//TODO callbacks
	mode_mainmenu_cleanup();

	glPopMatrix();
}