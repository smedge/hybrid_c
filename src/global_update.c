#include "global_update.h"
#include <stdio.h>

#define MAX_GLOBAL_UPDATE_FUNCS 32

static GlobalUpdateFunc preCollision[MAX_GLOBAL_UPDATE_FUNCS];
static int preCount;

static GlobalUpdateFunc postCollision[MAX_GLOBAL_UPDATE_FUNCS];
static int postCount;

void GlobalUpdate_register_pre_collision(GlobalUpdateFunc func)
{
	for (int i = 0; i < preCount; i++)
		if (preCollision[i] == func) return;
	if (preCount >= MAX_GLOBAL_UPDATE_FUNCS) {
		printf("WARNING: GlobalUpdate pre-collision registry full\n");
		return;
	}
	preCollision[preCount++] = func;
}

void GlobalUpdate_register_post_collision(GlobalUpdateFunc func)
{
	for (int i = 0; i < postCount; i++)
		if (postCollision[i] == func) return;
	if (postCount >= MAX_GLOBAL_UPDATE_FUNCS) {
		printf("WARNING: GlobalUpdate post-collision registry full\n");
		return;
	}
	postCollision[postCount++] = func;
}

void GlobalUpdate_pre_collision(const unsigned int ticks)
{
	for (int i = 0; i < preCount; i++)
		preCollision[i](ticks);
}

void GlobalUpdate_post_collision(const unsigned int ticks)
{
	for (int i = 0; i < postCount; i++)
		postCollision[i](ticks);
}

void GlobalUpdate_clear(void)
{
	preCount = 0;
	postCount = 0;
}
