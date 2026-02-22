#include "player_stats.h"

#include "sub_aegis.h"
#include "render.h"
#include "text.h"
#include "graphics.h"
#include "mat4.h"
#include "audio.h"

#include <SDL2/SDL_mixer.h>
#include <stdio.h>

/* --- Tuning constants --- */
#define INTEGRITY_MAX       100.0
#define INTEGRITY_REGEN     5.0     /* HP/sec */
#define REGEN_DELAY_MS      2000    /* ms after last damage before regen starts */

#define FEEDBACK_MAX        100.0
#define FEEDBACK_DECAY      15.0    /* units/sec */
#define FEEDBACK_GRACE_MS   500     /* ms after last feedback before decay starts */

#define DAMAGE_FEEDBACK_RATIO 0.5    /* damage taken → feedback (no spillover) */

#define FLASH_DURATION      200     /* ms for red border flash */
#define FLASH_THICKNESS     4.0f

/* --- Bar rendering constants --- */
#define MARGIN_X     10.0f
#define MARGIN_Y     10.0f
#define COL_GAP      8.0f
#define BAR_WIDTH    160.0f
#define BAR_HEIGHT   16.0f
#define BAR_GAP      10.0f
#define BORDER_COLOR 0.3f

/* --- State --- */
static double integrity;
static double feedback;
static unsigned int timeSinceLastDamage;   /* ms since last integrity damage */
static unsigned int timeSinceLastFeedback; /* ms since last feedback added */
static bool dead;

static int flashTicksLeft;
static unsigned int feedbackFlashTimer; /* running timer for 4Hz feedback-full blink */
static bool shielded;
static int shieldBreakGrace;
static int iframeMs;
static int regenBoostMs;
static double regenBoostMultiplier;
static Mix_Chunk *sampleHurt = 0;

void PlayerStats_initialize(void)
{
	integrity = INTEGRITY_MAX;
	feedback = 0.0;
	timeSinceLastDamage = REGEN_DELAY_MS; /* can regen immediately */
	timeSinceLastFeedback = FEEDBACK_GRACE_MS;
	dead = false;
	flashTicksLeft = 0;
	feedbackFlashTimer = 0;
	shielded = false;
	shieldBreakGrace = 0;
	iframeMs = 0;
	regenBoostMs = 0;
	regenBoostMultiplier = 1.0;

	Audio_load_sample(&sampleHurt, "resources/sounds/samus_hurt.wav");
}

void PlayerStats_cleanup(void)
{
	Audio_unload_sample(&sampleHurt);
}

void PlayerStats_update(unsigned int ticks)
{
	if (dead)
		return;

	double dt = ticks / 1000.0;

	/* Advance timers */
	timeSinceLastDamage += ticks;
	if (timeSinceLastDamage > 10000) timeSinceLastDamage = 10000;
	timeSinceLastFeedback += ticks;
	if (timeSinceLastFeedback > 10000) timeSinceLastFeedback = 10000;

	/* Shield break grace decay */
	if (shieldBreakGrace > 0)
		shieldBreakGrace -= ticks;

	/* I-frame decay */
	if (iframeMs > 0) {
		iframeMs -= (int)ticks;
		if (iframeMs < 0) iframeMs = 0;
	}

	/* Regen boost decay */
	if (regenBoostMs > 0) {
		regenBoostMs -= (int)ticks;
		if (regenBoostMs <= 0) {
			regenBoostMs = 0;
			regenBoostMultiplier = 1.0;
		}
	}

	/* Flash decay */
	if (flashTicksLeft > 0) {
		flashTicksLeft -= ticks;
		if (flashTicksLeft < 0) flashTicksLeft = 0;
	}

	/* Feedback full blink timer (4Hz = 250ms period) */
	if (feedback >= FEEDBACK_MAX)
		feedbackFlashTimer += ticks;
	else
		feedbackFlashTimer = 0;

	/* Feedback decay after grace period */
	if (timeSinceLastFeedback >= FEEDBACK_GRACE_MS) {
		feedback -= FEEDBACK_DECAY * dt;
		if (feedback < 0.0)
			feedback = 0.0;
	}

	/* Integrity regen after delay — 2x rate when feedback is empty, boosted by mend */
	if (timeSinceLastDamage >= REGEN_DELAY_MS && integrity < INTEGRITY_MAX) {
		double rate = (feedback <= 0.0) ? INTEGRITY_REGEN * 2.0 : INTEGRITY_REGEN;
		rate *= regenBoostMultiplier;
		integrity += rate * dt;
		if (integrity > INTEGRITY_MAX)
			integrity = INTEGRITY_MAX;
	}

	/* Death check */
	if (integrity <= 0.0) {
		integrity = 0.0;
		dead = true;
	}
}

static void render_bar_border(float x, float y, float w, float h)
{
	Render_thick_line(x, y, x + w, y,
		1.0f, BORDER_COLOR, BORDER_COLOR, BORDER_COLOR, 0.8f);
	Render_thick_line(x, y + h, x + w, y + h,
		1.0f, BORDER_COLOR, BORDER_COLOR, BORDER_COLOR, 0.8f);
	Render_thick_line(x, y, x, y + h,
		1.0f, BORDER_COLOR, BORDER_COLOR, BORDER_COLOR, 0.8f);
	Render_thick_line(x + w, y, x + w, y + h,
		1.0f, BORDER_COLOR, BORDER_COLOR, BORDER_COLOR, 0.8f);
}

void PlayerStats_render(const Screen *screen)
{
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	float intFrac = (float)(integrity / INTEGRITY_MAX);
	float fbFrac = (float)(feedback / FEEDBACK_MAX);

	/* Compute column layout from actual font metrics so nothing overlaps */
	float labelW = Text_measure_width(tr, "INTEGRITY"); /* widest label */
	float valueW = Text_measure_width(tr, "100");        /* widest value */
	float valueCol = MARGIN_X + labelW + COL_GAP;  /* value column left edge */
	float barX = valueCol + valueW + COL_GAP;       /* bar left edge */

	float textY_off = BAR_HEIGHT * 0.5f + tr->font_size * 0.15f + 1.0f;

	/* --- Damage flash: red border around entire screen --- */
	if (flashTicksLeft > 0) {
		float fade = (float)flashTicksLeft / FLASH_DURATION;
		float sw = (float)screen->width;
		float sh = (float)screen->height;
		float t = FLASH_THICKNESS;
		/* Top */
		Render_quad_absolute(0, 0, sw, t, 1.0f, 0.0f, 0.0f, fade);
		/* Bottom */
		Render_quad_absolute(0, sh - t, sw, sh, 1.0f, 0.0f, 0.0f, fade);
		/* Left */
		Render_quad_absolute(0, t, t, sh - t, 1.0f, 0.0f, 0.0f, fade);
		/* Right */
		Render_quad_absolute(sw - t, t, sw, sh - t, 1.0f, 0.0f, 0.0f, fade);
	}

	/* --- Integrity bar --- */
	float iy = MARGIN_Y;

	Render_quad_absolute(barX, iy, barX + BAR_WIDTH, iy + BAR_HEIGHT,
		0.1f, 0.1f, 0.1f, 0.8f);

	/* Fill: green -> yellow -> red */
	float ir, ig, ib;
	if (intFrac > 0.5f) {
		float t = (intFrac - 0.5f) * 2.0f;
		ir = 1.0f - t;
		ig = 0.8f;
		ib = 0.0f;
	} else {
		float t = intFrac * 2.0f;
		ir = 1.0f;
		ig = 0.8f * t;
		ib = 0.0f;
	}
	float fillWidth = BAR_WIDTH * intFrac;
	if (fillWidth > 0.0f) {
		Render_quad_absolute(barX, iy, barX + fillWidth, iy + BAR_HEIGHT,
			ir, ig, ib, 0.9f);
	}

	render_bar_border(barX, iy, BAR_WIDTH, BAR_HEIGHT);

	/* --- Feedback bar --- */
	float fy = iy + BAR_HEIGHT + BAR_GAP;

	Render_quad_absolute(barX, fy, barX + BAR_WIDTH, fy + BAR_HEIGHT,
		0.1f, 0.1f, 0.1f, 0.8f);

	/* Fill: cyan -> yellow -> magenta */
	float fr, fg, fb;
	if (fbFrac < 0.5f) {
		float t = fbFrac * 2.0f;
		fr = t;
		fg = 0.8f;
		fb = 0.8f * (1.0f - t);
	} else {
		float t = (fbFrac - 0.5f) * 2.0f;
		fr = 1.0f;
		fg = 0.8f * (1.0f - t);
		fb = 0.8f * t;
	}
	float fbFillWidth = BAR_WIDTH * fbFrac;
	/* 4Hz blink when feedback is full: 250ms period, visible first 125ms */
	bool fbFillVisible = (feedback < FEEDBACK_MAX) || ((feedbackFlashTimer % 250) < 125);
	if (fbFillWidth > 0.0f && fbFillVisible) {
		Render_quad_absolute(barX, fy, barX + fbFillWidth, fy + BAR_HEIGHT,
			fr, fg, fb, 0.9f);
	}

	render_bar_border(barX, fy, BAR_WIDTH, BAR_HEIGHT);

	/* --- Flush geometry, then render all text --- */
	Render_flush(&proj, &ident);

	/* Integrity: label + right-aligned value */
	Text_render(tr, shaders, &proj, &ident,
		"INTEGRITY", MARGIN_X, iy + textY_off,
		0.5f, 0.5f, 0.5f, 0.8f);

	char intBuf[16];
	snprintf(intBuf, sizeof(intBuf), "%.0f", integrity);
	float intValW = Text_measure_width(tr, intBuf);
	Text_render(tr, shaders, &proj, &ident,
		intBuf, valueCol + valueW - intValW, iy + textY_off,
		1.0f, 1.0f, 1.0f, 0.9f);

	/* Feedback: label + right-aligned value */
	Text_render(tr, shaders, &proj, &ident,
		"FEEDBACK", MARGIN_X, fy + textY_off,
		0.5f, 0.5f, 0.5f, 0.8f);

	char fbBuf[16];
	snprintf(fbBuf, sizeof(fbBuf), "%.0f", feedback);
	float fbValW = Text_measure_width(tr, fbBuf);
	Text_render(tr, shaders, &proj, &ident,
		fbBuf, valueCol + valueW - fbValW, fy + textY_off,
		1.0f, 1.0f, 1.0f, 0.9f);
}

void PlayerStats_add_feedback(double amount)
{
	feedback += amount;

	/* Any feedback past max spills over as integrity damage */
	if (feedback > FEEDBACK_MAX) {
		double spillover = feedback - FEEDBACK_MAX;
		feedback = FEEDBACK_MAX;
		integrity -= spillover;
		if (integrity < 0.0)
			integrity = 0.0;
		timeSinceLastDamage = 0;
		flashTicksLeft = FLASH_DURATION;
		Audio_play_sample(&sampleHurt);
	}

	timeSinceLastFeedback = 0;
}

void PlayerStats_damage(double amount)
{
	if (shielded || iframeMs > 0) {
		if (shielded)
			Sub_Aegis_on_hit();
		return;
	}
	integrity -= amount;
	if (integrity < 0.0)
		integrity = 0.0;
	timeSinceLastDamage = 0;
	flashTicksLeft = FLASH_DURATION;
	Audio_play_sample(&sampleHurt);

	/* Combat feedback — taking damage generates feedback (no spillover) */
	feedback += amount * DAMAGE_FEEDBACK_RATIO;
	if (feedback > FEEDBACK_MAX)
		feedback = FEEDBACK_MAX;
	timeSinceLastFeedback = 0;
}

void PlayerStats_force_kill(void)
{
	if (iframeMs > 0)
		return; /* Dashing through — invulnerable */
	if (shielded) {
		shielded = false;  /* Mine breaks shield, no damage */
		shieldBreakGrace = 200; /* Outlasts mine explosion (100ms) */
		return;
	}
	if (shieldBreakGrace > 0)
		return; /* Still invulnerable from recent shield break */
	integrity = 0.0;
	timeSinceLastDamage = 0;
}

void PlayerStats_heal(double amount)
{
	integrity += amount;
	if (integrity > INTEGRITY_MAX)
		integrity = INTEGRITY_MAX;
}

bool PlayerStats_is_shielded(void)
{
	return shielded;
}

void PlayerStats_set_shielded(bool value)
{
	shielded = value;
}

void PlayerStats_set_iframes(int ms)
{
	iframeMs = ms;
}

bool PlayerStats_has_iframes(void)
{
	return iframeMs > 0;
}

void PlayerStats_boost_regen(int duration_ms, double multiplier)
{
	timeSinceLastDamage = REGEN_DELAY_MS; /* activate regen immediately */
	regenBoostMs = duration_ms;
	regenBoostMultiplier = multiplier;
}

bool PlayerStats_is_dead(void)
{
	return dead;
}

double PlayerStats_get_feedback(void)
{
	return feedback;
}

void PlayerStats_reset(void)
{
	integrity = INTEGRITY_MAX;
	feedback = 0.0;
	timeSinceLastDamage = REGEN_DELAY_MS;
	timeSinceLastFeedback = FEEDBACK_GRACE_MS;
	dead = false;
	flashTicksLeft = 0;
	feedbackFlashTimer = 0;
	shielded = false;
	shieldBreakGrace = 0;
	iframeMs = 0;
	regenBoostMs = 0;
	regenBoostMultiplier = 1.0;
}

PlayerStatsSnapshot PlayerStats_snapshot(void)
{
	PlayerStatsSnapshot snap;
	snap.integrity = integrity;
	snap.feedback = feedback;
	snap.shielded = shielded;
	return snap;
}

void PlayerStats_restore(PlayerStatsSnapshot snap)
{
	integrity = snap.integrity;
	feedback = snap.feedback;
	timeSinceLastDamage = REGEN_DELAY_MS;
	timeSinceLastFeedback = FEEDBACK_GRACE_MS;
	dead = false;
	flashTicksLeft = 0;
	feedbackFlashTimer = 0;
	shielded = snap.shielded;
	shieldBreakGrace = 0;
}
