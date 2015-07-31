#include "mode_mainmenu.h"

static ButtonState newButton = {{98.0, 190.0, 157.0, 202.0}, false, false};
static ButtonState loadButton = {{98.0, 208.0, 157.0, 222.0}, false, false};
static ButtonState exitButton = {{98.0, 228.0, 157.0, 242.0}, false, false};
static FTGLfont *font = 0;

static void render_menu_text();
static void render_menu_button(const ButtonState *buttonState, 
							   const double x, 
							   const double y, 
							   const char *text);

void mode_mainmenu_initialize()
{
	font = ftglCreatePixmapFont(SQUARE_FONT_PATH);
}

void mode_mainmenu_cleanup()
{
	ftglDestroyFont(font);
}

void mode_mainmenu_update(const Input *input, 
						  const unsigned int ticks, 
						  void (*quit)(), 
						  void (*mode)())
{
	imgui_update_button(input, &newButton, mode);
	imgui_update_button(input, &exitButton, quit);

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

	mode_mainmenu_initialize();

	ftglSetFontFaceSize(font, 80, 80);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glRasterPos2f(100.0f, 100.0f);
	ftglRenderFont(font, "HYBRID", FTGL_RENDER_ALL);

	render_menu_button(&newButton, 100.0, 200.0, "NEW");
	render_menu_button(&loadButton, 100.0, 220.0, "LOAD");
	render_menu_button(&exitButton, 100.0, 240.0, "EXIT");

	mode_mainmenu_cleanup();

	glPopMatrix();
}

static void render_menu_button(const ButtonState *buttonState, 
							   const double x, 
							   const double y, 
							   const char *text)
{
	ftglSetFontFaceSize(font, 20, 20);
	
	if (buttonState->active)
		glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
	else if (buttonState->hover)
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	else
		glColor4f(1.0f, 1.0f, 1.0f, 0.50f);
	glRasterPos2f(x, y);
	ftglRenderFont(font, text, FTGL_RENDER_ALL);
}