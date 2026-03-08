#include "hud.h"
#include "render.h"
#include "text.h"
#include "graphics.h"
#include "mat4.h"
#include "map.h"
#include "ship.h"
#include "skillbar.h"
#include "portal.h"
#include "savepoint.h"
#include "data_node.h"
#include "zone.h"

#define RADAR_SIZE 200.0f
#define RADAR_RANGE 15000.0f

static void render_radar(const Screen *screen);

float Hud_get_minimap_range(void)
{
	return RADAR_RANGE;
}

void Hud_initialize(void)
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
	Skillbar_render(screen);
}

static void render_radar(const Screen *screen)
{
	float s = Graphics_get_ui_scale();
	float radar_size = RADAR_SIZE * s;
	float margin = 10.0f * s;
	float rx = (float)screen->width - radar_size - margin;
	float ry = (float)screen->height - radar_size - margin;

	/* Background */
	Render_quad_absolute(rx, ry, rx + radar_size, ry + radar_size,
		0.1f, 0.1f, 0.1f, 0.8f);

	/* Map cells — same RADAR_RANGE so same terrain coverage at any scale */
	Position ship_pos = Ship_get_position();
	Map_render_minimap((float)ship_pos.x, (float)ship_pos.y,
		rx, ry, radar_size, RADAR_RANGE);

	/* Portal blips */
	Portal_render_minimap((float)ship_pos.x, (float)ship_pos.y,
		rx, ry, radar_size, RADAR_RANGE);

	/* Save point blips */
	Savepoint_render_minimap((float)ship_pos.x, (float)ship_pos.y,
		rx, ry, radar_size, RADAR_RANGE);

	/* Data node blips */
	DataNode_render_minimap((float)ship_pos.x, (float)ship_pos.y,
		rx, ry, radar_size, RADAR_RANGE);

	/* Player blip (center) */
	float cx = rx + radar_size * 0.5f;
	float cy = ry + radar_size * 0.5f;
	float blip = 2.0f * s;
	Render_quad_absolute(cx - blip, cy - blip, cx + blip, cy + blip,
		1.0f, 0.3f, 0.3f, 1.0f);

	/* Zone name — right-justified above minimap */
	{
		const Zone *z = Zone_get();
		if (z && z->name[0]) {
			TextRenderer *tr = Graphics_get_text_renderer();
			Shaders *shaders = Graphics_get_shaders();
			Mat4 proj = Graphics_get_ui_projection();
			Mat4 ident = Mat4_identity();

			float right_edge = rx + radar_size;
			float tw = Text_measure_width(tr, z->name);
			float tx = right_edge - tw;
			float ty = ry - 4.0f * s;

			/* Flush geometry before text so quads don't cover it */
			Render_flush(&proj, &ident);

			Text_render(tr, shaders, &proj, &ident,
				z->name, tx, ty,
				0.5f, 0.5f, 0.5f, 0.8f);
		}
	}

	/* Border */
	float brc = 0.3f;
	float lw = 1.0f * s;
	Render_thick_line(rx, ry, rx + radar_size, ry,
		lw, brc, brc, brc, 0.8f);
	Render_thick_line(rx, ry + radar_size, rx + radar_size, ry + radar_size,
		lw, brc, brc, brc, 0.8f);
	Render_thick_line(rx, ry, rx, ry + radar_size,
		lw, brc, brc, brc, 0.8f);
	Render_thick_line(rx + radar_size, ry, rx + radar_size, ry + radar_size,
		lw, brc, brc, brc, 0.8f);
}
