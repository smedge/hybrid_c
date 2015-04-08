#include "timer.h"

static unsigned int currentTicks = 0;
static unsigned int lastTicks = 0;
static unsigned int ticksThisIteration = 0;

unsigned int timer_tick() {
	lastTicks = currentTicks;
	currentTicks = SDL_GetTicks();
	
	ticksThisIteration = currentTicks - lastTicks;
	if (ticksThisIteration > MAX_TICKS)
		return MAX_TICKS;
	else
		return ticksThisIteration;
}