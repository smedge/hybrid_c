#include "sub_pea.h"

#include "graphics.h"
#include "audio.h"
#include "position.h"
#include "render.h"
#include "view.h"
#include "shipstate.h"

#include <SDL2/SDL_mixer.h>

static Entity *parent;
static bool active;
static double velocity;
static Position position;
static ColorFloat color  = {1.0, 1.0, 1.0, 1.0};
static double heading;
static int timeToLive;
static int ticksLived;
static double headingSin, headingCos;

static Mix_Chunk *sample01 = 0;
static Mix_Chunk *sample02 = 0;

static double calculateDeltaX (int ticks);
static double calculateDeltaY (int ticks);
static void doTrig(void);
static double getRadians(double degrees);

void Sub_Pea_initialize(Entity *p)
{
	parent = p;
	active = false;
	velocity = 2500.0;
	position.x = 0.0;
	position.y = 0.0;
	heading = 0;
	timeToLive = 1000;
	ticksLived = 0;

	Audio_load_sample(&sample01, "resources/sounds/long_beam.wav");
	Audio_load_sample(&sample02, "resources/sounds/ricochet.wav");
}

void Sub_Pea_cleanup()
{
	Audio_unload_sample(&sample01);
	Audio_unload_sample(&sample02);
}

void Sub_Pea_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable) 
{
	ShipState *state = (ShipState*)parent->state;

	if (userInput->mouseLeft && !active && !state->destroyed) {
		active = true;

		position.x = placeable->position.x;
		position.y = placeable->position.y;

		Position position_cursor = {userInput->mouseX, userInput->mouseY};
		Screen screen = Graphics_get_screen();
		Position position_cursor_world = View_get_world_position_gl(&screen, position_cursor);

		heading = Position_get_heading(position, position_cursor_world);
		velocity = 3500;
		doTrig();

		Audio_play_sample(&sample01);
	}

	if (active) {
		ticksLived += ticks;
		if (ticksLived > timeToLive) {
			active = false;
			ticksLived = 0;
			position.x = 0.0;
			position.y = 0.0;
			Audio_play_sample(&sample02);
		}

		Position positionDelta;
		positionDelta.x = calculateDeltaX(ticks);
		positionDelta.y = calculateDeltaY(ticks);

		position.x += positionDelta.x;
		position.y += positionDelta.y;
	}
}

void Sub_Pea_render() 
{
	View view = View_get_view();

	const double UNSCALED_POINT_SIZE = 8.0;
	const double MIN_POINT_SIZE = 2.0;

	if (active) {
		double size = UNSCALED_POINT_SIZE * view.scale;
		if (size < MIN_POINT_SIZE)
			size = MIN_POINT_SIZE;

		Render_point(&position, size, &color);
	}
}

static double calculateDeltaX (int ticks)
{
	return headingSin * velocity / (1000/ticks);
}

static double calculateDeltaY (int ticks)
{
	return headingCos * velocity / (1000/ticks);
}

static void doTrig(void)
{
	headingSin = sin(getRadians(heading));
	headingCos = cos(getRadians(heading));
}

static double getRadians(double degrees) 
{
	return degrees * 3.14159265358979323846 / 180.0;
}
