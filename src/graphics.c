#include "graphics.h"
#include "map_reflect.h"
#include "map_lighting.h"
#include "particle_instance.h"
#include "circuit_atlas.h"
#include "map_window.h"

#include <OpenGL/gl3.h>

#define TITLE_FONT_PATH "./resources/fonts/square_sans_serif_7.ttf"

static Graphics graphics = {0, HYBRID_START_FULLSCREEN};
static Shaders shaders;
static BatchRenderer batch;
static TextRenderer text_renderer;
static TextRenderer title_text_renderer;
static Bloom bloom;
static Bloom bg_bloom;
static Bloom disint_bloom;
static Bloom light_bloom;
static bool multisamplingEnabled = true;
static bool antialiasingEnabled = true;
static bool bloomEnabled = true;

static void create_window(void);
static void create_fullscreen_window(void);
static void create_windowed_window(void);
static void initialize_gl(void);
static void destroy_window(void);

void Graphics_initialize(void)
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
		SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
		SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	if (multisamplingEnabled) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
	} else {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}
	create_window();
	initialize_gl();
	Graphics_clear();
	Graphics_flip();
}

void Graphics_cleanup(void)
{
	MapWindow_cleanup();
	CircuitAtlas_cleanup();
	ParticleInstance_cleanup();
	MapLighting_cleanup();
	MapReflect_cleanup();
	Bloom_cleanup(&light_bloom);
	Bloom_cleanup(&disint_bloom);
	Bloom_cleanup(&bg_bloom);
	Bloom_cleanup(&bloom);
	Text_cleanup(&title_text_renderer);
	Text_cleanup(&text_renderer);
	Batch_cleanup(&batch);
	Shaders_cleanup(&shaders);
	destroy_window();
}

void Graphics_resize_window(const unsigned int width,
		const unsigned int height)
{
	graphics.screen.width = width;
	graphics.screen.height = height;

	int draw_w, draw_h;
	SDL_GL_GetDrawableSize(graphics.window, &draw_w, &draw_h);
	glViewport(0, 0, draw_w, draw_h);
	Bloom_resize(&bloom, draw_w, draw_h);
	Bloom_resize(&bg_bloom, draw_w, draw_h);
	Bloom_resize(&disint_bloom, draw_w, draw_h);
	Bloom_resize(&light_bloom, draw_w, draw_h);
}

void Graphics_toggle_fullscreen(void)
{
	Graphics_cleanup();
	graphics.fullScreen = !graphics.fullScreen;
	Graphics_initialize();
}

void Graphics_clear(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void Graphics_flip(void)
{
	SDL_GL_SwapWindow(graphics.window);
}

Mat4 Graphics_get_ui_projection(void)
{
	return Mat4_ortho(0, (float)graphics.screen.width,
		(float)graphics.screen.height, 0, -1, 1);
}

Mat4 Graphics_get_world_projection(void)
{
	return Mat4_ortho(0, (float)graphics.screen.width,
		0, (float)graphics.screen.height, -1, 1);
}

const Screen Graphics_get_screen(void)
{
	return graphics.screen;
}

Shaders *Graphics_get_shaders(void)
{
	return &shaders;
}

BatchRenderer *Graphics_get_batch(void)
{
	return &batch;
}

TextRenderer *Graphics_get_text_renderer(void)
{
	return &text_renderer;
}

TextRenderer *Graphics_get_title_text_renderer(void)
{
	return &title_text_renderer;
}

Bloom *Graphics_get_bloom(void)
{
	return &bloom;
}

Bloom *Graphics_get_bg_bloom(void)
{
	return &bg_bloom;
}

Bloom *Graphics_get_disint_bloom(void)
{
	return &disint_bloom;
}

Bloom *Graphics_get_light_bloom(void)
{
	return &light_bloom;
}

void Graphics_get_drawable_size(int *w, int *h)
{
	SDL_GL_GetDrawableSize(graphics.window, w, h);
}

void Graphics_set_multisampling(bool enabled)
{
	multisamplingEnabled = enabled;
}

void Graphics_set_antialiasing(bool enabled)
{
	antialiasingEnabled = enabled;
}

void Graphics_set_fullscreen(bool enabled)
{
	graphics.fullScreen = enabled;
}

void Graphics_recreate(void)
{
	Graphics_cleanup();
	Graphics_initialize();
}

bool Graphics_get_multisampling(void)
{
	return multisamplingEnabled;
}

bool Graphics_get_antialiasing(void)
{
	return antialiasingEnabled;
}

bool Graphics_get_fullscreen(void)
{
	return graphics.fullScreen;
}

void Graphics_set_bloom_enabled(bool enabled)
{
	bloomEnabled = enabled;
}

bool Graphics_get_bloom_enabled(void)
{
	return bloomEnabled;
}

static void create_window(void)
{
	if (graphics.fullScreen)
		create_fullscreen_window();
	else
		create_windowed_window();
}

static void create_fullscreen_window(void)
{
	graphics.window = SDL_CreateWindow(
		HYBRID_WINDOW_NAME,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		0, 0,
		SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI |
			SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN
	);

	/* Query actual logical size — avoids Retina scaling mismatch */
	int w, h;
	SDL_GetWindowSize(graphics.window, &w, &h);
	graphics.screen.width = w;
	graphics.screen.height = h;
}

static void create_windowed_window(void)
{
	graphics.window = SDL_CreateWindow(
		HYBRID_WINDOW_NAME,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		HYBRID_WINDOWED_WIDTH,
		HYBRID_WINDOWED_HEIGHT,
		SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI |
			SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
	);

	graphics.screen.width = HYBRID_WINDOWED_WIDTH;
	graphics.screen.height = HYBRID_WINDOWED_HEIGHT;
}

static void initialize_gl(void)
{
	graphics.glcontext = SDL_GL_CreateContext(graphics.window);
	SDL_GL_SetSwapInterval(1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_PROGRAM_POINT_SIZE);
	if (multisamplingEnabled)
		glEnable(GL_MULTISAMPLE);
	else
		glDisable(GL_MULTISAMPLE);
	glClearColor(0, 0, 0, 1);

	int draw_w, draw_h;
	SDL_GL_GetDrawableSize(graphics.window, &draw_w, &draw_h);
	glViewport(0, 0, draw_w, draw_h);

	Shaders_initialize(&shaders);
	Batch_initialize(&batch);
	Text_initialize(&text_renderer, TITLE_FONT_PATH, 14.0f);
	Text_initialize(&title_text_renderer, TITLE_FONT_PATH, 80.0f);
	Bloom_initialize(&bloom, draw_w, draw_h, 2, 2.0f, 5);
	Bloom_initialize(&bg_bloom, draw_w, draw_h, 8, 2.5f, 20);
	Bloom_initialize(&disint_bloom, draw_w, draw_h, 2, 3.0f, 7);
	Bloom_initialize(&light_bloom, draw_w, draw_h, 8, 1.0f, 7);
	MapReflect_initialize();
	MapLighting_initialize();
	CircuitAtlas_initialize();
	MapWindow_initialize();
}

static void destroy_window(void)
{
	SDL_GL_DeleteContext(graphics.glcontext);
	SDL_DestroyWindow(graphics.window);
}
