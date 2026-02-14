#include "progression.h"

#include "fragment.h"
#include "graphics.h"
#include "text.h"
#include "mat4.h"
#include "skillbar.h"

#include <stdio.h>
#include <string.h>

#define NOTIFY_DURATION_MS 4000
#define NOTIFY_FADE_MS 1500

typedef struct {
	const char *name;
	const char *sub_name;
	FragmentType frag_type;
	int threshold;
	bool unlocked;
	bool discovered;
} ProgressionEntry;

static ProgressionEntry entries[SUB_ID_COUNT] = {
	[SUB_ID_PEA]    = { "PEA",    "sub_pea",    FRAG_TYPE_MINE, 0, false },
	[SUB_ID_MINE]   = { "MINES",  "sub_mine",   FRAG_TYPE_MINE, 5, false },
	[SUB_ID_BOOST]  = { "BOOST",  "sub_boost",  FRAG_TYPE_ELITE, 1, false },
	[SUB_ID_EGRESS] = { "EGRESS", "sub_egress", FRAG_TYPE_MINE, 0, false },
	[SUB_ID_MGUN]   = { "MGUN",   "sub_mgun",   FRAG_TYPE_HUNTER, 3, false },
};

/* Notification state */
static bool notifyActive = false;
static bool notifyElite = false;
static unsigned int notifyTimer = 0;
static char notifyText[64];


static float text_width(TextRenderer *tr, const char *text)
{
	float w = 0.0f;
	for (int i = 0; text[i]; i++) {
		int ch = (unsigned char)text[i];
		if (ch < 32 || ch > 127)
			continue;
		w += tr->char_data[ch - 32][6];
	}
	return w;
}

void Progression_initialize(void)
{
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		entries[i].unlocked = (entries[i].threshold == 0);
		entries[i].discovered = entries[i].unlocked;
	}

	notifyActive = false;
	notifyElite = false;
	notifyTimer = 0;

}

void Progression_cleanup(void)
{
}

void Progression_update(unsigned int ticks)
{
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (entries[i].unlocked)
			continue;

		int count = Fragment_get_count(entries[i].frag_type);

		/* First fragment — discovery notification */
		if (!entries[i].discovered && count >= 1) {
			entries[i].discovered = true;
			notifyElite = false;

			const char *type_names[] = {
				"Projectile", "Deployable", "Movement", "Shield", "Healing"
			};
			SubroutineType type = Skillbar_get_sub_type(i);
			snprintf(notifyText, sizeof(notifyText),
				">> New %s skill discovered <<", type_names[type]);
			notifyActive = true;
			notifyTimer = 0;
		}

		if (count >= entries[i].threshold) {
			entries[i].unlocked = true;

			notifyElite = Skillbar_is_elite(i);
			if (notifyElite)
				snprintf(notifyText, sizeof(notifyText),
					">> %s ", entries[i].sub_name);
			else
				snprintf(notifyText, sizeof(notifyText),
					">> %s UNLOCKED <<", entries[i].sub_name);
			notifyActive = true;
			notifyTimer = 0;

			Skillbar_auto_equip(i);
		}
	}

	if (notifyActive) {
		notifyTimer += ticks;
		if (notifyTimer >= NOTIFY_DURATION_MS)
			notifyActive = false;
	}
}

void Progression_render(const Screen *screen)
{
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	/* Centered fading unlock notification */
	if (notifyActive) {
		float alpha = 1.0f;
		unsigned int remaining = NOTIFY_DURATION_MS - notifyTimer;
		if (remaining < NOTIFY_FADE_MS)
			alpha = (float)remaining / NOTIFY_FADE_MS;

		float tw = text_width(tr, notifyText);
		if (notifyElite)
			tw += text_width(tr, "ELITE ") + text_width(tr, "UNLOCKED <<");
		float nx = (float)screen->width * 0.5f - tw * 0.5f;
		float ny = (float)screen->height * 0.3f;

		Text_render(tr, shaders, &proj, &ident,
			notifyText, nx, ny,
			1.0f, 0.0f, 1.0f, alpha);

		if (notifyElite) {
			float cx = nx + text_width(tr, notifyText);
			Text_render(tr, shaders, &proj, &ident,
				"ELITE ", cx, ny,
				1.0f, 0.84f, 0.0f, alpha);
			cx += text_width(tr, "ELITE ");
			Text_render(tr, shaders, &proj, &ident,
				"UNLOCKED <<", cx, ny,
				1.0f, 0.0f, 1.0f, alpha);
		}
	}
}

void Progression_restore(const bool *unlocked, const bool *discovered)
{
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		entries[i].unlocked = unlocked[i];
		entries[i].discovered = discovered[i];
	}
}

bool Progression_is_unlocked(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT)
		return entries[id].unlocked;
	return false;
}

bool Progression_is_discovered(SubroutineId id)
{
	if (id < 0 || id >= SUB_ID_COUNT)
		return false;
	if (entries[id].unlocked)
		return true;
	if (entries[id].threshold == 0)
		return true;
	if (Fragment_get_count(entries[id].frag_type) >= 1)
		return true;
	return false;
}

int Progression_get_current(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT)
		return Fragment_get_count(entries[id].frag_type);
	return 0;
}

int Progression_get_threshold(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT)
		return entries[id].threshold;
	return 0;
}

float Progression_get_progress(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT) {
		if (entries[id].unlocked)
			return 1.0f;
		float p = (float)Fragment_get_count(entries[id].frag_type)
			/ entries[id].threshold;
		return p > 1.0f ? 1.0f : p;
	}
	return 0.0f;
}

const char *Progression_get_name(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT)
		return entries[id].name;
	return "";
}

bool Progression_would_complete(FragmentType frag_type, int new_count)
{
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (entries[i].unlocked)
			continue;
		if (entries[i].frag_type == frag_type &&
		    entries[i].threshold > 0 &&
		    new_count >= entries[i].threshold)
			return true;
	}
	return false;
}
