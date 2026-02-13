#include "hud.h"
#include "render.h"
#include "text.h"
#include "graphics.h"
#include "map.h"
#include "ship.h"
#include "skillbar.h"

#define RADAR_SIZE 200.0f
#define RADAR_RANGE 15000.0f

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
	Skillbar_render(screen);
}

static void render_radar(const Screen *screen)
{
	float rx = (float)screen->width - 210.0f;
	float ry = (float)screen->height - 210.0f;

	/* Background */
	Render_quad_absolute(rx, ry, rx + RADAR_SIZE, ry + RADAR_SIZE,
		0.1f, 0.1f, 0.1f, 0.8f);

	/* Map cells */
	Position ship_pos = Ship_get_position();
	Map_render_minimap((float)ship_pos.x, (float)ship_pos.y,
		rx, ry, RADAR_SIZE, RADAR_RANGE);

	/* Player blip (center) */
	float cx = rx + RADAR_SIZE * 0.5f;
	float cy = ry + RADAR_SIZE * 0.5f;
	Render_quad_absolute(cx - 2.0f, cy - 2.0f, cx + 2.0f, cy + 2.0f,
		1.0f, 0.3f, 0.3f, 1.0f);

	/* Border */
	float brc = 0.3f;
	Render_thick_line(rx, ry, rx + RADAR_SIZE, ry,
		1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(rx, ry + RADAR_SIZE, rx + RADAR_SIZE, ry + RADAR_SIZE,
		1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(rx, ry, rx, ry + RADAR_SIZE,
		1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(rx + RADAR_SIZE, ry, rx + RADAR_SIZE, ry + RADAR_SIZE,
		1.0f, brc, brc, brc, 0.8f);
}
