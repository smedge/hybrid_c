#include "sub_cauterize_core.h"
#include "render.h"
#include "audio.h"

#include <string.h>

/* --- Config singletons --- */

static const SubCauterizeConfig playerConfig = {
	.heal_amount = 30.0,
	.cooldown_ms = 8000,
	.immunity_duration_ms = 3000,
	.cleanse_radius = 100.0,
	.aura_radius = 80.0,
	.aura_duration_ms = 3000,
	.aura_burn_interval_ms = 500,
	.beam_duration_ms = 0,       /* player self-heal — no beam */
	.beam_thickness = 0.0f,
	.beam_color_r = 0.0f, .beam_color_g = 0.0f, .beam_color_b = 0.0f,
	.bloom_beam_thickness = 0.0f,
};

static const SubCauterizeConfig defenderConfig = {
	.heal_amount = 30.0,
	.cooldown_ms = 8000,
	.immunity_duration_ms = 3000,
	.cleanse_radius = 100.0,
	.aura_radius = 80.0,
	.aura_duration_ms = 3000,
	.aura_burn_interval_ms = 500,
	.beam_duration_ms = 200,
	.beam_thickness = 2.5f,
	.beam_color_r = 1.0f, .beam_color_g = 0.5f, .beam_color_b = 0.1f,
	.bloom_beam_thickness = 3.0f,
};

const SubCauterizeConfig *SubCauterize_get_config(void)
{
	return &playerConfig;
}

const SubCauterizeConfig *SubCauterize_get_defender_config(void)
{
	return &defenderConfig;
}

/* --- Lifecycle --- */

void SubCauterize_init(SubCauterizeCore *core)
{
	SubHeal_init(&core->heal);
	for (int i = 0; i < CAUTERIZE_AURA_MAX; i++) {
		core->auras[i].active = false;
		Burn_reset(&core->auras[i].burn);
	}
}

bool SubCauterize_is_ready(const SubCauterizeCore *core)
{
	return SubHeal_is_ready(&core->heal);
}

float SubCauterize_get_cooldown_fraction(const SubCauterizeCore *core)
{
	/* Use player config for cooldown display */
	static const SubHealConfig healCfg = {
		.cooldown_ms = 8000,
	};
	return SubHeal_get_cooldown_fraction(&core->heal, &healCfg);
}

static void spawn_aura(SubCauterizeCore *core, Position pos, int duration_ms)
{
	for (int i = 0; i < CAUTERIZE_AURA_MAX; i++) {
		if (!core->auras[i].active) {
			core->auras[i].active = true;
			core->auras[i].position = pos;
			core->auras[i].life_ms = duration_ms;
			core->auras[i].burn_tick_ms = 0;
			Burn_reset(&core->auras[i].burn);
			Burn_apply(&core->auras[i].burn, duration_ms);
			return;
		}
	}
}

bool SubCauterize_try_activate(SubCauterizeCore *core, const SubCauterizeConfig *cfg,
	Position origin, Position target)
{
	if (core->heal.cooldownMs > 0)
		return false;

	/* Build a SubHealConfig from cauterize config for the beam */
	SubHealConfig healCfg = {
		.heal_amount = cfg->heal_amount,
		.cooldown_ms = cfg->cooldown_ms,
		.beam_duration_ms = cfg->beam_duration_ms,
		.beam_thickness = cfg->beam_thickness,
		.beam_color_r = cfg->beam_color_r,
		.beam_color_g = cfg->beam_color_g,
		.beam_color_b = cfg->beam_color_b,
		.bloom_beam_thickness = cfg->bloom_beam_thickness,
	};

	if (!SubHeal_try_activate(&core->heal, &healCfg, origin, target))
		return false;

	/* Spawn burn aura at target position */
	spawn_aura(core, target, cfg->aura_duration_ms);
	return true;
}

/* --- Update --- */

void SubCauterize_update(SubCauterizeCore *core, const SubCauterizeConfig *cfg, unsigned int ticks)
{
	/* Tick heal cooldown + beam */
	SubHealConfig healCfg = {
		.cooldown_ms = cfg->cooldown_ms,
		.beam_duration_ms = cfg->beam_duration_ms,
	};
	SubHeal_update(&core->heal, &healCfg, ticks);

	/* Tick auras */
	for (int i = 0; i < CAUTERIZE_AURA_MAX; i++) {
		CauterizeAura *a = &core->auras[i];
		if (!a->active)
			continue;

		a->life_ms -= (int)ticks;
		if (a->life_ms <= 0) {
			a->active = false;
			Burn_reset(&a->burn);
			continue;
		}

		/* Tick the burn visual state */
		Burn_update(&a->burn, ticks);
		if (!Burn_is_active(&a->burn))
			Burn_apply(&a->burn, a->life_ms);

		/* Tick burn interval timer */
		if (a->burn_tick_ms > 0)
			a->burn_tick_ms -= (int)ticks;

		/* Register with centralized burn renderer */
		Burn_register(&a->burn, a->position);
	}
}

/* --- Aura burn check --- */

static bool circle_aabb_overlap(Position center, double radius, Rectangle r)
{
	double cx = center.x;
	double cy = center.y;
	double minX = r.aX < r.bX ? r.aX : r.bX;
	double maxX = r.aX > r.bX ? r.aX : r.bX;
	double minY = r.aY < r.bY ? r.aY : r.bY;
	double maxY = r.aY > r.bY ? r.aY : r.bY;

	double nearX = cx < minX ? minX : (cx > maxX ? maxX : cx);
	double nearY = cy < minY ? minY : (cy > maxY ? maxY : cy);

	double dx = cx - nearX;
	double dy = cy - nearY;
	return (dx * dx + dy * dy) <= radius * radius;
}

int SubCauterize_check_aura_burn(SubCauterizeCore *core, Rectangle target)
{
	int hits = 0;
	const SubCauterizeConfig *cfg = SubCauterize_get_config();
	for (int i = 0; i < CAUTERIZE_AURA_MAX; i++) {
		CauterizeAura *a = &core->auras[i];
		if (!a->active)
			continue;
		if (a->burn_tick_ms > 0)
			continue;

		if (circle_aabb_overlap(a->position, cfg->aura_radius, target)) {
			a->burn_tick_ms = cfg->aura_burn_interval_ms;
			hits++;
		}
	}
	return hits;
}

/* --- Deactivate --- */

void SubCauterize_deactivate_all(SubCauterizeCore *core)
{
	for (int i = 0; i < CAUTERIZE_AURA_MAX; i++) {
		core->auras[i].active = false;
		Burn_reset(&core->auras[i].burn);
	}
}

/* --- Rendering --- */

void SubCauterize_render_aura(const SubCauterizeCore *core)
{
	const SubCauterizeConfig *cfg = SubCauterize_get_config();
	for (int i = 0; i < CAUTERIZE_AURA_MAX; i++) {
		if (!core->auras[i].active)
			continue;

		float life_frac = (float)core->auras[i].life_ms / (float)cfg->aura_duration_ms;
		float alpha = 0.25f * life_frac;
		float cx = (float)core->auras[i].position.x;
		float cy = (float)core->auras[i].position.y;

		Render_filled_circle(cx, cy, (float)cfg->aura_radius, 12,
			1.0f, 0.4f, 0.05f, alpha);
		Render_filled_circle(cx, cy, (float)cfg->aura_radius * 0.5f, 8,
			1.0f, 0.6f, 0.15f, alpha * 1.5f);
	}
}

void SubCauterize_render_beam(const SubCauterizeCore *core, const SubCauterizeConfig *cfg)
{
	SubHealConfig healCfg = {
		.beam_duration_ms = cfg->beam_duration_ms,
		.beam_thickness = cfg->beam_thickness,
		.beam_color_r = cfg->beam_color_r,
		.beam_color_g = cfg->beam_color_g,
		.beam_color_b = cfg->beam_color_b,
		.bloom_beam_thickness = cfg->bloom_beam_thickness,
	};
	SubHeal_render_beam(&core->heal, &healCfg);
}

void SubCauterize_render_aura_bloom(const SubCauterizeCore *core)
{
	const SubCauterizeConfig *cfg = SubCauterize_get_config();
	for (int i = 0; i < CAUTERIZE_AURA_MAX; i++) {
		if (!core->auras[i].active)
			continue;

		float life_frac = (float)core->auras[i].life_ms / (float)cfg->aura_duration_ms;
		float alpha = 0.4f * life_frac;
		float cx = (float)core->auras[i].position.x;
		float cy = (float)core->auras[i].position.y;

		Render_filled_circle(cx, cy, (float)cfg->aura_radius * 1.3f, 12,
			1.0f, 0.5f, 0.1f, alpha);
	}
}

void SubCauterize_render_beam_bloom(const SubCauterizeCore *core, const SubCauterizeConfig *cfg)
{
	SubHealConfig healCfg = {
		.beam_duration_ms = cfg->beam_duration_ms,
		.beam_thickness = cfg->beam_thickness,
		.beam_color_r = cfg->beam_color_r,
		.beam_color_g = cfg->beam_color_g,
		.beam_color_b = cfg->beam_color_b,
		.bloom_beam_thickness = cfg->bloom_beam_thickness,
	};
	SubHeal_render_beam_bloom(&core->heal, &healCfg);
}

void SubCauterize_render_aura_light(const SubCauterizeCore *core)
{
	for (int i = 0; i < CAUTERIZE_AURA_MAX; i++) {
		if (!core->auras[i].active)
			continue;

		float life_frac = (float)core->auras[i].life_ms /
			(float)SubCauterize_get_config()->aura_duration_ms;
		float alpha = 0.35f * life_frac;
		float cx = (float)core->auras[i].position.x;
		float cy = (float)core->auras[i].position.y;

		Render_filled_circle(cx, cy, 120.0f, 12,
			1.0f, 0.5f, 0.1f, alpha);
	}
}

/* --- Audio lifecycle (reuses heal.wav) --- */

void SubCauterize_initialize_audio(void)
{
	SubHeal_initialize_audio();
}

void SubCauterize_cleanup_audio(void)
{
	SubHeal_cleanup_audio();
}
