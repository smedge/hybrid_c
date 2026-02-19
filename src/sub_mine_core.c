#include "sub_mine_core.h"
#include "render.h"
#include "view.h"

void SubMine_init(SubMineCore *core)
{
	core->phase = MINE_PHASE_DORMANT;
	core->position = (Position){0, 0};
	core->phaseTicks = 0;
	core->blinkTimer = 0;
}

bool SubMine_arm(SubMineCore *core, const SubMineConfig *cfg, Position pos)
{
	if (core->phase != MINE_PHASE_DORMANT)
		return false;
	core->phase = MINE_PHASE_ARMED;
	core->position = pos;
	core->phaseTicks = 0;
	(void)cfg;
	return true;
}

void SubMine_detonate(SubMineCore *core)
{
	if (core->phase == MINE_PHASE_EXPLODING || core->phase == MINE_PHASE_DEAD)
		return;
	core->phase = MINE_PHASE_EXPLODING;
	core->phaseTicks = 0;
}

MinePhase SubMine_update(SubMineCore *core, const SubMineConfig *cfg, unsigned int ticks)
{
	switch (core->phase) {
	case MINE_PHASE_ARMED:
		core->phaseTicks += ticks;
		if (core->phaseTicks >= cfg->armed_duration_ms) {
			core->phase = MINE_PHASE_EXPLODING;
			core->phaseTicks = 0;
			return MINE_PHASE_EXPLODING;
		}
		break;

	case MINE_PHASE_EXPLODING:
		core->phaseTicks += ticks;
		if (core->phaseTicks >= cfg->exploding_duration_ms) {
			if (cfg->dead_duration_ms > 0) {
				core->phase = MINE_PHASE_DEAD;
				core->phaseTicks = 0;
			} else {
				core->phase = MINE_PHASE_DORMANT;
				core->phaseTicks = 0;
			}
		}
		break;

	case MINE_PHASE_DEAD:
		core->phaseTicks += ticks;
		if (core->phaseTicks >= cfg->dead_duration_ms) {
			core->phase = MINE_PHASE_DORMANT;
			core->phaseTicks = 0;
		}
		break;

	case MINE_PHASE_DORMANT:
		core->blinkTimer += ticks;
		if (core->blinkTimer >= 1000)
			core->blinkTimer -= 1000;
		break;
	}

	return core->phase;
}

bool SubMine_check_explosion_hit(const SubMineCore *core, const SubMineConfig *cfg, Rectangle target)
{
	if (core->phase != MINE_PHASE_EXPLODING)
		return false;

	float hs = cfg->explosion_half_size;
	Rectangle blastArea = Collision_transform_bounding_box(core->position,
		(Rectangle){-hs, hs, hs, -hs});

	return Collision_aabb_test(blastArea, target);
}

bool SubMine_is_active(const SubMineCore *core)
{
	return core->phase == MINE_PHASE_ARMED || core->phase == MINE_PHASE_EXPLODING;
}

bool SubMine_is_exploding(const SubMineCore *core)
{
	return core->phase == MINE_PHASE_EXPLODING;
}

MinePhase SubMine_get_phase(const SubMineCore *core)
{
	return core->phase;
}

void SubMine_render(const SubMineCore *core, const SubMineConfig *cfg)
{
	switch (core->phase) {
	case MINE_PHASE_DORMANT: {
		/* Dormant: body diamond + blinking red dot */
		float bs = cfg->body_half_size;
		Rectangle rect = {-bs, bs, bs, -bs};
		View view = View_get_view();
		if (view.scale > 0.09) {
			ColorFloat bodyColor = {cfg->body_r, cfg->body_g, cfg->body_b, cfg->body_a};
			Render_quad(&core->position, 45.0, rect, &bodyColor);
		}
		if (core->blinkTimer < 100) {
			float dh = 3.0f;
			if (view.scale > 0.001f && 0.5f / (float)view.scale > dh)
				dh = 0.5f / (float)view.scale;
			Rectangle dot = {-dh, dh, dh, -dh};
			ColorFloat blinkColor = {cfg->blink_r, cfg->blink_g, cfg->blink_b, cfg->blink_a};
			Render_quad(&core->position, 45.0, dot, &blinkColor);
		}
		break;
	}

	case MINE_PHASE_ARMED: {
		/* Armed: body diamond + solid active dot */
		float bs = cfg->body_half_size;
		Rectangle rect = {-bs, bs, bs, -bs};
		View view = View_get_view();
		if (view.scale > 0.09) {
			ColorFloat bodyColor = {cfg->body_r, cfg->body_g, cfg->body_b, cfg->body_a};
			Render_quad(&core->position, 45.0, rect, &bodyColor);
		}
		float dh = 3.0f;
		if (view.scale > 0.001f && 0.5f / (float)view.scale > dh)
			dh = 0.5f / (float)view.scale;
		Rectangle dot = {-dh, dh, dh, -dh};
		ColorFloat blinkColor = {cfg->blink_r, cfg->blink_g, cfg->blink_b, cfg->blink_a};
		Render_quad(&core->position, 45.0, dot, &blinkColor);
		break;
	}

	case MINE_PHASE_EXPLODING: {
		float hs = cfg->explosion_half_size;
		Rectangle explosion = {-hs, hs, hs, -hs};
		ColorFloat expColor = {cfg->explosion_r, cfg->explosion_g, cfg->explosion_b, cfg->explosion_a};
		Render_quad(&core->position, 22.5, explosion, &expColor);
		Render_quad(&core->position, 67.5, explosion, &expColor);
		break;
	}

	default:
		break;
	}
}

void SubMine_render_bloom(const SubMineCore *core, const SubMineConfig *cfg)
{
	switch (core->phase) {
	case MINE_PHASE_DORMANT:
		if (core->blinkTimer >= 100)
			break;
		/* fallthrough — blink active, render same as armed */
	case MINE_PHASE_ARMED: {
		View view = View_get_view();
		float dh = 3.0f;
		if (view.scale > 0.001f && 0.5f / (float)view.scale > dh)
			dh = 0.5f / (float)view.scale;
		Rectangle dot = {-dh, dh, dh, -dh};
		ColorFloat blinkColor = {cfg->blink_r, cfg->blink_g, cfg->blink_b, cfg->blink_a};
		Render_quad(&core->position, 45.0, dot, &blinkColor);
		break;
	}

	case MINE_PHASE_EXPLODING: {
		float hs = cfg->explosion_half_size;
		Rectangle explosion = {-hs, hs, hs, -hs};
		ColorFloat expColor = {cfg->explosion_r, cfg->explosion_g, cfg->explosion_b, cfg->explosion_a};
		Render_quad(&core->position, 22.5, explosion, &expColor);
		Render_quad(&core->position, 67.5, explosion, &expColor);
		break;
	}

	default:
		break;
	}
}

void SubMine_render_light(const SubMineCore *core, const SubMineConfig *cfg)
{
	if (core->phase == MINE_PHASE_ARMED ||
		(core->phase == MINE_PHASE_DORMANT && core->blinkTimer < 100)) {
		Render_filled_circle(
			(float)core->position.x, (float)core->position.y,
			cfg->light_armed_radius, 12,
			cfg->light_armed_r, cfg->light_armed_g, cfg->light_armed_b, cfg->light_armed_a);
	} else if (core->phase == MINE_PHASE_EXPLODING) {
		Render_filled_circle(
			(float)core->position.x, (float)core->position.y,
			cfg->light_explode_radius, 16,
			cfg->light_explode_r, cfg->light_explode_g, cfg->light_explode_b, cfg->light_explode_a);
	}
}
