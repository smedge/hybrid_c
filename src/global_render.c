#include "global_render.h"
#include <stdio.h>

#define MAX_GLOBAL_RENDER_FUNCS 64

static GlobalRenderFunc registry[RENDER_PASS_COUNT][MAX_GLOBAL_RENDER_FUNCS];
static int counts[RENDER_PASS_COUNT];

void GlobalRender_register(RenderPass pass, GlobalRenderFunc func)
{
	if (pass >= RENDER_PASS_COUNT) return;
	for (int i = 0; i < counts[pass]; i++)
		if (registry[pass][i] == func) return;
	if (counts[pass] >= MAX_GLOBAL_RENDER_FUNCS) {
		printf("WARNING: GlobalRender registry full for pass %d\n", pass);
		return;
	}
	registry[pass][counts[pass]++] = func;
}

void GlobalRender_pass(RenderPass pass)
{
	if (pass >= RENDER_PASS_COUNT) return;
	for (int i = 0; i < counts[pass]; i++)
		registry[pass][i]();
}

void GlobalRender_clear(void)
{
	for (int i = 0; i < RENDER_PASS_COUNT; i++)
		counts[i] = 0;
}
