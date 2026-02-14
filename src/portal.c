#include "portal.h"
#include "ship.h"
#include "render.h"
#include "view.h"
#include "color.h"
#include "text.h"
#include "graphics.h"
#include "mat4.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#define DWELL_THRESHOLD_MS 1000
#define NUM_PARTICLES 8
#define DIAMOND_SIZE 60.0

/* Colors */
static const ColorRGB COLOR_CYAN = {0, 220, 220, 255};
static const ColorRGB COLOR_WHITE = {255, 255, 255, 255};
static ColorFloat colorCyan, colorWhite;
static bool colorsInitialized = false;

typedef struct {
	bool active;
	Position position;
	char id[32];
	char dest_zone[256];
	char dest_portal_id[32];
	unsigned int dwell_timer;
	unsigned int anim_timer;
	bool ship_inside;
	bool deactivated;  /* must leave and re-enter to reactivate */
} PortalState;

static PortalState portals[PORTAL_COUNT];
static PlaceableComponent placeables[PORTAL_COUNT];
static int portalCount = 0;

static RenderableComponent renderable = {Portal_render};

/* Pending transition state */
static bool pendingTransition = false;
static char pendingDestZone[256];
static char pendingDestPortalId[32];

static void init_colors(void)
{
	if (colorsInitialized) return;
	colorCyan = Color_rgb_to_float(&COLOR_CYAN);
	colorWhite = Color_rgb_to_float(&COLOR_WHITE);
	colorsInitialized = true;
}

void Portal_initialize(Position position, const char *id,
	const char *dest_zone, const char *dest_portal_id)
{
	if (portalCount >= PORTAL_COUNT) {
		printf("Portal_initialize: max portals reached\n");
		return;
	}

	init_colors();

	PortalState *p = &portals[portalCount];
	p->active = true;
	p->position = position;
	strncpy(p->id, id, sizeof(p->id) - 1);
	p->id[sizeof(p->id) - 1] = '\0';
	strncpy(p->dest_zone, dest_zone, sizeof(p->dest_zone) - 1);
	p->dest_zone[sizeof(p->dest_zone) - 1] = '\0';
	strncpy(p->dest_portal_id, dest_portal_id, sizeof(p->dest_portal_id) - 1);
	p->dest_portal_id[sizeof(p->dest_portal_id) - 1] = '\0';
	p->dwell_timer = 0;
	p->anim_timer = 0;
	p->ship_inside = false;
	p->deactivated = false;

	placeables[portalCount].position = position;
	placeables[portalCount].heading = 0.0;

	Entity entity = Entity_initialize_entity();
	entity.state = p;
	entity.placeable = &placeables[portalCount];
	entity.renderable = &renderable;

	Entity_add_entity(entity);

	portalCount++;
}

void Portal_cleanup(void)
{
	portalCount = 0;
	pendingTransition = false;
}

void Portal_update_all(unsigned int ticks)
{
	Position ship_pos = Ship_get_position();

	for (int i = 0; i < portalCount; i++) {
		PortalState *p = &portals[i];
		if (!p->active) continue;

		p->anim_timer += ticks;

		/* Point-in-AABB test for ship center */
		double dx = ship_pos.x - p->position.x;
		double dy = ship_pos.y - p->position.y;
		bool inside = (dx >= -PORTAL_HALF_SIZE && dx <= PORTAL_HALF_SIZE &&
		               dy >= -PORTAL_HALF_SIZE && dy <= PORTAL_HALF_SIZE);

		if (inside && !Ship_is_destroyed()) {
			p->ship_inside = true;

			/* Don't charge while deactivated — must leave first */
			if (p->deactivated) continue;

			p->dwell_timer += ticks;

			/* Pull ship toward portal center as charge builds */
			float charge = (float)p->dwell_timer / DWELL_THRESHOLD_MS;
			if (charge > 1.0f) charge = 1.0f;
			float pull = charge * charge; /* ease-in */
			Position pulled;
			pulled.x = ship_pos.x + (p->position.x - ship_pos.x) * pull;
			pulled.y = ship_pos.y + (p->position.y - ship_pos.y) * pull;
			Ship_set_position(pulled);

			if (p->dwell_timer >= DWELL_THRESHOLD_MS && !pendingTransition) {
				pendingTransition = true;
				strncpy(pendingDestZone, p->dest_zone, sizeof(pendingDestZone) - 1);
				pendingDestZone[sizeof(pendingDestZone) - 1] = '\0';
				strncpy(pendingDestPortalId, p->dest_portal_id, sizeof(pendingDestPortalId) - 1);
				pendingDestPortalId[sizeof(pendingDestPortalId) - 1] = '\0';
				printf("Portal: transition triggered -> %s @ %s\n",
					p->dest_zone, p->dest_portal_id);
			}
		} else {
			/* Ship left — reactivate if deactivated */
			if (p->deactivated && !inside)
				p->deactivated = false;
			p->ship_inside = false;
			p->dwell_timer = 0;
		}
	}
}

void Portal_render(const void *state, const PlaceableComponent *placeable)
{
	const PortalState *p = (const PortalState *)state;
	if (!p->active) return;

	float t = p->anim_timer / 1000.0f;
	float charge = 0.0f;
	if (p->ship_inside && !p->deactivated)
		charge = (float)p->dwell_timer / DWELL_THRESHOLD_MS;
	if (charge > 1.0f) charge = 1.0f;

	/* Diamond is white, intensifies with charge, dims when deactivated */
	float pulse = p->deactivated ? 1.0f : 1.0f + 0.1f * sinf(t * 3.0f + charge * 8.0f);
	float ds = DIAMOND_SIZE * pulse;
	Rectangle diamond = {-ds, ds, ds, -ds};
	float innerAlpha = p->deactivated ? 0.25f : 0.6f + 0.3f * charge;
	ColorFloat innerColor = {1.0f, 1.0f, 1.0f, innerAlpha};
	Render_quad(&p->position, 45.0, diamond, &innerColor);
}

void Portal_render_bloom_source(void)
{
	for (int i = 0; i < portalCount; i++) {
		PortalState *p = &portals[i];
		if (!p->active) continue;

		float t = p->anim_timer / 1000.0f;
		float charge = 0.0f;
		if (p->ship_inside && !p->deactivated)
			charge = (float)p->dwell_timer / DWELL_THRESHOLD_MS;
		if (charge > 1.0f) charge = 1.0f;

		/* White diamond bloom — suppressed when deactivated */
		if (p->deactivated) continue;
		float pulse = 1.0f + 0.15f * sinf(t * 3.0f + charge * 8.0f);
		float ds = (DIAMOND_SIZE + 5.0f) * pulse;
		Rectangle diamond = {-ds, ds, ds, -ds};
		float bloomAlpha = 0.5f + 0.5f * charge;
		ColorFloat bloomColor = {1.0f, 1.0f, 1.0f, bloomAlpha};
		Render_quad(&p->position, 45.0, diamond, &bloomColor);

	}
}

void Portal_render_god_labels(void)
{
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_world_projection();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);

	for (int i = 0; i < portalCount; i++) {
		PortalState *p = &portals[i];
		if (!p->active) continue;

		/* Convert world position to screen position */
		float wx = (float)p->position.x;
		float wy = (float)p->position.y;

		char buf[128];
		snprintf(buf, sizeof(buf), "[%s]", p->id);
		Text_render(tr, shaders, &proj, &view,
			buf, wx - 30.0f, wy + 55.0f,
			0.0f, 1.0f, 1.0f, 0.9f);

		snprintf(buf, sizeof(buf), "-> %s", p->dest_portal_id);
		Text_render(tr, shaders, &proj, &view,
			buf, wx - 30.0f, wy + 70.0f,
			0.0f, 0.8f, 0.8f, 0.7f);
	}
}

bool Portal_has_pending_transition(void)
{
	return pendingTransition;
}

const char *Portal_get_pending_dest_zone(void)
{
	return pendingDestZone;
}

const char *Portal_get_pending_dest_portal_id(void)
{
	return pendingDestPortalId;
}

void Portal_clear_pending_transition(void)
{
	pendingTransition = false;
}

bool Portal_get_position_by_id(const char *id, Position *out)
{
	for (int i = 0; i < portalCount; i++) {
		if (portals[i].active && strcmp(portals[i].id, id) == 0) {
			*out = portals[i].position;
			return true;
		}
	}
	return false;
}

void Portal_suppress_arrival(const char *portal_id)
{
	for (int i = 0; i < portalCount; i++) {
		if (portals[i].active && strcmp(portals[i].id, portal_id) == 0) {
			portals[i].deactivated = true;
			portals[i].dwell_timer = 0;
			portals[i].ship_inside = false;
			return;
		}
	}
}

void Portal_render_minimap(float ship_x, float ship_y,
	float screen_x, float screen_y, float size, float range)
{
	float half_range = range * 0.5f;
	float half_size = size * 0.5f;

	for (int i = 0; i < portalCount; i++) {
		PortalState *p = &portals[i];
		if (!p->active) continue;

		float dx = (float)p->position.x - ship_x;
		float dy = (float)p->position.y - ship_y;

		if (dx < -half_range || dx > half_range ||
		    dy < -half_range || dy > half_range)
			continue;

		float mx = screen_x + half_size + (dx / half_range) * half_size;
		float my = screen_y + half_size - (dy / half_range) * half_size;

		/* Bright cyan dot */
		float s = 2.5f;
		Render_quad_absolute(mx - s, my - s, mx + s, my + s,
			1.0f, 1.0f, 1.0f, 1.0f);
	}
}
