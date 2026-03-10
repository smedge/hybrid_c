#include "boss_hud.h"
#include "render.h"
#include "graphics.h"
#include "text.h"
#include "shader.h"
#include "mat4.h"

#define BAR_WIDTH 600.0f
#define BAR_HEIGHT 16.0f
#define BAR_TOP_MARGIN 40.0f
#define BAR_BORDER 2.0f

/* Phase threshold notch positions (fraction of max HP) */
#define PHASE_2_THRESHOLD 0.65f
#define PHASE_3_THRESHOLD 0.25f

static bool active;
static double currentHP;
static double maxHP;

void BossHUD_set_active(bool a)
{
	active = a;
}

void BossHUD_set_health(double current, double max)
{
	currentHP = current;
	maxHP = max;
}

void BossHUD_render(void)
{
	if (!active || maxHP <= 0.0)
		return;

	Screen screen = Graphics_get_screen();
	Mat4 uiProj = Graphics_get_ui_projection();
	Mat4 identity = Mat4_identity();

	float cx = screen.width * 0.5f;
	float y = screen.height - BAR_TOP_MARGIN;

	float left = cx - BAR_WIDTH * 0.5f;
	float right = cx + BAR_WIDTH * 0.5f;
	float top = y;
	float bottom = y - BAR_HEIGHT;

	/* Background (dark) */
	Render_quad_absolute(left - BAR_BORDER, top + BAR_BORDER,
	                     right + BAR_BORDER, bottom - BAR_BORDER,
	                     0.1f, 0.1f, 0.1f, 0.9f);

	/* Fill (fire gradient — dark red to orange based on HP fraction) */
	float frac = (float)(currentHP / maxHP);
	if (frac < 0.0f) frac = 0.0f;
	if (frac > 1.0f) frac = 1.0f;

	float fill_right = left + (right - left) * frac;
	Render_quad_absolute(left, top, fill_right, bottom,
	                     0.8f, 0.2f + frac * 0.3f, 0.05f, 0.9f);

	/* Phase threshold notches */
	float notch_w = 1.5f;
	float p2x = left + (right - left) * PHASE_2_THRESHOLD;
	float p3x = left + (right - left) * PHASE_3_THRESHOLD;

	Render_quad_absolute(p2x - notch_w, top + 2.0f, p2x + notch_w, bottom - 2.0f,
	                     1.0f, 1.0f, 1.0f, 0.6f);
	Render_quad_absolute(p3x - notch_w, top + 2.0f, p3x + notch_w, bottom - 2.0f,
	                     1.0f, 1.0f, 1.0f, 0.6f);

	/* Border (thin outline) */
	Render_line_segment(left - BAR_BORDER, top + BAR_BORDER,
	                    right + BAR_BORDER, top + BAR_BORDER,
	                    0.6f, 0.3f, 0.1f, 0.8f);
	Render_line_segment(left - BAR_BORDER, bottom - BAR_BORDER,
	                    right + BAR_BORDER, bottom - BAR_BORDER,
	                    0.6f, 0.3f, 0.1f, 0.8f);
	Render_line_segment(left - BAR_BORDER, top + BAR_BORDER,
	                    left - BAR_BORDER, bottom - BAR_BORDER,
	                    0.6f, 0.3f, 0.1f, 0.8f);
	Render_line_segment(right + BAR_BORDER, top + BAR_BORDER,
	                    right + BAR_BORDER, bottom - BAR_BORDER,
	                    0.6f, 0.3f, 0.1f, 0.8f);

	Render_flush(&uiProj, &identity);

	/* Boss name (rendered after flush, uses text shader) */
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Text_render(tr, shaders, &uiProj, &identity,
	            "PYRAXIS", cx - 28.0f, top + 16.0f,
	            0.9f, 0.5f, 0.2f, 0.9f);
}
