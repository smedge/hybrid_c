#include "sub_gravwell.h"

#include "player_stats.h"
#include "ship.h"
#include "audio.h"
#include "render.h"
#include "graphics.h"
#include "view.h"
#include "sub_stealth.h"
#include "skillbar.h"

#include <OpenGL/gl3.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Timing */
#define DURATION_MS      8000
#define FADE_MS          2000
#define COOLDOWN_MS      20000
#define FEEDBACK_COST    25.0

/* Pull physics */
#define RADIUS           600.0
#define PULL_MIN         20.0
#define PULL_MAX         300.0
#define SLOW_EDGE        0.6
#define SLOW_CENTER      0.05

/* ================================================================
 * Instanced whirlpool blob system
 * ================================================================ */

#define BLOB_COUNT 640
#define BLOB_BASE_SIZE 14.0f
#define TRAIL_GHOSTS 8
#define TRAIL_DT 0.025f   /* 25ms between ghost positions */
#define MAX_INSTANCES (BLOB_COUNT * (1 + TRAIL_GHOSTS))

/* Per-blob CPU state */
typedef struct {
	float angle;         /* orbital position in radians */
	float radius_frac;   /* 0=center, 1=edge */
	float angular_speed; /* base radians/sec */
	float inward_speed;  /* base frac/sec */
	float size;          /* base size multiplier */
} WhirlpoolBlob;

/* Per-instance GPU data: 9 floats = 36 bytes */
typedef struct {
	float x, y;          /* world position */
	float size;          /* radius */
	float rotation;      /* radians */
	float stretch;       /* elongation along local X */
	float r, g, b, a;   /* color */
} InstanceData;

/* Instanced renderer GL resources */
static GLuint gwShaderProgram;
static GLint gwLocProjection;
static GLint gwLocView;
static GLuint gwVAO;
static GLuint gwTemplateVBO;
static GLuint gwInstanceVBO;
static bool gwGLInitialized;

/* Embedded shaders */
static const char *gw_vert_src =
	"#version 330 core\n"
	"layout(location = 0) in vec2 a_vertex;\n"
	"layout(location = 1) in vec2 a_position;\n"
	"layout(location = 2) in float a_size;\n"
	"layout(location = 3) in float a_rotation;\n"
	"layout(location = 4) in float a_stretch;\n"
	"layout(location = 5) in vec4 a_color;\n"
	"uniform mat4 u_projection;\n"
	"uniform mat4 u_view;\n"
	"out vec2 v_uv;\n"
	"out vec4 v_color;\n"
	"void main() {\n"
	"    vec2 scaled = vec2(a_vertex.x * a_stretch, a_vertex.y) * a_size;\n"
	"    float c = cos(a_rotation);\n"
	"    float s = sin(a_rotation);\n"
	"    vec2 rotated = vec2(scaled.x * c - scaled.y * s,\n"
	"                        scaled.x * s + scaled.y * c);\n"
	"    vec2 world = rotated + a_position;\n"
	"    gl_Position = u_projection * u_view * vec4(world, 0.0, 1.0);\n"
	"    v_uv = a_vertex;\n"
	"    v_color = a_color;\n"
	"}\n";

static const char *gw_frag_src =
	"#version 330 core\n"
	"in vec2 v_uv;\n"
	"in vec4 v_color;\n"
	"out vec4 fragColor;\n"
	"void main() {\n"
	"    float d = length(v_uv);\n"
	"    float alpha = smoothstep(1.0, 0.2, d);\n"
	"    fragColor = vec4(v_color.rgb, v_color.a * alpha);\n"
	"}\n";

/* State */
static bool wellActive;
static Position wellPosition;
static int durationMs;
static int cooldownMs;
static float animTimer;

/* Cached mouse world position from last update */
static Position cachedCursorWorld;

static WhirlpoolBlob blobs[BLOB_COUNT];
static InstanceData instanceData[MAX_INSTANCES];

static Mix_Chunk *samplePlace = 0;
static Mix_Chunk *sampleExpire = 0;

/* ================================================================
 * GL resource management
 * ================================================================ */

static GLuint gw_compile_shader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		fprintf(stderr, "Gravwell shader compile error: %s\n", log);
		exit(1);
	}
	return shader;
}

static void gw_init_gl(void)
{
	if (gwGLInitialized) return;
	gwGLInitialized = true;

	/* Compile & link shader program */
	GLuint vert = gw_compile_shader(GL_VERTEX_SHADER, gw_vert_src);
	GLuint frag = gw_compile_shader(GL_FRAGMENT_SHADER, gw_frag_src);

	gwShaderProgram = glCreateProgram();
	glAttachShader(gwShaderProgram, vert);
	glAttachShader(gwShaderProgram, frag);
	glLinkProgram(gwShaderProgram);

	GLint ok;
	glGetProgramiv(gwShaderProgram, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetProgramInfoLog(gwShaderProgram, sizeof(log), NULL, log);
		fprintf(stderr, "Gravwell shader link error: %s\n", log);
		exit(1);
	}
	glDeleteShader(vert);
	glDeleteShader(frag);

	gwLocProjection = glGetUniformLocation(gwShaderProgram, "u_projection");
	gwLocView = glGetUniformLocation(gwShaderProgram, "u_view");

	/* Template quad: unit quad from (-1,-1) to (1,1), 6 vertices */
	float quad[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f,  1.0f,
	};

	glGenBuffers(1, &gwTemplateVBO);
	glBindBuffer(GL_ARRAY_BUFFER, gwTemplateVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

	/* Instance data VBO (dynamic) */
	glGenBuffers(1, &gwInstanceVBO);

	/* VAO */
	glGenVertexArrays(1, &gwVAO);
	glBindVertexArray(gwVAO);

	/* Attribute 0: template quad vertex (per-vertex) */
	glBindBuffer(GL_ARRAY_BUFFER, gwTemplateVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
	glVertexAttribDivisor(0, 0);

	/* Attributes 1-5: per-instance data */
	glBindBuffer(GL_ARRAY_BUFFER, gwInstanceVBO);
	int stride = sizeof(InstanceData);

	/* location 1: a_position (vec2) — offset 0 */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
	glVertexAttribDivisor(1, 1);

	/* location 2: a_size (float) — offset 8 */
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void *)(2 * sizeof(float)));
	glVertexAttribDivisor(2, 1);

	/* location 3: a_rotation (float) — offset 12 */
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
	glVertexAttribDivisor(3, 1);

	/* location 4: a_stretch (float) — offset 16 */
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void *)(4 * sizeof(float)));
	glVertexAttribDivisor(4, 1);

	/* location 5: a_color (vec4) — offset 20 */
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void *)(5 * sizeof(float)));
	glVertexAttribDivisor(5, 1);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void gw_cleanup_gl(void)
{
	if (!gwGLInitialized) return;
	glDeleteProgram(gwShaderProgram);
	glDeleteVertexArrays(1, &gwVAO);
	glDeleteBuffers(1, &gwTemplateVBO);
	glDeleteBuffers(1, &gwInstanceVBO);
	gwGLInitialized = false;
}

/* Draw instances with current projection/view matrices */
static void gw_draw_instanced(const InstanceData *data, int count,
	const Mat4 *projection, const Mat4 *view)
{
	if (count <= 0) return;

	glUseProgram(gwShaderProgram);
	glUniformMatrix4fv(gwLocProjection, 1, GL_FALSE, projection->m);
	glUniformMatrix4fv(gwLocView, 1, GL_FALSE, view->m);

	glBindVertexArray(gwVAO);
	glBindBuffer(GL_ARRAY_BUFFER, gwInstanceVBO);
	glBufferData(GL_ARRAY_BUFFER, count * sizeof(InstanceData),
		data, GL_DYNAMIC_DRAW);

	glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);

	glBindVertexArray(0);
	glUseProgram(0);
}

/* ================================================================
 * Blob particle system
 * ================================================================ */

/* Spawn blob at outer edge — used for continuous respawning */
static void respawn_blob(WhirlpoolBlob *b)
{
	b->angle = (float)(rand() % 6283) / 1000.0f;
	b->radius_frac = 0.88f + (float)(rand() % 120) / 1000.0f;
	b->angular_speed = 1.8f * (0.7f + (float)(rand() % 600) / 1000.0f);
	b->inward_speed = 0.08f * (0.7f + (float)(rand() % 600) / 1000.0f);
	b->size = BLOB_BASE_SIZE * (0.7f + (float)(rand() % 600) / 1000.0f);
}

/* Spawn blob anywhere across the radius — fill the vortex instantly on activation */
static void init_blob(WhirlpoolBlob *b)
{
	respawn_blob(b);
	b->radius_frac = 0.06f + (float)(rand() % 940) / 1000.0f;
}

/* Thin bright rim → swirling blues/indigos → deep black core */
static void blob_color_by_radius(float radius_frac, float fade,
	float *r, float *g, float *b, float *a)
{
	float t = radius_frac;

	if (t > 0.85f) {
		/* Thin bright rim — white-blue */
		float f = (t - 0.85f) / 0.15f;
		*r = 0.15f + f * 0.45f;
		*g = 0.25f + f * 0.55f;
		*b = 0.5f  + f * 0.5f;
	} else if (t > 0.5f) {
		/* Mid zone — swirling blues */
		float f = (t - 0.5f) / 0.35f;
		*r = 0.04f + f * 0.11f;
		*g = 0.06f + f * 0.19f;
		*b = 0.15f + f * 0.35f;
	} else if (t > 0.25f) {
		/* Deep indigo transition */
		float f = (t - 0.25f) / 0.25f;
		*r = 0.01f + f * 0.03f;
		*g = 0.01f + f * 0.05f;
		*b = 0.03f + f * 0.12f;
	} else {
		/* Black core */
		*r = 0.01f;
		*g = 0.01f;
		*b = 0.03f;
	}

	*a = fade;
}

/* Fill instance data array with ghost trails, returns count */
static int build_instance_data(InstanceData *out, float brightness_boost,
	float min_radius_frac)
{
	float fade = 1.0f;
	if (durationMs < FADE_MS)
		fade = (float)durationMs / FADE_MS;

	float cx = (float)wellPosition.x;
	float cy = (float)wellPosition.y;
	float radius = (float)RADIUS;

	int count = 0;
	for (int i = 0; i < BLOB_COUNT; i++) {
		WhirlpoolBlob *bl = &blobs[i];

		if (bl->radius_frac < min_radius_frac)
			continue;

		/* Color from radial position */
		float cr, cg, cb, ca;
		blob_color_by_radius(bl->radius_frac, fade, &cr, &cg, &cb, &ca);
		if (ca <= 0.01f) continue;

		/* Compute current angular velocity for trail spacing */
		float r_clamped = bl->radius_frac < 0.06f ? 0.06f : bl->radius_frac;
		float speed_mult = 1.0f / (r_clamped * r_clamped);
		if (speed_mult > 200.0f) speed_mult = 200.0f;
		float ang_vel = bl->angular_speed * speed_mult;

		float pr = radius * bl->radius_frac * fade;

		/* Render blob + ghost trail behind it (counterclockwise motion,
		   so ghosts trail at earlier/smaller angles) */
		for (int g = 0; g <= TRAIL_GHOSTS; g++) {
			float ghost_angle = bl->angle - (float)g * ang_vel * TRAIL_DT;
			float ghost_t = (float)g / (float)(TRAIL_GHOSTS + 1);
			float ghost_alpha = ca * (1.0f - ghost_t);

			if (ghost_alpha <= 0.01f) break;

			float px = cx + cosf(ghost_angle) * pr;
			float py = cy + sinf(ghost_angle) * pr;

			/* Tangent direction (counterclockwise = +PI/2) */
			float tangent_angle = ghost_angle + (float)M_PI * 0.5f;

			InstanceData *inst = &out[count++];
			inst->x = px;
			inst->y = py;
			inst->size = bl->size;
			inst->rotation = tangent_angle;
			inst->stretch = 1.5f;
			inst->r = cr * brightness_boost;
			inst->g = cg * brightness_boost;
			inst->b = cb * brightness_boost;
			inst->a = ghost_alpha;

			if (count >= MAX_INSTANCES) return count;
		}
	}
	return count;
}

/* ================================================================
 * Public API — lifecycle
 * ================================================================ */

void Sub_Gravwell_initialize(void)
{
	wellActive = false;
	durationMs = 0;
	cooldownMs = 0;
	animTimer = 0.0f;
	gwGLInitialized = false;
	cachedCursorWorld.x = 0.0;
	cachedCursorWorld.y = 0.0;

	for (int i = 0; i < BLOB_COUNT; i++)
		init_blob(&blobs[i]);

	Audio_load_sample(&samplePlace, "resources/sounds/bomb_set.wav");
	Audio_load_sample(&sampleExpire, "resources/sounds/door.wav");
}

void Sub_Gravwell_cleanup(void)
{
	wellActive = false;
	cooldownMs = 0;
	gw_cleanup_gl();
	Audio_unload_sample(&samplePlace);
	Audio_unload_sample(&sampleExpire);
}

void Sub_Gravwell_update(const Input *userInput, unsigned int ticks)
{
	/* Cache cursor world position for try_activate */
	Screen screen = Graphics_get_screen();
	Position cursor = {userInput->mouseX, userInput->mouseY};
	cachedCursorWorld = View_get_world_position(&screen, cursor);

	/* Cooldown tick */
	if (cooldownMs > 0) {
		cooldownMs -= (int)ticks;
		if (cooldownMs < 0) cooldownMs = 0;
	}

	if (!wellActive)
		return;

	/* Duration tick */
	durationMs -= (int)ticks;
	if (durationMs <= 0) {
		wellActive = false;
		Audio_play_sample(&sampleExpire);
		return;
	}

	/* Animate */
	float dt = ticks / 1000.0f;
	animTimer += dt;

	/* Fade multiplier for last 2 seconds */
	float fade = 1.0f;
	if (durationMs < FADE_MS)
		fade = (float)durationMs / FADE_MS;

	/* Update whirlpool blobs — spiral inward */
	for (int i = 0; i < BLOB_COUNT; i++) {
		WhirlpoolBlob *bl = &blobs[i];

		/* Angular velocity: 1/r^2 — dramatic acceleration into center */
		float r_clamped = bl->radius_frac < 0.06f ? 0.06f : bl->radius_frac;
		float speed_mult = 1.0f / (r_clamped * r_clamped);
		if (speed_mult > 200.0f) speed_mult = 200.0f;
		/* All counterclockwise (positive angle in world space) */
		bl->angle += bl->angular_speed * speed_mult * dt;

		/* Keep angle in [0, 2PI] */
		if (bl->angle > 2.0f * (float)M_PI)
			bl->angle -= 2.0f * (float)M_PI;

		/* Inward speed accelerates toward center — blobs linger at edge,
		   streak through the middle. depth^2 ramp. */
		float depth = 1.0f - bl->radius_frac;
		float inward = bl->inward_speed * (1.0f + depth * depth * 4.0f);
		if (fade < 1.0f)
			inward *= 3.0f;
		bl->radius_frac -= inward * dt;

		/* Respawn at outer edge when reaching center */
		if (bl->radius_frac < 0.05f) {
			if (fade >= 1.0f) {
				respawn_blob(&blobs[i]);
			} else {
				bl->radius_frac = 0.01f; /* clamp, let it collapse */
			}
		}
	}
}

/* ================================================================
 * Public API — activation / mechanics
 * ================================================================ */

void Sub_Gravwell_try_activate(void)
{
	if (Ship_is_destroyed())
		return;
	if (cooldownMs > 0)
		return;

	/* Pay feedback cost */
	PlayerStats_add_feedback(FEEDBACK_COST);

	/* Break stealth */
	Sub_Stealth_break_attack();

	/* Replace existing well */
	wellActive = true;
	wellPosition = cachedCursorWorld;
	durationMs = DURATION_MS;
	cooldownMs = COOLDOWN_MS;
	animTimer = 0.0f;

	/* Reset blobs */
	for (int i = 0; i < BLOB_COUNT; i++)
		init_blob(&blobs[i]);

	Audio_play_sample(&samplePlace);
}

/* ================================================================
 * Public API — rendering
 * ================================================================ */

void Sub_Gravwell_render(void)
{
	if (!wellActive)
		return;

	gw_init_gl();

	float fade = 1.0f;
	if (durationMs < FADE_MS)
		fade = (float)durationMs / FADE_MS;

	/* Solid black circle base — covers background so the void is truly black */
	float cx = (float)wellPosition.x;
	float cy = (float)wellPosition.y;
	Render_filled_circle(cx, cy, (float)RADIUS * 0.85f * fade, 48,
		0.0f, 0.0f, 0.0f, fade);

	/* Flush batch (including black circle) before instanced pass */
	Mat4 world_proj = Graphics_get_world_projection();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);
	Render_flush(&world_proj, &view);

	/* Build instance data (all blobs, 1.0x brightness) */
	int count = build_instance_data(instanceData, 1.0f, 0.0f);

	/* Draw instanced */
	gw_draw_instanced(instanceData, count, &world_proj, &view);
}

void Sub_Gravwell_render_bloom_source(void)
{
	if (!wellActive)
		return;

	gw_init_gl();

	/* Flush pending batch geometry in the bloom FBO */
	Mat4 world_proj = Graphics_get_world_projection();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);
	Render_flush(&world_proj, &view);

	/* Only outer blobs (radius_frac > 0.5) at 1.5x brightness for bloom */
	int count = build_instance_data(instanceData, 1.5f, 0.5f);

	gw_draw_instanced(instanceData, count, &world_proj, &view);
}

/* ================================================================
 * Public API — query / state
 * ================================================================ */

void Sub_Gravwell_deactivate_all(void)
{
	wellActive = false;
}

float Sub_Gravwell_get_cooldown_fraction(void)
{
	if (wellActive)
		return 0.0f;
	if (cooldownMs > 0)
		return (float)cooldownMs / COOLDOWN_MS;
	return 0.0f;
}

bool Sub_Gravwell_is_active(void)
{
	return wellActive;
}

bool Sub_Gravwell_get_pull(Position pos, double dt,
	double *out_dx, double *out_dy, double *out_speed_mult)
{
	if (!wellActive) {
		*out_dx = 0.0;
		*out_dy = 0.0;
		*out_speed_mult = 1.0;
		return false;
	}

	double dx = wellPosition.x - pos.x;
	double dy = wellPosition.y - pos.y;
	double dist = sqrt(dx * dx + dy * dy);

	if (dist > RADIUS) {
		*out_dx = 0.0;
		*out_dy = 0.0;
		*out_speed_mult = 1.0;
		return false;
	}

	/* t = 0 at center, 1 at edge */
	double t = dist / RADIUS;
	if (t < 0.001) t = 0.001;

	/* Fade-out: last 2 seconds, pull and slow linearly ramp to zero */
	double fade = 1.0;
	if (durationMs < FADE_MS)
		fade = (double)durationMs / FADE_MS;

	/* Pull formula: pull = MIN + (MAX - MIN) * (1 - t^2) */
	double pull = PULL_MIN + (PULL_MAX - PULL_MIN) * (1.0 - t * t);
	pull *= fade;

	/* Direction toward center */
	double nx = dx / dist;
	double ny = dy / dist;

	*out_dx = nx * pull * dt;
	*out_dy = ny * pull * dt;

	/* Slow formula: slow = EDGE + (CENTER - EDGE) * (1 - t)^2 */
	double inv_t = 1.0 - t;
	double slow = SLOW_EDGE + (SLOW_CENTER - SLOW_EDGE) * inv_t * inv_t;
	/* Fade slow back to 1.0 */
	*out_speed_mult = slow + (1.0 - slow) * (1.0 - fade);

	return true;
}
