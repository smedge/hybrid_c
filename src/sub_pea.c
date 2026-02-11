#include "sub_pea.h"

#include "graphics.h"
#include "audio.h"
#include "map.h"
#include "position.h"
#include "render.h"
#include "view.h"
#include "shipstate.h"

#include <math.h>
#include <SDL2/SDL_mixer.h>

#define MAX_PROJECTILES 8
#define TIME_TO_LIVE 1000
#define VELOCITY 3500.0
#define FIRE_COOLDOWN 500
#define SPARK_DURATION 80
#define SPARK_SIZE 15.0

typedef struct {
	bool active;
	Position position;
	Position prevPosition;
	double headingSin;
	double headingCos;
	int ticksLived;
} Projectile;

static Entity *parent;
static Projectile projectiles[MAX_PROJECTILES];
static ColorFloat color = {1.0, 1.0, 1.0, 1.0};
static int cooldownTimer;

static Mix_Chunk *sample01 = 0;
static Mix_Chunk *sample02 = 0;

static bool sparkActive;
static Position sparkPosition;
static int sparkTicksLeft;

static double get_radians(double degrees);

void Sub_Pea_initialize(Entity *p)
{
	parent = p;
	cooldownTimer = 0;
	sparkActive = false;

	for (int i = 0; i < MAX_PROJECTILES; i++)
		projectiles[i].active = false;

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

	if (cooldownTimer > 0)
		cooldownTimer -= ticks;

	if (userInput->mouseLeft && cooldownTimer <= 0 && !state->destroyed) {
		cooldownTimer = FIRE_COOLDOWN;

		/* Find an inactive slot */
		int slot = -1;
		for (int i = 0; i < MAX_PROJECTILES; i++) {
			if (!projectiles[i].active) {
				slot = i;
				break;
			}
		}
		if (slot < 0)
			slot = 0; /* reuse oldest if all full */

		Projectile *p = &projectiles[slot];
		p->active = true;
		p->ticksLived = 0;
		p->position.x = placeable->position.x;
		p->position.y = placeable->position.y;
		p->prevPosition = p->position;

		Position position_cursor = {userInput->mouseX, userInput->mouseY};
		Screen screen = Graphics_get_screen();
		Position position_cursor_world = View_get_world_position(&screen, position_cursor);

		double heading = Position_get_heading(p->position, position_cursor_world);
		double rad = get_radians(heading);
		p->headingSin = sin(rad);
		p->headingCos = cos(rad);

		Audio_play_sample(&sample01);
	}

	for (int i = 0; i < MAX_PROJECTILES; i++) {
		Projectile *p = &projectiles[i];
		if (!p->active)
			continue;

		p->ticksLived += ticks;
		if (p->ticksLived > TIME_TO_LIVE) {
			p->active = false;
			continue;
		}

		p->prevPosition = p->position;

		double dt = ticks / 1000.0;
		double dx = p->headingSin * VELOCITY * dt;
		double dy = p->headingCos * VELOCITY * dt;
		p->position.x += dx;
		p->position.y += dy;

		double hx, hy;
		if (Map_line_test_hit(p->prevPosition.x, p->prevPosition.y,
				p->position.x, p->position.y, &hx, &hy)) {
			sparkActive = true;
			sparkPosition.x = hx;
			sparkPosition.y = hy;
			sparkTicksLeft = SPARK_DURATION;
			p->active = false;
			Audio_play_sample(&sample02);
		}
	}

	if (sparkActive) {
		sparkTicksLeft -= ticks;
		if (sparkTicksLeft <= 0)
			sparkActive = false;
	}
}

void Sub_Pea_render()
{
	View view = View_get_view();

	const double UNSCALED_POINT_SIZE = 8.0;
	const double MIN_POINT_SIZE = 2.0;

	for (int i = 0; i < MAX_PROJECTILES; i++) {
		if (projectiles[i].active) {
			double size = UNSCALED_POINT_SIZE * view.scale;
			if (size < MIN_POINT_SIZE)
				size = MIN_POINT_SIZE;
			Render_point(&projectiles[i].position, size, &color);
		}
	}

	if (sparkActive) {
		float fade = (float)sparkTicksLeft / SPARK_DURATION;
		ColorFloat sparkColor = {1.0f, 1.0f, 1.0f, fade};
		Rectangle sparkRect = {-SPARK_SIZE, SPARK_SIZE, SPARK_SIZE, -SPARK_SIZE};
		Render_quad(&sparkPosition, 0.0, sparkRect, &sparkColor);
		Render_quad(&sparkPosition, 45.0, sparkRect, &sparkColor);
	}
}

void Sub_Pea_render_bloom_source(void)
{
	View view = View_get_view();

	const double UNSCALED_POINT_SIZE = 8.0;
	const double MIN_POINT_SIZE = 2.0;

	for (int i = 0; i < MAX_PROJECTILES; i++) {
		if (projectiles[i].active) {
			double size = UNSCALED_POINT_SIZE * view.scale;
			if (size < MIN_POINT_SIZE)
				size = MIN_POINT_SIZE;
			Render_point(&projectiles[i].position, size, &color);
		}
	}

	if (sparkActive) {
		float fade = (float)sparkTicksLeft / SPARK_DURATION;
		ColorFloat sparkColor = {1.0f, 1.0f, 1.0f, fade};
		Rectangle sparkRect = {-SPARK_SIZE, SPARK_SIZE, SPARK_SIZE, -SPARK_SIZE};
		Render_quad(&sparkPosition, 0.0, sparkRect, &sparkColor);
		Render_quad(&sparkPosition, 45.0, sparkRect, &sparkColor);
	}
}

bool Sub_Pea_check_hit(Rectangle target)
{
	for (int i = 0; i < MAX_PROJECTILES; i++) {
		Projectile *p = &projectiles[i];
		if (!p->active)
			continue;
		if (Collision_line_aabb_test(p->prevPosition.x, p->prevPosition.y,
				p->position.x, p->position.y, target, NULL)) {
			p->active = false;
			return true;
		}
	}
	return false;
}

void Sub_Pea_deactivate_all(void)
{
	for (int i = 0; i < MAX_PROJECTILES; i++)
		projectiles[i].active = false;
}

static double get_radians(double degrees)
{
	return degrees * 3.14159265358979323846 / 180.0;
}
