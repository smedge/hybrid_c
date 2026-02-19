#include "enemy_util.h"

#include "map.h"
#include "render.h"
#include "color.h"
#include "ship.h"
#include "sub_pea.h"
#include "sub_mgun.h"
#include "sub_mine.h"
#include "sub_inferno.h"
#include "sub_disintegrate.h"
#include "sub_egress.h"
#include "sub_stealth.h"
#include "sub_gravwell.h"
#include "fragment.h"
#include "progression.h"

#include <math.h>
#include <stdlib.h>

PlayerDamageResult Enemy_check_player_damage(Rectangle hitBox, Position enemyPos)
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
	double mine_dmg = Sub_Mine_check_hit(hitBox);
	if (mine_dmg > 0) {
		r.mine_damage = mine_dmg * mul;
		r.mine_hit = true;
		r.hit = true;
	}
	if (Sub_Inferno_check_hit(hitBox)) {
		r.damage += 10.0 * mul;
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

	return r;
}

void Enemy_on_player_kill(const PlayerDamageResult *dmg)
{
	if (dmg->ambush)
		Sub_Stealth_notify_kill();
}

#define STEALTH_DETECT_RANGE 100.0
#define STEALTH_DETECT_HALF_CONE 45.0  /* 90° cone = ±45° from facing */

void Enemy_check_stealth_proximity(Position enemyPos, double facingDegrees)
{
	if (!Sub_Stealth_is_stealthed())
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
		Sub_Stealth_break();
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
	double dist = (rand() % (int)radius);
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
		|| Sub_Inferno_check_nearby(pos, radius)
		|| Sub_Disintegrate_check_nearby(pos, radius);
}

bool Enemy_check_any_hit(Rectangle hitBox)
{
	return Sub_Pea_check_hit(hitBox)
		|| Sub_Mgun_check_hit(hitBox)
		|| Sub_Mine_check_hit(hitBox)
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

void Enemy_drop_fragments(Position deathPos, const CarriedSubroutine *subs, int subCount)
{
	/* Collect indices of locked (not yet unlocked) subroutines */
	int locked[8];
	int lockedCount = 0;

	for (int i = 0; i < subCount && lockedCount < 8; i++) {
		if (!Progression_is_unlocked(subs[i].sub_id))
			locked[lockedCount++] = i;
	}

	if (lockedCount == 0)
		return;

	/* Pick one at random, equal weight */
	int pick = rand() % lockedCount;
	Fragment_spawn(deathPos, subs[locked[pick]].frag_type);
}
