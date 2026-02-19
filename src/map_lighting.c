#include "map_lighting.h"

#include <OpenGL/gl3.h>
#include <stdio.h>
#include <stdlib.h>

#include "graphics.h"

/* --- Embedded GLSL 330 core shaders --- */

static const char *lighting_vert_src =
	"#version 330 core\n"
	"layout(location = 0) in vec2 a_position;\n"
	"layout(location = 1) in vec2 a_texcoord;\n"
	"out vec2 v_texcoord;\n"
	"void main() {\n"
	"    gl_Position = vec4(a_position, 0.0, 1.0);\n"
	"    v_texcoord = a_texcoord;\n"
	"}\n";

static const char *lighting_frag_src =
	"#version 330 core\n"
	"in vec2 v_texcoord;\n"
	"uniform sampler2D u_image;\n"
	"uniform float u_intensity;\n"
	"out vec4 fragColor;\n"
	"void main() {\n"
	"    vec3 light = texture(u_image, v_texcoord).rgb;\n"
	"    fragColor = vec4(light * u_intensity, 1.0);\n"
	"}\n";

/* --- GL resources --- */

static bool lightingEnabled = true;

static GLuint lighting_program;
static GLint u_image;
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
		fprintf(stderr, "MapLighting shader compile error: %s\n", log);
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
		fprintf(stderr, "MapLighting shader link error: %s\n", log);
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

void MapLighting_initialize(void)
{
	GLuint vert = compile_shader(GL_VERTEX_SHADER, lighting_vert_src);
	GLuint frag = compile_shader(GL_FRAGMENT_SHADER, lighting_frag_src);
	lighting_program = link_program(vert, frag);

	u_image = glGetUniformLocation(lighting_program, "u_image");
	u_intensity = glGetUniformLocation(lighting_program, "u_intensity");

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

void MapLighting_cleanup(void)
{
	glDeleteBuffers(1, &quad_vbo);
	glDeleteVertexArrays(1, &quad_vao);
	glDeleteProgram(lighting_program);
}

void MapLighting_set_enabled(bool enabled)
{
	lightingEnabled = enabled;
}

bool MapLighting_get_enabled(void)
{
	return lightingEnabled;
}

void MapLighting_render(int draw_w, int draw_h)
{
	if (!lightingEnabled) return;
	Bloom *lb = Graphics_get_light_bloom();
	if (!lb->valid) return;

	/* Restore full-res viewport (Bloom_blur leaves it at quarter-res) */
	glViewport(0, 0, draw_w, draw_h);

	/* Stencil buffer was written by Map_render_stencil_mask_all (circuit=1, solid=2).
	   Enable stencil test: pass where stencil >= 1 (all map cells). */
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_LEQUAL, 1, 0xFF);
	glStencilMask(0x00);

	/* Additive blend */
	glBlendFunc(GL_ONE, GL_ONE);

	glUseProgram(lighting_program);
	glUniform1i(u_image, 0);
	glUniform1f(u_intensity, lb->intensity);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, lb->pong_tex);

	glBindVertexArray(quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	/* Restore state — stencil mask MUST be 0xFF so glClear can wipe it */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glStencilMask(0xFF);
	glDisable(GL_STENCIL_TEST);
}
