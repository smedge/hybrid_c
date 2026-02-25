#include "enemy_registry.h"

#include <stdio.h>

static EnemyTypeCallbacks types[MAX_ENEMY_TYPES];
static int typeCount = 0;

void EnemyRegistry_clear(void)
{
	typeCount = 0;
}

int EnemyRegistry_register(EnemyTypeCallbacks callbacks)
{
	if (typeCount >= MAX_ENEMY_TYPES) {
		printf("FATAL ERROR: Too many enemy types registered.\n");
		return -1;
	}
	types[typeCount] = callbacks;
	return typeCount++;
}

int EnemyRegistry_type_count(void)
{
	return typeCount;
}

const EnemyTypeCallbacks *EnemyRegistry_get_type(int type_id)
{
	if (type_id < 0 || type_id >= typeCount)
		return NULL;
	return &types[type_id];
}

void EnemyRegistry_heal(int type_id, int index, double amount)
{
	if (type_id < 0 || type_id >= typeCount)
		return;
	if (types[type_id].heal)
		types[type_id].heal(index, amount);
}

void EnemyRegistry_alert_nearby(Position origin, double radius, Position threat)
{
	for (int i = 0; i < typeCount; i++) {
		if (types[i].alert_nearby)
			types[i].alert_nearby(origin, radius, threat);
	}
}
