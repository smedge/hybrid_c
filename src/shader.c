#include "shader.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- GLSL 330 core shaders ---- */

static const char *color_vert_src =
	"#version 330 core\n"
	"layout(location = 0) in vec2 a_position;\n"
	"layout(location = 1) in vec4 a_color;\n"
	"layout(location = 2) in float a_pointSize;\n"
	"uniform mat4 u_projection;\n"
	"uniform mat4 u_view;\n"
	"out vec4 v_color;\n"
	"void main() {\n"
	"    gl_Position = u_projection * u_view * vec4(a_position, 0.0, 1.0);\n"
	"    gl_PointSize = a_pointSize;\n"
	"    v_color = a_color;\n"
	"}\n";

static const char *color_frag_src =
	"#version 330 core\n"
	"in vec4 v_color;\n"
	"out vec4 fragColor;\n"
	"void main() {\n"
	"    fragColor = v_color;\n"
	"}\n";

static const char *text_vert_src =
	"#version 330 core\n"
	"layout(location = 0) in vec2 a_position;\n"
	"layout(location = 1) in vec4 a_color;\n"
	"layout(location = 2) in vec2 a_texcoord;\n"
	"uniform mat4 u_projection;\n"
	"uniform mat4 u_view;\n"
	"out vec4 v_color;\n"
	"out vec2 v_texcoord;\n"
	"void main() {\n"
	"    gl_Position = u_projection * u_view * vec4(a_position, 0.0, 1.0);\n"
	"    v_color = a_color;\n"
	"    v_texcoord = a_texcoord;\n"
	"}\n";

static const char *text_frag_src =
	"#version 330 core\n"
	"in vec4 v_color;\n"
	"in vec2 v_texcoord;\n"
	"uniform sampler2D u_texture;\n"
	"out vec4 fragColor;\n"
	"void main() {\n"
	"    float alpha = texture(u_texture, v_texcoord).r;\n"
	"    fragColor = vec4(v_color.rgb, v_color.a * alpha);\n"
	"}\n";

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
		fprintf(stderr, "Shader compile error: %s\n", log);
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
		fprintf(stderr, "Shader link error: %s\n", log);
		exit(1);
	}

	glDeleteShader(vert);
	glDeleteShader(frag);
	return program;
}

static ShaderProgram build_program(const char *vert_src, const char *frag_src)
{
	GLuint v = compile_shader(GL_VERTEX_SHADER, vert_src);
	GLuint f = compile_shader(GL_FRAGMENT_SHADER, frag_src);
	ShaderProgram sp;
	sp.program = link_program(v, f);
	sp.u_projection = glGetUniformLocation(sp.program, "u_projection");
	sp.u_view = glGetUniformLocation(sp.program, "u_view");
	return sp;
}

void Shaders_initialize(Shaders *shaders)
{
	shaders->color_shader = build_program(color_vert_src, color_frag_src);
	shaders->text_shader = build_program(text_vert_src, text_frag_src);
	shaders->text_u_texture = glGetUniformLocation(
		shaders->text_shader.program, "u_texture");
}

void Shaders_cleanup(Shaders *shaders)
{
	glDeleteProgram(shaders->color_shader.program);
	glDeleteProgram(shaders->text_shader.program);
}

void Shader_set_matrices(const ShaderProgram *sp,
	const Mat4 *projection, const Mat4 *view)
{
	glUseProgram(sp->program);
	glUniformMatrix4fv(sp->u_projection, 1, GL_FALSE, projection->m);
	glUniformMatrix4fv(sp->u_view, 1, GL_FALSE, view->m);
}
