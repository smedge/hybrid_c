#include "bloom.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- Embedded GLSL 330 core shaders ---- */

static const char *fullscreen_vert_src =
	"#version 330 core\n"
	"layout(location = 0) in vec2 a_position;\n"
	"layout(location = 1) in vec2 a_texcoord;\n"
	"out vec2 v_texcoord;\n"
	"void main() {\n"
	"    gl_Position = vec4(a_position, 0.0, 1.0);\n"
	"    v_texcoord = a_texcoord;\n"
	"}\n";

static const char *blur_frag_src =
	"#version 330 core\n"
	"in vec2 v_texcoord;\n"
	"uniform sampler2D u_image;\n"
	"uniform bool u_horizontal;\n"
	"uniform vec2 u_texel_size;\n"
	"out vec4 fragColor;\n"
	"void main() {\n"
	"    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);\n"
	"    vec2 dir = u_horizontal ? vec2(u_texel_size.x, 0.0) : vec2(0.0, u_texel_size.y);\n"
	"    vec3 result = texture(u_image, v_texcoord).rgb * weights[0];\n"
	"    for (int i = 1; i < 5; i++) {\n"
	"        result += texture(u_image, v_texcoord + dir * float(i)).rgb * weights[i];\n"
	"        result += texture(u_image, v_texcoord - dir * float(i)).rgb * weights[i];\n"
	"    }\n"
	"    fragColor = vec4(result, 1.0);\n"
	"}\n";

static const char *composite_frag_src =
	"#version 330 core\n"
	"in vec2 v_texcoord;\n"
	"uniform sampler2D u_image;\n"
	"uniform float u_intensity;\n"
	"out vec4 fragColor;\n"
	"void main() {\n"
	"    vec3 bloom = texture(u_image, v_texcoord).rgb;\n"
	"    fragColor = vec4(bloom * u_intensity, 1.0);\n"
	"}\n";

/* ---- Shader helpers (duplicated from shader.c since they're static) ---- */

static GLuint bloom_compile_shader(GLenum type, const char *source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		fprintf(stderr, "Bloom shader compile error: %s\n", log);
		exit(1);
	}
	return shader;
}

static GLuint bloom_link_program(GLuint vert, GLuint frag)
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
		fprintf(stderr, "Bloom shader link error: %s\n", log);
		exit(1);
	}

	glDeleteShader(vert);
	glDeleteShader(frag);
	return program;
}

/* ---- Fullscreen quad ---- */

static const float quad_vertices[] = {
	/* pos        texcoord */
	-1.0f, -1.0f,  0.0f, 0.0f,
	 1.0f, -1.0f,  1.0f, 0.0f,
	 1.0f,  1.0f,  1.0f, 1.0f,
	-1.0f, -1.0f,  0.0f, 0.0f,
	 1.0f,  1.0f,  1.0f, 1.0f,
	-1.0f,  1.0f,  0.0f, 1.0f
};

static void create_quad(Bloom *bloom)
{
	glGenVertexArrays(1, &bloom->quad_vao);
	glGenBuffers(1, &bloom->quad_vbo);
	glBindVertexArray(bloom->quad_vao);
	glBindBuffer(GL_ARRAY_BUFFER, bloom->quad_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices),
		quad_vertices, GL_STATIC_DRAW);

	/* position: location 0 */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float), (void *)0);

	/* texcoord: location 1 */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float), (void *)(2 * sizeof(float)));

	glBindVertexArray(0);
}

/* ---- FBO helpers ---- */

static bool create_fbo(GLuint *fbo, GLuint *tex, int w, int h)
{
	glGenFramebuffers(1, fbo);
	glGenTextures(1, tex);

	glBindTexture(GL_TEXTURE_2D, *tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0,
		GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, *tex, 0);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "Bloom FBO incomplete: 0x%x\n", status);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}

static void destroy_fbo(GLuint *fbo, GLuint *tex)
{
	glDeleteFramebuffers(1, fbo);
	glDeleteTextures(1, tex);
	*fbo = 0;
	*tex = 0;
}

/* ---- Public API ---- */

void Bloom_initialize(Bloom *bloom, int draw_w, int draw_h)
{
	bloom->intensity = 2.0f;
	bloom->blur_passes = 5;
	bloom->divisor = 2;

	bloom->width = draw_w / bloom->divisor;
	bloom->height = draw_h / bloom->divisor;

	bool ok = true;
	if (!create_fbo(&bloom->source_fbo, &bloom->source_tex,
		bloom->width, bloom->height))
		ok = false;
	if (!create_fbo(&bloom->ping_fbo, &bloom->ping_tex,
		bloom->width, bloom->height))
		ok = false;
	if (!create_fbo(&bloom->pong_fbo, &bloom->pong_tex,
		bloom->width, bloom->height))
		ok = false;

	bloom->valid = ok;

	create_quad(bloom);

	/* Build shaders */
	GLuint vert = bloom_compile_shader(GL_VERTEX_SHADER, fullscreen_vert_src);

	GLuint blur_frag = bloom_compile_shader(GL_FRAGMENT_SHADER, blur_frag_src);
	bloom->blur_program = bloom_link_program(vert, blur_frag);
	bloom->blur_u_image = glGetUniformLocation(bloom->blur_program, "u_image");
	bloom->blur_u_horizontal = glGetUniformLocation(bloom->blur_program, "u_horizontal");
	bloom->blur_u_texel_size = glGetUniformLocation(bloom->blur_program, "u_texel_size");

	/* Need a fresh vert since link_program deletes shaders */
	vert = bloom_compile_shader(GL_VERTEX_SHADER, fullscreen_vert_src);
	GLuint comp_frag = bloom_compile_shader(GL_FRAGMENT_SHADER, composite_frag_src);
	bloom->composite_program = bloom_link_program(vert, comp_frag);
	bloom->composite_u_image = glGetUniformLocation(bloom->composite_program, "u_image");
	bloom->composite_u_intensity = glGetUniformLocation(bloom->composite_program, "u_intensity");
}

void Bloom_cleanup(Bloom *bloom)
{
	destroy_fbo(&bloom->source_fbo, &bloom->source_tex);
	destroy_fbo(&bloom->ping_fbo, &bloom->ping_tex);
	destroy_fbo(&bloom->pong_fbo, &bloom->pong_tex);

	glDeleteBuffers(1, &bloom->quad_vbo);
	glDeleteVertexArrays(1, &bloom->quad_vao);

	glDeleteProgram(bloom->blur_program);
	glDeleteProgram(bloom->composite_program);
}

void Bloom_resize(Bloom *bloom, int draw_w, int draw_h)
{
	destroy_fbo(&bloom->source_fbo, &bloom->source_tex);
	destroy_fbo(&bloom->ping_fbo, &bloom->ping_tex);
	destroy_fbo(&bloom->pong_fbo, &bloom->pong_tex);

	bloom->width = draw_w / bloom->divisor;
	bloom->height = draw_h / bloom->divisor;

	bool ok = true;
	if (!create_fbo(&bloom->source_fbo, &bloom->source_tex,
		bloom->width, bloom->height))
		ok = false;
	if (!create_fbo(&bloom->ping_fbo, &bloom->ping_tex,
		bloom->width, bloom->height))
		ok = false;
	if (!create_fbo(&bloom->pong_fbo, &bloom->pong_tex,
		bloom->width, bloom->height))
		ok = false;
	bloom->valid = ok;
}

void Bloom_begin_source(Bloom *bloom)
{
	if (!bloom->valid) return;
	glBindFramebuffer(GL_FRAMEBUFFER, bloom->source_fbo);
	glViewport(0, 0, bloom->width, bloom->height);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Bloom_end_source(Bloom *bloom, int draw_w, int draw_h)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, draw_w, draw_h);
}

void Bloom_blur(Bloom *bloom)
{
	if (!bloom->valid) return;
	glUseProgram(bloom->blur_program);
	glUniform1i(bloom->blur_u_image, 0);
	glUniform2f(bloom->blur_u_texel_size,
		1.0f / (float)bloom->width,
		1.0f / (float)bloom->height);

	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(bloom->quad_vao);

	GLuint read_tex = bloom->source_tex;
	int horizontal = 1;

	glViewport(0, 0, bloom->width, bloom->height);

	for (int i = 0; i < bloom->blur_passes * 2; i++) {
		GLuint write_fbo = horizontal ? bloom->ping_fbo : bloom->pong_fbo;

		glBindFramebuffer(GL_FRAMEBUFFER, write_fbo);
		glBindTexture(GL_TEXTURE_2D, read_tex);
		glUniform1i(bloom->blur_u_horizontal, horizontal);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		read_tex = horizontal ? bloom->ping_tex : bloom->pong_tex;
		horizontal = !horizontal;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindVertexArray(0);
}

void Bloom_composite(Bloom *bloom, int draw_w, int draw_h)
{
	if (!bloom->valid) return;
	/* The last blur pass wrote to whichever was the write target.
	   With blur_passes=3, we do 6 passes (H,V,H,V,H,V).
	   Pass 0: H→ping, Pass 1: V→pong, Pass 2: H→ping,
	   Pass 3: V→pong, Pass 4: H→ping, Pass 5: V→pong
	   Last pass is V→pong, so result is in pong_tex. */
	GLuint result_tex = bloom->pong_tex;

	glViewport(0, 0, draw_w, draw_h);
	glBlendFunc(GL_ONE, GL_ONE);

	glUseProgram(bloom->composite_program);
	glUniform1i(bloom->composite_u_image, 0);
	glUniform1f(bloom->composite_u_intensity, bloom->intensity);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, result_tex);

	glBindVertexArray(bloom->quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
