#include "skillbar.h"

#include "render.h"
#include "text.h"
#include "graphics.h"
#include "sub_pea.h"
#include "sub_mine.h"

#include <math.h>

#define SLOT_SIZE 50.0f
#define SLOT_SPACING 60.0f
#define SLOT_MARGIN 10.0f
#define PIE_SEGMENTS 32
#define PIE_RADIUS 22.0f

typedef struct {
	SubroutineId id;
	SubroutineType type;
	const char *name;
	const char *short_name;
	const char *description;
	bool elite;
} SubroutineInfo;

static const SubroutineInfo sub_registry[SUB_ID_COUNT] = {
	[SUB_ID_PEA]  = { SUB_ID_PEA,  SUB_TYPE_PROJECTILE, "sub_pea",  "PEA",
		"Basic projectile. Fires toward cursor.", false },
	[SUB_ID_MINE] = { SUB_ID_MINE, SUB_TYPE_DEPLOYABLE, "sub_mine", "MINE",
		"Deployable mine. Detonates after 2 seconds.", false },
};

static int slots[SKILLBAR_SLOTS];
static int active_sub[SUB_TYPE_COUNT];

static void render_icon(SubroutineId id, float cx, float cy, float alpha);
static float get_cooldown_fraction(SubroutineId id);

void Skillbar_initialize(void)
{
	for (int i = 0; i < SKILLBAR_SLOTS; i++)
		slots[i] = SUB_NONE;

	for (int i = 0; i < SUB_TYPE_COUNT; i++)
		active_sub[i] = SUB_NONE;

	/* Auto-equip anything already unlocked (e.g. sub_pea with threshold=0) */
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (Progression_is_unlocked(i))
			Skillbar_auto_equip(i);
	}
}

void Skillbar_cleanup(void)
{
}

void Skillbar_update(const Input *input, const unsigned int ticks)
{
	(void)ticks;

	if (input->keySlot >= 0 && input->keySlot < SKILLBAR_SLOTS) {
		int slot = input->keySlot;
		int id = slots[slot];
		if (id != SUB_NONE) {
			SubroutineType type = sub_registry[id].type;
			if (active_sub[type] == id)
				active_sub[type] = SUB_NONE;
			else
				active_sub[type] = id;
		}
	}
}

void Skillbar_render(const Screen *screen)
{
	float base_x = SLOT_MARGIN;
	float base_y = (float)screen->height - SLOT_SIZE - SLOT_MARGIN;

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	const char *labels[] = {"1","2","3","4","5","6","7","8","9","0"};

	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		float bx = base_x + i * SLOT_SPACING;
		float by = base_y;
		int id = slots[i];

		/* Slot background */
		Render_quad_absolute(bx, by, bx + SLOT_SIZE, by + SLOT_SIZE,
			0.1f, 0.1f, 0.1f, 0.8f);

		if (id != SUB_NONE) {
			SubroutineType type = sub_registry[id].type;
			bool is_active = (active_sub[type] == id);
			float cooldown = get_cooldown_fraction(id);

			/* Border */
			float br, bg, bb, ba;
			float thickness;
			if (is_active) {
				br = 1.0f; bg = 1.0f; bb = 1.0f; ba = 0.9f;
				thickness = 2.0f;
			} else {
				br = 0.3f; bg = 0.3f; bb = 0.3f; ba = 0.6f;
				thickness = 1.0f;
			}

			/* Top */
			Render_thick_line(bx, by, bx + SLOT_SIZE, by,
				thickness, br, bg, bb, ba);
			/* Bottom */
			Render_thick_line(bx, by + SLOT_SIZE, bx + SLOT_SIZE, by + SLOT_SIZE,
				thickness, br, bg, bb, ba);
			/* Left */
			Render_thick_line(bx, by, bx, by + SLOT_SIZE,
				thickness, br, bg, bb, ba);
			/* Right */
			Render_thick_line(bx + SLOT_SIZE, by, bx + SLOT_SIZE, by + SLOT_SIZE,
				thickness, br, bg, bb, ba);

			/* Icon */
			float icon_cx = bx + SLOT_SIZE * 0.5f;
			float icon_cy = by + SLOT_SIZE * 0.5f;
			float icon_alpha = (cooldown > 0.0f) ? 0.3f : 1.0f;
			render_icon(id, icon_cx, icon_cy, icon_alpha);

			/* Cooldown pie overlay */
			if (cooldown > 0.0f) {
				Render_cooldown_pie(icon_cx, icon_cy, PIE_RADIUS,
					cooldown, PIE_SEGMENTS,
					0.0f, 0.0f, 0.0f, 0.6f);
			}
		}

		/* Number label */
		float lx = bx + 2.0f;
		float ly = by + SLOT_SIZE - 2.0f;
		Text_render(tr, shaders, &proj, &ident,
			labels[i], lx, ly,
			1.0f, 1.0f, 1.0f, 0.5f);
	}
}

bool Skillbar_is_active(SubroutineId id)
{
	if (id < 0 || id >= SUB_ID_COUNT)
		return false;
	SubroutineType type = sub_registry[id].type;
	return active_sub[type] == id;
}

void Skillbar_auto_equip(SubroutineId id)
{
	/* Don't double-equip */
	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		if (slots[i] == id)
			return;
	}

	/* Find first empty slot */
	int slot = -1;
	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		if (slots[i] == SUB_NONE) {
			slot = i;
			break;
		}
	}
	if (slot < 0)
		return;

	slots[slot] = id;

	/* Auto-activate if no other sub of this type is active */
	SubroutineType type = sub_registry[id].type;
	if (active_sub[type] == SUB_NONE)
		active_sub[type] = id;
}

void Skillbar_equip(int slot, SubroutineId id)
{
	if (slot < 0 || slot >= SKILLBAR_SLOTS)
		return;
	slots[slot] = id;
}

void Skillbar_clear_slot(int slot)
{
	if (slot < 0 || slot >= SKILLBAR_SLOTS)
		return;

	int id = slots[slot];
	slots[slot] = SUB_NONE;

	/* If this was the active sub for its type, deactivate */
	if (id != SUB_NONE) {
		SubroutineType type = sub_registry[id].type;
		if (active_sub[type] == id)
			active_sub[type] = SUB_NONE;
	}
}

static void render_icon(SubroutineId id, float cx, float cy, float alpha)
{
	switch (id) {
	case SUB_ID_PEA: {
		/* White dot — matches the projectile visual */
		Position p = {cx, cy};
		ColorFloat c = {1.0f, 1.0f, 1.0f, alpha};
		Render_point(&p, 8.0, &c);
		break;
	}
	case SUB_ID_MINE: {
		/* Gray diamond + red center diamond — matches mine visual */
		Position p = {cx, cy};
		Rectangle body = {-8.0, 8.0, 8.0, -8.0};
		Rectangle dot = {-3.0, 3.0, 3.0, -3.0};
		ColorFloat gray = {0.2f, 0.2f, 0.2f, alpha};
		ColorFloat red = {1.0f, 0.0f, 0.0f, alpha};
		Render_quad(&p, 45.0, body, &gray);
		Render_quad(&p, 45.0, dot, &red);
		break;
	}
	default:
		break;
	}
}

static float get_cooldown_fraction(SubroutineId id)
{
	switch (id) {
	case SUB_ID_PEA:  return Sub_Pea_get_cooldown_fraction();
	case SUB_ID_MINE: return Sub_Mine_get_cooldown_fraction();
	default: return 0.0f;
	}
}

const char *Skillbar_get_sub_name(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT)
		return sub_registry[id].name;
	return "";
}

const char *Skillbar_get_sub_description(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT)
		return sub_registry[id].description;
	return "";
}

SubroutineType Skillbar_get_sub_type(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT)
		return sub_registry[id].type;
	return SUB_TYPE_PROJECTILE;
}

int Skillbar_get_slot_sub(int slot)
{
	if (slot >= 0 && slot < SKILLBAR_SLOTS)
		return slots[slot];
	return SUB_NONE;
}

int Skillbar_find_equipped_slot(SubroutineId id)
{
	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		if (slots[i] == id)
			return i;
	}
	return -1;
}

void Skillbar_swap_slots(int a, int b)
{
	if (a < 0 || a >= SKILLBAR_SLOTS) return;
	if (b < 0 || b >= SKILLBAR_SLOTS) return;
	int tmp = slots[a];
	slots[a] = slots[b];
	slots[b] = tmp;
}

int Skillbar_slot_at_position(float x, float y, const Screen *screen)
{
	float base_x = SLOT_MARGIN;
	float base_y = (float)screen->height - SLOT_SIZE - SLOT_MARGIN;

	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		float bx = base_x + i * SLOT_SPACING;
		float by = base_y;
		if (x >= bx && x <= bx + SLOT_SIZE && y >= by && y <= by + SLOT_SIZE)
			return i;
	}
	return -1;
}

void Skillbar_render_icon_at(SubroutineId id, float cx, float cy, float alpha)
{
	render_icon(id, cx, cy, alpha);
}

bool Skillbar_is_elite(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT)
		return sub_registry[id].elite;
	return false;
}
