#include "enemy_util.h"

#include "map.h"
#include "render.h"
#include "color.h"

#include <math.h>
#include <stdlib.h>

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
