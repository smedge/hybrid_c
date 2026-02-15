#ifndef ENEMY_UTIL_H
#define ENEMY_UTIL_H

#include <stdbool.h>
#include "entity.h"

#define PI 3.14159265358979323846

double Enemy_distance_between(Position a, Position b);
bool Enemy_has_line_of_sight(Position from, Position to);
void Enemy_move_toward(PlaceableComponent *pl, Position target, double speed, double dt, double wallCheckDist);
void Enemy_move_away_from(PlaceableComponent *pl, Position threat, double speed, double dt, double wallCheckDist);
void Enemy_pick_wander_target(Position spawnPoint, double radius, int baseInterval,
	Position *out_target, int *out_timer);

#endif
