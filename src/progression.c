#include "progression.h"

#include "fragment.h"
#include "graphics.h"
#include "text.h"
#include "mat4.h"
#include "audio.h"

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
} ProgressionEntry;

static ProgressionEntry entries[SUB_ID_COUNT] = {
	[SUB_ID_MINE] = { "MINES", "sub_mine", FRAG_TYPE_MINE, 5, false },
};

/* Notification state */
static bool notifyActive = false;
static unsigned int notifyTimer = 0;
static char notifyText[64];

static Mix_Chunk *unlockSample = 0;

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
	for (int i = 0; i < SUB_ID_COUNT; i++)
		entries[i].unlocked = false;

	notifyActive = false;
	notifyTimer = 0;

	Audio_load_sample(&unlockSample, "resources/sounds/statue_rise.wav");
}

void Progression_cleanup(void)
{
	Audio_unload_sample(&unlockSample);
}

void Progression_update(unsigned int ticks)
{
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (entries[i].unlocked)
			continue;

		int count = Fragment_get_count(entries[i].frag_type);
		if (count >= entries[i].threshold) {
			entries[i].unlocked = true;

			snprintf(notifyText, sizeof(notifyText),
				">> %s UNLOCKED <<", entries[i].sub_name);
			notifyActive = true;
			notifyTimer = 0;

			Audio_play_sample(&unlockSample);
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

	/* Upper-left progress counter for each entry */
	float x = 10.0f;
	float y = 40.0f;

	for (int i = 0; i < SUB_ID_COUNT; i++) {
		char buf[64];
		if (entries[i].unlocked)
			snprintf(buf, sizeof(buf), "%s: UNLOCKED", entries[i].name);
		else
			snprintf(buf, sizeof(buf), "%s: %d/%d",
				entries[i].name,
				Fragment_get_count(entries[i].frag_type),
				entries[i].threshold);

		/* Magenta for unlocked, white for in-progress */
		float r = entries[i].unlocked ? 1.0f : 1.0f;
		float g = entries[i].unlocked ? 0.0f : 1.0f;
		float b = entries[i].unlocked ? 1.0f : 1.0f;

		Text_render(tr, shaders, &proj, &ident,
			buf, x, y,
			r, g, b, 0.9f);

		y += 20.0f;
	}

	/* Centered fading unlock notification */
	if (notifyActive) {
		float alpha = 1.0f;
		unsigned int remaining = NOTIFY_DURATION_MS - notifyTimer;
		if (remaining < NOTIFY_FADE_MS)
			alpha = (float)remaining / NOTIFY_FADE_MS;

		float tw = text_width(tr, notifyText);
		float nx = (float)screen->width * 0.5f - tw * 0.5f;
		float ny = (float)screen->height * 0.3f;

		Text_render(tr, shaders, &proj, &ident,
			notifyText, nx, ny,
			1.0f, 0.0f, 1.0f, alpha);
	}
}

bool Progression_is_unlocked(SubroutineId id)
{
	if (id >= 0 && id < SUB_ID_COUNT)
		return entries[id].unlocked;
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
