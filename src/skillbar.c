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
#include "sub_disintegrate.h"
#include "sub_gravwell.h"
#include "sub_tgun.h"
#include "sub_flak.h"
#include "sub_ember.h"
#include "sub_sprint.h"
#include "sub_emp.h"
#include "sub_resist.h"
#include "sub_blaze.h"
#include "sub_cauterize.h"
#include "sub_immolate.h"
#include "sub_cinder.h"
#include "sub_scorch.h"
#include "sub_smolder.h"
#include "sub_heatwave.h"
#include "sub_temper.h"
#include "keybinds.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SLOT_SIZE 50.0f
#define SLOT_SPACING 60.0f
#define SLOT_MARGIN 10.0f
#define SLOT_CHAMF 8.0f
#define PIE_SEGMENTS 32
#define PIE_RADIUS 22.0f

/* Gravwell icon colors */
#define EDGE_R_ICON 0.15f
#define EDGE_G_ICON 0.2f
#define EDGE_B_ICON 0.5f
#define MID_R_ICON  0.08f
#define MID_G_ICON  0.1f
#define MID_B_ICON  0.3f

typedef struct {
	SubroutineId id;
	SubroutineType type;
	const char *name;
	const char *short_name;
	const char *description;
	SubroutineTier tier;
} SubroutineInfo;

static const SubroutineInfo sub_registry[SUB_ID_COUNT] = {
	[SUB_ID_PEA]    = { SUB_ID_PEA,    SUB_TYPE_PROJECTILE, "sub_pea",    "PEA",
		"Basic projectile. Respectable damage.", TIER_NORMAL },
	[SUB_ID_MINE]   = { SUB_ID_MINE,   SUB_TYPE_DEPLOYABLE, "sub_mine",   "MINE",
		"Deployable mine. Detonates after 2 seconds.", TIER_NORMAL },
	[SUB_ID_BOOST]  = { SUB_ID_BOOST,  SUB_TYPE_MOVEMENT,   "sub_boost",  "BOOST",
		"Toggle 2x speed. Halts health regen and feedback decay while active.", TIER_ELITE },
	[SUB_ID_EGRESS] = { SUB_ID_EGRESS, SUB_TYPE_MOVEMENT,   "sub_egress", "EGRESS",
		"Dash thru an enemy causing damage. Quick escape a dangerous spot.", TIER_NORMAL },
	[SUB_ID_MGUN]   = { SUB_ID_MGUN,   SUB_TYPE_PROJECTILE, "sub_mgun",   "MGUN",
		"Machine gun. Rapid-fire projectiles.", TIER_NORMAL },
	[SUB_ID_MEND]   = { SUB_ID_MEND,   SUB_TYPE_HEALING,    "sub_mend",   "MEND",
		"Instant heal. Restores 50 integrity.", TIER_NORMAL },
	[SUB_ID_AEGIS]  = { SUB_ID_AEGIS,  SUB_TYPE_SHIELD,     "sub_aegis",  "AEGIS",
		"Damage shield. Invulnerable for 10 seconds.", TIER_NORMAL },
	[SUB_ID_STEALTH] = { SUB_ID_STEALTH, SUB_TYPE_STEALTH, "sub_stealth", "STEALTH",
		"Cloak. Undetectable until you attack. 15s cooldown.", TIER_NORMAL },
	[SUB_ID_INFERNO] = { SUB_ID_INFERNO, SUB_TYPE_PROJECTILE, "sub_inferno", "INFERNO",
		"Channel a devastating beam of fire. Melts everything.", TIER_ELITE },
	[SUB_ID_DISINTEGRATE] = { SUB_ID_DISINTEGRATE, SUB_TYPE_PROJECTILE, "sub_disintegrate", "DISINT",
		"Precision beam. Carves through all targets in its path.", TIER_ELITE },
	[SUB_ID_GRAVWELL] = { SUB_ID_GRAVWELL, SUB_TYPE_CONTROL, "sub_gravwell", "GRAV",
		"Gravity well. Pulls and slows enemies at cursor position.", TIER_NORMAL },
	[SUB_ID_TGUN] = { SUB_ID_TGUN, SUB_TYPE_PROJECTILE, "sub_tgun", "TGUN",
		"Twin gun. Dual alternating streams at a massive fire rate.", TIER_RARE },
	[SUB_ID_SPRINT] = { SUB_ID_SPRINT, SUB_TYPE_MOVEMENT, "sub_sprint", "SPRINT",
		"Sprint burst. 3x speed for 5 seconds.", TIER_NORMAL },
	[SUB_ID_EMP] = { SUB_ID_EMP, SUB_TYPE_AREA_EFFECT, "sub_emp", "EMP",
		"EMP blast. Maxes feedback on all nearby enemies.", TIER_RARE },
	[SUB_ID_RESIST] = { SUB_ID_RESIST, SUB_TYPE_SHIELD, "sub_resist", "RESIST",
		"Damage resistance. 50% damage reduction for 5 seconds.", TIER_NORMAL },
	/* Fire zone (The Crucible) */
	[SUB_ID_EMBER] = { SUB_ID_EMBER, SUB_TYPE_PROJECTILE, "sub_ember", "EMBER",
		"Shoot concentrated fire energy. Targets burn on impact.", TIER_NORMAL },
	[SUB_ID_FLAK] = { SUB_ID_FLAK, SUB_TYPE_PROJECTILE, "sub_flak", "FLAK",
		"Flak cannon. Narrow cone of fire pellets that burn on impact.", TIER_RARE },
	[SUB_ID_BLAZE] = { SUB_ID_BLAZE, SUB_TYPE_MOVEMENT, "sub_blaze", "BLAZE",
		"Dash thru an enemy. Leaves a flame corridor behind you.", TIER_NORMAL },
	[SUB_ID_SCORCH] = { SUB_ID_SCORCH, SUB_TYPE_MOVEMENT, "sub_scorch", "SCORCH",
		"Scorch sprint. Speed boost that leaves burning footprints.", TIER_NORMAL },
	[SUB_ID_CINDER] = { SUB_ID_CINDER, SUB_TYPE_DEPLOYABLE, "sub_cinder", "CINDR",
		"Cinder mine. Detonates into a lingering fire pool.", TIER_NORMAL },
	[SUB_ID_CAUTERIZE] = { SUB_ID_CAUTERIZE, SUB_TYPE_HEALING, "sub_cauterize", "CAUT",
		"Cauterize. Heals, stops burning and burns nearby enemies.", TIER_NORMAL },
	[SUB_ID_IMMOLATE] = { SUB_ID_IMMOLATE, SUB_TYPE_SHIELD, "sub_immolate", "IMMOL",
		"Immolation shield. Burns anything near you.", TIER_NORMAL },
	[SUB_ID_SMOLDER] = { SUB_ID_SMOLDER, SUB_TYPE_STEALTH, "sub_smolder", "SMOLD",
		"Smolder cloak. Heat shimmer camo, burn on ambush.", TIER_NORMAL },
	[SUB_ID_HEATWAVE] = { SUB_ID_HEATWAVE, SUB_TYPE_AREA_EFFECT, "sub_heatwave", "HEAT",
		"Heatwave pulse. Doubles feedback gain in radius.", TIER_RARE },
	[SUB_ID_TEMPER] = { SUB_ID_TEMPER, SUB_TYPE_SHIELD, "sub_temper", "TEMPR",
		"Temper aura. Fire resist + allies burn on attack.", TIER_NORMAL },
};

static int slots[SKILLBAR_SLOTS];
static int active_sub[SUB_TYPE_COUNT];
static bool mouseWasDown;
static bool clickConsumed;
static int pressedSlot;  /* slot pressed on mouse-down, -1 if none */

static void render_icon(SubroutineId id, float cx, float cy, float alpha, float s);
static float get_cooldown_fraction(SubroutineId id);
static void activate_slot(int slot);

static void activate_slot(int slot)
{
	int id = slots[slot];
	if (id == SUB_NONE) return;

	SubroutineType type = sub_registry[id].type;

	/* Projectile subs: toggle active/inactive (existing behavior) */
	if (type == SUB_TYPE_PROJECTILE) {
		active_sub[type] = (active_sub[type] == id) ? SUB_NONE : id;
		return;
	}

	/* All other subs: instant-use activation */
	switch (id) {
	/* Movement */
	case SUB_ID_EGRESS:  Sub_Egress_try_activate(); break;
	case SUB_ID_SPRINT:  Sub_Sprint_try_activate(); break;
	case SUB_ID_BLAZE:   Sub_Blaze_try_activate(); break;
	case SUB_ID_BOOST:   Sub_Boost_try_activate(); break;
	case SUB_ID_SCORCH:  Sub_Scorch_try_activate(); break;

	/* Shield */
	case SUB_ID_AEGIS:   Sub_Aegis_try_activate(); break;
	case SUB_ID_IMMOLATE: Sub_Immolate_try_activate_player(); break;
	case SUB_ID_RESIST:  Sub_Resist_try_activate(); break;
	case SUB_ID_TEMPER:  Sub_Temper_try_activate(); break;

	/* Healing */
	case SUB_ID_MEND:      Sub_Mend_try_activate(); break;
	case SUB_ID_CAUTERIZE: Sub_Cauterize_try_activate_player(); break;

	/* Deployable */
	case SUB_ID_MINE:   Sub_Mine_try_deploy(); break;
	case SUB_ID_CINDER: Sub_Cinder_try_deploy(); break;

	/* Stealth */
	case SUB_ID_STEALTH: Sub_Stealth_try_activate(); break;
	case SUB_ID_SMOLDER: Sub_Smolder_try_activate(); break;

	/* Control */
	case SUB_ID_GRAVWELL: Sub_Gravwell_try_activate(); break;

	/* Area Effect */
	case SUB_ID_EMP: Sub_Emp_try_activate(); break;
	case SUB_ID_HEATWAVE: Sub_Heatwave_try_activate(); break;

	default: break;
	}
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
	if (active_sub[SUB_TYPE_STEALTH] == SUB_ID_SMOLDER
			&& !Sub_Smolder_is_active())
		active_sub[SUB_TYPE_STEALTH] = SUB_NONE;

	/* Sync gravwell border with active state (clears on expire) */
	if (active_sub[SUB_TYPE_CONTROL] == SUB_ID_GRAVWELL
			&& !Sub_Gravwell_is_active())
		active_sub[SUB_TYPE_CONTROL] = SUB_NONE;

	/* Sync EMP/heatwave border with active state (clears when visual ends) */
	if (active_sub[SUB_TYPE_AREA_EFFECT] == SUB_ID_EMP
			&& !Sub_Emp_is_active())
		active_sub[SUB_TYPE_AREA_EFFECT] = SUB_NONE;
	if (active_sub[SUB_TYPE_AREA_EFFECT] == SUB_ID_HEATWAVE
			&& !Sub_Heatwave_is_active())
		active_sub[SUB_TYPE_AREA_EFFECT] = SUB_NONE;

	clickConsumed = false;

	/* Number key activation (permanent, non-rebindable) */
	if (input->keySlot >= 0 && input->keySlot < SKILLBAR_SLOTS)
		activate_slot(input->keySlot);

	/* Alternate keybind activation (rebindable per slot) */
	static const BindAction slot_binds[SKILLBAR_SLOTS] = {
		BIND_SLOT_1, BIND_SLOT_2, BIND_SLOT_3, BIND_SLOT_4, BIND_SLOT_5,
		BIND_SLOT_6, BIND_SLOT_7, BIND_SLOT_8, BIND_SLOT_9, BIND_SLOT_0,
	};
	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		if (Keybinds_pressed(slot_binds[i]))
			activate_slot(i);
	}

	/* Mouse click activation — activate on mouse-up */
	int hover = slot_under_mouse(input);

	if (input->mouseLeft && hover >= 0) {
		clickConsumed = true;
		if (!mouseWasDown)
			pressedSlot = hover;
	} else if (!input->mouseLeft && mouseWasDown && hover == pressedSlot && pressedSlot >= 0) {
		activate_slot(pressedSlot);
		pressedSlot = -1;
	} else if (!input->mouseLeft) {
		pressedSlot = -1;
	}

	mouseWasDown = input->mouseLeft;
}

void Skillbar_render(const Screen *screen)
{
	float s = Graphics_get_ui_scale();
	float slot_size = SLOT_SIZE * s;
	float slot_spacing = SLOT_SPACING * s;
	float slot_margin = SLOT_MARGIN * s;
	float slot_chamf = SLOT_CHAMF * s;
	float pie_radius = PIE_RADIUS * s;

	float base_x = slot_margin;
	float base_y = (float)screen->height - slot_size - slot_margin;

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	const char *labels[] = {"1","2","3","4","5","6","7","8","9","0"};

	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		float bx = base_x + i * slot_spacing;
		float by = base_y;
		int id = slots[i];
		float ch = slot_chamf;

		/* Chamfered vertices: sharp NW + SE, chamfered NE + SW */
		float vx[6] = { bx,              bx + slot_size - ch, bx + slot_size,
		                 bx + slot_size,  bx + ch,             bx };
		float vy[6] = { by,              by,                   by + ch,
		                 by + slot_size,  by + slot_size,       by + slot_size - ch };

		/* Slot background — chamfered polygon fill */
		float fcx = bx + slot_size * 0.5f;
		float fcy = by + slot_size * 0.5f;
		BatchRenderer *batch = Graphics_get_batch();
		for (int v = 0; v < 6; v++) {
			int nv = (v + 1) % 6;
			Batch_push_triangle_vertices(batch,
				fcx, fcy, vx[v], vy[v], vx[nv], vy[nv],
				0.1f, 0.1f, 0.1f, 0.8f);
		}

		if (id == SUB_NONE) {
			/* Empty slot border */
			float brc = 0.3f;
			for (int v = 0; v < 6; v++) {
				int nv = (v + 1) % 6;
				Render_thick_line(vx[v], vy[v], vx[nv], vy[nv],
					1.0f * s, brc, brc, brc, 0.8f);
			}
		}

		if (id != SUB_NONE) {
			SubroutineType type = sub_registry[id].type;
			float cooldown = get_cooldown_fraction(id);
			bool is_active;
			if (type == SUB_TYPE_PROJECTILE)
				is_active = (active_sub[type] == id);
			else
				is_active = (cooldown <= 0.0f);

			/* Border — gold for elite, blue for rare, white for normal */
			float br, bg, bb, ba;
			float thickness;
			SubroutineTier tier = sub_registry[id].tier;
			if (is_active) {
				if (tier == TIER_ELITE) {
					br = 1.0f; bg = 0.84f; bb = 0.0f;
				} else if (tier == TIER_RARE) {
					br = 0.1f; bg = 0.1f; bb = 1.0f;
				} else {
					br = 1.0f; bg = 1.0f; bb = 1.0f;
				}
				ba = 0.9f;
				thickness = 2.0f;
			} else {
				if (tier == TIER_ELITE) {
					br = 0.5f; bg = 0.42f; bb = 0.0f;
				} else if (tier == TIER_RARE) {
					br = 0.05f; bg = 0.05f; bb = 0.5f;
				} else {
					br = 0.3f; bg = 0.3f; bb = 0.3f;
				}
				ba = 0.6f;
				thickness = 1.0f;
			}

			/* Chamfered border */
			for (int v = 0; v < 6; v++) {
				int nv = (v + 1) % 6;
				Render_thick_line(vx[v], vy[v], vx[nv], vy[nv],
					thickness * s, br, bg, bb, ba);
			}

			/* Icon */
			float icon_cx = bx + slot_size * 0.5f;
			float icon_cy = by + slot_size * 0.5f;
			float icon_alpha = (cooldown > 0.0f) ? 0.3f : 1.0f;
			render_icon(id, icon_cx, icon_cy, icon_alpha, s);

			/* Cooldown pie overlay */
			if (cooldown > 0.0f) {
				Render_cooldown_pie(icon_cx, icon_cy, pie_radius,
					cooldown, PIE_SEGMENTS,
					0.0f, 0.0f, 0.0f, 0.6f);
			}
		}

		/* Number label — SE corner with padding */
		float lw = Text_measure_width(tr, labels[i]);
		float lx = bx + slot_size - lw - 4.0f * s;
		float ly = by + slot_size - 4.0f * s;
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
	if (sub_registry[id].tier == TIER_ELITE) {
		for (int i = 0; i < SKILLBAR_SLOTS; i++) {
			if (slots[i] != SUB_NONE && sub_registry[slots[i]].tier == TIER_ELITE)
				return;
		}
	}

	/* Rare limit: max two rares on the skillbar.
	   Auto-equip skips if two rares are already equipped. */
	if (sub_registry[id].tier == TIER_RARE) {
		int rare_count = 0;
		for (int i = 0; i < SKILLBAR_SLOTS; i++) {
			if (slots[i] != SUB_NONE && sub_registry[slots[i]].tier == TIER_RARE)
				rare_count++;
		}
		if (rare_count >= 2)
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

	/* Auto-activate if no other sub of this type is active.
	   Stealth/Gravwell are ability-activated, not toggle-activated — never auto-activate. */
	SubroutineType type = sub_registry[id].type;
	if (type != SUB_TYPE_STEALTH && type != SUB_TYPE_CONTROL && type != SUB_TYPE_AREA_EFFECT && active_sub[type] == SUB_NONE)
		active_sub[type] = id;
}

void Skillbar_equip(int slot, SubroutineId id)
{
	if (slot < 0 || slot >= SKILLBAR_SLOTS)
		return;
	if (id < 0 || id >= SUB_ID_COUNT)
		return;

	SubroutineType new_type = sub_registry[id].type;

	/* Check if the skill being replaced was active for the same type */
	int old_id = slots[slot];
	bool inherit_active = false;
	if (old_id != SUB_NONE && sub_registry[old_id].type == new_type
			&& active_sub[new_type] == old_id)
		inherit_active = true;

	/* Clear any existing slot that has this id to prevent duplicates */
	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		if (slots[i] == id)
			slots[i] = SUB_NONE;
	}

	/* Elite limit: only one elite on the bar. Remove existing elite. */
	if (sub_registry[id].tier == TIER_ELITE) {
		for (int i = 0; i < SKILLBAR_SLOTS; i++) {
			if (slots[i] != SUB_NONE && sub_registry[slots[i]].tier == TIER_ELITE) {
				SubroutineType oldType = sub_registry[slots[i]].type;
				if (active_sub[oldType] == slots[i]) {
					if (oldType == new_type)
						inherit_active = true;
					active_sub[oldType] = SUB_NONE;
				}
				slots[i] = SUB_NONE;
			}
		}
	}

	/* Rare limit: max two rares. Remove oldest if already at 2. */
	if (sub_registry[id].tier == TIER_RARE) {
		int rare_count = 0;
		int oldest_rare_slot = -1;
		for (int i = 0; i < SKILLBAR_SLOTS; i++) {
			if (slots[i] != SUB_NONE && sub_registry[slots[i]].tier == TIER_RARE) {
				rare_count++;
				if (oldest_rare_slot < 0)
					oldest_rare_slot = i;
			}
		}
		if (rare_count >= 2 && oldest_rare_slot >= 0) {
			SubroutineType oldType = sub_registry[slots[oldest_rare_slot]].type;
			if (active_sub[oldType] == slots[oldest_rare_slot]) {
				if (oldType == new_type)
					inherit_active = true;
				active_sub[oldType] = SUB_NONE;
			}
			slots[oldest_rare_slot] = SUB_NONE;
		}
	}

	slots[slot] = id;

	/* Inherit active state from the displaced same-type skill */
	if (inherit_active)
		active_sub[new_type] = id;
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

static void render_icon(SubroutineId id, float cx, float cy, float alpha, float s)
{
	switch (id) {
	case SUB_ID_PEA: {
		Position p = {cx, cy};
		ColorFloat c = {1.0f, 1.0f, 1.0f, alpha};
		Render_point(&p, 8.0 * s, &c);
		break;
	}
	case SUB_ID_MINE: {
		Position p = {cx, cy};
		Rectangle body = {-8.0 * s, 8.0 * s, 8.0 * s, -8.0 * s};
		Rectangle dot = {-3.0 * s, 3.0 * s, 3.0 * s, -3.0 * s};
		ColorFloat gray = {0.2f, 0.2f, 0.2f, alpha};
		ColorFloat red = {1.0f, 0.0f, 0.0f, alpha};
		Render_quad(&p, 45.0, body, &gray);
		Render_quad(&p, 45.0, dot, &red);
		break;
	}
	case SUB_ID_BOOST: {
		float sz = 6.0f * s;
		float t = 1.5f * s;
		Render_thick_line(cx - 5*s, cy - sz, cx + 1*s, cy, t, 1.0f, 0.8f, 0.0f, alpha);
		Render_thick_line(cx + 1*s, cy, cx - 5*s, cy + sz, t, 1.0f, 0.8f, 0.0f, alpha);
		Render_thick_line(cx - 1*s, cy - sz, cx + 5*s, cy, t, 1.0f, 0.8f, 0.0f, alpha);
		Render_thick_line(cx + 5*s, cy, cx - 1*s, cy + sz, t, 1.0f, 0.8f, 0.0f, alpha);
		break;
	}
	case SUB_ID_EGRESS: {
		float sz = 8.0f * s;
		float t = 1.5f * s;
		Render_thick_line(cx, cy - sz, cx, cy + sz, t, 0.4f, 1.0f, 1.0f, alpha);
		Render_thick_line(cx - sz, cy, cx + sz, cy, t, 0.4f, 1.0f, 1.0f, alpha);
		float d = sz * 0.7f;
		Render_thick_line(cx - d, cy - d, cx + d, cy + d, t, 0.4f, 1.0f, 1.0f, alpha);
		Render_thick_line(cx + d, cy - d, cx - d, cy + d, t, 0.4f, 1.0f, 1.0f, alpha);
		break;
	}
	case SUB_ID_MGUN: {
		Position p1 = {cx - 4*s, cy - 4*s};
		Position p2 = {cx, cy};
		Position p3 = {cx + 4*s, cy + 4*s};
		ColorFloat c = {1.0f, 1.0f, 1.0f, alpha};
		Render_point(&p1, 5.0 * s, &c);
		Render_point(&p2, 5.0 * s, &c);
		Render_point(&p3, 5.0 * s, &c);
		break;
	}
	case SUB_ID_MEND: {
		float sz = 7.0f * s;
		float t = 2.0f * s;
		Render_thick_line(cx, cy - sz, cx, cy + sz, t, 0.3f, 0.7f, 1.0f, alpha);
		Render_thick_line(cx - sz, cy, cx + sz, cy, t, 0.3f, 0.7f, 1.0f, alpha);
		break;
	}
	case SUB_ID_AEGIS: {
		float r = 8.0f * s;
		float t = 1.5f * s;
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
		float r = 8.0f * s;
		float t = 1.5f * s;
		Render_thick_line(cx - r, cy, cx - r * 0.4f, cy + r * 0.6f, t,
			0.6f, 0.3f, 0.8f, alpha);
		Render_thick_line(cx - r * 0.4f, cy + r * 0.6f, cx + r * 0.4f, cy + r * 0.6f, t,
			0.6f, 0.3f, 0.8f, alpha);
		Render_thick_line(cx + r * 0.4f, cy + r * 0.6f, cx + r, cy, t,
			0.6f, 0.3f, 0.8f, alpha);
		Render_thick_line(cx - r, cy, cx - r * 0.4f, cy - r * 0.6f, t,
			0.6f, 0.3f, 0.8f, alpha);
		Render_thick_line(cx - r * 0.4f, cy - r * 0.6f, cx + r * 0.4f, cy - r * 0.6f, t,
			0.6f, 0.3f, 0.8f, alpha);
		Render_thick_line(cx + r * 0.4f, cy - r * 0.6f, cx + r, cy, t,
			0.6f, 0.3f, 0.8f, alpha);
		Render_filled_circle(cx, cy, 2.5f * s, 6, 0.6f, 0.3f, 0.8f, alpha);
		break;
	}
	case SUB_ID_INFERNO: {
		float t = 1.5f * s;
		Render_thick_line(cx, cy + 9*s, cx - 5*s, cy - 3*s, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx - 5*s, cy - 3*s, cx - 2*s, cy - 1*s, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx - 2*s, cy - 1*s, cx - 3*s, cy - 7*s, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx - 3*s, cy - 7*s, cx, cy - 3*s, t, 1.0f, 0.6f, 0.1f, alpha);
		Render_thick_line(cx, cy - 3*s, cx + 3*s, cy - 7*s, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx + 3*s, cy - 7*s, cx + 2*s, cy - 1*s, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx + 2*s, cy - 1*s, cx + 5*s, cy - 3*s, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_thick_line(cx + 5*s, cy - 3*s, cx, cy + 9*s, t, 1.0f, 0.5f, 0.0f, alpha);
		Render_filled_circle(cx, cy, 3.0f * s, 6, 1.0f, 1.0f, 0.7f, alpha);
		break;
	}
	case SUB_ID_DISINTEGRATE: {
		float t = 1.5f * s;
		Render_thick_line(cx - 10*s, cy - 6*s, cx - 2*s, cy, t, 0.6f, 0.2f, 0.9f, alpha);
		Render_thick_line(cx - 10*s, cy + 6*s, cx - 2*s, cy, t, 0.6f, 0.2f, 0.9f, alpha);
		Render_filled_circle(cx - 2*s, cy, 2.5f * s, 6, 1.0f, 0.95f, 1.0f, alpha);
		Render_thick_line(cx - 2*s, cy, cx + 10*s, cy, 3.0f * s, 0.8f, 0.5f, 1.0f, alpha * 0.7f);
		Render_thick_line(cx - 2*s, cy, cx + 10*s, cy, 1.5f * s, 1.0f, 0.95f, 1.0f, alpha);
		break;
	}
	case SUB_ID_GRAVWELL: {
		float t = 1.5f * s;
		float r1 = 9.0f * s, r2 = 6.0f * s, r3 = 3.0f * s;
		Render_thick_line(cx + r1, cy, cx, cy + r1, t,
			EDGE_R_ICON, EDGE_G_ICON, EDGE_B_ICON, alpha);
		Render_thick_line(cx, cy + r1, cx - r1, cy, t,
			EDGE_R_ICON, EDGE_G_ICON, EDGE_B_ICON, alpha);
		Render_thick_line(cx - r2, cy, cx, cy - r2, t,
			MID_R_ICON, MID_G_ICON, MID_B_ICON, alpha);
		Render_thick_line(cx, cy - r2, cx + r2, cy, t,
			MID_R_ICON, MID_G_ICON, MID_B_ICON, alpha);
		Render_filled_circle(cx, cy, r3, 6,
			0.1f, 0.1f, 0.2f, alpha);
		break;
	}
	case SUB_ID_TGUN: {
		float gap = 5.0f * s;
		ColorFloat c = {1.0f, 1.0f, 1.0f, alpha};
		Position l1 = {cx - gap, cy - 5*s};
		Position l2 = {cx - gap, cy};
		Position l3 = {cx - gap, cy + 5*s};
		Render_point(&l1, 4.0 * s, &c);
		Render_point(&l2, 4.0 * s, &c);
		Render_point(&l3, 4.0 * s, &c);
		Position r1 = {cx + gap, cy - 5*s};
		Position r2 = {cx + gap, cy};
		Position r3 = {cx + gap, cy + 5*s};
		Render_point(&r1, 4.0 * s, &c);
		Render_point(&r2, 4.0 * s, &c);
		Render_point(&r3, 4.0 * s, &c);
		break;
	}
	case SUB_ID_SPRINT: {
		float sz = 7.0f * s;
		float t = 1.5f * s;
		Render_thick_line(cx - 4*s, cy - sz, cx + 4*s, cy, t, 1.0f, 1.0f, 1.0f, alpha);
		Render_thick_line(cx + 4*s, cy, cx - 4*s, cy + sz, t, 1.0f, 1.0f, 1.0f, alpha);
		break;
	}
	case SUB_ID_EMP: {
		float t = 1.5f * s;
		Render_filled_circle(cx, cy, 3.0f * s, 8, 0.3f, 0.7f, 1.0f, alpha);
		for (int ring = 0; ring < 2; ring++) {
			float r = (6.0f + ring * 4.0f) * s;
			float a = alpha * (1.0f - ring * 0.3f);
			for (int seg = 0; seg < 8; seg++) {
				float a0 = seg * 45.0f * (float)M_PI / 180.0f;
				float a1 = (seg + 1) * 45.0f * (float)M_PI / 180.0f;
				Render_thick_line(cx + cosf(a0) * r, cy + sinf(a0) * r,
					cx + cosf(a1) * r, cy + sinf(a1) * r,
					t, 0.3f, 0.7f, 1.0f, a);
			}
		}
		break;
	}
	case SUB_ID_RESIST: {
		float r = 8.0f * s;
		float t = 1.5f * s;
		for (int i = 0; i < 6; i++) {
			float a0 = i * 60.0f * (float)M_PI / 180.0f;
			float a1 = (i + 1) * 60.0f * (float)M_PI / 180.0f;
			Render_thick_line(cx + cosf(a0) * r, cy + sinf(a0) * r,
				cx + cosf(a1) * r, cy + sinf(a1) * r,
				t, 1.0f, 0.7f, 0.2f, alpha);
		}
		Render_filled_circle(cx, cy, 3.0f * s, 6, 1.0f, 0.7f, 0.2f, alpha * 0.5f);
		break;
	}
	case SUB_ID_EMBER: {
		Position p1 = {cx - 4*s, cy - 4*s};
		Position p2 = {cx, cy};
		Position p3 = {cx + 4*s, cy + 4*s};
		ColorFloat ec = {1.0f, 0.5f, 0.1f, alpha};
		Render_point(&p1, 5.0f * s, &ec);
		Render_point(&p2, 5.0f * s, &ec);
		Render_point(&p3, 5.0f * s, &ec);
		break;
	}
	case SUB_ID_FLAK: {
		float t = 1.5f * s;
		ColorFloat fc = {1.0f, 0.5f, 0.1f, alpha};
		Render_thick_line(cx, cy + 4*s, cx, cy - 7*s, t, 1.0f, 0.5f, 0.1f, alpha);
		Render_thick_line(cx - 2*s, cy + 4*s, cx - 5*s, cy - 6*s, t, 1.0f, 0.4f, 0.0f, alpha);
		Render_thick_line(cx + 2*s, cy + 4*s, cx + 5*s, cy - 6*s, t, 1.0f, 0.4f, 0.0f, alpha);
		Position mp = {cx, cy + 4*s};
		Render_point(&mp, 3.0f * s, &fc);
		break;
	}
	case SUB_ID_BLAZE: {
		float sz = 8.0f * s;
		float t = 1.5f * s;
		Render_thick_line(cx, cy - sz, cx, cy + sz, t, 1.0f, 0.5f, 0.1f, alpha);
		Render_thick_line(cx - sz, cy, cx + sz, cy, t, 1.0f, 0.5f, 0.1f, alpha);
		break;
	}
	case SUB_ID_SCORCH: {
		float sz = 7.0f * s;
		float t = 1.5f * s;
		Render_thick_line(cx - 4*s, cy - sz, cx + 4*s, cy, t, 1.0f, 0.5f, 0.1f, alpha);
		Render_thick_line(cx + 4*s, cy, cx - 4*s, cy + sz, t, 1.0f, 0.5f, 0.1f, alpha);
		break;
	}
	case SUB_ID_CINDER: {
		Position p = {cx, cy};
		Rectangle body = {-8.0 * s, 8.0 * s, 8.0 * s, -8.0 * s};
		Rectangle dot = {-3.0 * s, 3.0 * s, 3.0 * s, -3.0 * s};
		ColorFloat dark_orange = {0.6f, 0.3f, 0.05f, alpha};
		ColorFloat bright_orange = {1.0f, 0.5f, 0.1f, alpha};
		Render_quad(&p, 45.0, body, &dark_orange);
		Render_quad(&p, 45.0, dot, &bright_orange);
		break;
	}
	case SUB_ID_CAUTERIZE: {
		float sz = 7.0f * s;
		float t = 2.0f * s;
		Render_thick_line(cx - sz, cy, cx + sz, cy, t, 1.0f, 0.5f, 0.1f, alpha);
		Render_thick_line(cx, cy - sz, cx, cy + sz, t, 1.0f, 0.5f, 0.1f, alpha);
		break;
	}
	case SUB_ID_IMMOLATE: {
		float r = 8.0f * s;
		float t = 1.5f * s;
		for (int i = 0; i < 6; i++) {
			float a0 = i * 60.0f * (float)M_PI / 180.0f;
			float a1 = (i + 1) * 60.0f * (float)M_PI / 180.0f;
			Render_thick_line(cx + cosf(a0) * r, cy + sinf(a0) * r,
				cx + cosf(a1) * r, cy + sinf(a1) * r,
				t, 1.0f, 0.5f, 0.1f, alpha);
		}
		break;
	}
	case SUB_ID_SMOLDER: {
		float r = 8.0f * s;
		float t = 1.5f * s;
		Render_thick_line(cx - r, cy, cx, cy - r * 0.5f, t, 1.0f, 0.5f, 0.1f, alpha);
		Render_thick_line(cx, cy - r * 0.5f, cx + r, cy, t, 1.0f, 0.5f, 0.1f, alpha);
		Render_thick_line(cx - r, cy, cx, cy + r * 0.5f, t, 1.0f, 0.5f, 0.1f, alpha);
		Render_thick_line(cx, cy + r * 0.5f, cx + r, cy, t, 1.0f, 0.5f, 0.1f, alpha);
		break;
	}
	case SUB_ID_HEATWAVE: {
		float t = 1.5f * s;
		Render_filled_circle(cx, cy, 3.0f * s, 8, 1.0f, 0.5f, 0.1f, alpha);
		for (int ring = 0; ring < 2; ring++) {
			float r = (6.0f + ring * 4.0f) * s;
			float a = alpha * (1.0f - ring * 0.3f);
			for (int seg = 0; seg < 8; seg++) {
				float a0 = seg * 45.0f * (float)M_PI / 180.0f;
				float a1 = (seg + 1) * 45.0f * (float)M_PI / 180.0f;
				Render_thick_line(cx + cosf(a0) * r, cy + sinf(a0) * r,
					cx + cosf(a1) * r, cy + sinf(a1) * r,
					t, 1.0f, 0.5f, 0.1f, a);
			}
		}
		break;
	}
	case SUB_ID_TEMPER: {
		float r = 8.0f * s;
		float t = 1.5f * s;
		for (int i = 0; i < 6; i++) {
			float a0 = i * 60.0f * (float)M_PI / 180.0f;
			float a1 = (i + 1) * 60.0f * (float)M_PI / 180.0f;
			Render_thick_line(cx + cosf(a0) * r, cy + sinf(a0) * r,
				cx + cosf(a1) * r, cy + sinf(a1) * r,
				t, 1.0f, 0.5f, 0.1f, alpha);
		}
		Render_filled_circle(cx, cy, 3.0f * s, 5, 1.0f, 0.3f, 0.0f, alpha);
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
	case SUB_ID_DISINTEGRATE: return Sub_Disintegrate_get_cooldown_fraction();
	case SUB_ID_GRAVWELL: return Sub_Gravwell_get_cooldown_fraction();
	case SUB_ID_TGUN: return Sub_Tgun_get_cooldown_fraction();
	case SUB_ID_FLAK: return Sub_Flak_get_cooldown_fraction();
	case SUB_ID_EMBER: return Sub_Ember_get_cooldown_fraction();
	case SUB_ID_SPRINT: return Sub_Sprint_get_cooldown_fraction();
	case SUB_ID_EMP: return Sub_Emp_get_cooldown_fraction();
	case SUB_ID_RESIST: return Sub_Resist_get_cooldown_fraction();
	case SUB_ID_BLAZE: return Sub_Blaze_get_cooldown_fraction();
	case SUB_ID_CAUTERIZE: return Sub_Cauterize_get_cooldown_fraction();
	case SUB_ID_IMMOLATE: return Sub_Immolate_get_cooldown_fraction();
	case SUB_ID_CINDER: return Sub_Cinder_get_cooldown_fraction();
	case SUB_ID_SCORCH: return Sub_Scorch_get_cooldown_fraction();
	case SUB_ID_SMOLDER: return Sub_Smolder_get_cooldown_fraction();
	case SUB_ID_HEATWAVE: return Sub_Heatwave_get_cooldown_fraction();
	case SUB_ID_TEMPER: return Sub_Temper_get_cooldown_fraction();
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
	float s = Graphics_get_ui_scale();
	float slot_size = SLOT_SIZE * s;
	float slot_spacing = SLOT_SPACING * s;
	float slot_margin = SLOT_MARGIN * s;
	float base_x = slot_margin;
	float base_y = (float)screen->height - slot_size - slot_margin;

	for (int i = 0; i < SKILLBAR_SLOTS; i++) {
		float bx = base_x + i * slot_spacing;
		float by = base_y;
		if (x >= bx && x <= bx + slot_size && y >= by && y <= by + slot_size)
			return i;
	}
	return -1;
}

void Skillbar_render_icon_at(SubroutineId id, float cx, float cy, float alpha)
{
	render_icon(id, cx, cy, alpha, Graphics_get_ui_scale());
}

SubroutineTier Skillbar_get_tier(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT)
		return sub_registry[id].tier;
	return TIER_NORMAL;
}

bool Skillbar_is_elite(SubroutineId id)
{
	return Skillbar_get_tier(id) == TIER_ELITE;
}

bool Skillbar_is_rare(SubroutineId id)
{
	return Skillbar_get_tier(id) == TIER_RARE;
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
