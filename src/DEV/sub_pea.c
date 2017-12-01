#include "sub_pea.h"

#include <stdbool.h>

static bool active;
static double velocity;
static Position position;
static Heading heading;
static int timeToLive;
static int ticksLived;
static Ship *ship;
static double headingSin, headingCos;

static double calculateDeltaX (int ticks);
static double calculateDeltaY (int ticks);
static void doTrig(void);

// void Sub_Pea_initialize()
// {
// 	active = false;
// 	velocity = 3000.0;
// 	position = {0.0, 0.0};
// 	heading = 0;
// 	timeToLive = 1000;
// 	ticksLived = 0;
// }

// void Sub_Pea_cleanup()
// {

// }

// void Sub_Pea_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable) 
// {
// 	if (input.mouseLeft && !active) {
// 		active = true;
// 		Position position = {placeable.position.x,placeable.poxition.y};
		



// 		heading = position.getHeading(Position(input.mouseWorldX,
// 			input.mouseWorldY));
// 		velocity = ship.getVelocity() + 3000;
// 		doTrig();
// 	}

// 	if (active) {
// 		ticksLived += ticks;
// 		if (ticksLived > timeToLive) {
// 			active = false;
// 			ticksLived = 0;
// 			position = Position(0,0);
// 		}

// 		Position positionDelta;
// 		positionDelta.setX(calculateDeltaX(ticks));
// 		positionDelta.setY(calculateDeltaY(ticks));

// 		position += positionDelta;
// 	}
// }

// void Sub_Pea_render(const ScreenMetrics &screenMetrics, const Camera &camera) 
// {
// 	const double UNSCALED_POINT_SIZE = 3.0;
// 	const double MIN_POINT_SIZE = 2.0;
	
// 	if (active) {
// 		double size = UNSCALED_POINT_SIZE * camera.getScale();
// 		if (size < MIN_POINT_SIZE)
// 			size = MIN_POINT_SIZE;

// 		RenderContext::renderPoint(position, size, 1.0, 1.0, 1.0, 1.0);
// 	}
// }

// static double calculateDeltaX (int ticks)
// {
// 	return headingSin * velocity / (1000/ticks);
// }

// static double calculateDeltaY (int ticks)
// {
// 	return headingCos * velocity / (1000/ticks);
// }

// static void doTrig(void)
// {
// 	headingSin = sin(heading.getRadians());
// 	headingCos = cos(heading.getRadians());
// }