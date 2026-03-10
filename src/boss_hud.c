#include "boss_hud.h"
#include "render.h"
#include "graphics.h"
#include "text.h"
#include "shader.h"
#include "mat4.h"
#include "batch.h"

#define BAR_WIDTH 600.0f
#define BAR_HEIGHT 16.0f
#define BAR_TOP_MARGIN 10.0f
#define BAR_CHAMFER 4.0f
#define BORDER_COLOR 0.3f

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

static void render_chamfer_border(float x, float y, float w, float h)
{
	float c = BAR_CHAMFER;
	float vx[6] = { x,     x+w-c, x+w,
	                 x+w,   x+c,   x };
	float vy[6] = { y,     y,     y+c,
	                 y+h,   y+h,   y+h-c };
	for (int v = 0; v < 6; v++) {
		int nv = (v + 1) % 6;
		Render_thick_line(vx[v], vy[v], vx[nv], vy[nv],
			1.0f, BORDER_COLOR, BORDER_COLOR, BORDER_COLOR, 0.8f);
	}
}

static void render_chamfer_bg(float x, float y, float w, float h)
{
	float c = BAR_CHAMFER;
	float vx[6] = { x,     x+w-c, x+w,
	                 x+w,   x+c,   x };
	float vy[6] = { y,     y,     y+c,
	                 y+h,   y+h,   y+h-c };
	float cx = x + w * 0.5f;
	float cy = y + h * 0.5f;
	BatchRenderer *batch = Graphics_get_batch();
	for (int v = 0; v < 6; v++) {
		int nv = (v + 1) % 6;
		Batch_push_triangle_vertices(batch,
			cx, cy, vx[v], vy[v], vx[nv], vy[nv],
			0.1f, 0.1f, 0.1f, 0.8f);
	}
}

static void render_chamfer_fill(float bx, float y, float w, float h,
	float fill_frac, float r, float g, float b, float a)
{
	if (fill_frac <= 0.0f) return;
	float fw = w * fill_frac;
	if (fw > w) fw = w;
	float c = BAR_CHAMFER;

	float vx[6], vy[6];
	int count;

	if (fw >= w - 0.5f) {
		/* Full bar — standard 6-vertex chamfer */
		vx[0] = bx;     vy[0] = y;
		vx[1] = bx+w-c; vy[1] = y;
		vx[2] = bx+w;   vy[2] = y+c;
		vx[3] = bx+w;   vy[3] = y+h;
		vx[4] = bx+c;   vy[4] = y+h;
		vx[5] = bx;     vy[5] = y+h-c;
		count = 6;
	} else if (fw > c) {
		/* Partial — bottom-left chamfered, right edge straight */
		vx[0] = bx;     vy[0] = y;
		vx[1] = bx+fw;  vy[1] = y;
		vx[2] = bx+fw;  vy[2] = y+h;
		vx[3] = bx+c;   vy[3] = y+h;
		vx[4] = bx;     vy[4] = y+h-c;
		count = 5;
	} else {
		/* Tiny fill — plain rect */
		vx[0] = bx;     vy[0] = y;
		vx[1] = bx+fw;  vy[1] = y;
		vx[2] = bx+fw;  vy[2] = y+h;
		vx[3] = bx;     vy[3] = y+h;
		count = 4;
	}

	float cx = bx + fw * 0.5f;
	float cy = y + h * 0.5f;
	BatchRenderer *batch = Graphics_get_batch();
	for (int v = 0; v < count; v++) {
		int nv = (v + 1) % count;
		Batch_push_triangle_vertices(batch,
			cx, cy, vx[v], vy[v], vx[nv], vy[nv],
			r, g, b, a);
	}
}

void BossHUD_render(void)
{
	if (!active || maxHP <= 0.0)
		return;

	Screen screen = Graphics_get_screen();
	Mat4 uiProj = Graphics_get_ui_projection();
	Mat4 identity = Mat4_identity();

	float cx = screen.width * 0.5f;
	float left = cx - BAR_WIDTH * 0.5f;
	float y = BAR_TOP_MARGIN;

	/* HP fraction */
	float frac = (float)(currentHP / maxHP);
	if (frac < 0.0f) frac = 0.0f;
	if (frac > 1.0f) frac = 1.0f;

	/* Background, fill, border — same chamfer style as player bars */
	render_chamfer_bg(left, y, BAR_WIDTH, BAR_HEIGHT);
	render_chamfer_fill(left, y, BAR_WIDTH, BAR_HEIGHT,
		frac, 0.8f, 0.2f + frac * 0.3f, 0.05f, 0.9f);
	render_chamfer_border(left, y, BAR_WIDTH, BAR_HEIGHT);

	/* Phase threshold notches */
	float notch_w = 1.5f;
	float p2x = left + BAR_WIDTH * PHASE_2_THRESHOLD;
	float p3x = left + BAR_WIDTH * PHASE_3_THRESHOLD;

	Render_quad_absolute(p2x - notch_w, y - 2.0f, p2x + notch_w, y + BAR_HEIGHT + 2.0f,
	                     1.0f, 1.0f, 1.0f, 0.6f);
	Render_quad_absolute(p3x - notch_w, y - 2.0f, p3x + notch_w, y + BAR_HEIGHT + 2.0f,
	                     1.0f, 1.0f, 1.0f, 0.6f);

	Render_flush(&uiProj, &identity);

	/* Boss name label (below bar) */
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Text_render(tr, shaders, &uiProj, &identity,
	            "PYRAXIS", cx - 28.0f, y + BAR_HEIGHT + 20.0f,
	            0.9f, 0.5f, 0.2f, 0.9f);
}
