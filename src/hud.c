#include "hud.h"
#include "render.h"
#include "text.h"
#include "graphics.h"

static void render_skill_bar(const Screen *screen);
static void render_radar(const Screen *screen);

void Hud_initialize()
{
}

void Hud_cleanup(void)
{
}

void Hud_update(const Input *input, const unsigned int ticks)
{
	(void)input;
	(void)ticks;
}

void Hud_render(const Screen *screen)
{
	render_radar(screen);
	render_skill_bar(screen);
}

static void render_skill_bar(const Screen *screen)
{
	float base_x = 10.0f;
	float base_y = (float)screen->height - 60.0f;

	/* 10 skill buttons */
	for (int i = 0; i < 10; i++) {
		float bx = base_x + i * 60.0f;
		Render_quad_absolute(bx, base_y, bx + 50.0f, base_y + 50.0f,
			0.1f, 0.1f, 0.1f, 0.8f);
	}

	/* Number labels */
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	const char *labels[] = {"1","2","3","4","5","6","7","8","9","0"};
	for (int i = 0; i < 10; i++) {
		float lx = base_x + i * 60.0f;
		float ly = (float)screen->height - 10.0f;
		Text_render(tr, shaders, &proj, &ident,
			labels[i], lx, ly,
			1.0f, 1.0f, 1.0f, 1.0f);
	}
}

static void render_radar(const Screen *screen)
{
	float rx = (float)screen->width - 210.0f;
	float ry = (float)screen->height - 210.0f;
	Render_quad_absolute(rx, ry, rx + 200.0f, ry + 200.0f,
		0.1f, 0.1f, 0.1f, 0.8f);
}
