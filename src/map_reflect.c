#include "map_reflect.h"

#include <OpenGL/gl3.h>
#include <stdio.h>
#include <stdlib.h>

#include "graphics.h"
#include "map.h"
#include "map_lighting.h"
#include "render.h"
#include "view.h"

/* Tunable constants */
#define REFLECT_PARALLAX  0.001f
#define REFLECT_INTENSITY 2.5f

/* --- Embedded GLSL 330 core shaders --- */

static const char *reflect_vert_src =
	"#version 330 core\n"
	"layout(location = 0) in vec2 a_position;\n"
	"layout(location = 1) in vec2 a_texcoord;\n"
	"out vec2 v_texcoord;\n"
	"void main() {\n"
	"    gl_Position = vec4(a_position, 0.0, 1.0);\n"
	"    v_texcoord = a_texcoord;\n"
	"}\n";

static const char *reflect_frag_src =
	"#version 330 core\n"
	"in vec2 v_texcoord;\n"
	"uniform sampler2D u_cloud_tex;\n"
	"uniform vec2 u_screen_size;\n"
	"uniform vec2 u_camera_offset;\n"
	"uniform float u_intensity;\n"
	"out vec4 fragColor;\n"
	"void main() {\n"
	"    vec2 uv = vec2(1.0) - gl_FragCoord.xy / u_screen_size + u_camera_offset;\n"
	"    vec3 cloud = texture(u_cloud_tex, uv).rgb;\n"
	"    fragColor = vec4(cloud * u_intensity, 1.0);\n"
	"}\n";

/* --- GL resources --- */

static bool reflectEnabled = true;

static GLuint reflect_program;
static GLint u_cloud_tex;
static GLint u_screen_size;
static GLint u_camera_offset;
static GLint u_intensity;

static GLuint quad_vao, quad_vbo;

/* --- Shader helpers --- */

static GLuint compile_shader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		fprintf(stderr, "MapReflect shader compile error: %s\n", log);
		exit(1);
	}
	return shader;
}

static GLuint link_program(GLuint vert, GLuint frag)
{
	GLuint program = glCreateProgram();
	glAttachShader(program, vert);
	glAttachShader(program, frag);
	glLinkProgram(program);

	GLint ok;
	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetProgramInfoLog(program, sizeof(log), NULL, log);
		fprintf(stderr, "MapReflect shader link error: %s\n", log);
		exit(1);
	}

	glDeleteShader(vert);
	glDeleteShader(frag);
	return program;
}

/* --- Fullscreen quad --- */

static const float quad_vertices[] = {
	/* pos        texcoord */
	-1.0f, -1.0f,  0.0f, 0.0f,
	 1.0f, -1.0f,  1.0f, 0.0f,
	 1.0f,  1.0f,  1.0f, 1.0f,
	-1.0f, -1.0f,  0.0f, 0.0f,
	 1.0f,  1.0f,  1.0f, 1.0f,
	-1.0f,  1.0f,  0.0f, 1.0f
};

/* --- Public API --- */

void MapReflect_initialize(void)
{
	/* Build shader */
	GLuint vert = compile_shader(GL_VERTEX_SHADER, reflect_vert_src);
	GLuint frag = compile_shader(GL_FRAGMENT_SHADER, reflect_frag_src);
	reflect_program = link_program(vert, frag);

	u_cloud_tex = glGetUniformLocation(reflect_program, "u_cloud_tex");
	u_screen_size = glGetUniformLocation(reflect_program, "u_screen_size");
	u_camera_offset = glGetUniformLocation(reflect_program, "u_camera_offset");
	u_intensity = glGetUniformLocation(reflect_program, "u_intensity");

	/* Build fullscreen quad */
	glGenVertexArrays(1, &quad_vao);
	glGenBuffers(1, &quad_vbo);
	glBindVertexArray(quad_vao);
	glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices),
		quad_vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float), (void *)(2 * sizeof(float)));

	glBindVertexArray(0);
}

void MapReflect_cleanup(void)
{
	glDeleteBuffers(1, &quad_vbo);
	glDeleteVertexArrays(1, &quad_vao);
	glDeleteProgram(reflect_program);
}

void MapReflect_set_enabled(bool enabled)
{
	reflectEnabled = enabled;
}

bool MapReflect_get_enabled(void)
{
	return reflectEnabled;
}

void MapReflect_render(const Mat4 *world_proj, const Mat4 *view,
	int draw_w, int draw_h)
{
	Bloom *bg_bloom = Graphics_get_bg_bloom();
	if (!bg_bloom->valid) return;

	/* Skip everything if both reflection and lighting are off */
	if (!reflectEnabled && !MapLighting_get_enabled())
		return;

	/* 1. Stencil write pass: circuit=1, solid=2 (shared with lighting) */
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilMask(0xFF);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	/* Map_render_stencil_mask_all manages stencilFunc refs and flushes internally */
	Map_render_stencil_mask_all(world_proj, view);

	/* 2. Reflection pass: draw fullscreen quad where stencil == 2 (solid only) */
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	if (reflectEnabled) {
		glStencilFunc(GL_EQUAL, 2, 0xFF);
		glStencilMask(0x00);

		/* Additive blend */
		glBlendFunc(GL_ONE, GL_ONE);

		/* Compute parallax camera offset */
		View cam = View_get_view();
		float cam_offset_x = -(float)cam.position.x * REFLECT_PARALLAX / (float)draw_w;
		float cam_offset_y = -(float)cam.position.y * REFLECT_PARALLAX / (float)draw_h;

		glUseProgram(reflect_program);
		glUniform1i(u_cloud_tex, 0);
		glUniform2f(u_screen_size, (float)draw_w, (float)draw_h);
		glUniform2f(u_camera_offset, cam_offset_x, cam_offset_y);
		glUniform1f(u_intensity, REFLECT_INTENSITY);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, bg_bloom->pong_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

		glBindVertexArray(quad_vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		/* Restore bloom texture wrapping */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	/* 3. Restore stencil state.
	   Stencil buffer DATA is preserved for the lighting pass that follows. */
	glStencilMask(0xFF);
	glDisable(GL_STENCIL_TEST);
}
