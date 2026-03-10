#include "enemy_util.h"

#include "map.h"
#include "render.h"
#include "color.h"
#include "ship.h"
#include "corruptor.h"
#include "sub_pea.h"
#include "sub_mgun.h"
#include "sub_mine.h"
#include "sub_inferno.h"
#include "sub_disintegrate.h"
#include "sub_egress.h"
#include "sub_stealth.h"
#include "sub_gravwell.h"
#include "sub_tgun.h"
#include "sub_flak.h"
#include "sub_flak_core.h"
#include "sub_ember.h"
#include "sub_ember_core.h"
#include "sub_blaze.h"
#include "sub_cauterize.h"
#include "sub_immolate.h"
#include "sub_cinder.h"
#include "sub_cinder_core.h"
#include "sub_scorch.h"
#include "sub_smolder.h"
#include "fragment.h"
#include "progression.h"
#include "skillbar.h"
#include "enemy_registry.h"

#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

static PlayerDamageResult check_player_damage_internal(Rectangle hitBox, Position enemyPos, int volley_cap)
{
	PlayerDamageResult r = {false, false, false, 0.0, 0.0};
	double dist = Enemy_distance_between(enemyPos, Ship_get_position());
	double mul = Sub_Stealth_get_damage_multiplier(dist);
	r.ambush = (mul > 1.0);

	double pea_dmg = Sub_Pea_check_hit(hitBox);
	if (pea_dmg > 0) {
		r.damage += pea_dmg * mul;
		r.hit = true;
	}
	double mgun_dmg = Sub_Mgun_check_hit(hitBox);
	if (mgun_dmg > 0) {
		r.damage += mgun_dmg * mul;
		r.hit = true;
	}
	double tgun_dmg = Sub_Tgun_check_hit(hitBox);
	if (tgun_dmg > 0) {
		r.damage += tgun_dmg * mul;
		r.hit = true;
	}
	int flak_hits = (volley_cap > 0)
		? Sub_Flak_check_hit_burn_capped(hitBox, volley_cap)
		: Sub_Flak_check_hit_burn(hitBox);
	if (flak_hits > 0) {
		const SubFlakConfig *flak_cfg = SubFlak_get_config();
		r.damage += flak_hits * flak_cfg->proj.damage * mul;
		r.burn_hits = flak_hits;
		r.hit = true;
	}
	/* Ember projectile direct hits */
	int ember_hits = (volley_cap > 0)
		? Sub_Ember_check_hit_burn_capped(hitBox, volley_cap)
		: Sub_Ember_check_hit_burn(hitBox);
	if (ember_hits > 0) {
		const SubEmberConfig *ember_cfg = SubEmber_get_config();
		r.damage += ember_hits * ember_cfg->proj.damage * mul;
		r.burn_hits += ember_hits;
		r.hit = true;
	}
	/* Ember AOE ignite — nearby enemies catch fire on impact */
	int ember_splash = SubEmber_check_burst(hitBox);
	if (ember_splash > 0) {
		r.burn_hits += ember_splash;
		r.hit = true;
	}
	double mine_dmg = Sub_Mine_check_hit(hitBox);
	if (mine_dmg > 0) {
		r.mine_damage = mine_dmg * mul;
		r.mine_hit = true;
		r.hit = true;
	}
	double cinder_dmg = Sub_Cinder_check_hit(hitBox);
	if (cinder_dmg > 0) {
		r.mine_damage += cinder_dmg * mul;
		r.mine_hit = true;
		r.hit = true;
		/* Cinder detonation applies burn stacks */
		const SubCinderConfig *cinder_cfg = SubCinder_get_config();
		r.burn_hits += cinder_cfg->detonation_burn_stacks;
	}
	/* Cinder fire pool burn */
	int cinder_pool_hits = Sub_Cinder_check_pool_burn(hitBox);
	if (cinder_pool_hits > 0) {
		r.burn_hits += cinder_pool_hits;
		r.hit = true;
	}
	if (Sub_Inferno_check_hit(hitBox)) {
		r.damage += 10.0 * mul;
		r.burn_hits++;
		r.hit = true;
	}
	if (Sub_Disintegrate_check_hit(hitBox)) {
		r.damage += 20.0 * mul;
		r.hit = true;
	}
	double egress_dmg = Sub_Egress_check_hit(hitBox);
	if (egress_dmg > 0) {
		r.damage += egress_dmg * mul;
		r.hit = true;
	}
	int blaze_corridor = Sub_Blaze_check_corridor_burn(hitBox);
	if (blaze_corridor > 0) {
		r.burn_hits += blaze_corridor;
		r.hit = true;
	}
	int cauterize_hits = Sub_Cauterize_check_aura_burn(hitBox);
	if (cauterize_hits > 0) {
		r.burn_hits += cauterize_hits;
		r.hit = true;
	}
	int immolate_hits = Sub_Immolate_check_burn(hitBox);
	if (immolate_hits > 0) {
		r.burn_hits += immolate_hits;
		r.hit = true;
	}
	/* Scorch footprint burn */
	int scorch_hits = Sub_Scorch_check_footprint_burn(hitBox);
	if (scorch_hits > 0) {
		r.burn_hits += scorch_hits;
		r.hit = true;
	}

	/* Corruptor resist aura — halves all damage */
	if (r.hit && Corruptor_is_resist_buffing(enemyPos)) {
		r.damage *= 0.5;
		r.mine_damage *= 0.5;
	}

	return r;
}

PlayerDamageResult Enemy_check_player_damage(Rectangle hitBox, Position enemyPos)
{
	return check_player_damage_internal(hitBox, enemyPos, 0);
}

PlayerDamageResult Enemy_check_player_damage_capped(Rectangle hitBox, Position enemyPos, int max_per_volley)
{
	return check_player_damage_internal(hitBox, enemyPos, max_per_volley);
}

void Enemy_on_player_kill(const PlayerDamageResult *dmg)
{
	if (dmg->ambush) {
		Sub_Stealth_notify_kill();
		Sub_Smolder_notify_kill();
	}
}

#define STEALTH_DETECT_RANGE 100.0
#define STEALTH_DETECT_HALF_CONE 45.0  /* 90° cone = ±45° from facing */

void Enemy_check_stealth_proximity(Position enemyPos, double facingDegrees)
{
	if (!Sub_Stealth_is_stealthed() && !Sub_Smolder_is_active())
		return;

	Position shipPos = Ship_get_position();
	if (Enemy_distance_between(enemyPos, shipPos) >= STEALTH_DETECT_RANGE)
		return;

	/* Angle from enemy to player */
	double dx = shipPos.x - enemyPos.x;
	double dy = shipPos.y - enemyPos.y;
	double angleToPlayer = atan2(dx, dy) * 180.0 / PI;

	/* Shortest angular difference */
	double diff = angleToPlayer - facingDegrees;
	while (diff > 180.0) diff -= 360.0;
	while (diff < -180.0) diff += 360.0;

	if (fabs(diff) <= STEALTH_DETECT_HALF_CONE)
		Enemy_break_cloak();
}

void Enemy_break_cloak(void)
{
	Sub_Stealth_break();
	Sub_Smolder_break();
}

void Enemy_break_cloak_attack(void)
{
	Sub_Stealth_break_attack();
	Sub_Smolder_break_attack();
}

double Enemy_distance_between(Position a, Position b)
{
	double dx = b.x - a.x;
	double dy = b.y - a.y;
	return sqrt(dx * dx + dy * dy);
}

bool Enemy_has_line_of_sight(Position from, Position to)
{
	double hx, hy;
	return !Map_line_test_hit(from.x, from.y, to.x, to.y, &hx, &hy);
}

void Enemy_move_toward(PlaceableComponent *pl, Position target, double speed, double dt, double wallCheckDist)
{
	double dx = target.x - pl->position.x;
	double dy = target.y - pl->position.y;
	double dist = sqrt(dx * dx + dy * dy);
	if (dist < 1.0)
		return;

	double nx = dx / dist;
	double ny = dy / dist;

	double move = speed * dt;
	if (move > dist)
		move = dist;

	/* Wall avoidance — try combined movement first, then axis-separated */
	double checkX = pl->position.x + nx * wallCheckDist;
	double checkY = pl->position.y + ny * wallCheckDist;
	double hx, hy;
	if (!Map_line_test_hit(pl->position.x, pl->position.y, checkX, checkY, &hx, &hy)) {
		pl->position.x += nx * move;
		pl->position.y += ny * move;
	} else {
		/* Try X movement only */
		double checkXOnly = pl->position.x + nx * wallCheckDist;
		if (!Map_line_test_hit(pl->position.x, pl->position.y, checkXOnly, pl->position.y, &hx, &hy)) {
			pl->position.x += nx * move;
		}
		/* Try Y movement only */
		double checkYOnly = pl->position.y + ny * wallCheckDist;
		if (!Map_line_test_hit(pl->position.x, pl->position.y, pl->position.x, checkYOnly, &hx, &hy)) {
			pl->position.y += ny * move;
		}
	}
}

void Enemy_move_away_from(PlaceableComponent *pl, Position threat, double speed, double dt, double wallCheckDist)
{
	double dx = pl->position.x - threat.x;
	double dy = pl->position.y - threat.y;
	double dist = sqrt(dx * dx + dy * dy);
	if (dist < 1.0) {
		dx = 1.0;
		dy = 0.0;
		dist = 1.0;
	}

	double nx = dx / dist;
	double ny = dy / dist;

	double moveAmount = speed * dt;

	/* Wall avoidance — try combined movement first, then axis-separated */
	double checkX = pl->position.x + nx * wallCheckDist;
	double checkY = pl->position.y + ny * wallCheckDist;
	double hx, hy;
	if (!Map_line_test_hit(pl->position.x, pl->position.y, checkX, checkY, &hx, &hy)) {
		pl->position.x += nx * moveAmount;
		pl->position.y += ny * moveAmount;
	} else {
		/* Try X movement only */
		double checkXOnly = pl->position.x + nx * wallCheckDist;
		if (!Map_line_test_hit(pl->position.x, pl->position.y, checkXOnly, pl->position.y, &hx, &hy)) {
			pl->position.x += nx * moveAmount;
		}
		/* Try Y movement only */
		double checkYOnly = pl->position.y + ny * wallCheckDist;
		if (!Map_line_test_hit(pl->position.x, pl->position.y, pl->position.x, checkYOnly, &hx, &hy)) {
			pl->position.y += ny * moveAmount;
		}
	}
}

void Enemy_pick_wander_target(Position spawnPoint, double radius, int baseInterval,
	Position *out_target, int *out_timer)
{
	double angle = (rand() % 360) * PI / 180.0;
	double dist = (radius >= 1.0) ? (rand() % (int)radius) : 0.0;
	out_target->x = spawnPoint.x + cos(angle) * dist;
	out_target->y = spawnPoint.y + sin(angle) * dist;
	*out_timer = baseInterval + (rand() % 1000);
}

void Enemy_render_death_flash(const PlaceableComponent *pl, float deathTimer, float deathDuration)
{
	float fade = 1.0f - deathTimer / deathDuration;
	ColorFloat flash = {1.0f, 1.0f, 1.0f, fade};
	Rectangle sparkRect = {-20.0, 20.0, 20.0, -20.0};
	Render_quad(&pl->position, 0.0, sparkRect, &flash);
	Render_quad(&pl->position, 45.0, sparkRect, &flash);
}

void Enemy_render_spark(Position sparkPosition, int sparkTicksLeft, int sparkDuration, float sparkSize, bool sparkShielded, float normalR, float normalG, float normalB)
{
	float fade = (float)sparkTicksLeft / sparkDuration;
	ColorFloat sparkColor = sparkShielded
		? (ColorFloat){0.6f, 0.9f, 1.0f, fade}
		: (ColorFloat){normalR, normalG, normalB, fade};
	Rectangle sparkRect = {-sparkSize, sparkSize, sparkSize, -sparkSize};
	Render_quad(&sparkPosition, 0.0, sparkRect, &sparkColor);
	Render_quad(&sparkPosition, 45.0, sparkRect, &sparkColor);
}

bool Enemy_check_any_nearby(Position pos, double radius)
{
	return Sub_Pea_check_nearby(pos, radius)
		|| Sub_Mgun_check_nearby(pos, radius)
		|| Sub_Tgun_check_nearby(pos, radius)
		|| Sub_Flak_check_nearby(pos, radius)
		|| Sub_Ember_check_nearby(pos, radius)
		|| Sub_Inferno_check_nearby(pos, radius)
		|| Sub_Disintegrate_check_nearby(pos, radius);
}

bool Enemy_check_any_hit(Rectangle hitBox)
{
	return Sub_Pea_check_hit(hitBox)
		|| Sub_Mgun_check_hit(hitBox)
		|| Sub_Tgun_check_hit(hitBox)
		|| Sub_Flak_check_hit(hitBox)
		|| Sub_Ember_check_hit(hitBox)
		|| Sub_Mine_check_hit(hitBox)
		|| Sub_Cinder_check_hit(hitBox)
		|| Sub_Inferno_check_hit(hitBox)
		|| Sub_Disintegrate_check_hit(hitBox)
		|| Sub_Egress_check_hit(hitBox);
}

double Enemy_apply_gravity(PlaceableComponent *pl, double dt)
{
	double dx, dy, speed_mult;
	if (!Sub_Gravwell_get_pull(pl->position, dt, &dx, &dy, &speed_mult))
		return 1.0;

	pl->position.x += dx;
	pl->position.y += dy;
	return speed_mult;
}

void Enemy_alert_nearby(Position origin, double radius)
{
	Position threat = Ship_get_position();
	EnemyRegistry_alert_nearby(origin, radius, threat);
}

void Enemy_drop_fragments(Position deathPos, const CarriedSubroutine *subs, int subCount)
{
	/* Separate locked subs into normal and rare pools.
	   Rare subs only become eligible once ALL normal subs are unlocked. */
	int normal[8];
	int normalCount = 0;
	int rare[8];
	int rareCount = 0;
	bool allNormalUnlocked = true;

	for (int i = 0; i < subCount; i++) {
		if (Skillbar_get_tier(subs[i].sub_id) == TIER_RARE) {
			if (!Progression_is_unlocked(subs[i].sub_id) && rareCount < 8)
				rare[rareCount++] = i;
		} else {
			if (!Progression_is_unlocked(subs[i].sub_id)) {
				if (normalCount < 8)
					normal[normalCount++] = i;
				allNormalUnlocked = false;
			}
		}
	}

	/* Drop normal fragments: always, pick one at random */
	if (normalCount > 0) {
		int pick = rand() % normalCount;
		Fragment_spawn(deathPos, subs[normal[pick]].frag_type, Skillbar_get_tier(subs[normal[pick]].sub_id));
		return;
	}

	/* All normal subs unlocked — rare subs become eligible at 20% drop rate */
	if (allNormalUnlocked && rareCount > 0) {
		if ((rand() % 100) < 20) {
			int pick = rand() % rareCount;
			Fragment_spawn(deathPos, subs[rare[pick]].frag_type, Skillbar_get_tier(subs[rare[pick]].sub_id));
		}
	}
}

void Enemy_render_resist_indicator(Position pos)
{
	if (!Corruptor_is_resist_buffing(pos))
		return;

	float t = (float)(SDL_GetTicks() % 2000) / 2000.0f;
	float pulse = 0.7f + 0.3f * sinf(t * 2.0f * (float)M_PI);
	float radius = 14.0f;
	float alpha = 0.45f * pulse;

	for (int i = 0; i < 6; i++) {
		float a0 = (float)i * 60.0f * (float)M_PI / 180.0f;
		float a1 = (float)(i + 1) * 60.0f * (float)M_PI / 180.0f;
		float x0 = (float)pos.x + cosf(a0) * radius;
		float y0 = (float)pos.y + sinf(a0) * radius;
		float x1 = (float)pos.x + cosf(a1) * radius;
		float y1 = (float)pos.y + sinf(a1) * radius;
		Render_thick_line(x0, y0, x1, y1, 1.5f,
			1.0f, 0.9f, 0.0f, alpha);
	}
}
