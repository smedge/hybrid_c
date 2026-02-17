#include "skillbar.h"

#include "render.h"
#include "text.h"
#include "graphics.h"
#include "sub_pea.h"
#include "sub_mgun.h"
#include "sub_mine.h"
#include "sub_boost.h"
#include "sub_egress.h"
#include "sub_mend.h"
#include "sub_aegis.h"
#include "sub_stealth.h"
#include "sub_inferno.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
	[SUB_ID_PEA]    = { SUB_ID_PEA,    SUB_TYPE_PROJECTILE, "sub_pea",    "PEA",
		"Basic projectile. Fires toward cursor.", false },
	[SUB_ID_MINE]   = { SUB_ID_MINE,   SUB_TYPE_DEPLOYABLE, "sub_mine",   "MINE",
		"Deployable mine. Detonates after 2 seconds.", false },
	[SUB_ID_BOOST]  = { SUB_ID_BOOST,  SUB_TYPE_MOVEMENT,   "sub_boost",  "BOOST",
		"Hold shift for unlimited speed boost.", true },
	[SUB_ID_EGRESS] = { SUB_ID_EGRESS, SUB_TYPE_MOVEMENT,   "sub_egress", "EGRESS",
		"Shift-tap dash burst. Quick escape with cooldown.", false },
	[SUB_ID_MGUN]   = { SUB_ID_MGUN,   SUB_TYPE_PROJECTILE, "sub_mgun",   "MGUN",
		"Machine gun. Rapid-fire projectiles at 5 shots/sec.", false },
	[SUB_ID_MEND]   = { SUB_ID_MEND,   SUB_TYPE_HEALING,    "sub_mend",   "MEND",
		"Instant heal. Restores 50 integrity.", false },
	[SUB_ID_AEGIS]  = { SUB_ID_AEGIS,  SUB_TYPE_SHIELD,     "sub_aegis",  "AEGIS",
		"Damage shield. Invulnerable for 10 seconds.", false },
	[SUB_ID_STEALTH] = { SUB_ID_STEALTH, SUB_TYPE_STEALTH, "sub_stealth", "STEALTH",
		"Cloak. Undetectable until you attack. 15s cooldown.", false },
	[SUB_ID_INFERNO] = { SUB_ID_INFERNO, SUB_TYPE_PROJECTILE, "sub_inferno", "INFERNO",
		"Channel a devastating beam of fire. Melts everything.", true },
};

static int slots[SKILLBAR_SLOTS];
static int active_sub[SUB_TYPE_COUNT];
static bool mouseWasDown;
static bool clickConsumed;
static int pressedSlot;  /* slot pressed on mouse-down, -1 if none */

static void render_icon(SubroutineId id, float cx, float cy, float alpha);
static float get_cooldown_fraction(SubroutineId id);
static void toggle_slot(int slot);

static void toggle_slot(int slot)
{
	int id = slots[slot];
	if (id == SUB_NONE) return;

	/* Stealth: slot key activates the ability, border tracks stealth state */
	if (id == SUB_ID_STEALTH) {
		Sub_Stealth_try_activate();
		if (Sub_Stealth_is_stealthed())
			active_sub[SUB_TYPE_STEALTH] = SUB_ID_STEALTH;
		return;
	}

	SubroutineType type = sub_registry[id].type;
	active_sub[type] = (active_sub[type] == id) ? SUB_NONE : id;
}

static int slot_under_mouse(const Input *input)
{
	Screen screen = Graphics_get_screen();
	return Skillbar_slot_at_position(
		(float)input->mouseX, (float)input->mouseY, &screen);
}

void Skillbar_initialize(void)
{
	for (int i = 0; i < SKILLBAR_SLOTS; i++)
		slots[i] = SUB_NONE;

	for (int i = 0; i < SUB_TYPE_COUNT; i++)
		active_sub[i] = SUB_NONE;

	mouseWasDown = false;
	clickConsumed = false;
	pressedSlot = -1;

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

	/* Sync stealth border with actual stealth state (clears on break) */
	if (active_sub[SUB_TYPE_STEALTH] == SUB_ID_STEALTH
			&& !Sub_Stealth_is_stealthed())
		active_sub[SUB_TYPE_STEALTH] = SUB_NONE;

	clickConsumed = false;

	/* Number key activation */
	if (input->keySlot >= 0 && input->keySlot < SKILLBAR_SLOTS)
		toggle_slot(input->keySlot);

	/* Mouse click activation — activate on mouse-up */
	int hover = slot_under_mouse(input);

	if (input->mouseLeft && hover >= 0) {
		clickConsumed = true;
		if (!mouseWasDown)
			pressedSlot = hover;
	} else if (!input->mouseLeft && mouseWasDown && hover == pressedSlot && pressedSlot >= 0) {
		toggle_slot(pressedSlot);
		pressedSlot = -1;
	} else if (!input->mouseLeft) {
		pressedSlot = -1;
	}

	mouseWasDown = input->mouseLeft;
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

		if (id == SUB_NONE) {
			/* Empty slot border */
			float brc = 0.3f;
			Render_thick_line(bx, by, bx + SLOT_SIZE, by,
				1.0f, brc, brc, brc, 0.8f);
			Render_thick_line(bx, by + SLOT_SIZE, bx + SLOT_SIZE, by + SLOT_SIZE,
				1.0f, brc, brc, brc, 0.8f);
			Render_thick_line(bx, by, bx, by + SLOT_SIZE,
				1.0f, brc, brc, brc, 0.8f);
			Render_thick_line(bx + SLOT_SIZE, by, bx + SLOT_SIZE, by + SLOT_SIZE,
				1.0f, brc, brc, brc, 0.8f);
		}

		if (id != SUB_NONE) {
			SubroutineType type = sub_registry[id].type;
			bool is_active = (active_sub[type] == id);
			float cooldown = get_cooldown_fraction(id);

			/* Border — gold for elite, white for normal */
			float br, bg, bb, ba;
			float thickness;
			bool elite = sub_registry[id].elite;
			if (is_active) {
				if (elite) {
					br = 1.0f; bg = 0.84f; bb = 0.0f;
				} else {
					br = 1.0f; bg = 1.0f; bb = 1.0f;
				}
				ba = 0.9f;
				thickness = 2.0f;
			} else {
				if (elite) {
					br = 0.5f; bg = 0.42f; bb = 0.0f;
				} else {
					br = 0.3f; bg = 0.3f; bb = 0.3f;
				}
				ba = 0.6f;
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

bool Skillbar_consumed_click(void)
{
	return clickConsumed;
}

void Skillbar_auto_equip(SubroutineId id)
{
	/* Don't double-equip */
	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		if (slots[i] == id)
			return;
	}

	/* Elite limit: only one elite on the skillbar at a time.
	   Auto-equip skips if an elite is already equipped. */
	if (sub_registry[id].elite) {
		for (int i = 0; i < SKILLBAR_SLOTS; i++) {
			if (slots[i] != SUB_NONE && sub_registry[slots[i]].elite)
				return;
		}
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

	/* Auto-activate if no other sub of this type is active.
	   Stealth is ability-activated, not toggle-activated — never auto-activate. */
	SubroutineType type = sub_registry[id].type;
	if (type != SUB_TYPE_STEALTH && active_sub[type] == SUB_NONE)
		active_sub[type] = id;
}

void Skillbar_equip(int slot, SubroutineId id)
{
	if (slot < 0 || slot >= SKILLBAR_SLOTS)
		return;
	if (id < 0 || id >= SUB_ID_COUNT)
		return;

	/* Clear any existing slot that has this id to prevent duplicates */
	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		if (slots[i] == id)
			slots[i] = SUB_NONE;
	}

	/* Elite limit: only one elite on the bar. Remove existing elite. */
	if (sub_registry[id].elite) {
		for (int i = 0; i < SKILLBAR_SLOTS; i++) {
			if (slots[i] != SUB_NONE && sub_registry[slots[i]].elite) {
				SubroutineType oldType = sub_registry[slots[i]].type;
				if (active_sub[oldType] == slots[i])
					active_sub[oldType] = SUB_NONE;
				slots[i] = SUB_NONE;
			}
		}
	}

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
	case SUB_ID_BOOST: {
		/* Double chevron (>>) — speed lines, centered on cx,cy */
		float sz = 6.0f;
		float t = 1.5f;
		/* Left chevron: tip at cx+1, extends left to cx-5 */
		Render_thick_line(cx - 5, cy - sz, cx + 1, cy, t, 1.0f, 0.8f, 0.0f, alpha);
		Render_thick_line(cx + 1, cy, cx - 5, cy + sz, t, 1.0f, 0.8f, 0.0f, alpha);
		/* Right chevron: tip at cx+5, extends left to cx-1 */
		Render_thick_line(cx - 1, cy - sz, cx + 5, cy, t, 1.0f, 0.8f, 0.0f, alpha);
		Render_thick_line(cx + 5, cy, cx - 1, cy + sz, t, 1.0f, 0.8f, 0.0f, alpha);
		break;
	}
	case SUB_ID_EGRESS: {
		/* Starburst — 4 lines radiating from center (thick_line = triangles) */
		float sz = 8.0f;
		float t = 1.5f;
		Render_thick_line(cx, cy - sz, cx, cy + sz, t, 0.4f, 1.0f, 1.0f, alpha);
		Render_thick_line(cx - sz, cy, cx + sz, cy, t, 0.4f, 1.0f, 1.0f, alpha);
		float d = sz * 0.7f;
		Render_thick_line(cx - d, cy - d, cx + d, cy + d, t, 0.4f, 1.0f, 1.0f, alpha);
		Render_thick_line(cx + d, cy - d, cx - d, cy + d, t, 0.4f, 1.0f, 1.0f, alpha);
		break;
	}
	case SUB_ID_MGUN: {
		/* Three dots in a spray pattern — suggests rapid fire */
		Position p1 = {cx - 4, cy - 4};
		Position p2 = {cx, cy};
		Position p3 = {cx + 4, cy + 4};
		ColorFloat c = {1.0f, 1.0f, 1.0f, alpha};
		Render_point(&p1, 5.0, &c);
		Render_point(&p2, 5.0, &c);
		Render_point(&p3, 5.0, &c);
		break;
	}
	case SUB_ID_MEND: {
		/* Plus/cross — healing symbol */
		float sz = 7.0f;
		float t = 2.0f;
		Render_thick_line(cx, cy - sz, cx, cy + sz, t, 0.3f, 0.7f, 1.0f, alpha);
		Render_thick_line(cx - sz, cy, cx + sz, cy, t, 0.3f, 0.7f, 1.0f, alpha);
		break;
	}
	case SUB_ID_AEGIS: {
		/* Hexagon outline — shield/defense */
		float r = 8.0f;
		float t = 1.5f;
		for (int i = 0; i < 6; i++) {
			float a0 = i * 60.0f * (float)M_PI / 180.0f;
			float a1 = (i + 1) * 60.0f * (float)M_PI / 180.0f;
			Render_thick_line(cx + cosf(a0) * r, cy + sinf(a0) * r,
				cx + cosf(a1) * r, cy + sinf(a1) * r,
				t, 0.6f, 0.9f, 1.0f, alpha);
		}
		break;
	}
	case SUB_ID_STEALTH: {
		/* Eye shape — two arcs forming an eye, with a dot pupil */
		float r = 8.0f;
		float t = 1.5f;
		/* Upper lid arc */
		Render_thick_line(cx - r, cy, cx - r * 0.4f, cy + r * 0.6f, t,
			0.6f, 0.3f, 0.8f, alpha);
		Render_thick_line(cx - r * 0.4f, cy + r * 0.6f, cx + r * 0.4f, cy + r * 0.6f, t,
			0.6f, 0.3f, 0.8f, alpha);
		Render_thick_line(cx + r * 0.4f, cy + r * 0.6f, cx + r, cy, t,
			0.6f, 0.3f, 0.8f, alpha);
		/* Lower lid arc */
		Render_thick_line(cx - r, cy, cx - r * 0.4f, cy - r * 0.6f, t,
			0.6f, 0.3f, 0.8f, alpha);
		Render_thick_line(cx - r * 0.4f, cy - r * 0.6f, cx + r * 0.4f, cy - r * 0.6f, t,
			0.6f, 0.3f, 0.8f, alpha);
		Render_thick_line(cx + r * 0.4f, cy - r * 0.6f, cx + r, cy, t,
			0.6f, 0.3f, 0.8f, alpha);
		/* Pupil */
		Render_filled_circle(cx, cy, 2.5f, 6, 0.6f, 0.3f, 0.8f, alpha);
		break;
	}
	case SUB_ID_INFERNO: {
		/* Flame icon — two overlapping fire shapes */
		float t = 1.5f;
		/* Outer flame — orange */
		Render_thick_line(cx, cy + 9, cx - 5, cy - 3, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx - 5, cy - 3, cx - 2, cy - 1, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx - 2, cy - 1, cx - 3, cy - 7, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx - 3, cy - 7, cx, cy - 3, t, 1.0f, 0.6f, 0.1f, alpha);
		Render_thick_line(cx, cy - 3, cx + 3, cy - 7, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx + 3, cy - 7, cx + 2, cy - 1, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx + 2, cy - 1, cx + 5, cy - 3, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx + 5, cy - 3, cx, cy + 9, t, 1.0f, 0.5f, 0.0f, alpha);
		/* Inner bright core */
		Render_filled_circle(cx, cy, 3.0f, 6, 1.0f, 1.0f, 0.7f, alpha);
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
	case SUB_ID_MINE:   return Sub_Mine_get_cooldown_fraction();
	case SUB_ID_BOOST:  return Sub_Boost_get_cooldown_fraction();
	case SUB_ID_EGRESS: return Sub_Egress_get_cooldown_fraction();
	case SUB_ID_MGUN:   return Sub_Mgun_get_cooldown_fraction();
	case SUB_ID_MEND:   return Sub_Mend_get_cooldown_fraction();
	case SUB_ID_AEGIS:  return Sub_Aegis_get_cooldown_fraction();
	case SUB_ID_STEALTH: return Sub_Stealth_get_cooldown_fraction();
	case SUB_ID_INFERNO: return Sub_Inferno_get_cooldown_fraction();
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

SkillbarSnapshot Skillbar_snapshot(void)
{
	SkillbarSnapshot snap;
	for (int i = 0; i < SKILLBAR_SLOTS; i++)
		snap.slots[i] = slots[i];
	for (int i = 0; i < SUB_TYPE_COUNT; i++)
		snap.active_sub[i] = active_sub[i];
	return snap;
}

void Skillbar_restore(SkillbarSnapshot snap)
{
	for (int i = 0; i < SKILLBAR_SLOTS; i++)
		slots[i] = snap.slots[i];
	for (int i = 0; i < SUB_TYPE_COUNT; i++)
		active_sub[i] = snap.active_sub[i];
}
