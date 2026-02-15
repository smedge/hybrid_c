#include "enemy_util.h"

#include "map.h"

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

	/* Wall avoidance */
	double checkX = pl->position.x + nx * wallCheckDist;
	double checkY = pl->position.y + ny * wallCheckDist;
	double hx, hy;
	if (Map_line_test_hit(pl->position.x, pl->position.y, checkX, checkY, &hx, &hy))
		return;

	double move = speed * dt;
	if (move > dist)
		move = dist;

	pl->position.x += nx * move;
	pl->position.y += ny * move;
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

	/* Wall avoidance */
	double checkX = pl->position.x + nx * wallCheckDist;
	double checkY = pl->position.y + ny * wallCheckDist;
	double hx, hy;
	if (Map_line_test_hit(pl->position.x, pl->position.y, checkX, checkY, &hx, &hy))
		return;

	pl->position.x += nx * speed * dt;
	pl->position.y += ny * speed * dt;
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
